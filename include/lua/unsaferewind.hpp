#ifndef _lua__unsaferewind__hpp__included__
#define _lua__unsaferewind__hpp__included__

#include "library/luabase.hpp"
#include "library/string.hpp"

struct lua_unsaferewind
{
	lua_unsaferewind(lua_state& L);
	std::vector<char> state;
	uint64_t frame;
	uint64_t lag;
	uint64_t ptr;
	uint64_t secs;
	uint64_t ssecs;
	std::vector<uint32_t> pollcounters;
	std::vector<char> hostmemory;
	std::string print()
	{
		return (stringfmt() << "to frame " << frame).str();
	}
};

#endif
