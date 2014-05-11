#include "core/command.hpp"
#include "lua/internal.hpp"
#include "core/debug.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rom.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/skein.hpp"
#include "library/minmax.hpp"
#include "library/hex.hpp"
#include "library/int24.hpp"

uint64_t lua_get_vmabase(const std::string& vma)
{
	for(auto i : CORE().memory.get_regions())
		if(i->name == vma)
			return i->base;
	throw std::runtime_error("No such VMA");
}

uint64_t lua_get_read_address(lua::parameters& P)
{
	static std::map<std::string, char> deprecation_keys;
	char* deprecation = &deprecation_keys[P.get_fname()];
	uint64_t vmabase = 0;
	if(P.is_string())
		vmabase = lua_get_vmabase(P.arg<std::string>());
	else {
		//Deprecated.
		if(P.get_state().do_once(deprecation))
			messages << P.get_fname() << ": Global memory form is deprecated." << std::endl; 
	}
	auto addr = P.arg<uint64_t>();
	return addr + vmabase;
}

namespace
{
	template<typename T, T (memory_space::*rfun)(uint64_t addr),
		bool (memory_space::*wfun)(uint64_t addr, T value)>
	void do_rw(lua::state& L, uint64_t addr, bool wrflag)
	{
		if(wrflag) {
			T value = L.get_numeric_argument<T>(3, "aperture(write)");
			(CORE().memory.*wfun)(addr, value);
		} else
			L.pushnumber(static_cast<T>((CORE().memory.*rfun)(addr)));
	}

	template<typename T, T (memory_space::*rfun)(uint64_t addr)>
	int lua_read_memory(lua::state& L, lua::parameters& P)
	{
		auto addr = lua_get_read_address(P);
		L.pushnumber(static_cast<T>((CORE().memory.*rfun)(addr)));
		return 1;
	}

	template<typename T, bool (memory_space::*wfun)(uint64_t addr, T value)>
	int lua_write_memory(lua::state& L, lua::parameters& P)
	{
		auto addr = lua_get_read_address(P);
		T value = P.arg<T>();
		(CORE().memory.*wfun)(addr, value);
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
		std::string c2(c);
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
		std::string c2(c);
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
	std::string print()
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
	std::map<std::string, mapping> mappings;
};

namespace
{
	template<typename T, T (memory_space::*rfun)(uint64_t addr)>
	int aperture_read_fun(lua_State* _L)
	{
		lua::state& mL = *reinterpret_cast<lua::state*>(lua_touserdata(_L, lua_upvalueindex(3)));
		lua::state L(mL, _L);

		uint64_t base = L.tonumber(lua_upvalueindex(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(L.type(lua_upvalueindex(2)) == LUA_TNUMBER)
			size = L.tonumber(lua_upvalueindex(2));
		uint64_t addr = L.get_numeric_argument<uint64_t>(2, "aperture(read)");
		if(addr > size || addr + base < addr) {
			L.pushnumber(0);
			return 1;
		}
		addr += base;
		L.pushnumber(static_cast<T>((CORE().memory.*rfun)(addr)));
		return 1;
	}

	template<typename T, bool (memory_space::*wfun)(uint64_t addr, T value)>
	int aperture_write_fun(lua_State* _L)
	{
		lua::state& mL = *reinterpret_cast<lua::state*>(lua_touserdata(_L, lua_upvalueindex(3)));
		lua::state L(mL, _L);

		uint64_t base = L.tonumber(lua_upvalueindex(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(L.type(lua_upvalueindex(2)) == LUA_TNUMBER)
			size = L.tonumber(lua_upvalueindex(2));
		uint64_t addr = L.get_numeric_argument<uint64_t>(2, "aperture(write)");
		if(addr > size || addr + base < addr)
			return 0;
		addr += base;
		T value = L.get_numeric_argument<T>(3, "aperture(write)");
		(CORE().memory.*wfun)(addr, value);
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
		L.pushlightuserdata(&L);
		L.pushcclosure(aperture_read_fun<T, rfun>, 3);
		L.settable(-3);
		L.pushstring("__newindex");
		L.pushnumber(base);
		if(!(size + 1))
			L.pushnil();
		else
			L.pushnumber(size);
		L.pushlightuserdata(&L);
		L.pushcclosure(aperture_write_fun<T, wfun>, 3);
		L.settable(-3);
		L.setmetatable(-2);
	}

	struct lua_debug_callback
	{
		uint64_t addr;
		debug_type type;
		debug_handle h;
		bool dead;
		const void* lua_fn;
		static int dtor(lua_State* L)
		{
			lua_debug_callback* D = (lua_debug_callback*)lua_touserdata(L, 1);
			return D->_dtor(L);
		}
		int _dtor(lua_State* L);
	};
	std::map<uint64_t, std::list<lua_debug_callback*>> cbs;

	int lua_debug_callback::_dtor(lua_State* L)
	{
		if(dead) return 0;
		dead = true;
		lua_pushlightuserdata(L, &type);
		lua_pushnil(L);
		lua_rawset(L, LUA_REGISTRYINDEX);
		debug_remove_callback(addr, type, h);
		for(auto j = cbs[addr].begin(); j != cbs[addr].end(); j++)
			if(*j == this) {
				cbs[addr].erase(j);
				break;
			}
		if(cbs[addr].empty())
			cbs.erase(addr);
		return 0;
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
}

template<debug_type type>
void handle_registerX(lua::state& L, uint64_t addr, int lfn)
{
	auto& cbl = cbs[addr];

	//Put the context in userdata so it can be gc'd when Lua context is terminated.
	lua_debug_callback* D = (lua_debug_callback*)L.newuserdata(sizeof(lua_debug_callback));
	L.newtable();
	L.pushstring("__gc");
	L.pushcclosure(&lua_debug_callback::dtor, 0);
	L.rawset(-3);
	L.setmetatable(-2);
	L.pushlightuserdata(&D->addr);
	L.pushvalue(-2);
	L.rawset(LUA_REGISTRYINDEX);
	L.pop(1); //Pop the copy of object.

	cbl.push_back(D);

	D->dead = false;
	D->addr = addr;
	D->type = type;
	D->lua_fn = L.topointer(lfn);
	lua::state* LL = &L.get_master();
	void* D2 = &D->type;
	if(type != DEBUG_TRACE)
		D->h = debug_add_callback(addr, type, [LL, D2](uint64_t addr, uint64_t value) {
			LL->pushlightuserdata(D2);
			LL->rawget(LUA_REGISTRYINDEX);
			LL->pushnumber(addr);
			LL->pushnumber(value);
			do_lua_error(*LL, LL->pcall(2, 0, 0));
		}, [LL, D]() {
			LL->pushlightuserdata(&D->addr);
			LL->pushnil();
			LL->rawset(LUA_REGISTRYINDEX);
			D->_dtor(LL->handle());
		});
	else
		D->h = debug_add_trace_callback(addr, [LL, D2](uint64_t proc, const char* str, bool true_insn) {
			LL->pushlightuserdata(D2);
			LL->rawget(LUA_REGISTRYINDEX);
			LL->pushnumber(proc);
			LL->pushstring(str);
			LL->pushboolean(true_insn);
			do_lua_error(*LL, LL->pcall(3, 0, 0));
		}, [LL, D]() {
			LL->pushlightuserdata(&D->addr);
			LL->pushnil();
			LL->rawset(LUA_REGISTRYINDEX);
			D->_dtor(LL->handle());
		});
	L.pushlightuserdata(D2);
	L.pushvalue(lfn);
	L.rawset(LUA_REGISTRYINDEX);
}

template<debug_type type>
void handle_unregisterX(lua::state& L, uint64_t addr, int lfn)
{
	if(!cbs.count(addr))
		return;
	auto& cbl = cbs[addr];
	for(auto i = cbl.begin(); i != cbl.end(); i++) {
		if((*i)->type != type) continue;
		if(L.topointer(lfn) != (*i)->lua_fn) continue;
		L.pushlightuserdata(&(*i)->type);
		L.pushnil();
		L.rawset(LUA_REGISTRYINDEX);
		(*i)->_dtor(L.handle());
		//Lua will GC the object.
		break;
	}
}

typedef void(*dummy1_t)(lua::state& L, uint64_t addr, int lfn);
dummy1_t dummy_628963286932869328692386963[] = {
	handle_registerX<DEBUG_READ>,
	handle_registerX<DEBUG_WRITE>,
	handle_registerX<DEBUG_EXEC>,
	handle_unregisterX<DEBUG_READ>,
	handle_unregisterX<DEBUG_WRITE>,
	handle_unregisterX<DEBUG_EXEC>,
};

namespace
{
	template<debug_type type, bool reg>
	int lua_registerX(lua::state& L, lua::parameters& P)
	{
		uint64_t addr;
		int lfn;
		if(P.is_nil() && type != DEBUG_TRACE) {
			addr = 0xFFFFFFFFFFFFFFFFULL;
			P.skip();
		} else if(type != DEBUG_TRACE)
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

	command::fnptr<> callbacks_show_lua(lsnes_cmds, "show-lua-callbacks", "", "",
		[]() throw(std::bad_alloc, std::runtime_error) {
		for(auto& i : cbs)
			for(auto& j : i.second)
				messages << "addr=" << j->addr << " type=" << j->type << " handle="
					<< j->h.handle << " dead=" << j->dead << " lua_fn="
					<< j->lua_fn << std::endl;
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

	int handle_push_vma(lua::state& L, memory_region& r)
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
			} else if(P.is_string()) {
				vmabase = lua_get_vmabase(P.arg<std::string>());
				have_vmabase = true;
				continue;
			} else
				addr = P.arg<uint64_t>();
			if(!have_vmabase && L.do_once(&deprecation))
				messages << P.get_fname() << ": Global memory form is deprecated." << std::endl;
			if(write)
				CORE().memory.write<uint8_t>(addr + vmabase, val >> shift);
			else
				val = val + ((uint64_t)CORE().memory.read<uint8_t>(addr + vmabase) << shift);
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
		L.pushnumber(CORE().memory.get_regions().size());
		return 1;
	}

	int cheat(lua::state& L, lua::parameters& P)
	{
		uint64_t addr, value;

		addr = lua_get_read_address(P);

		if(P.is_novalue()) {
			debug_clear_cheat(addr);
		} else {
			P(value);
			debug_set_cheat(addr, value);
		}
		return 0;
	}

	int setxmask(lua::state& L, lua::parameters& P)
	{
		auto value = P.arg<uint64_t>();
		debug_setxmask(value);
		return 0;
	}

	int read_vma(lua::state& L, lua::parameters& P)
	{
		uint32_t num;

		P(num);

		std::list<memory_region*> regions = CORE().memory.get_regions();
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

		auto r = CORE().memory.lookup(addr);
		if(r.first)
			return handle_push_vma(L, *r.first);
		L.pushnil();
		return 1;
	}

	int hash_state(lua::state& L, lua::parameters& P)
	{
		auto x = our_rom.save_core_state();
		size_t offset = x.size() - 32;
		L.pushlstring(hex::b_to((uint8_t*)&x[offset], 32));
		return 1;
	}

	template<typename H, void(*update)(H& state, const char* mem, size_t memsize),
		std::string(*read)(H& state), bool extra>
	int hash_core(H& state, lua::state& L, lua::parameters& P)
	{
		std::string hash;
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

		char* pbuffer = mappable ? CORE().memory.get_physical_mapping(low, high - low + 1) : NULL;
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
						buffer[i] = CORE().memory.read<uint8_t>(offset + i);
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

	std::string lua_sha256_read(sha256& s)
	{
		return s.read();
	}

	void lua_skein_update(skein::hash& s, const char* ptr, size_t size)
	{
		s.write(reinterpret_cast<const uint8_t*>(ptr), size);
	}

	std::string lua_skein_read(skein::hash& s)
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

		auto& h = CORE().mlogic.get_mfile().host_memory;
		if(daddr + rows * size > h.size()) {
			equals = false;
			h.resize(daddr + rows * size);
		}

		char* pbuffer = mappable ? CORE().memory.get_physical_mapping(low, high - low + 1) : NULL;
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
					uint8_t byte = CORE().memory.read<uint8_t>(addr1 + j);
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
		uint64_t addr, size;

		addr = lua_get_read_address(P);
		P(size);

		L.newtable();
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			CORE().memory.read_range(addr, buffer, rsize);
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
			CORE().memory.write_range(addr, buffer, rsize);
			addr += rsize;
			size -= rsize;
		}
		return 1;
	}

	lua::functions memoryfuncs(lua_func_misc, "memory", {
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
		{"writeregion", readregion},
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
		{"registerread", lua_registerX<DEBUG_READ, true>},
		{"unregisterread", lua_registerX<DEBUG_READ, false>},
		{"registerwrite", lua_registerX<DEBUG_WRITE, true>},
		{"unregisterwrite", lua_registerX<DEBUG_WRITE, false>},
		{"registerexec", lua_registerX<DEBUG_EXEC, true>},
		{"unregisterexec", lua_registerX<DEBUG_EXEC, false>},
		{"registertrace", lua_registerX<DEBUG_TRACE, true>},
		{"unregistertrace", lua_registerX<DEBUG_TRACE, false>},
	});

	lua::_class<lua_mmap_struct> class_mmap_struct(lua_class_memory, "MMAP_STRUCT", {
		{"new", &lua_mmap_struct::create},
	}, {
		{"__index", &lua_mmap_struct::index},
		{"__newindex", &lua_mmap_struct::newindex},
		{"__call", &lua_mmap_struct::map},
	}, &lua_mmap_struct::print);
}

int lua_mmap_struct::map(lua::state& L, lua::parameters& P)
{
	std::string name, type;
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
