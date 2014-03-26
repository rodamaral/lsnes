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

	lua::functions movie_fns(lua_func_bit, "", {
		{"_lsnes_string_regex", l_regex}
	});
}
