#include "lua/internal.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
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
		lua_read_memory(const std::string& name) : lua_function(LS, name) {}
		int invoke(lua_state& L)
		{
			uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
			L.pushnumber(static_cast<T>(rfun(addr)));
			return 1;
		}
	};

	template<typename T, bool (*wfun)(uint64_t addr, T value)>
	class lua_write_memory : public lua_function
	{
	public:
		lua_write_memory(const std::string& name) : lua_function(LS, name) {}
		int invoke(lua_state& L)
		{
			uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
			T value = L.get_numeric_argument<T>(2, fname.c_str());
			wfun(addr, value);
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


	template<typename T, typename U, U (*rfun)(uint64_t addr), bool (*wfun)(uint64_t addr, U value)>
	class lua_mmap_memory_helper : public mmap_base
	{
	public:
		~lua_mmap_memory_helper() {}
		void read(lua_state& L, uint64_t addr)
		{
			L.pushnumber(static_cast<T>(rfun(addr)));
		}

		void write(lua_state& L, uint64_t addr)
		{
			T value = L.get_numeric_argument<T>(3, "aperture(write)");
			wfun(addr, value);
		}
	};
}

class lua_mmap_struct
{
public:
	lua_mmap_struct(lua_state* L);

	~lua_mmap_struct()
	{
	}

	int index(lua_state& L)
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
	int newindex(lua_state& L)
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

	int map(lua_state& L);
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
		lua_mmap_memory(const std::string& name, mmap_base& _h) : lua_function(LS, name), h(_h) {}
		int invoke(lua_state& L)
		{
			if(L.isnoneornil(1)) {
				aperture_make_fun(L, 0, 0xFFFFFFFFFFFFFFFFULL, h);
				return 1;
			}
			uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
			uint64_t size = L.get_numeric_argument<uint64_t>(2, fname.c_str());
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

	function_ptr_luafun vmacount(LS, "memory.vma_count", [](lua_state& L, const std::string& fname) -> int {
		L.pushnumber(get_regions().size());
		return 1;
	});

	int handle_push_vma(lua_state& L, std::vector<memory_region>& regions, size_t idx)
	{
		if(idx >= regions.size()) {
			L.pushnil();
			return 1;
		}
		memory_region& r = regions[idx];
		L.newtable();
		L.pushstring("region_name");
		L.pushlstring(r.region_name.c_str(), r.region_name.size());
		L.settable(-3);
		L.pushstring("baseaddr");
		L.pushnumber(r.baseaddr);
		L.settable(-3);
		L.pushstring("size");
		L.pushnumber(r.size);
		L.settable(-3);
		L.pushstring("lastaddr");
		L.pushnumber(r.lastaddr);
		L.settable(-3);
		L.pushstring("readonly");
		L.pushboolean(r.readonly);
		L.settable(-3);
		L.pushstring("iospace");
		L.pushboolean(r.iospace);
		L.settable(-3);
		L.pushstring("native_endian");
		L.pushboolean(r.native_endian);
		L.settable(-3);
		return 1;
	}

	function_ptr_luafun readvma(LS, "memory.read_vma", [](lua_state& L, const std::string& fname) -> int {
		std::vector<memory_region> regions = get_regions();
		uint32_t num = L.get_numeric_argument<uint32_t>(1, fname.c_str());
		return handle_push_vma(L, regions, num);
	});

	function_ptr_luafun findvma(LS, "memory.find_vma", [](lua_state& L, const std::string& fname) -> int {
		std::vector<memory_region> regions = get_regions();
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		size_t i;
		for(i = 0; i < regions.size(); i++)
			if(addr >= regions[i].baseaddr && addr <= regions[i].lastaddr)
				break;
		return handle_push_vma(L, regions, i);
	});

	const char* hexes = "0123456789ABCDEF";

	function_ptr_luafun hashstate(LS, "memory.hash_state", [](lua_state& L, const std::string& fname) -> int {
		char hash[64];
		auto x = save_core_state();
		size_t offset = x.size() - 32;
		for(unsigned i = 0; i < 32; i++) {
			hash[2 * i + 0] = hexes[static_cast<unsigned char>(x[offset + i]) >> 4];
			hash[2 * i + 1] = hexes[static_cast<unsigned char>(x[offset + i]) & 0xF];
		}
		L.pushlstring(hash, 64);
		return 1;
	});

#define BLOCKSIZE 256

	function_ptr_luafun hashmemory(LS, "memory.hash_region", [](lua_state& L, const std::string& fname) -> int {
		std::string hash;
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t size = L.get_numeric_argument<uint64_t>(2, fname.c_str());
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
		L.pushlstring(hash.c_str(), 64);
		return 1;
	});

	function_ptr_luafun readmemoryr(LS, "memory.readregion", [](lua_state& L, const std::string& fname) -> int {
		std::string hash;
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t size = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		L.newtable();
		char buffer[BLOCKSIZE];
		uint64_t ctr = 0;
		while(size > 0) {
			size_t rsize = min(size, static_cast<uint64_t>(BLOCKSIZE));
			memory_read_bytes(addr, rsize, buffer);
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

	function_ptr_luafun writememoryr(LS, "memory.writeregion", [](lua_state& L, const std::string& fname) -> int {
		std::string hash;
		uint64_t addr = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t size = L.get_numeric_argument<uint64_t>(2, fname.c_str());
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
			memory_write_bytes(addr, rsize, buffer);
			addr += rsize;
			size -= rsize;
		}
		return 1;
	});

	function_ptr_luafun gui_cbitmap(LS, "memory.map_structure", [](lua_state& L, const std::string& fname) ->
		int {
		lua_mmap_struct* b = lua_class<lua_mmap_struct>::create(L, &L);
		return 1;
	});

	function_ptr_luafun memory_watchexpr(LS, "memory.read_expr", [](lua_state& L, const std::string& fname) ->
		int {
		std::string val = evaluate_watch(L.get_string(1, fname.c_str()));
		L.pushstring(val.c_str());
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

int lua_mmap_struct::map(lua_state& L)
{
	const char* name = L.tostring(2);
	uint64_t addr = L.get_numeric_argument<uint64_t>(3, "lua_mmap_struct::map");
	const char* type = L.tostring(4);
	if(!name) {
		L.pushstring("lua_mmap_struct::map: Bad name");
		L.error();
		return 0;
	}
	if(!type) {
		L.pushstring("lua_mmap_struct::map: Bad type");
		L.error();
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
		L.pushstring("lua_mmap_struct::map: Bad type");
		L.error();
		return 0;
	}
	return 0;
}

DECLARE_LUACLASS(lua_mmap_struct, "MMAP_STRUCT");

lua_mmap_struct::lua_mmap_struct(lua_state* L)
{
	static char key;
	L->pushlightuserdata(&key);
	L->rawget(LUA_REGISTRYINDEX);
	if(L->type(-1) == LUA_TNIL) {
		L->pop(1);
		L->pushlightuserdata(&key);
		L->pushboolean(true);
		L->rawset(LUA_REGISTRYINDEX);
		objclass<lua_mmap_struct>().bind(*L, "__index", &lua_mmap_struct::index, true);
		objclass<lua_mmap_struct>().bind(*L, "__newindex", &lua_mmap_struct::newindex, true);
		objclass<lua_mmap_struct>().bind(*L, "__call", &lua_mmap_struct::map);
	} else
		L->pop(1);
}
