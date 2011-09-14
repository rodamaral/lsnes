#include "lua-int.hpp"
#include "mainloop.hpp"

namespace
{
	class lua_hostmemory_read : public lua_function
	{
	public:
		lua_hostmemory_read() : lua_function("hostmemory.read") {}
		int invoke(lua_State* LS, window* win)
		{
			size_t address = get_numeric_argument<size_t>(LS, 1, fname.c_str());
			auto& h = get_host_memory();
			if(address >= h.size()) {
				lua_pushboolean(LS, 0);
				return 1;
			}
			lua_pushnumber(LS, static_cast<uint8_t>(h[address]));
			return 1;
		}
	} hostmemory_read;

	class lua_hostmemory_write : public lua_function
	{
	public:
		lua_hostmemory_write() : lua_function("hostmemory.write") {}
		int invoke(lua_State* LS, window* win)
		{
			size_t address = get_numeric_argument<size_t>(LS, 1, fname.c_str());
			uint8_t value = get_numeric_argument<uint8_t>(LS, 2, fname.c_str());
			auto& h = get_host_memory();
			if(address >= h.size())
				h.resize(address + 1);
			h[address] = value;
			return 0;
		}
	} hostmemory_write;
}
