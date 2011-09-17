#include "lua-int.hpp"
#include "moviedata.hpp"

namespace
{
	function_ptr_luafun hm_read("hostmemory.read", [](lua_State* LS, const std::string& fname) -> int {
		size_t address = get_numeric_argument<size_t>(LS, 1, fname.c_str());
		auto& h = get_host_memory();
		if(address >= h.size()) {
			lua_pushboolean(LS, 0);
			return 1;
		}
		lua_pushnumber(LS, static_cast<uint8_t>(h[address]));
		return 1;
	});

	function_ptr_luafun hm_write("hostmemory.write", [](lua_State* LS, const std::string& fname) -> int {
		size_t address = get_numeric_argument<size_t>(LS, 1, fname.c_str());
		uint8_t value = get_numeric_argument<uint8_t>(LS, 2, fname.c_str());
		auto& h = get_host_memory();
		if(address >= h.size())
			h.resize(address + 1);
		h[address] = value;
		return 0;
	});
}
