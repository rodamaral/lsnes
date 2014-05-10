#ifndef _instance__hpp__included__
#define _instance__hpp__included__

#include "core/movie.hpp"
#include "core/memorywatch.hpp"
#include "library/lua-base.hpp"
#include "library/memoryspace.hpp"

struct emulator_instance
{
	movie_logic mlogic;
	memory_space memory;
	lua::state lua;
	lsnes_memorywatch_set mwatch;
};

extern emulator_instance lsnes_instance;

#endif
