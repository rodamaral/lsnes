#include "lua/internal.hpp"
#include "core/moviedata.hpp"
#include "library/serialization.hpp"
#include "library/int24.hpp"

namespace
{
	template<typename S>
	int do_read(lua::state& L, lua::parameters& P)
	{
		size_t address = P.arg<size_t>();
		auto& h = get_host_memory();
		if(address + sizeof(S) > h.size()) {
			L.pushboolean(0);
			return 1;
		}
		L.pushnumber(serialization::read_endian<S>(&h[address], 1));
		return 1;
	}

	template<typename S>
	int do_write(lua::state& L, lua::parameters& P)
	{
		auto address = P.arg<size_t>();
		S value = P.arg<S>();
		auto& h = get_host_memory();
		if(address + sizeof(S) > h.size())
			h.resize(address + sizeof(S));
		serialization::write_endian<S>(&h[address], value, 1);
		return 0;
	}

	lua::fnptr2 hm_read(lua_func_misc, "hostmemory.read", do_read<uint8_t>);
	lua::fnptr2 hm_write(lua_func_misc, "hostmemory.write", do_write<uint8_t>);
	lua::fnptr2 hm_readb(lua_func_misc, "hostmemory.readbyte", do_read<uint8_t>);
	lua::fnptr2 hm_writeb(lua_func_misc, "hostmemory.writebyte", do_write<uint8_t>);
	lua::fnptr2 hm_readsb(lua_func_misc, "hostmemory.readsbyte", do_read<int8_t>);
	lua::fnptr2 hm_writesb(lua_func_misc, "hostmemory.writesbyte", do_write<int8_t>);
	lua::fnptr2 hm_readw(lua_func_misc, "hostmemory.readword", do_read<uint16_t>);
	lua::fnptr2 hm_writew(lua_func_misc, "hostmemory.writeword", do_write<uint16_t>);
	lua::fnptr2 hm_readsw(lua_func_misc, "hostmemory.readsword", do_read<int16_t>);
	lua::fnptr2 hm_writesw(lua_func_misc, "hostmemory.writesword", do_write<int16_t>);
	lua::fnptr2 hm_readh(lua_func_misc, "hostmemory.readhword", do_read<ss_uint24_t>);
	lua::fnptr2 hm_writeh(lua_func_misc, "hostmemory.writehword", do_write<ss_uint24_t>);
	lua::fnptr2 hm_readsh(lua_func_misc, "hostmemory.readshword", do_read<ss_int24_t>);
	lua::fnptr2 hm_writesh(lua_func_misc, "hostmemory.writeshword", do_write<ss_int24_t>);
	lua::fnptr2 hm_readd(lua_func_misc, "hostmemory.readdword", do_read<uint32_t>);
	lua::fnptr2 hm_writed(lua_func_misc, "hostmemory.writedword", do_write<uint32_t>);
	lua::fnptr2 hm_readsd(lua_func_misc, "hostmemory.readsdword", do_read<int32_t>);
	lua::fnptr2 hm_writesd(lua_func_misc, "hostmemory.writesdword", do_write<int32_t>);
	lua::fnptr2 hm_readq(lua_func_misc, "hostmemory.readqword", do_read<uint64_t>);
	lua::fnptr2 hm_writeq(lua_func_misc, "hostmemory.writeqword", do_write<uint64_t>);
	lua::fnptr2 hm_readsq(lua_func_misc, "hostmemory.readsqword", do_read<int64_t>);
	lua::fnptr2 hm_writesq(lua_func_misc, "hostmemory.writesqword", do_write<int64_t>);
	lua::fnptr2 hm_readf4(lua_func_misc, "hostmemory.readfloat", do_read<float>);
	lua::fnptr2 hm_writef4(lua_func_misc, "hostmemory.writefloat", do_write<float>);
	lua::fnptr2 hm_readf8(lua_func_misc, "hostmemory.readdouble", do_read<double>);
	lua::fnptr2 hm_writef8(lua_func_misc, "hostmemory.writedouble", do_write<double>);
}
