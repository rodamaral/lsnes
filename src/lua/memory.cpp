#include "cmdhelp/lua.hpp"
#include "core/command.hpp"
#include "core/debug.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rom.hpp"
#include "lua/address.hpp"
#include "lua/internal.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/skein.hpp"
#include "library/memoryspace.hpp"
#include "library/minmax.hpp"
#include "library/hex.hpp"
#include "library/int24.hpp"

namespace
{
	template<typename T, T (memory_space::*rfun)(uint64_t addr),
		bool (memory_space::*wfun)(uint64_t addr, T value)>
	void do_rw(lua::state& L, uint64_t addr, bool wrflag)
	{
		auto& core = CORE();
		if(wrflag) {
			T value = L.get_numeric_argument<T>(3, "aperture(write)");
			(core.memory->*wfun)(addr, value);
		} else
			L.pushnumber(static_cast<T>((core.memory->*rfun)(addr)));
	}

	template<typename T, T (memory_space::*rfun)(uint64_t addr)>
	int lua_read_memory(lua::state& L, lua::parameters& P)
	{
		auto addr = lua_get_read_address(P);
		L.pushnumber(static_cast<T>((CORE().memory->*rfun)(addr)));
		return 1;
	}

	template<typename T, bool (memory_space::*wfun)(uint64_t addr, T value)>
	int lua_write_memory(lua::state& L, lua::parameters& P)
	{
		auto addr = lua_get_read_address(P);
		T value = P.arg<T>();
		(CORE().memory->*wfun)(addr, value);
		return 0;
	}
}

class lua_mmap_struct
{
public:
	lua_mmap_struct(lua::state& L);
	static size_t overcommit() { return 0; }

	~lua_mmap_struct()
	{
	}

	int index(lua::state& L, lua::parameters& P)
	{
		const char* c = L.tostring(2);
		if(!c) {
			L.pushnil();
			return 1;
		}
		text c2(c);
		if(!mappings.count(c2)) {
			L.pushnil();
			return 1;
		}
		auto& x = mappings[c2];
		x.rw(L, x.addr, false);
		return 1;
	}
	int newindex(lua::state& L, lua::parameters& P)
	{
		const char* c = L.tostring(2);
		if(!c)
			return 0;
		text c2(c);
		if(!mappings.count(c2))
			return 0;
		auto& x = mappings[c2];
		x.rw(L, x.addr, true);
		return 0;
	}
	static int create(lua::state& L, lua::parameters& P)
	{
		lua::_class<lua_mmap_struct>::create(L);
		return 1;
	}
	int map(lua::state& L, lua::parameters& P);
	text print()
	{
		size_t s = mappings.size();
		return (stringfmt() << s << " " << ((s != 1) ? "mappings" : "mapping")).str();
	}
private:
	struct mapping
	{
		mapping() {}
		mapping(uint64_t _addr, void (*_rw)(lua::state& L, uint64_t addr, bool wrflag))
			: addr(_addr), rw(_rw)
		{
		}
		uint64_t addr;
		void (*rw)(lua::state& L, uint64_t addr, bool wrflag);
	};
	std::map<text, mapping> mappings;
};

namespace
{
	template<typename T, T (memory_space::*rfun)(uint64_t addr)>
	int aperture_read_fun(lua::state& L)
	{
		uint64_t base = L.tointeger(L.trampoline_upval(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(L.type(L.trampoline_upval(2)) == LUA_TNUMBER)
			size = L.tointeger(L.trampoline_upval(2));
		uint64_t addr = L.get_numeric_argument<uint64_t>(2, "aperture(read)");
		if(addr > size || addr + base < addr) {
			L.pushnumber(0);
			return 1;
		}
		addr += base;
		L.pushnumber(static_cast<T>((CORE().memory->*rfun)(addr)));
		return 1;
	}

	template<typename T, bool (memory_space::*wfun)(uint64_t addr, T value)>
	int aperture_write_fun(lua::state& L)
	{
		uint64_t base = L.tointeger(L.trampoline_upval(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(L.type(lua_upvalueindex(2)) == LUA_TNUMBER)
			size = L.tointeger(L.trampoline_upval(2));
		uint64_t addr = L.get_numeric_argument<uint64_t>(2, "aperture(write)");
		if(addr > size || addr + base < addr)
			return 0;
		addr += base;
		T value = L.get_numeric_argument<T>(3, "aperture(write)");
		(CORE().memory->*wfun)(addr, value);
		return 0;
	}

	template<typename T, T (memory_space::*rfun)(uint64_t addr), bool (memory_space::*wfun)(uint64_t addr,
		T value)>
	void aperture_make_fun(lua::state& L, uint64_t base, uint64_t size)
	{
		L.newtable();
		L.newtable();
		L.pushstring( "__index");
		L.pushnumber(base);
		if(!(size + 1))
			L.pushnil();
		else
			L.pushnumber(size);
		L.push_trampoline(aperture_read_fun<T, rfun>, 2);
		L.settable(-3);
		L.pushstring("__newindex");
		L.pushnumber(base);
		if(!(size + 1))
			L.pushnil();
		else
			L.pushnumber(size);
		L.push_trampoline(aperture_write_fun<T, wfun>, 2);
		L.settable(-3);
		L.setmetatable(-2);
	}

	void do_lua_error(lua::state& L, int ret)
	{
		if(!ret) return;
		switch(ret) {
		case LUA_ERRRUN:
			messages << "Error in Lua memory callback: " << L.get_string(-1, "errhnd") << std::endl;
			L.pop(1);
			return;
		case LUA_ERRMEM:
			messages << "Error in Lua memory callback (Out of memory)" << std::endl;
			return;
		case LUA_ERRERR:
			messages << "Error in Lua memory callback (Double fault)" << std::endl;
			return;
		default:
			messages << "Error in Lua memory callback (\?\?\?)" << std::endl;
			return;
		}
	}

	char CONST_lua_cb_list_key = 0;

	struct lua_debug_callback2 : public debug_context::callback_base
	{
		lua::state* L;
		uint64_t addr;
		debug_context::etype type;
		bool dead;
		const void* lua_fn;
		~lua_debug_callback2();
		void link_to_list();
		void set_lua_fn(int slot);
		void unregister();
		void callback(const debug_context::params& p);
		void killed(uint64_t addr, debug_context::etype type);
		static int on_lua_gc(lua::state& L);
		lua_debug_callback2* prev;
		lua_debug_callback2* next;
	};

	struct lua_debug_callback_dict
	{
		~lua_debug_callback_dict();
		std::map<std::pair<debug_context::etype, uint64_t>, lua_debug_callback2*> cblist;
		static int on_lua_gc(lua::state& L);
	};

	lua_debug_callback2::~lua_debug_callback2()
	{
		if(!prev) {
			L->pushlightuserdata(&CONST_lua_cb_list_key);
			L->rawget(LUA_REGISTRYINDEX);
			if(!L->isnil(-1)) {
				lua_debug_callback_dict* dc = (lua_debug_callback_dict*)L->touserdata(-1);
				std::pair<debug_context::etype, uint64_t> key = std::make_pair(type, addr);
				if(dc->cblist.count(key)) {
					dc->cblist[key] = next;
					if(!next)
						dc->cblist.erase(key);
				}
			}
			L->pop(1);
		}
		//Unlink from list.
		if(prev) prev->next = next;
		if(next) next->prev = prev;
		prev = next = NULL;
	}

	void lua_debug_callback2::link_to_list()
	{
		prev = NULL;
		L->pushlightuserdata(&CONST_lua_cb_list_key);
		L->rawget(LUA_REGISTRYINDEX);
		if(L->isnil(-1)) {
			//No existing dict, create one.
			L->pop(1);
			L->pushlightuserdata(&CONST_lua_cb_list_key);
			lua_debug_callback_dict* D = (lua_debug_callback_dict*)
				L->newuserdata(sizeof(lua_debug_callback_dict));
			new(D) lua_debug_callback_dict;
			L->newtable();
			L->pushstring("__gc");
			L->push_trampoline(&lua_debug_callback_dict::on_lua_gc, 0);
			L->rawset(-3);
			L->setmetatable(-2);
			L->rawset(LUA_REGISTRYINDEX);
		}
		L->pushlightuserdata(&CONST_lua_cb_list_key);
		L->rawget(LUA_REGISTRYINDEX);
		lua_debug_callback2* was = NULL;
		lua_debug_callback_dict* dc = (lua_debug_callback_dict*)L->touserdata(-1);
		std::pair<debug_context::etype, uint64_t> key = std::make_pair(type, addr);
		if(dc->cblist.count(key))
			was = dc->cblist[key];
		dc->cblist[key] = this;
		next = was;
		L->pop(1);
	}

	void lua_debug_callback2::set_lua_fn(int slot)
	{
		//Convert to absolute slot.
		if(slot < 0)
			slot = L->gettop() + slot;
		//Write the function.
		L->pushlightuserdata((char*)this + 1);
		L->pushvalue(slot);
		L->rawset(LUA_REGISTRYINDEX);
		lua_fn = L->topointer(slot);
	}

	void lua_debug_callback2::unregister()
	{
		//Unregister.
		CORE().dbg->remove_callback(addr, type, *this);
		dead = true;
		//Delink from Lua, prompting Lua to GC this.
		L->pushlightuserdata(this);
		L->pushnil();
		L->rawset(LUA_REGISTRYINDEX);
		L->pushlightuserdata((char*)this + 1);
		L->pushnil();
		L->rawset(LUA_REGISTRYINDEX);
	}

	void lua_debug_callback2::callback(const debug_context::params& p)
	{
		L->pushlightuserdata((char*)this + 1);
		L->rawget(LUA_REGISTRYINDEX);
		switch(p.type) {
		case debug_context::DEBUG_READ:
		case debug_context::DEBUG_WRITE:
		case debug_context::DEBUG_EXEC:
			L->pushnumber(p.rwx.addr);
			L->pushnumber(p.rwx.value);
			do_lua_error(*L, L->pcall(2, 0, 0));
			break;
		case debug_context::DEBUG_TRACE:
			L->pushnumber(p.trace.cpu);
			L->pushstring(p.trace.decoded_insn);
			L->pushboolean(p.trace.true_insn);
			do_lua_error(*L, L->pcall(3, 0, 0));
			break;
		case debug_context::DEBUG_FRAME:
			L->pushnumber(p.frame.frame);
			L->pushboolean(p.frame.loadstated);
			do_lua_error(*L, L->pcall(2, 0, 0));
			break;
		default:
			//Remove the junk from stack.
			L->pop(1);
			break;
		}
	}

	void lua_debug_callback2::killed(uint64_t addr, debug_context::etype type)
	{
		//Assume this has been unregistered.
		dead = true;
		//Delink from Lua, lua will GC this.
		L->pushlightuserdata(this);
		L->pushnil();
		L->rawset(LUA_REGISTRYINDEX);
		L->pushlightuserdata((char*)this + 1);
		L->pushnil();
		L->rawset(LUA_REGISTRYINDEX);
	}

	int lua_debug_callback2::on_lua_gc(lua::state& L)
	{
		//We need to destroy the object.
		lua_debug_callback2* D = (lua_debug_callback2*)L.touserdata(1);
		if(!D->dead) {
			//Unregister this!
			CORE().dbg->remove_callback(D->addr, D->type, *D);
			D->dead = true;
		}
		D->~lua_debug_callback2();
		return 0;
	}

	lua_debug_callback_dict::~lua_debug_callback_dict()
	{
	}

	int lua_debug_callback_dict::on_lua_gc(lua::state& L)
	{
		L.pushlightuserdata(&CONST_lua_cb_list_key);
		L.pushnil();
		L.rawset(LUA_REGISTRYINDEX);
		lua_debug_callback_dict* D = (lua_debug_callback_dict*)L.touserdata(1);
		D->~lua_debug_callback_dict();
		return 0;
	}
}

template<debug_context::etype type>
void handle_registerX(lua::state& L, uint64_t addr, int lfn)
{
	//Put the context in userdata so it can be gc'd when Lua context is terminated.
	lua_debug_callback2* D = (lua_debug_callback2*)L.newuserdata(sizeof(lua_debug_callback2));
	new(D) lua_debug_callback2;
	L.newtable();
	L.pushstring("__gc");
	L.push_trampoline(&lua_debug_callback2::on_lua_gc, 0);
	L.rawset(-3);
	L.setmetatable(-2);
	L.pushlightuserdata(D);
	L.pushvalue(-2);
	L.rawset(LUA_REGISTRYINDEX);
	L.pop(1); //Pop the copy of object.

	D->L = &L.get_master();
	D->addr = addr;
	D->type = type;
	D->dead = false;
	D->set_lua_fn(lfn);
	D->link_to_list();

	CORE().dbg->add_callback(addr, type, *D);
}

template<debug_context::etype type>
void handle_unregisterX(lua::state& L, uint64_t addr, int lfn)
{
	lua_debug_callback_dict* Dx;
	lua_debug_callback2* D = NULL;
	L.pushlightuserdata(&CONST_lua_cb_list_key);
	L.rawget(LUA_REGISTRYINDEX);
	if(!L.isnil(-1)) {
		Dx = (lua_debug_callback_dict*)L.touserdata(-1);
		auto key = std::make_pair(type, addr);
		if(Dx->cblist.count(key))
			D = Dx->cblist[key];
		L.pop(1);
		while(D) {
			if(D->dead || D->type != type || D->addr != addr || L.topointer(lfn) != D->lua_fn) {
				D = D->next;
				continue;
			}
			//Remove this.
			auto Dold = D;
			D = D->next;
			Dold->unregister();
		}
	} else
		L.pop(1);
}

typedef void(*dummy1_t)(lua::state& L, uint64_t addr, int lfn);
dummy1_t dummy_628963286932869328692386963[] = {
	handle_registerX<debug_context::DEBUG_READ>,
	handle_registerX<debug_context::DEBUG_WRITE>,
	handle_registerX<debug_context::DEBUG_EXEC>,
	handle_unregisterX<debug_context::DEBUG_READ>,
	handle_unregisterX<debug_context::DEBUG_WRITE>,
	handle_unregisterX<debug_context::DEBUG_EXEC>,
};

namespace
{
	template<debug_context::etype type, bool reg>
	int lua_registerX(lua::state& L, lua::parameters& P)
	{
		uint64_t addr;
		int lfn;
		if(P.is_nil() && type != debug_context::DEBUG_TRACE) {
			addr = 0xFFFFFFFFFFFFFFFFULL;
			P.skip();
		} else if(type != debug_context::DEBUG_TRACE)
			addr = lua_get_read_address(P);
		else
			P(addr);
		P(P.function(lfn));

		if(reg) {
			handle_registerX<type>(L, addr, lfn);
			L.pushvalue(lfn);
			return 1;
		} else {
			handle_unregisterX<type>(L, addr, lfn);
			return 0;
		}
	}

	command::fnptr<> CMD_callbacks_show_lua(lsnes_cmds, CLUA::scb,
		[]() throw(std::bad_alloc, std::runtime_error) {
		auto& core = CORE();
		lua::state& L = *core.lua;
		lua_debug_callback2* D;
		lua_debug_callback_dict* Dx;
		L.pushlightuserdata(&CONST_lua_cb_list_key);
		L.rawget(LUA_REGISTRYINDEX);
		if(!L.isnil(-1)) {
			Dx = (lua_debug_callback_dict*)L.touserdata(-1);
			for(auto Dy : Dx->cblist) {
				D = Dy.second;
				while(D) {
					messages << "addr=" << core.memory->address_to_textual(D->addr)
						<< " type=" << D->type << " handle=" << D << " dead=" << D->dead
						<< " lua_fn=" << D->lua_fn << std::endl;
					D = D->next;
				}
			}
		}
		L.pop(1);
		});

	template<typename T, T (memory_space::*rfun)(uint64_t addr), bool (memory_space::*wfun)(uint64_t addr,
		T value)>
	int lua_mmap_memory(lua::state& L, lua::parameters& P)
	{
		if(P.is_novalue()) {
			static char deprecation;
			if(L.do_once(&deprecation))
				messages << P.get_fname() << ": Mapping entiere space is deprecated." << std::endl;
			aperture_make_fun<T, rfun, wfun>(L.get_master(), 0, 0xFFFFFFFFFFFFFFFFULL);
			return 1;
		}
		auto addr = lua_get_read_address(P);
		auto size = P.arg<uint64_t>();
		if(!size)
			throw std::runtime_error("Aperture with zero size is not valid");
		aperture_make_fun<T, rfun, wfun>(L.get_master(), addr, size - 1);
		return 1;
	}

	int handle_push_vma(lua::state& L, memory_space::region& r)
	{
		L.newtable();
		L.pushstring("region_name");
		L.pushlstring(r.name.c_str(), r.name.size());
		L.settable(-3);
		L.pushstring("baseaddr");
		L.pushnumber(r.base);
		L.settable(-3);
		L.pushstring("size");
		L.pushnumber(r.size);
		L.settable(-3);
		L.pushstring("lastaddr");
		L.pushnumber(r.last_address());
		L.settable(-3);
		L.pushstring("readonly");
		L.pushboolean(r.readonly);
		L.settable(-3);
		L.pushstring("iospace");
		L.pushboolean(r.special);
		L.settable(-3);
		L.pushstring("native_endian");
		L.pushboolean(r.endian == 0);
		L.settable(-3);
		L.pushstring("endian");
		L.pushnumber(r.endian);
		L.settable(-3);
		return 1;
	}

	template<bool write, bool sign> int memory_scattergather(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		static char deprecation;
		uint64_t val = 0;
		unsigned shift = 0;
		uint64_t addr = 0;
		uint64_t vmabase = 0;
		bool have_vmabase = false;
		if(write)
			val = P.arg<uint64_t>();
		while(P.more()) {
			if(P.is_boolean()) {
				if(P.arg<bool>())
					addr++;
				else
					addr--;
			} else if(P.is<lua_address>()) {
				auto laddr = P.arg<lua_address*>();
				vmabase = lua_get_vmabase(laddr->get_vma());
				have_vmabase = true;
				addr = laddr->get_offset();
				//No continue, fall through.
			} else if(P.is_string()) {
				vmabase = lua_get_vmabase(P.arg<text>());
				have_vmabase = true;
				continue;
			} else
				addr = P.arg<uint64_t>();
			if(!have_vmabase && L.do_once(&deprecation))
				messages << P.get_fname() << ": Global memory form is deprecated." << std::endl;
			if(write)
				core.memory->write<uint8_t>(addr + vmabase, val >> shift);
			else
				val = val + ((uint64_t)core.memory->read<uint8_t>(addr + vmabase) << shift);
			shift += 8;
		}
		if(!write) {
			int64_t sval = val;
			if(val >= (1ULL << (shift - 1))) sval -= (1ULL << shift);
			if(sign) L.pushnumber(sval); else L.pushnumber(val);
		}
		return write ? 0 : 1;
	}

#define BLOCKSIZE 256

	int vma_count(lua::state& L, lua::parameters& P)
	{
		L.pushnumber(CORE().memory->get_regions().size());
		return 1;
	}

	int cheat(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, value;

		addr = lua_get_read_address(P);

		if(P.is_novalue()) {
			core.dbg->clear_cheat(addr);
		} else {
			P(value);
			core.dbg->set_cheat(addr, value);
		}
		return 0;
	}

	int setxmask(lua::state& L, lua::parameters& P)
	{
		auto value = P.arg<uint64_t>();
		CORE().dbg->setxmask(value);
		return 0;
	}

	int read_vma(lua::state& L, lua::parameters& P)
	{
		uint32_t num;

		P(num);

		std::list<memory_space::region*> regions = CORE().memory->get_regions();
		uint32_t j = 0;
		for(auto i = regions.begin(); i != regions.end(); i++, j++)
			if(j == num)
				return handle_push_vma(L, **i);
		L.pushnil();
		return 1;
	}

	int find_vma(lua::state& L, lua::parameters& P)
	{
		uint64_t addr;

		P(addr);

		auto r = CORE().memory->lookup(addr);
		if(r.first)
			return handle_push_vma(L, *r.first);
		L.pushnil();
		return 1;
	}

	int hash_state(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		auto x = core.rom->save_core_state();
		size_t offset = x.size() - 32;
		L.pushlstring(hex::b_to((uint8_t*)&x[offset], 32));
		return 1;
	}

	template<typename H, void(*update)(H& state, const char* mem, size_t memsize),
		text(*read)(H& state), bool extra>
	int hash_core(H& state, lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		text hash;
		uint64_t addr, size, low, high;
		uint64_t stride = 0, rows = 1;
		bool mappable = true;
		char buffer[BLOCKSIZE];

		addr = lua_get_read_address(P);
		P(size);
		if(extra) {
			P(P.optional(rows, 1));
			if(rows > 1)
				P(stride);
		}

		rpair(low, high) = memoryspace_row_bounds(addr, size, rows, stride);
		if(low > high || high - low + 1 == 0)
			mappable = false;

		char* pbuffer = mappable ? core.memory->get_physical_mapping(low, high - low + 1) : NULL;
		if(low > high) {
		} else if(pbuffer) {
			uint64_t offset = addr - low;
			for(uint64_t i = 0; i < rows; i++) {
				update(state, pbuffer + offset, size);
				offset += stride;
			}
		} else {
			uint64_t offset = addr;
			for(uint64_t i = 0; i < rows; i++) {
				size_t sz = size;
				while(sz > 0) {
					size_t ssz = min(sz, static_cast<size_t>(BLOCKSIZE));
					for(size_t i = 0; i < ssz; i++)
						buffer[i] = core.memory->read<uint8_t>(offset + i);
					offset += ssz;
					sz -= ssz;
					update(state, buffer, ssz);
				}
				offset += (stride - size);
			}
		}
		hash = read(state);
		L.pushlstring(hash);
		return 1;
	}

	void lua_sha256_update(sha256& s, const char* ptr, size_t size)
	{
		s.write(ptr, size);
	}

	text lua_sha256_read(sha256& s)
	{
		return s.read();
	}

	void lua_skein_update(skein::hash& s, const char* ptr, size_t size)
	{
		s.write(reinterpret_cast<const uint8_t*>(ptr), size);
	}

	text lua_skein_read(skein::hash& s)
	{
		uint8_t buf[32];
		s.read(buf);
		return hex::b_to(buf, 32, false);
	}

	template<bool extended>
	int hash_region(lua::state& L, lua::parameters& P)
	{
		sha256 h;
		return hash_core<sha256, lua_sha256_update, lua_sha256_read, extended>(h, L, P);
	}

	int hash_region_skein(lua::state& L, lua::parameters& P)
	{
		skein::hash h(skein::hash::PIPE_512, 256);
		return hash_core<skein::hash, lua_skein_update, lua_skein_read, true>(h, L, P);
	}

	template<bool cmp>
	int copy_to_host(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, daddr, size, low, high;
		uint64_t stride = 0, rows = 1;
		bool equals = true, mappable = true;

		addr = lua_get_read_address(P);
		P(daddr, size, P.optional(rows, 1));
		if(rows > 1)
			P(stride);

		rpair(low, high) = memoryspace_row_bounds(addr, size, rows, stride);
		if(low > high || high - low + 1 == 0)
			mappable = false;
		if(rows && (size_t)(size * rows) / rows != size)
			throw std::runtime_error("Size to copy too large");
		if((size_t)(daddr + rows * size) < daddr)
			throw std::runtime_error("Size to copy too large");

		auto& h = core.mlogic->get_mfile().dyn.host_memory;
		if(daddr + rows * size > h.size()) {
			equals = false;
			h.resize(daddr + rows * size);
		}

		char* pbuffer = mappable ? core.memory->get_physical_mapping(low, high - low + 1) : NULL;
		if(!size && !rows) {
		} else if(pbuffer) {
			//Mapable.
			uint64_t offset = addr - low;
			for(uint64_t i = 0; i < rows; i++) {
				bool eq = (cmp && !memcmp(&h[daddr + i * size], pbuffer + offset, size));
				if(!eq)
					memcpy(&h[daddr + i * size], pbuffer + offset, size);
				equals &= eq;
				offset += stride;
			}
		} else {
			//Not mapable.
			for(uint64_t i = 0; i < rows; i++) {
				uint64_t addr1 = addr + i * stride;
				uint64_t addr2 = daddr + i * size;
				for(uint64_t j = 0; j < size; j++) {
					uint8_t byte = core.memory->read<uint8_t>(addr1 + j);
					bool eq = (cmp && h[addr2 + j] == (char)byte);
					if(!eq)
						h[addr2 + j] = byte;
					equals &= eq;
				}
			}
		}
		if(cmp)
			L.pushboolean(equals);
		return cmp ? 1 : 0;
	}

	int readregion(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, size;

		addr = lua_get_read_address(P);
		P(size);

		L.newtable();
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			core.memory->read_range(addr, buffer, rsize);
			for(size_t i = 0; i < rsize; i++) {
				L.pushnumber(ctr++);
				L.pushnumber(static_cast<unsigned char>(buffer[i]));
				L.settable(-3);
			}
			addr += rsize;
			size -= rsize;
		}
		return 1;
	}

	int writeregion(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, size;
		int ltbl;

		addr = lua_get_read_address(P);
		P(size, P.table(ltbl));

		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			for(size_t i = 0; i < rsize; i++) {
				L.pushnumber(ctr++);
				L.gettable(ltbl);
				buffer[i] = L.tointeger(-1);
				L.pop(1);
			}
			core.memory->write_range(addr, buffer, rsize);
			addr += rsize;
			size -= rsize;
		}
		return 1;
	}

	lua::functions LUA_memory_fns(lua_func_misc, "memory", {
		{"vma_count", vma_count},
		{"cheat", cheat},
		{"setxmask", setxmask},
		{"read_vma", read_vma},
		{"find_vma", find_vma},
		{"hash_state", hash_state},
		{"hash_region", hash_region<false>},
		{"hash_region2", hash_region<true>},
		{"hash_region_skein", hash_region_skein},
		{"store", copy_to_host<false>},
		{"storecmp", copy_to_host<true>},
		{"readregion", readregion},
		{"writeregion", writeregion},
		{"read_sg", memory_scattergather<false, false>},
		{"sread_sg", memory_scattergather<false, true>},
		{"write_sg", memory_scattergather<true, false>},
		{"readbyte", lua_read_memory<uint8_t, &memory_space::read<uint8_t>>},
		{"readsbyte", lua_read_memory<int8_t, &memory_space::read<int8_t>>},
		{"readword", lua_read_memory<uint16_t, &memory_space::read<uint16_t>>},
		{"readsword", lua_read_memory<int16_t, &memory_space::read<int16_t>>},
		{"readhword", lua_read_memory<ss_uint24_t, &memory_space::read<ss_uint24_t>>},
		{"readshword", lua_read_memory<ss_int24_t, &memory_space::read<ss_int24_t>>},
		{"readdword", lua_read_memory<uint32_t, &memory_space::read<uint32_t>>},
		{"readsdword", lua_read_memory<int32_t, &memory_space::read<int32_t>>},
		{"readqword", lua_read_memory<uint64_t, &memory_space::read<uint64_t>>},
		{"readsqword", lua_read_memory<int64_t, &memory_space::read<int64_t>>},
		{"readfloat", lua_read_memory<float, &memory_space::read<float>>},
		{"readdouble", lua_read_memory<double, &memory_space::read<double>>},
		{"writebyte", lua_write_memory<uint8_t, &memory_space::write<uint8_t>>},
		{"writeword", lua_write_memory<uint16_t, &memory_space::write<uint16_t>>},
		{"writehword", lua_write_memory<ss_uint24_t, &memory_space::write<ss_uint24_t>>},
		{"writedword", lua_write_memory<uint32_t, &memory_space::write<uint32_t>>},
		{"writeqword", lua_write_memory<uint64_t, &memory_space::write<uint64_t>>},
		{"writefloat", lua_write_memory<float, &memory_space::write<float>>},
		{"writedouble", lua_write_memory<double, &memory_space::write<double>>},
		{"mapbyte", lua_mmap_memory<uint8_t, &memory_space::read<uint8_t>, &memory_space::write<uint8_t>>},
		{"mapsbyte", lua_mmap_memory<int8_t, &memory_space::read<int8_t>, &memory_space::write<int8_t>>},
		{"mapword", lua_mmap_memory<uint16_t, &memory_space::read<uint16_t>, &memory_space::write<uint16_t>>},
		{"mapsword", lua_mmap_memory<int16_t, &memory_space::read<int16_t>, &memory_space::write<int16_t>>},
		{"maphword", lua_mmap_memory<ss_uint24_t, &memory_space::read<ss_uint24_t>,
			&memory_space::write<ss_uint24_t>>},
		{"mapshword", lua_mmap_memory<ss_int24_t, &memory_space::read<ss_int24_t>,
			&memory_space::write<ss_int24_t>>},
		{"mapdword", lua_mmap_memory<uint32_t, &memory_space::read<uint32_t>,
			&memory_space::write<uint32_t>>},
		{"mapsdword", lua_mmap_memory<int32_t, &memory_space::read<int32_t>, &memory_space::write<int32_t>>},
		{"mapqword", lua_mmap_memory<uint64_t, &memory_space::read<uint64_t>,
			&memory_space::write<uint64_t>>},
		{"mapsqword", lua_mmap_memory<int64_t, &memory_space::read<int64_t>, &memory_space::write<int64_t>>},
		{"mapfloat", lua_mmap_memory<float, &memory_space::read<float>, &memory_space::write<float>>},
		{"mapdouble", lua_mmap_memory<double, &memory_space::read<double>, &memory_space::write<double>>},
		{"registerread", lua_registerX<debug_context::DEBUG_READ, true>},
		{"unregisterread", lua_registerX<debug_context::DEBUG_READ, false>},
		{"registerwrite", lua_registerX<debug_context::DEBUG_WRITE, true>},
		{"unregisterwrite", lua_registerX<debug_context::DEBUG_WRITE, false>},
		{"registerexec", lua_registerX<debug_context::DEBUG_EXEC, true>},
		{"unregisterexec", lua_registerX<debug_context::DEBUG_EXEC, false>},
		{"registertrace", lua_registerX<debug_context::DEBUG_TRACE, true>},
		{"unregistertrace", lua_registerX<debug_context::DEBUG_TRACE, false>},
	});

	lua::_class<lua_mmap_struct> LUA_class_mmap_struct(lua_class_memory, "MMAP_STRUCT", {
		{"new", &lua_mmap_struct::create},
	}, {
		{"__index", &lua_mmap_struct::index},
		{"__newindex", &lua_mmap_struct::newindex},
		{"__call", &lua_mmap_struct::map},
	}, &lua_mmap_struct::print);
}

int lua_mmap_struct::map(lua::state& L, lua::parameters& P)
{
	text name, type;
	uint64_t addr;

	P(P.skipped(), name);
	addr = lua_get_read_address(P);
	P(type);

	if(type == "byte")
		mappings[name] = mapping(addr, do_rw<uint8_t, &memory_space::read<uint8_t>,
			&memory_space::write<uint8_t>>);
	else if(type == "sbyte")
		mappings[name] = mapping(addr, do_rw<int8_t, &memory_space::read<int8_t>,
			&memory_space::write<int8_t>>);
	else if(type == "word")
		mappings[name] = mapping(addr, do_rw<uint16_t, &memory_space::read<uint16_t>,
			&memory_space::write<uint16_t>>);
	else if(type == "sword")
		mappings[name] = mapping(addr, do_rw<int16_t, &memory_space::read<int16_t>,
			&memory_space::write<int16_t>>);
	else if(type == "hword")
		mappings[name] = mapping(addr, do_rw<ss_uint24_t, &memory_space::read<ss_uint24_t>,
			&memory_space::write<ss_uint24_t>>);
	else if(type == "shword")
		mappings[name] = mapping(addr, do_rw<ss_int24_t, &memory_space::read<ss_int24_t>,
			&memory_space::write<ss_int24_t>>);
	else if(type == "dword")
		mappings[name] = mapping(addr, do_rw<uint32_t, &memory_space::read<uint32_t>,
			&memory_space::write<uint32_t>>);
	else if(type == "sdword")
		mappings[name] = mapping(addr, do_rw<int32_t, &memory_space::read<int32_t>,
			&memory_space::write<int32_t>>);
	else if(type == "qword")
		mappings[name] = mapping(addr, do_rw<uint64_t, &memory_space::read<uint64_t>,
			&memory_space::write<uint64_t>>);
	else if(type == "sqword")
		mappings[name] = mapping(addr, do_rw<int64_t, &memory_space::read<int64_t>,
			&memory_space::write<int64_t>>);
	else if(type == "float")
		mappings[name] = mapping(addr, do_rw<float, &memory_space::read<float>,
			&memory_space::write<float>>);
	else if(type == "double")
		mappings[name] = mapping(addr, do_rw<double, &memory_space::read<double>,
			&memory_space::write<double>>);
	else
		(stringfmt() << P.get_fname() << ": Bad type").throwex();
	return 0;
}

lua_mmap_struct::lua_mmap_struct(lua::state& L)
{
}
