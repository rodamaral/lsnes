#include "lua-int.hpp"
#include "memorymanip.hpp"

namespace
{
	template<typename T, typename U, U (*rfun)(uint32_t addr)>
	class lua_read_memory : public lua_function
	{
	public:
		lua_read_memory(const std::string& name) : lua_function(name) {}
		int invoke(lua_State* LS, window* win)
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
		int invoke(lua_State* LS, window* win)
		{
			uint32_t addr = get_numeric_argument<uint32_t>(LS, 1, fname.c_str());
			T value = get_numeric_argument<T>(LS, 2, fname.c_str());
			wfun(addr, value);
			return 0;
		}
	};

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
