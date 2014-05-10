#ifndef _instance__hpp__included__
#define _instance__hpp__included__

#include "core/emustatus.hpp"
#include "core/inthread.hpp"
#include "core/movie.hpp"
#include "core/mbranch.hpp"
#include "core/memorywatch.hpp"
#include "core/multitrack.hpp"
#include "library/lua-base.hpp"
#include "library/memoryspace.hpp"
#include "library/settingvar.hpp"

struct emulator_instance
{
	emulator_instance();
	movie_logic mlogic;
	memory_space memory;
	lua::state lua;
	lsnes_memorywatch_set mwatch;
	settingvar::cache setcache;
	voice_commentary commentary;
	subtitle_commentary subtitles;
	movie_branches mbranch;
	multitrack_edit mteditor;
	_lsnes_status status_A;
	_lsnes_status status_B;
	_lsnes_status status_C;
	triplebuffer::triplebuffer<_lsnes_status> status;
};

extern emulator_instance lsnes_instance;

emulator_instance& CORE();

#endif
