#ifndef _lua__unsaferewind__hpp__included__
#define _lua__unsaferewind__hpp__included__

#include "library/lua-base.hpp"
#include "library/string.hpp"
#include "core/moviefile.hpp"

struct lua_unsaferewind
{
	lua_unsaferewind(lua::state& L);
	static size_t overcommit() { return 0; }
	//The console state.
	dynamic_state console_state;
	//Extra state variable involved in fast movie restore. It is not part of normal console state.
	uint64_t ptr;
	std::string print()
	{
		return (stringfmt() << "to frame " << console_state.save_frame).str();
	}
};

#endif
