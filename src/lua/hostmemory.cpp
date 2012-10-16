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

	function_ptr_luafun hm_read(LS, "hostmemory.read", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_write(LS, "hostmemory.write", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_readb(LS, "hostmemory.readbyte", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_writeb(LS, "hostmemory.writebyte", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint8_t, uint8_t>(L, fname);
	});

	function_ptr_luafun hm_readsb(LS, "hostmemory.readsbyte", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint8_t, int8_t>(L, fname);
	});

	function_ptr_luafun hm_writesb(LS, "hostmemory.writesbyte", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint8_t, int8_t>(L, fname);
	});

	function_ptr_luafun hm_readw(LS, "hostmemory.readword", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint16_t, uint16_t>(L, fname);
	});

	function_ptr_luafun hm_writew(LS, "hostmemory.writeword", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint16_t, uint16_t>(L, fname);
	});

	function_ptr_luafun hm_readsw(LS, "hostmemory.readsword", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint16_t, int16_t>(L, fname);
	});

	function_ptr_luafun hm_writesw(LS, "hostmemory.writesword", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint16_t, int16_t>(L, fname);
	});

	function_ptr_luafun hm_readd(LS, "hostmemory.readdword", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint32_t, uint32_t>(L, fname);
	});

	function_ptr_luafun hm_writed(LS, "hostmemory.writedword", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint32_t, uint32_t>(L, fname);
	});

	function_ptr_luafun hm_readsd(LS, "hostmemory.readsdword", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint32_t, int32_t>(L, fname);
	});

	function_ptr_luafun hm_writesd(LS, "hostmemory.writesdword", [](lua_state& L, const std::string& fname) ->
		int {
		return do_write<uint32_t, int32_t>(L, fname);
	});

	function_ptr_luafun hm_readq(LS, "hostmemory.readqword", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint64_t, uint64_t>(L, fname);
	});

	function_ptr_luafun hm_writeq(LS, "hostmemory.writeqword", [](lua_state& L, const std::string& fname) -> int {
		return do_write<uint64_t, uint64_t>(L, fname);
	});

	function_ptr_luafun hm_readsq(LS, "hostmemory.readsqword", [](lua_state& L, const std::string& fname) -> int {
		return do_read<uint64_t, int64_t>(L, fname);
	});

	function_ptr_luafun hm_writesq(LS, "hostmemory.writesqword", [](lua_state& L, const std::string& fname) ->
		int {
		return do_write<uint64_t, int64_t>(L, fname);
	});
}
