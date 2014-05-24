#include <deque>
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/debug.hpp"
#include "core/emustatus.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/inthread.hpp"
#include "core/keymapper.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/mbranch.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/multitrack.hpp"
#include "core/project.hpp"
#include "core/queue.hpp"
#include "core/random.hpp"
#include "core/settings.hpp"
#include "library/command.hpp"
#include "library/lua-base.hpp"
#include "library/memoryspace.hpp"
#include "library/settingvar.hpp"
#include "library/keyboard.hpp"
#include "library/keyboard-mapper.hpp"

#ifdef __linux__
#include <execinfo.h>
#endif

emulator_instance::emulator_instance()
{
	//Preinit.
	fbuf = (emu_framebuffer*)new char[sizeof(emu_framebuffer) + 32];
	project = (project_state*)new char[sizeof(project_state) + 32];

	mlogic = new movie_logic;
	memory = new memory_space;
	lua = new lua::state;
	mwatch = new memwatch_set(*memory, *project, *fbuf);
	settings = new settingvar::group;
	setcache = new settingvar::cache(*settings);
	commentary = new voice_commentary(*settings);
	subtitles = new subtitle_commentary(*mlogic, *fbuf);
	mbranch = new movie_branches(*mlogic);
	controls = new controller_state(*project, *mlogic);
	mteditor = new multitrack_edit(*mlogic, *controls);
	status_A = new _lsnes_status;
	status_B = new _lsnes_status;
	status_C = new _lsnes_status;
	status = new triplebuffer::triplebuffer<_lsnes_status>(*status_A, *status_B, *status_C);
	keyboard = new keyboard::keyboard;
	command = new command::group;
	mapper = new keyboard::mapper(*keyboard, *command);
	abindmanager = new alias_binds_manager(*mapper, *command);
	nrrdata = new rrdata;
	cmapper = new cart_mappings_refresher(*memory);
	new(project) project_state(*commentary, *mwatch, *command, *controls, *setcache);
	dbg = new debug_context;
	framerate = new framerate_regulator;
	new(fbuf) emu_framebuffer(*subtitles, *settings, *mwatch, *keyboard);
	iqueue = new input_queue(*command);

	status_A->valid = false;
	status_B->valid = false;
	status_C->valid = false;
	command->add_set(lsnes_cmds);
	mapper->add_invbind_set(lsnes_invbinds);
	settings->add_set(lsnes_setgrp);
}

emulator_instance::~emulator_instance()
{
	delete iqueue;
	fbuf->~emu_framebuffer();
	delete framerate;
	delete dbg;
	project->~project_state();
	delete cmapper;
	delete nrrdata;
	delete abindmanager;
	delete mapper;
	delete command;
	delete keyboard;
	delete status;
	delete status_C;
	delete status_B;
	delete status_A;
	delete mteditor;
	delete controls;
	delete mbranch;
	delete subtitles;
	delete commentary;
	delete setcache;
	delete settings;
	delete mwatch;
	delete lua;
	delete memory;
	delete mlogic;

	delete[] reinterpret_cast<char*>(project);
	delete[] reinterpret_cast<char*>(fbuf);
}

emulator_instance lsnes_instance;

emulator_instance& CORE()
{
	if(threads::id() != lsnes_instance.emu_thread) {
		std::cerr << "WARNING: CORE() called in wrong thread." << std::endl;
#ifdef __linux__
		void* arr[256];
		backtrace_symbols_fd(arr, 256, 2);
#endif
	}
	return lsnes_instance;
}
