#ifndef _lua__unsaferewind__hpp__included__
#define _lua__unsaferewind__hpp__included__

#include "library/lua-base.hpp"
#include "library/string.hpp"
#include "core/moviefile.hpp"

struct lua_unsaferewind
{
	lua_unsaferewind(lua::state& L);
	static size_t overcommit() { return 0; }
	moviefile::dynamic_state dstate;
	std::string print()
	{
		return (stringfmt() << "to frame " << dstate.save_frame).str();
	}
};

#endif
