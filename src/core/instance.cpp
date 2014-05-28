#include <deque>
#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/controller.hpp"
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

dtor_list::dtor_list()
{
	list = NULL;
}

dtor_list::~dtor_list()
{
	destroy();
}

void dtor_list::destroy()
{
	dtor_list::entry* e = list;
	while(e) {
		e->free1(e->ptr);
		e = e->prev;
	}
	e = list;
	while(e) {
		dtor_list::entry* f = e;
		e = e->prev;
		f->free2(f->ptr);
		delete f;
	}
	list = NULL;
}

emulator_instance::emulator_instance()
{
	//Preinit.
	D.prealloc(fbuf);
	D.prealloc(project);
	D.prealloc(buttons);

	D.init(mlogic);
	D.init(memory);
	D.init(lua);
	D.init(mwatch, *memory, *project, *fbuf);
	D.init(settings);
	D.init(setcache, *settings);
	D.init(commentary, *settings);
	D.init(subtitles, *mlogic, *fbuf);
	D.init(mbranch, *mlogic);
	D.init(controls, *project, *mlogic, *buttons);
	D.init(keyboard);
	D.init(command);
	D.init(mapper, *keyboard, *command);
	D.init(fbuf, *subtitles, *settings, *mwatch, *keyboard);
	D.init(buttons, *controls, *mapper, *keyboard, *fbuf);
	D.init(mteditor, *mlogic, *controls);
	D.init(status_A);
	D.init(status_B);
	D.init(status_C);
	D.init(status, *status_A, *status_B, *status_C);
	D.init(abindmanager, *mapper, *command);
	D.init(nrrdata);
	D.init(cmapper, *memory);
	D.init(project, *commentary, *mwatch, *command, *controls, *setcache, *buttons);
	D.init(dbg);
	D.init(framerate);
	D.init(iqueue, *command);
	D.init(mdumper);

	status_A->valid = false;
	status_B->valid = false;
	status_C->valid = false;
	command->add_set(lsnes_cmds);
	mapper->add_invbind_set(lsnes_invbinds);
	settings->add_set(lsnes_setgrp);
}

emulator_instance::~emulator_instance()
{
	D.destroy();
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
