#ifndef _instance__hpp__included__
#define _instance__hpp__included__

#include "core/command.hpp"
#include "core/emustatus.hpp"
#include "core/inthread.hpp"
#include "core/movie.hpp"
#include "core/mbranch.hpp"
#include "core/memorywatch.hpp"
#include "core/multitrack.hpp"
#include "library/command.hpp"
#include "library/lua-base.hpp"
#include "library/memoryspace.hpp"
#include "library/settingvar.hpp"
#include "library/keyboard.hpp"
#include "library/keyboard-mapper.hpp"

struct emulator_instance
{
	emulator_instance();
	movie_logic mlogic;
	memory_space memory;
	lua::state lua;
	memwatch_set mwatch;
	settingvar::group settings;
	settingvar::cache setcache;
	voice_commentary commentary;
	subtitle_commentary subtitles;
	movie_branches mbranch;
	multitrack_edit mteditor;
	_lsnes_status status_A;
	_lsnes_status status_B;
	_lsnes_status status_C;
	triplebuffer::triplebuffer<_lsnes_status> status;
	keyboard::keyboard keyboard;
	keyboard::mapper mapper;
	command::group command;
	alias_binds_manager abindmanager;
};

extern emulator_instance lsnes_instance;

emulator_instance& CORE();

#endif
