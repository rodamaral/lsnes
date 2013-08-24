#include "lua/internal.hpp"
#include "core/moviedata.hpp"
#include "library/serialization.hpp"

namespace
{
	template<typename S>
	int do_read(lua_state& L, const std::string& fname)
	{
		size_t address = L.get_numeric_argument<size_t>(1, fname.c_str());
		auto& h = get_host_memory();
		if(address + sizeof(S) > h.size()) {
			L.pushboolean(0);
			return 1;
		}
		L.pushnumber(read_of_endian<S>(&h[address], 1));
		return 1;
	}

	template<typename S>
	int do_write(lua_state& L, const std::string& fname)
	{
		size_t address = L.get_numeric_argument<size_t>(1, fname.c_str());
		S value = static_cast<S>(L.get_numeric_argument<S>(2, fname.c_str()));
		auto& h = get_host_memory();
		if(address + sizeof(S) > h.size())
			h.resize(address + sizeof(S));
		write_of_endian<S>(&h[address], value, 1);
		return 0;
	}

	function_ptr_luafun hm_read(lua_func_misc, "hostmemory.read", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint8_t>(L, fname);
	});

	function_ptr_luafun hm_write(lua_func_misc, "hostmemory.write", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint8_t>(L, fname);
	});

	function_ptr_luafun hm_readb(lua_func_misc, "hostmemory.readbyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint8_t>(L, fname);
	});

	function_ptr_luafun hm_writeb(lua_func_misc, "hostmemory.writebyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint8_t>(L, fname);
	});

	function_ptr_luafun hm_readsb(lua_func_misc, "hostmemory.readsbyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<int8_t>(L, fname);
	});

	function_ptr_luafun hm_writesb(lua_func_misc, "hostmemory.writesbyte", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<int8_t>(L, fname);
	});

	function_ptr_luafun hm_readw(lua_func_misc, "hostmemory.readword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint16_t>(L, fname);
	});

	function_ptr_luafun hm_writew(lua_func_misc, "hostmemory.writeword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint16_t>(L, fname);
	});

	function_ptr_luafun hm_readsw(lua_func_misc, "hostmemory.readsword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<int16_t>(L, fname);
	});

	function_ptr_luafun hm_writesw(lua_func_misc, "hostmemory.writesword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<int16_t>(L, fname);
	});

	function_ptr_luafun hm_readd(lua_func_misc, "hostmemory.readdword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint32_t>(L, fname);
	});

	function_ptr_luafun hm_writed(lua_func_misc, "hostmemory.writedword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint32_t>(L, fname);
	});

	function_ptr_luafun hm_readsd(lua_func_misc, "hostmemory.readsdword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<int32_t>(L, fname);
	});

	function_ptr_luafun hm_writesd(lua_func_misc, "hostmemory.writesdword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<int32_t>(L, fname);
	});

	function_ptr_luafun hm_readq(lua_func_misc, "hostmemory.readqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<uint64_t>(L, fname);
	});

	function_ptr_luafun hm_writeq(lua_func_misc, "hostmemory.writeqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<uint64_t>(L, fname);
	});

	function_ptr_luafun hm_readsq(lua_func_misc, "hostmemory.readsqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_read<int64_t>(L, fname);
	});

	function_ptr_luafun hm_writesq(lua_func_misc, "hostmemory.writesqword", [](lua_state& L,
		const std::string& fname) -> int {
		return do_write<int64_t>(L, fname);
	});
}
