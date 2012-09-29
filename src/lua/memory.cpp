#include "lua/internal.hpp"
#include "core/memorymanip.hpp"
#include "core/rom.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"

namespace
{
	template<typename T, typename U, U (*rfun)(uint64_t addr)>
	class lua_read_memory : public lua_function
	{
	public:
		lua_read_memory(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS)
		{
			uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
			lua_pushnumber(LS, static_cast<T>(rfun(addr)));
			return 1;
		}
	};

	template<typename T, bool (*wfun)(uint64_t addr, T value)>
	class lua_write_memory : public lua_function
	{
	public:
		lua_write_memory(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS)
		{
			uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
			T value = get_numeric_argument<T>(LS, 2, fname.c_str());
			wfun(addr, value);
			return 0;
		}
	};

	class mmap_base
	{
	public:
		~mmap_base() {}
		virtual void read(lua_State* LS, uint64_t addr) = 0;
		virtual void write(lua_State* LS, uint64_t addr) = 0;
	};


	template<typename T, typename U, U (*rfun)(uint64_t addr), bool (*wfun)(uint64_t addr, U value)>
	class lua_mmap_memory_helper : public mmap_base
	{
	public:
		~lua_mmap_memory_helper() {}
		void read(lua_State* LS, uint64_t addr)
		{
			lua_pushnumber(LS, static_cast<T>(rfun(addr)));
		}

		void write(lua_State* LS, uint64_t addr)
		{
			T value = get_numeric_argument<T>(LS, 3, "aperture(write)");
			wfun(addr, value);
		}
	};
}

class lua_mmap_struct
{
public:
	lua_mmap_struct(lua_State* LS);

	~lua_mmap_struct()
	{
	}

	int index(lua_State* LS)
	{
		const char* c = lua_tostring(LS, 2);
		if(!c) {
			lua_pushnil(LS);
			return 1;
		}
		std::string c2(c);
		if(!mappings.count(c2)) {
			lua_pushnil(LS);
			return 1;
		}
		auto& x = mappings[c2];
		x.first->read(LS, x.second);
		return 1;
	}
	int newindex(lua_State* LS)
	{
		const char* c = lua_tostring(LS, 2);
		if(!c)
			return 0;
		std::string c2(c);
		if(!mappings.count(c2))
			return 0;
		auto& x = mappings[c2];
		x.first->write(LS, x.second);
		return 0;
	}

	int map(lua_State* LS);
private:
	std::map<std::string, std::pair<mmap_base*, uint64_t>> mappings;
};

namespace
{
	int aperture_read_fun(lua_State* LS)
	{
		uint64_t base = lua_tonumber(LS, lua_upvalueindex(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(lua_type(LS, lua_upvalueindex(2)) == LUA_TNUMBER)
			size = lua_tonumber(LS, lua_upvalueindex(2));
		mmap_base* fn = reinterpret_cast<mmap_base*>(lua_touserdata(LS, lua_upvalueindex(3)));
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 2, "aperture(read)");
		if(addr > size || addr + base < addr) {
			lua_pushnumber(LS, 0);
			return 1;
		}
		addr += base;
		fn->read(LS, addr);
		return 1;
	}

	int aperture_write_fun(lua_State* LS)
	{

		uint64_t base = lua_tonumber(LS, lua_upvalueindex(1));
		uint64_t size = 0xFFFFFFFFFFFFFFFFULL;
		if(lua_type(LS, lua_upvalueindex(2)) == LUA_TNUMBER)
			size = lua_tonumber(LS, lua_upvalueindex(2));
		mmap_base* fn = reinterpret_cast<mmap_base*>(lua_touserdata(LS, lua_upvalueindex(3)));
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 2, "aperture(write)");
		if(addr > size || addr + base < addr)
			return 0;
		addr += base;
		fn->write(LS, addr);
		return 0;
	}

	void aperture_make_fun(lua_State* LS, uint64_t base, uint64_t size, mmap_base& type)
	{
		lua_newtable(LS);
		lua_newtable(LS);
		lua_pushstring(LS, "__index");
		lua_pushnumber(LS, base);
		if(!(size + 1))
			lua_pushnil(LS);
		else
			lua_pushnumber(LS, size);
		lua_pushlightuserdata(LS, &type);
		lua_pushcclosure(LS, aperture_read_fun, 3);
		lua_settable(LS, -3);
		lua_pushstring(LS, "__newindex");
		lua_pushnumber(LS, base);
		if(!(size + 1))
			lua_pushnil(LS);
		else
			lua_pushnumber(LS, size);
		lua_pushlightuserdata(LS, &type);
		lua_pushcclosure(LS, aperture_write_fun, 3);
		lua_settable(LS, -3);
		lua_setmetatable(LS, -2);
	}

	class lua_mmap_memory : public lua_function
	{
	public:
		lua_mmap_memory(const std::string& name, mmap_base& _h) : lua_function(name), h(_h) {}
		int invoke(lua_State* LS)
		{
			if(lua_isnoneornil(LS, 1)) {
				aperture_make_fun(LS, 0, 0xFFFFFFFFFFFFFFFFULL, h);
				return 1;
			}
			uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
			uint64_t size = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
			if(!size) {
				lua_pushstring(LS, "Aperture with zero size is not valid");
				lua_error(LS);
				return 0;
			}
			aperture_make_fun(LS, addr, size - 1, h);
			return 1;
		}
		mmap_base& h;
	};

	function_ptr_luafun vmacount("memory.vma_count", [](lua_State* LS, const std::string& fname) -> int {
		lua_pushnumber(LS, get_regions().size());
		return 1;
	});

	int handle_push_vma(lua_State* LS, std::vector<memory_region>& regions, size_t idx)
	{
		if(idx >= regions.size()) {
			lua_pushnil(LS);
			return 1;
		}
		memory_region& r = regions[idx];
		lua_newtable(LS);
		lua_pushstring(LS, "region_name");
		lua_pushlstring(LS, r.region_name.c_str(), r.region_name.size());
		lua_settable(LS, -3);
		lua_pushstring(LS, "baseaddr");
		lua_pushnumber(LS, r.baseaddr);
		lua_settable(LS, -3);
		lua_pushstring(LS, "size");
		lua_pushnumber(LS, r.size);
		lua_settable(LS, -3);
		lua_pushstring(LS, "lastaddr");
		lua_pushnumber(LS, r.lastaddr);
		lua_settable(LS, -3);
		lua_pushstring(LS, "readonly");
		lua_pushboolean(LS, r.readonly);
		lua_settable(LS, -3);
		lua_pushstring(LS, "iospace");
		lua_pushboolean(LS, r.iospace);
		lua_settable(LS, -3);
		lua_pushstring(LS, "native_endian");
		lua_pushboolean(LS, r.native_endian);
		lua_settable(LS, -3);
		return 1;
	}

	function_ptr_luafun readvma("memory.read_vma", [](lua_State* LS, const std::string& fname) -> int {
		std::vector<memory_region> regions = get_regions();
		uint32_t num = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
		return handle_push_vma(LS, regions, num);
	});

	function_ptr_luafun findvma("memory.find_vma", [](lua_State* LS, const std::string& fname) -> int {
		std::vector<memory_region> regions = get_regions();
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		size_t i;
		for(i = 0; i < regions.size(); i++)
			if(addr >= regions[i].baseaddr && addr <= regions[i].lastaddr)
				break;
		return handle_push_vma(LS, regions, i);
	});

	const char* hexes = "0123456789ABCDEF";

	function_ptr_luafun hashstate("memory.hash_state", [](lua_State* LS, const std::string& fname) -> int {
		char hash[64];
		auto x = save_core_state();
		size_t offset = x.size() - 32;
		for(unsigned i = 0; i < 32; i++) {
			hash[2 * i + 0] = hexes[static_cast<unsigned char>(x[offset + i]) >> 4];
			hash[2 * i + 1] = hexes[static_cast<unsigned char>(x[offset + i]) & 0xF];
		}
		lua_pushlstring(LS, hash, 64);
		return 1;
	});

#define BLOCKSIZE 256

	function_ptr_luafun hashmemory("memory.hash_region", [](lua_State* LS, const std::string& fname) -> int {
		std::string hash;
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t size = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		char buffer[BLOCKSIZE];
		sha256 h;
		while(size > BLOCKSIZE) {
			for(size_t i = 0; i < BLOCKSIZE; i++)
				buffer[i] = memory_read_byte(addr + i);
			h.write(buffer, BLOCKSIZE);
			addr += BLOCKSIZE;
			size -= BLOCKSIZE;
		}
		for(size_t i = 0; i < size; i++)
			buffer[i] = memory_read_byte(addr + i);
		h.write(buffer, size);
		hash = h.read();
		lua_pushlstring(LS, hash.c_str(), 64);
		return 1;
	});

	function_ptr_luafun readmemoryr("memory.readregion", [](lua_State* LS, const std::string& fname) -> int {
		std::string hash;
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t size = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		lua_newtable(LS);
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			memory_read_bytes(addr, rsize, buffer);
			for(size_t i = 0; i < rsize; i++) {
				lua_pushnumber(LS, ctr++);
				lua_pushnumber(LS, static_cast<unsigned char>(buffer[i]));
				lua_settable(LS, -3);
			}
			addr += rsize;
			size -= rsize;
		}
		return 1;
	});

	function_ptr_luafun writememoryr("memory.writeregion", [](lua_State* LS, const std::string& fname) -> int {
		std::string hash;
		uint64_t addr = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t size = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			for(size_t i = 0; i < rsize; i++) {
				lua_pushnumber(LS, ctr++);
				lua_gettable(LS, 3);
				buffer[i] = lua_tointeger(LS, -1);
				lua_pop(LS, 1);
			}
			memory_write_bytes(addr, rsize, buffer);
			addr += rsize;
			size -= rsize;
		}
		return 1;
	});

	function_ptr_luafun gui_cbitmap("memory.map_structure", [](lua_State* LS, const std::string& fname) -> int {
		lua_mmap_struct* b = lua_class<lua_mmap_struct>::create(LS, LS);
		return 1;
	});

	lua_read_memory<uint8_t, uint8_t, memory_read_byte> rub("memory.readbyte");
	lua_read_memory<int8_t, uint8_t, memory_read_byte> rsb("memory.readsbyte");
	lua_read_memory<uint16_t, uint16_t, memory_read_word> ruw("memory.readword");
	lua_read_memory<int16_t, uint16_t, memory_read_word> rsw("memory.readsword");
	lua_read_memory<uint32_t, uint32_t, memory_read_dword> rud("memory.readdword");
	lua_read_memory<int32_t, uint32_t, memory_read_dword> rsd("memory.readsdword");
	lua_read_memory<uint64_t, uint64_t, memory_read_qword> ruq("memory.readqword");
	lua_read_memory<int64_t, uint64_t, memory_read_qword> rsq("memory.readsqword");
	lua_write_memory<uint8_t, memory_write_byte> wb("memory.writebyte");
	lua_write_memory<uint16_t, memory_write_word> ww("memory.writeword");
	lua_write_memory<uint32_t, memory_write_dword> wd("memory.writedword");
	lua_write_memory<uint64_t, memory_write_qword> wq("memory.writeqword");
	lua_mmap_memory_helper<uint8_t, uint8_t, memory_read_byte, memory_write_byte> mhub;
	lua_mmap_memory_helper<int8_t, uint8_t, memory_read_byte, memory_write_byte> mhsb;
	lua_mmap_memory_helper<uint16_t, uint16_t, memory_read_word, memory_write_word> mhuw;
	lua_mmap_memory_helper<int16_t, uint16_t, memory_read_word, memory_write_word> mhsw;
	lua_mmap_memory_helper<uint32_t, uint32_t, memory_read_dword, memory_write_dword> mhud;
	lua_mmap_memory_helper<int32_t, uint32_t, memory_read_dword, memory_write_dword> mhsd;
	lua_mmap_memory_helper<uint64_t, uint64_t, memory_read_qword, memory_write_qword> mhuq;
	lua_mmap_memory_helper<int64_t, uint64_t, memory_read_qword, memory_write_qword> mhsq;
	lua_mmap_memory mub("memory.mapbyte", mhub);
	lua_mmap_memory msb("memory.mapsbyte", mhsb);
	lua_mmap_memory muw("memory.mapword", mhuw);
	lua_mmap_memory msw("memory.mapsword", mhsw);
	lua_mmap_memory mud("memory.mapdword", mhud);
	lua_mmap_memory msd("memory.mapsdword", mhsd);
	lua_mmap_memory muq("memory.mapqword", mhuq);
	lua_mmap_memory msq("memory.mapsqword", mhsq);

}

int lua_mmap_struct::map(lua_State* LS)
{
	const char* name = lua_tostring(LS, 2);
	uint64_t addr = get_numeric_argument<uint64_t>(LS, 3, "lua_mmap_struct::map");
	const char* type = lua_tostring(LS, 4);
	if(!name) {
		lua_pushstring(LS, "lua_mmap_struct::map: Bad name");
		lua_error(LS);
		return 0;
	}
	if(!type) {
		lua_pushstring(LS, "lua_mmap_struct::map: Bad type");
		lua_error(LS);
		return 0;
	}
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
	else if(type2 == "dword")
		mappings[name2] = std::make_pair(&mhud, addr);
	else if(type2 == "sdword")
		mappings[name2] = std::make_pair(&mhsd, addr);
	else if(type2 == "qword")
		mappings[name2] = std::make_pair(&mhuq, addr);
	else if(type2 == "sqword")
		mappings[name2] = std::make_pair(&mhsq, addr);
	else {
		lua_pushstring(LS, "lua_mmap_struct::map: Bad type");
		lua_error(LS);
		return 0;
	}
	return 0;
}

DECLARE_LUACLASS(lua_mmap_struct, "MMAP_STRUCT");

lua_mmap_struct::lua_mmap_struct(lua_State* LS)
{
	static bool done = false;
	if(!done) {
		objclass<lua_mmap_struct>().bind(LS, "__index", &lua_mmap_struct::index, true);
		objclass<lua_mmap_struct>().bind(LS, "__newindex", &lua_mmap_struct::newindex, true);
		objclass<lua_mmap_struct>().bind(LS, "__call", &lua_mmap_struct::map);
		done = true;
	}
}
