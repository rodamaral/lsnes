#include "lua/internal.hpp"
#include "core/moviedata.hpp"

namespace
{
	template<typename U, typename S>
	int do_read(lua_State* LS, const std::string& fname)
	{
		size_t address = get_numeric_argument<size_t>(LS, 1, fname.c_str());
		auto& h = get_host_memory();
		if(address + sizeof(U) > h.size()) {
			lua_pushboolean(LS, 0);
			return 1;
		}
		U ret = 0;
		for(size_t i = 0; i < sizeof(U); i++)
			ret = 256 * ret + static_cast<uint8_t>(h[address + i]);
		lua_pushnumber(LS, static_cast<S>(ret));
		return 1;
	}

	template<typename U, typename S>
	int do_write(lua_State* LS, const std::string& fname)
	{
		size_t address = get_numeric_argument<size_t>(LS, 1, fname.c_str());
		U value = static_cast<U>(get_numeric_argument<S>(LS, 2, fname.c_str()));
		auto& h = get_host_memory();
		if(address + sizeof(U) > h.size())
			h.resize(address + sizeof(U));
		for(size_t i = sizeof(U) - 1; i < sizeof(U); i--) {
			h[address + i] = value;
			value >>= 8;
		}
		return 0;
	}

	function_ptr_luafun hm_read("hostmemory.read", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint8_t, uint8_t>(LS, fname);
	});

	function_ptr_luafun hm_write("hostmemory.write", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint8_t, uint8_t>(LS, fname);
	});

	function_ptr_luafun hm_readb("hostmemory.readbyte", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint8_t, uint8_t>(LS, fname);
	});

	function_ptr_luafun hm_writeb("hostmemory.writebyte", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint8_t, uint8_t>(LS, fname);
	});

	function_ptr_luafun hm_readsb("hostmemory.readsbyte", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint8_t, int8_t>(LS, fname);
	});

	function_ptr_luafun hm_writesb("hostmemory.writesbyte", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint8_t, int8_t>(LS, fname);
	});

	function_ptr_luafun hm_readw("hostmemory.readword", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint16_t, uint16_t>(LS, fname);
	});

	function_ptr_luafun hm_writew("hostmemory.writeword", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint16_t, uint16_t>(LS, fname);
	});

	function_ptr_luafun hm_readsw("hostmemory.readsword", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint16_t, int16_t>(LS, fname);
	});

	function_ptr_luafun hm_writesw("hostmemory.writesword", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint16_t, int16_t>(LS, fname);
	});

	function_ptr_luafun hm_readd("hostmemory.readdword", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint32_t, uint32_t>(LS, fname);
	});

	function_ptr_luafun hm_writed("hostmemory.writedword", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint32_t, uint32_t>(LS, fname);
	});

	function_ptr_luafun hm_readsd("hostmemory.readsdword", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint32_t, int32_t>(LS, fname);
	});

	function_ptr_luafun hm_writesd("hostmemory.writesdword", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint32_t, int32_t>(LS, fname);
	});

	function_ptr_luafun hm_readq("hostmemory.readqword", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint64_t, uint64_t>(LS, fname);
	});

	function_ptr_luafun hm_writeq("hostmemory.writeqword", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint64_t, uint64_t>(LS, fname);
	});

	function_ptr_luafun hm_readsq("hostmemory.readsqword", [](lua_State* LS, const std::string& fname) -> int {
		return do_read<uint64_t, int64_t>(LS, fname);
	});

	function_ptr_luafun hm_writesq("hostmemory.writesqword", [](lua_State* LS, const std::string& fname) -> int {
		return do_write<uint64_t, int64_t>(LS, fname);
	});
}
