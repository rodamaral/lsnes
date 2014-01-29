#include "core/command.hpp"
#include "lua/internal.hpp"
#include "core/debug.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rom.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/hex.hpp"
#include "library/int24.hpp"

namespace
{
	uint64_t get_vmabase(const std::string& vma)
	{
		for(auto i : lsnes_memory.get_regions())
			if(i->name == vma)
				return i->base;
		throw std::runtime_error("No such VMA");
	}

	uint64_t get_read_address(lua::parameters& P)
	{
		uint64_t vmabase = 0;
		if(P.is_string())
			vmabase = get_vmabase(P.arg<std::string>());
		auto addr = P.arg<uint64_t>();
		return addr + vmabase;
	}

	template<typename T, T (memory_space::*rfun)(uint64_t addr),
		bool (memory_space::*wfun)(uint64_t addr, T value)>
	void do_rw(lua::state& L, uint64_t addr, bool wrflag)
	{
		if(wrflag) {
			T value = L.get_numeric_argument<T>(3, "aperture(write)");
			(lsnes_memory.*wfun)(addr, value);
		} else
			L.pushnumber(static_cast<T>((lsnes_memory.*rfun)(addr)));
	}

	template<typename T, T (memory_space::*rfun)(uint64_t addr)>
	int lua_read_memory(lua::state& L, lua::parameters& P)
	{
		auto addr = get_read_address(P);
		L.pushnumber(static_cast<T>((lsnes_memory.*rfun)(addr)));
		return 1;
	}

	template<typename T, bool (memory_space::*wfun)(uint64_t addr, T value)>
	int lua_write_memory(lua::state& L, lua::parameters& P)
	{
		auto addr = get_read_address(P);
		T value = P.arg<T>();
		(lsnes_memory.*wfun)(addr, value);
		return 0;
	}
}

class lua_mmap_struct
{
public:
	lua_mmap_struct(lua::state& L);

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
		L.pushnumber(static_cast<T>((lsnes_memory.*rfun)(addr)));
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
		(lsnes_memory.*wfun)(addr, value);
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

	template<debug_type type, bool reg>
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
			D->h = debug_add_trace_callback(addr, [LL, D2](uint64_t proc, const char* str) {
				LL->pushlightuserdata(D2);
				LL->rawget(LUA_REGISTRYINDEX);
				LL->pushnumber(proc);
				LL->pushstring(str);
				do_lua_error(*LL, LL->pcall(2, 0, 0));
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

	template<debug_type type, bool reg>
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

	template<debug_type type, bool reg>
	int lua_registerX(lua::state& L, lua::parameters& P)
	{
		uint64_t addr;
		int lfn;
		if(P.is_nil() && type != DEBUG_TRACE) {
			addr = 0xFFFFFFFFFFFFFFFFULL;
			P.skip();
		} else if(type != DEBUG_TRACE)
			addr = get_read_address(P);
		else
			P(addr);
		P(P.function(lfn));

		if(reg) {
			handle_registerX<type, reg>(L, addr, lfn);
			L.pushvalue(lfn);
			return 1;
		} else {
			handle_unregisterX<type, reg>(L, addr, lfn);
			return 0;
		}
	}

	command::fnptr<> callbacks_show_lua(lsnes_cmd, "show-lua-callbacks", "", "",
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
			aperture_make_fun<T, rfun, wfun>(L.get_master(), 0, 0xFFFFFFFFFFFFFFFFULL);
			return 1;
		}
		auto addr = get_read_address(P);
		auto size = P.arg<uint64_t>();
		if(!size)
			throw std::runtime_error("Aperture with zero size is not valid");
		aperture_make_fun<T, rfun, wfun>(L.get_master(), addr, size - 1);
		return 1;
	}

	lua::fnptr2 vmacount(lua_func_misc, "memory.vma_count", [](lua::state& L, lua::parameters& P) -> int {
		L.pushnumber(lsnes_memory.get_regions().size());
		return 1;
	});

	lua::fnptr2 cheat(lua_func_misc, "memory.cheat", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t addr, value;

		addr = get_read_address(P);

		if(P.is_novalue()) {
			debug_clear_cheat(addr);
		} else {
			P(value);
			debug_set_cheat(addr, value);
		}
		return 0;
	});

	lua::fnptr2 xmask(lua_func_misc, "memory.setxmask", [](lua::state& L, lua::parameters& P) -> int {
		auto value = P.arg<uint64_t>();
		debug_setxmask(value);
		return 0;
	});

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

	lua::fnptr2 readvma(lua_func_misc, "memory.read_vma", [](lua::state& L, lua::parameters& P) -> int {
		uint32_t num;

		P(num);

		std::list<memory_region*> regions = lsnes_memory.get_regions();
		uint32_t j = 0;
		for(auto i = regions.begin(); i != regions.end(); i++, j++)
			if(j == num)
				return handle_push_vma(L, **i);
		L.pushnil();
		return 1;
	});

	lua::fnptr2 findvma(lua_func_misc, "memory.find_vma", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t addr;

		P(addr);

		auto r = lsnes_memory.lookup(addr);
		if(r.first)
			return handle_push_vma(L, *r.first);
		L.pushnil();
		return 1;
	});

	const char* hexes = "0123456789ABCDEF";

	lua::fnptr2 hashstate(lua_func_misc, "memory.hash_state", [](lua::state& L, lua::parameters& P) -> int {
		char hash[64];
		auto x = our_rom.save_core_state();
		size_t offset = x.size() - 32;
		L.pushlstring(hex::b_to((uint8_t*)&x[offset], 32));
		return 1;
	});

#define BLOCKSIZE 256

	lua::fnptr2 hashmemory(lua_func_misc, "memory.hash_region", [](lua::state& L, lua::parameters& P) -> int {
		std::string hash;
		uint64_t addr, size;

		addr = get_read_address(P);
		P(size);

		char buffer[BLOCKSIZE];
		sha256 h;
		while(size > BLOCKSIZE) {
			for(size_t i = 0; i < BLOCKSIZE; i++)
				buffer[i] = lsnes_memory.read<uint8_t>(addr + i);
			h.write(buffer, BLOCKSIZE);
			addr += BLOCKSIZE;
			size -= BLOCKSIZE;
		}
		for(size_t i = 0; i < size; i++)
			buffer[i] = lsnes_memory.read<uint8_t>(addr + i);
		h.write(buffer, size);
		hash = h.read();
		L.pushlstring(hash.c_str(), 64);
		return 1;
	});

	lua::fnptr2 readmemoryr(lua_func_misc, "memory.readregion", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t addr, size;

		addr = get_read_address(P);
		P(size);

		L.newtable();
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			lsnes_memory.read_range(addr, buffer, rsize);
			for(size_t i = 0; i < rsize; i++) {
				L.pushnumber(ctr++);
				L.pushnumber(static_cast<unsigned char>(buffer[i]));
				L.settable(-3);
			}
			addr += rsize;
			size -= rsize;
		}
		return 1;
	});

	lua::fnptr2 writememoryr(lua_func_misc, "memory.writeregion", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t addr, size;
		int ltbl;

		addr = get_read_address(P);
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
			lsnes_memory.write_range(addr, buffer, rsize);
			addr += rsize;
			size -= rsize;
		}
		return 1;
	});

	template<bool write, bool sign> int memory_scattergather(lua::state& L, lua::parameters& P)
	{
		uint64_t val = 0;
		unsigned shift = 0;
		uint64_t addr = 0;
		uint64_t vmabase = 0;
		if(write)
			val = P.arg<uint64_t>();
		while(P.more()) {
			if(P.is_boolean()) {
				if(P.arg<bool>())
					addr++;
				else
					addr--;
			} else if(P.is_string()) {
				vmabase = get_vmabase(P.arg<std::string>());
				continue;
			} else
				addr = P.arg<uint64_t>();
			if(write)
				lsnes_memory.write<uint8_t>(addr + vmabase, val >> shift);
			else
				val = val + ((uint64_t)lsnes_memory.read<uint8_t>(addr + vmabase) << shift);
			shift += 8;
		}
		if(!write) {
			int64_t sval = val;
			if(val >= (1ULL << (shift - 1))) sval -= (1ULL << shift);
			if(sign) L.pushnumber(sval); else L.pushnumber(val);
		}
		return write ? 0 : 1;
	}

	lua::fnptr2 scattergather1(lua_func_misc, "memory.read_sg", memory_scattergather<false, false>);
	lua::fnptr2 scattergather2(lua_func_misc, "memory.sread_sg", memory_scattergather<false, true>);
	lua::fnptr2 scattergather3(lua_func_misc, "memory.write_sg", memory_scattergather<true, false>);
	lua::fnptr2 rub(lua_func_misc, "memory.readbyte", lua_read_memory<uint8_t, &memory_space::read<uint8_t>>);
	lua::fnptr2 rsb(lua_func_misc, "memory.readsbyte", lua_read_memory<int8_t, &memory_space::read<int8_t>>);
	lua::fnptr2 ruw(lua_func_misc, "memory.readword", lua_read_memory<uint16_t, &memory_space::read<uint16_t>>);
	lua::fnptr2 rsw(lua_func_misc, "memory.readsword", lua_read_memory<int16_t, &memory_space::read<int16_t>>);
	lua::fnptr2 ruh(lua_func_misc, "memory.readhword", lua_read_memory<ss_uint24_t,
		&memory_space::read<ss_uint24_t>>);
	lua::fnptr2 rsh(lua_func_misc, "memory.readshword", lua_read_memory<ss_int24_t,
		&memory_space::read<ss_int24_t>>);
	lua::fnptr2 rud(lua_func_misc, "memory.readdword", lua_read_memory<uint32_t, &memory_space::read<uint32_t>>);
	lua::fnptr2 rsd(lua_func_misc, "memory.readsdword", lua_read_memory<int32_t, &memory_space::read<int32_t>>);
	lua::fnptr2 ruq(lua_func_misc, "memory.readqword", lua_read_memory<uint64_t, &memory_space::read<uint64_t>>);
	lua::fnptr2 rsq(lua_func_misc, "memory.readsqword", lua_read_memory<int64_t, &memory_space::read<int64_t>>);
	lua::fnptr2 rf4(lua_func_misc, "memory.readfloat", lua_read_memory<float, &memory_space::read<float>>);
	lua::fnptr2 rf8(lua_func_misc, "memory.readdouble", lua_read_memory<double, &memory_space::read<double>>);
	lua::fnptr2 wb(lua_func_misc, "memory.writebyte", lua_write_memory<uint8_t, &memory_space::write<uint8_t>>);
	lua::fnptr2 ww(lua_func_misc, "memory.writeword", lua_write_memory<uint16_t, &memory_space::write<uint16_t>>);
	lua::fnptr2 wh(lua_func_misc, "memory.writehword", lua_write_memory<ss_uint24_t,
		&memory_space::write<ss_uint24_t>>);
	lua::fnptr2 wd(lua_func_misc, "memory.writedword", lua_write_memory<uint32_t,
		&memory_space::write<uint32_t>>);
	lua::fnptr2 wq(lua_func_misc, "memory.writeqword", lua_write_memory<uint64_t,
		&memory_space::write<uint64_t>>);
	lua::fnptr2 wf4(lua_func_misc, "memory.writefloat", lua_write_memory<float, &memory_space::write<float>>);
	lua::fnptr2 wf8(lua_func_misc, "memory.writedouble", lua_write_memory<double, &memory_space::write<double>>);
	lua::fnptr2 mub(lua_func_misc, "memory.mapbyte", lua_mmap_memory<uint8_t, &memory_space::read<uint8_t>,
		&memory_space::write<uint8_t>>);
	lua::fnptr2 msb(lua_func_misc, "memory.mapsbyte", lua_mmap_memory<int8_t, &memory_space::read<int8_t>,
		&memory_space::write<int8_t>>);
	lua::fnptr2 muw(lua_func_misc, "memory.mapword", lua_mmap_memory<uint16_t, &memory_space::read<uint16_t>,
		&memory_space::write<uint16_t>>);
	lua::fnptr2 msw(lua_func_misc, "memory.mapsword", lua_mmap_memory<int16_t, &memory_space::read<int16_t>,
		&memory_space::write<int16_t>>);
	lua::fnptr2 muh(lua_func_misc, "memory.maphword", lua_mmap_memory<ss_uint24_t,
		&memory_space::read<ss_uint24_t>, &memory_space::write<ss_uint24_t>>);
	lua::fnptr2 msh(lua_func_misc, "memory.mapshword", lua_mmap_memory<ss_int24_t,
		&memory_space::read<ss_int24_t>, &memory_space::write<ss_int24_t>>);
	lua::fnptr2 mud(lua_func_misc, "memory.mapdword", lua_mmap_memory<uint32_t, &memory_space::read<uint32_t>,
		&memory_space::write<uint32_t>>);
	lua::fnptr2 msd(lua_func_misc, "memory.mapsdword", lua_mmap_memory<int32_t, &memory_space::read<int32_t>,
		&memory_space::write<int32_t>>);
	lua::fnptr2 muq(lua_func_misc, "memory.mapqword", lua_mmap_memory<uint64_t, &memory_space::read<uint64_t>,
		&memory_space::write<uint64_t>>);
	lua::fnptr2 msq(lua_func_misc, "memory.mapsqword", lua_mmap_memory<int64_t, &memory_space::read<int64_t>,
		&memory_space::write<int64_t>>);
	lua::fnptr2 mf4(lua_func_misc, "memory.mapfloat", lua_mmap_memory<float, &memory_space::read<float>,
		&memory_space::write<float>>);
	lua::fnptr2 mf8(lua_func_misc, "memory.mapdouble", lua_mmap_memory<double, &memory_space::read<double>,
		&memory_space::write<double>>);
	lua::fnptr2 mrr(lua_func_misc, "memory.registerread", lua_registerX<DEBUG_READ, true>);
	lua::fnptr2 murr(lua_func_misc, "memory.unregisterread", lua_registerX<DEBUG_READ, false>);
	lua::fnptr2 mrw(lua_func_misc, "memory.registerwrite", lua_registerX<DEBUG_WRITE, true>);
	lua::fnptr2 murw(lua_func_misc, "memory.unregisterwrite", lua_registerX<DEBUG_WRITE, false>);
	lua::fnptr2 mrx(lua_func_misc, "memory.registerexec", lua_registerX<DEBUG_EXEC, true>);
	lua::fnptr2 murx(lua_func_misc, "memory.unregisterexec", lua_registerX<DEBUG_EXEC, false>);
	lua::fnptr2 mrt(lua_func_misc, "memory.registertrace", lua_registerX<DEBUG_TRACE, true>);
	lua::fnptr2 murt(lua_func_misc, "memory.unregistertrace", lua_registerX<DEBUG_TRACE, false>);

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
	uint64_t vmabase = 0, addr;

	P(P.skipped(), name);
	if(P.is_string())
		vmabase = get_vmabase(P.arg<std::string>());
	P(addr, type);
	addr += vmabase;

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
