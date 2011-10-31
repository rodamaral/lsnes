#include "lua-int.hpp"
#include "memorymanip.hpp"

namespace
{
	template<typename T, typename U, U (*rfun)(uint32_t addr)>
	class lua_read_memory : public lua_function
	{
	public:
		lua_read_memory(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS)
		{
			uint32_t addr = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
			lua_pushnumber(LS, static_cast<T>(rfun(addr)));
			return 1;
		}
	};

	template<typename T, bool (*wfun)(uint32_t addr, T value)>
	class lua_write_memory : public lua_function
	{
	public:
		lua_write_memory(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS)
		{
			uint32_t addr = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
			T value = get_numeric_argument<T>(LS, 2, fname.c_str());
			wfun(addr, value);
			return 0;
		}
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
		uint32_t addr = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
		size_t i;
		for(i = 0; i < regions.size(); i++)
			if(addr >= regions[i].baseaddr && addr <= regions[i].lastaddr)
				break;
		return handle_push_vma(LS, regions, i);
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
}
