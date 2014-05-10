#ifndef _instance__hpp__included__
#define _instance__hpp__included__

#include "movie.hpp"

struct emulator_instance
{
	movie_logic mlogic;
};

extern emulator_instance lsnes_instance;

#endif
