#include "lua/internal.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "library/serialization.hpp"
#include "library/int24.hpp"

namespace
{
	template<typename S>
	int do_read(lua::state& L, lua::parameters& P)
	{
		size_t address;

		P(address);

		auto& h = CORE().mlogic->get_mfile().dyn.host_memory;
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
		size_t address;
		S value;

		P(address, value);

		auto& h = CORE().mlogic->get_mfile().dyn.host_memory;
		if(address + sizeof(S) > h.size())
			h.resize(address + sizeof(S));
		serialization::write_endian<S>(&h[address], value, 1);
		return 0;
	}

	lua::functions LUA_hostops_fns(lua_func_misc, "hostmemory", {
		{"read", do_read<uint8_t>},
		{"write", do_write<uint8_t>},
		{"readbyte", do_read<uint8_t>},
		{"writebyte", do_write<uint8_t>},
		{"readsbyte", do_read<int8_t>},
		{"writesbyte", do_write<int8_t>},
		{"readword", do_read<uint16_t>},
		{"writeword", do_write<uint16_t>},
		{"readsword", do_read<int16_t>},
		{"writesword", do_write<int16_t>},
		{"readhword", do_read<ss_uint24_t>},
		{"writehword", do_write<ss_uint24_t>},
		{"readshword", do_read<ss_int24_t>},
		{"writeshword", do_write<ss_int24_t>},
		{"readdword", do_read<uint32_t>},
		{"writedword", do_write<uint32_t>},
		{"readsdword", do_read<int32_t>},
		{"writesdword", do_write<int32_t>},
		{"readqword", do_read<uint64_t>},
		{"writeqword", do_write<uint64_t>},
		{"readsqword", do_read<int64_t>},
		{"writesqword", do_write<int64_t>},
		{"readfloat", do_read<float>},
		{"writefloat", do_write<float>},
		{"readdouble", do_read<double>},
		{"writedouble", do_write<double>},
	});
}
