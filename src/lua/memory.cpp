#include "lua/internal.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rom.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/int24.hpp"

namespace
{
	uint64_t get_vmabase(lua_state& L, const std::string& vma)
	{
		for(auto i : lsnes_memory.get_regions())
			if(i->name == vma)
				return i->base;
		throw std::runtime_error("No such VMA");
	}

	template<typename T, T (memory_space::*rfun)(uint64_t addr)>
	class lua_read_memory : public lua_function
	{
	public:
		lua_read_memory(const std::string& name) : lua_function(lua_func_misc, name) {}
		int invoke(lua_state& L)
		{
			int base = 0;
			uint64_t vmabase = 0;
			if(L.type(1) == LUA_TSTRING) {
				vmabase = get_vmabase(L, L.get_string(1, "lua_read_memory"));
				base = 1;
			}
			uint64_t addr = L.get_numeric_argument<uint64_t>(base + 1, fname.c_str()) + vmabase;
			L.pushnumber(static_cast<T>((lsnes_memory.*rfun)(addr)));
			return 1;
		}
	};

	template<typename T, bool (memory_space::*wfun)(uint64_t addr, T value)>
	class lua_write_memory : public lua_function
	{
	public:
		lua_write_memory(const std::string& name) : lua_function(lua_func_misc, name) {}
		int invoke(lua_state& L)
		{
			int base = 0;
			uint64_t vmabase = 0;
			if(L.type(1) == LUA_TSTRING) {
				vmabase = get_vmabase(L, L.get_string(1, "lua_read_memory"));
				base = 1;
			}
			uint64_t addr = L.get_numeric_argument<uint64_t>(base + 1, fname.c_str()) + vmabase;
			T value = L.get_numeric_argument<T>(base + 2, fname.c_str());
			(lsnes_memory.*wfun)(addr, value);
			return 0;
		}
	};

	class mmap_base
	{
	public:
		~mmap_base() {}
		virtual void read(lua_state& L, uint64_t addr) = 0;
		virtual void write(lua_state& L, uint64_t addr) = 0;
	};


	template<typename T, T (memory_space::*rfun)(uint64_t addr), bool (memory_space::*wfun)(uint64_t addr,
		T value)>
	class lua_mmap_memory_helper : public mmap_base
	{
	public:
		~lua_mmap_memory_helper() {}
		void read(lua_state& L, uint64_t addr)
		{
			L.pushnumber(static_cast<T>((lsnes_memory.*rfun)(addr)));
		}

		void write(lua_state& L, uint64_t addr)
		{
			T value = L.get_numeric_argument<T>(3, "aperture(write)");
			(lsnes_memory.*wfun)(addr, value);
		}
	};
}

class lua_mmap_struct
{
public:
	lua_mmap_struct(lua_state& L);

	~lua_mmap_struct()
	{
	}

	int index(lua_state& L, const std::string& fname)
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
		x.first->read(L, x.second);
		return 1;
	}
	int newindex(lua_state& L, const std::string& fname)
	{
		const char* c = L.tostring(2);
		if(!c)
			return 0;
		std::string c2(c);
		if(!mappings.count(c2))
			return 0;
		auto& x = mappings[c2];
		x.first->write(L, x.second);
		return 0;
	}
	int map(lua_state& L, const std::string& fname);
	std::string print()
	{
		size_t s = mappings.size();
		return (stringfmt() << s << " " << ((s != 1) ? "mappings" : "mapping")).str();
	}
private:
	std::map<std::string, std::pair<mmap_base*, uint64_t>> mappings;
};

namespace
{
	int aperture_read_fun(lua_State* _L)
	{
		lua_state& L = *reinterpret_cast<lua_state*>(lua_touserdata(_L, lua_upvalueindex(4)));
		uint64_t base = L.tonumber(lua_upvalueindex(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(L.type(lua_upvalueindex(2)) == LUA_TNUMBER)
			size = L.tonumber(lua_upvalueindex(2));
		mmap_base* fn = reinterpret_cast<mmap_base*>(L.touserdata(lua_upvalueindex(3)));
		uint64_t addr = L.get_numeric_argument<uint64_t>(2, "aperture(read)");
		if(addr > size || addr + base < addr) {
			L.pushnumber(0);
			return 1;
		}
		addr += base;
		fn->read(L, addr);
		return 1;
	}

	int aperture_write_fun(lua_State* _L)
	{
		lua_state& L = *reinterpret_cast<lua_state*>(lua_touserdata(_L, lua_upvalueindex(4)));
		uint64_t base = L.tonumber(lua_upvalueindex(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(L.type(lua_upvalueindex(2)) == LUA_TNUMBER)
			size = L.tonumber(lua_upvalueindex(2));
		mmap_base* fn = reinterpret_cast<mmap_base*>(L.touserdata(lua_upvalueindex(3)));
		uint64_t addr = L.get_numeric_argument<uint64_t>(2, "aperture(write)");
		if(addr > size || addr + base < addr)
			return 0;
		addr += base;
		fn->write(L, addr);
		return 0;
	}

	void aperture_make_fun(lua_state& L, uint64_t base, uint64_t size, mmap_base& type)
	{
		L.newtable();
		L.newtable();
		L.pushstring( "__index");
		L.pushnumber(base);
		if(!(size + 1))
			L.pushnil();
		else
			L.pushnumber(size);
		L.pushlightuserdata(&type);
		L.pushlightuserdata(&L);
		L.pushcclosure(aperture_read_fun, 4);
		L.settable(-3);
		L.pushstring("__newindex");
		L.pushnumber(base);
		if(!(size + 1))
			L.pushnil();
		else
			L.pushnumber(size);
		L.pushlightuserdata(&type);
		L.pushlightuserdata(&L);
		L.pushcclosure(aperture_write_fun, 4);
		L.settable(-3);
		L.setmetatable(-2);
	}

	class lua_mmap_memory : public lua_function
	{
	public:
		lua_mmap_memory(const std::string& name, mmap_base& _h) : lua_function(lua_func_misc, name), h(_h) {}
		int invoke(lua_state& L)
		{
			if(L.isnoneornil(1)) {
				aperture_make_fun(L, 0, 0xFFFFFFFFFFFFFFFFULL, h);
				return 1;
			}
			int base = 0;
			uint64_t vmabase = 0;
			if(L.type(1) == LUA_TSTRING) {
				vmabase = get_vmabase(L, L.get_string(1, "lua_mmap_memory"));
				base = 1;
			}
			uint64_t addr = L.get_numeric_argument<uint64_t>(base + 1, fname.c_str()) + vmabase;
			uint64_t size = L.get_numeric_argument<uint64_t>(base + 2, fname.c_str());
			if(!size) {
				L.pushstring("Aperture with zero size is not valid");
				L.error();
				return 0;
			}
			aperture_make_fun(L, addr, size - 1, h);
			return 1;
		}
		mmap_base& h;
	};

	function_ptr_luafun vmacount(lua_func_misc, "memory.vma_count", [](lua_state& L, const std::string& fname)
		-> int {
		L.pushnumber(lsnes_memory.get_regions().size());
		return 1;
	});

	int handle_push_vma(lua_state& L, memory_region& r)
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

	function_ptr_luafun readvma(lua_func_misc, "memory.read_vma", [](lua_state& L, const std::string& fname)
		-> int {
		std::list<memory_region*> regions = lsnes_memory.get_regions();
		uint32_t num = L.get_numeric_argument<uint32_t>(1, fname.c_str());
		uint32_t j = 0;
		for(auto i = regions.begin(); i != regions.end(); i++, j++)
			if(j == num)
				return handle_push_vma(L, **i);
		L.pushnil();
		return 1;
	});

	function_ptr_luafun findvma(lua_func_misc, "memory.find_vma", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		auto r = lsnes_memory.lookup(addr);
		if(r.first)
			return handle_push_vma(L, *r.first);
		L.pushnil();
		return 1;
	});

	const char* hexes = "0123456789ABCDEF";

	function_ptr_luafun hashstate(lua_func_misc, "memory.hash_state", [](lua_state& L, const std::string& fname)
		-> int {
		char hash[64];
		auto x = our_rom.save_core_state();
		size_t offset = x.size() - 32;
		for(unsigned i = 0; i < 32; i++) {
			hash[2 * i + 0] = hexes[static_cast<unsigned char>(x[offset + i]) >> 4];
			hash[2 * i + 1] = hexes[static_cast<unsigned char>(x[offset + i]) & 0xF];
		}
		L.pushlstring(hash, 64);
		return 1;
	});

#define BLOCKSIZE 256

	function_ptr_luafun hashmemory(lua_func_misc, "memory.hash_region", [](lua_state& L, const std::string& fname)
		-> int {
		std::string hash;
		int base = 0;
		uint64_t vmabase = 0;
		if(L.type(1) == LUA_TSTRING) {
			vmabase = get_vmabase(L, L.get_string(1, fname.c_str()));
			base = 1;
		}
		uint64_t addr = L.get_numeric_argument<uint64_t>(base + 1, fname.c_str()) + vmabase;
		uint64_t size = L.get_numeric_argument<uint64_t>(base + 2, fname.c_str());
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

	function_ptr_luafun readmemoryr(lua_func_misc, "memory.readregion", [](lua_state& L, const std::string& fname)
		-> int {
		std::string hash;
		int base = 0;
		uint64_t vmabase = 0;
		if(L.type(1) == LUA_TSTRING) {
			vmabase = get_vmabase(L, L.get_string(1, fname.c_str()));
			base = 1;
		}
		uint64_t addr = L.get_numeric_argument<uint64_t>(base + 1, fname.c_str()) + vmabase;
		uint64_t size = L.get_numeric_argument<uint64_t>(base + 2, fname.c_str());
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

	function_ptr_luafun writememoryr(lua_func_misc, "memory.writeregion", [](lua_state& L,
		const std::string& fname) -> int {
		std::string hash;
		int base = 0;
		uint64_t vmabase = 0;
		if(L.type(1) == LUA_TSTRING) {
			vmabase = get_vmabase(L, L.get_string(1, fname.c_str()));
			base = 1;
		}
		uint64_t addr = L.get_numeric_argument<uint64_t>(base + 1, fname.c_str()) + vmabase;
		uint64_t size = L.get_numeric_argument<uint64_t>(base + 2, fname.c_str());
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			for(size_t i = 0; i < rsize; i++) {
				L.pushnumber(ctr++);
				L.gettable(3);
				buffer[i] = L.tointeger(-1);
				L.pop(1);
			}
			lsnes_memory.write_range(addr, buffer, rsize);
			addr += rsize;
			size -= rsize;
		}
		return 1;
	});

	function_ptr_luafun gui_cbitmap(lua_func_misc, "memory.map_structure", [](lua_state& L,
		const std::string& fname) -> int {
		lua_class<lua_mmap_struct>::create(L);
		return 1;
	});

	function_ptr_luafun memory_watchexpr(lua_func_misc, "memory.read_expr", [](lua_state& L,
		const std::string& fname) -> int {
		std::string val = evaluate_watch(L.get_string(1, fname.c_str()));
		L.pushstring(val.c_str());
		return 1;
	});

	lua_read_memory<uint8_t, &memory_space::read<uint8_t>> rub("memory.readbyte");
	lua_read_memory<int8_t, &memory_space::read<int8_t>> rsb("memory.readsbyte");
	lua_read_memory<uint16_t, &memory_space::read<uint16_t>> ruw("memory.readword");
	lua_read_memory<int16_t, &memory_space::read<int16_t>> rsw("memory.readsword");
	lua_read_memory<ss_uint24_t, &memory_space::read<ss_uint24_t>> ruh("memory.readhword");
	lua_read_memory<ss_int24_t, &memory_space::read<ss_int24_t>> rsh("memory.readshword");
	lua_read_memory<uint32_t, &memory_space::read<uint32_t>> rud("memory.readdword");
	lua_read_memory<int32_t, &memory_space::read<int32_t>> rsd("memory.readsdword");
	lua_read_memory<uint64_t, &memory_space::read<uint64_t>> ruq("memory.readqword");
	lua_read_memory<int64_t, &memory_space::read<int64_t>> rsq("memory.readsqword");
	lua_read_memory<float, &memory_space::read<float>> rf4("memory.readfloat");
	lua_read_memory<double, &memory_space::read<double>> rf8("memory.readdouble");
	lua_write_memory<uint8_t, &memory_space::write<uint8_t>> wb("memory.writebyte");
	lua_write_memory<uint16_t, &memory_space::write<uint16_t>> ww("memory.writeword");
	lua_write_memory<ss_uint24_t, &memory_space::write<ss_uint24_t>> wh("memory.writehword");
	lua_write_memory<uint32_t, &memory_space::write<uint32_t>> wd("memory.writedword");
	lua_write_memory<uint64_t, &memory_space::write<uint64_t>> wq("memory.writeqword");
	lua_write_memory<float, &memory_space::write<float>> wf4("memory.writefloat");
	lua_write_memory<double, &memory_space::write<double>> wf8("memory.writedouble");
	lua_mmap_memory_helper<uint8_t, &memory_space::read<uint8_t>, &memory_space::write<uint8_t>> mhub;
	lua_mmap_memory_helper<int8_t, &memory_space::read<int8_t>, &memory_space::write<int8_t>> mhsb;
	lua_mmap_memory_helper<uint16_t, &memory_space::read<uint16_t>, &memory_space::write<uint16_t>> mhuw;
	lua_mmap_memory_helper<int16_t, &memory_space::read<int16_t>, &memory_space::write<int16_t>> mhsw;
	lua_mmap_memory_helper<ss_uint24_t, &memory_space::read<ss_uint24_t>, &memory_space::write<ss_uint24_t>> mhuh;
	lua_mmap_memory_helper<ss_int24_t, &memory_space::read<ss_int24_t>, &memory_space::write<ss_int24_t>> mhsh;
	lua_mmap_memory_helper<uint32_t, &memory_space::read<uint32_t>, &memory_space::write<uint32_t>> mhud;
	lua_mmap_memory_helper<int32_t, &memory_space::read<int32_t>, &memory_space::write<int32_t>> mhsd;
	lua_mmap_memory_helper<uint64_t, &memory_space::read<uint64_t>, &memory_space::write<uint64_t>> mhuq;
	lua_mmap_memory_helper<int64_t, &memory_space::read<int64_t>, &memory_space::write<int64_t>> mhsq;
	lua_mmap_memory_helper<float, &memory_space::read<float>, &memory_space::write<float>> mhf4;
	lua_mmap_memory_helper<double, &memory_space::read<double>, &memory_space::write<double>> mhf8;
	lua_mmap_memory mub("memory.mapbyte", mhub);
	lua_mmap_memory msb("memory.mapsbyte", mhsb);
	lua_mmap_memory muw("memory.mapword", mhuw);
	lua_mmap_memory msw("memory.mapsword", mhsw);
	lua_mmap_memory muh("memory.maphword", mhuh);
	lua_mmap_memory msh("memory.mapshword", mhsh);
	lua_mmap_memory mud("memory.mapdword", mhud);
	lua_mmap_memory msd("memory.mapsdword", mhsd);
	lua_mmap_memory muq("memory.mapqword", mhuq);
	lua_mmap_memory msq("memory.mapsqword", mhsq);
	lua_mmap_memory mf4("memory.mapfloat", mhf4);
	lua_mmap_memory mf8("memory.mapdouble", mhf8);
}

int lua_mmap_struct::map(lua_state& L, const std::string& fname)
{
	const char* name = L.tostring(2);
	int base = 0;
	uint64_t vmabase = 0;
	if(L.type(3) == LUA_TSTRING) {
		vmabase = get_vmabase(L, L.get_string(3, fname.c_str()));
		base = 1;
	}
	uint64_t addr = L.get_numeric_argument<uint64_t>(base + 3, fname.c_str());
	const char* type = L.tostring(base + 4);
	if(!name)
		(stringfmt() << fname << ": Bad name").throwex();
	if(!type)
		(stringfmt() << fname << ": Bad type").throwex();
	std::string name2(name);
	std::string type2(type);
	if(type2 == "byte")
		mappings[name2] = std::make_pair(&mhub, addr);
	else if(type2 == "sbyte")
		mappings[name2] = std::make_pair(&mhsb, addr);
	else if(type2 == "word")
		mappings[name2] = std::make_pair(&mhuw, addr);
	else if(type2 == "sword")
		mappings[name2] = std::make_pair(&mhsw, addr);
	else if(type2 == "hword")
		mappings[name2] = std::make_pair(&mhuh, addr);
	else if(type2 == "shword")
		mappings[name2] = std::make_pair(&mhsh, addr);
	else if(type2 == "dword")
		mappings[name2] = std::make_pair(&mhud, addr);
	else if(type2 == "sdword")
		mappings[name2] = std::make_pair(&mhsd, addr);
	else if(type2 == "qword")
		mappings[name2] = std::make_pair(&mhuq, addr);
	else if(type2 == "sqword")
		mappings[name2] = std::make_pair(&mhsq, addr);
	else if(type2 == "float")
		mappings[name2] = std::make_pair(&mhf4, addr);
	else if(type2 == "double")
		mappings[name2] = std::make_pair(&mhf8, addr);
	else
		(stringfmt() << fname << ": Bad type").throwex();
	return 0;
}

DECLARE_LUACLASS(lua_mmap_struct, "MMAP_STRUCT");

lua_mmap_struct::lua_mmap_struct(lua_state& L)
{
	objclass<lua_mmap_struct>().bind_multi(L, {
		{"__index", &lua_mmap_struct::index},
		{"__newindex", &lua_mmap_struct::newindex},
		{"__call", &lua_mmap_struct::map},
	});
}
