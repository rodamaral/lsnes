#include "core/instance.hpp"
#include "core/settings.hpp"
#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "core/random.hpp"
#ifdef __linux__
#include <execinfo.h>
#endif

emulator_instance::emulator_instance()
	: mwatch(memory, project, fbuf), setcache(settings), commentary(settings), subtitles(mlogic, fbuf),
	mbranch(&mlogic), mteditor(mlogic, controls), status(status_A, status_B, status_C), mapper(keyboard, command),
	abindmanager(mapper, command), cmapper(memory), controls(project, mlogic),
	project(commentary, mwatch, command, controls, setcache), fbuf(subtitles, settings, mwatch, keyboard),
	iqueue(command)
{
	status_A.valid = false;
	status_B.valid = false;
	status_C.valid = false;
	command.add_set(lsnes_cmds);
	mapper.add_invbind_set(lsnes_invbinds);
	settings.add_set(lsnes_setgrp);
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
