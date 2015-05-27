#include "lua/internal.hpp"
#include "library/string.hpp"

namespace
{
	int l_regex(lua::state& L, lua::parameters& P)
	{
		std::string regexp;
		std::string against;

		P(regexp, against);

		regex_results r = regex(regexp, against);
		if(!r) {
			L.pushboolean(false);
			return 1;
		}
		if(r.size() == 1) {
			L.pushboolean(true);
			return 1;
		}
		for(size_t i = 1; i < r.size(); i++)
			L.pushlstring(r[i]);
		return r.size() - 1;
	}

	int l_hex(lua::state& L, lua::parameters& P)
	{
		uint64_t v, p;

		P(v, P.optional(p, -1));

		if(p < 0)
			L.pushlstring((stringfmt() << std::hex << v).str());
		else
			L.pushlstring((stringfmt() << std::hex << std::setw(p) << std::setfill('0') << v).str());
		return 1;
	}

	template<bool right>
	int l_pad(lua::state& L, lua::parameters& P)
	{
		std::string x;
		uint64_t l;

		P(x, l);

		if(x.length() >= l)
			L.pushlstring(x);
		else if(right)
			L.pushlstring(x + std::string(l - x.length(), ' '));
		else
			L.pushlstring(std::string(l - x.length(), ' ') + x);
		return 1;
	}

	lua::functions LUA_string_fns(lua_func_bit, "", {
		{"_lsnes_string_regex", l_regex},
		{"_lsnes_string_hex", l_hex},
		{"_lsnes_string_lpad", l_pad<false>},
		{"_lsnes_string_rpad", l_pad<true>},
	});
}
