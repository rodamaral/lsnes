#include "lua/internal.hpp"
#include "core/moviedata.hpp"

namespace
{
	template<typename U, typename S>
	int do_read(lua_state& L, const std::string& fname)
	{
		size_t address = L.get_numeric_argument<size_t>(1, fname.c_str());
		auto& h = get_host_memory();
		if(address + sizeof(U) > h.size()) {
			L.pushboolean(0);
			return 1;
		}
		U ret = 0;
		for(size_t i = 0; i < sizeof(U); i++)
			ret = 256 * ret + static_cast<uint8_t>(h[address + i]);
		L.pushnumber(static_cast<S>(ret));
		return 1;
	}

	template<typename U, typename S>
	int do_write(lua_state& L, const std::string& fname)
	{
		size_t address = L.get_numeric_argument<size_t>(1, fname.c_str());
		U value = static_cast<U>(L.get_numeric_argument<S>(2, fname.c_str()));
		auto& h = get_host_memory();
		if(address + sizeof(U) > h.size())
			h.resize(address + sizeof(U));
		for(size_t i = sizeof(U) - 1; i < sizeof(U); i--) {
			h[address + i] = value;
			value >>= 8;
		}
		return 0;
	}

	function_ptr_luafun hm_read(lua_func_misc, "hostmemory.read", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_write(lua_func_misc, "hostmemory.write", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_readb(lua_func_misc, "hostmemory.readbyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_writeb(lua_func_misc, "hostmemory.writebyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_readsb(lua_func_misc, "hostmemory.readsbyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint8_t, int8_t>(L, fname);
	});

	function_ptr_luafun hm_writesb(lua_func_misc, "hostmemory.writesbyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint8_t, int8_t>(L, fname);
	});

	function_ptr_luafun hm_readw(lua_func_misc, "hostmemory.readword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint16_t, uint16_t>(L, fname);
	});

	function_ptr_luafun hm_writew(lua_func_misc, "hostmemory.writeword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint16_t, uint16_t>(L, fname);
	});

	function_ptr_luafun hm_readsw(lua_func_misc, "hostmemory.readsword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint16_t, int16_t>(L, fname);
	});

	function_ptr_luafun hm_writesw(lua_func_misc, "hostmemory.writesword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint16_t, int16_t>(L, fname);
	});

	function_ptr_luafun hm_readd(lua_func_misc, "hostmemory.readdword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint32_t, uint32_t>(L, fname);
	});

	function_ptr_luafun hm_writed(lua_func_misc, "hostmemory.writedword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint32_t, uint32_t>(L, fname);
	});

	function_ptr_luafun hm_readsd(lua_func_misc, "hostmemory.readsdword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint32_t, int32_t>(L, fname);
	});

	function_ptr_luafun hm_writesd(lua_func_misc, "hostmemory.writesdword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint32_t, int32_t>(L, fname);
	});

	function_ptr_luafun hm_readq(lua_func_misc, "hostmemory.readqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint64_t, uint64_t>(L, fname);
	});

	function_ptr_luafun hm_writeq(lua_func_misc, "hostmemory.writeqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint64_t, uint64_t>(L, fname);
	});

	function_ptr_luafun hm_readsq(lua_func_misc, "hostmemory.readsqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint64_t, int64_t>(L, fname);
	});

	function_ptr_luafun hm_writesq(lua_func_misc, "hostmemory.writesqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint64_t, int64_t>(L, fname);
	});
}
