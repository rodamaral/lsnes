#include "core/advdumper.hpp"
#include "core/audioapi.hpp"
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/controller.hpp"
#include "core/debug.hpp"
#include "core/dispatch.hpp"
#include "core/emustatus.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/inthread.hpp"
#include "core/jukebox.hpp"
#include "core/keymapper.hpp"
#include "core/mbranch.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/messages.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/movie.hpp"
#include "core/multitrack.hpp"
#include "core/project.hpp"
#include "core/queue.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/runmode.hpp"
#include "core/settings.hpp"
#include "fonts/wrapper.hpp"
#include "library/command.hpp"
#include "library/framebuffer.hpp"
#include "library/keyboard.hpp"
#include "library/keyboard-mapper.hpp"
#include "library/lua-base.hpp"
#include "library/memoryspace.hpp"
#include "library/settingvar.hpp"
#include "lua/lua.hpp"

#include <deque>
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
	D.prealloc(dispatch);
	D.prealloc(supdater);
	D.prealloc(rom);

	D.init(dispatch);
	D.init(command);
	D.init(iqueue, *command);
	D.init(mlogic);
	D.init(slotcache, *mlogic, *command);
	D.init(memory);
	D.init(settings);
	D.init(lua);
	D.init(lua2, *lua, *command, *settings);
	D.init(mwatch, *memory, *project, *fbuf, *rom);
	D.init(jukebox, *settings, *command);
	D.init(setcache, *settings);
	D.init(audio);
	D.init(commentary, *settings, *dispatch, *audio, *command);
	D.init(subtitles, *mlogic, *fbuf, *dispatch, *command);
	D.init(mbranch, *mlogic, *dispatch, *supdater);
	D.init(controls, *project, *mlogic, *buttons, *dispatch, *supdater, *command);
	D.init(keyboard);
	D.init(mapper, *keyboard, *command);
	D.init(rom);
	D.init(fbuf, *subtitles, *settings, *mwatch, *keyboard, *dispatch, *lua2, *rom, *supdater, *command);
	D.init(buttons, *controls, *mapper, *keyboard, *fbuf, *dispatch, *lua2, *command);
	D.init(mteditor, *mlogic, *controls, *dispatch, *supdater, *buttons, *command);
	D.init(status_A);
	D.init(status_B);
	D.init(status_C);
	D.init(status, *status_A, *status_B, *status_C);
	D.init(abindmanager, *mapper, *command);
	D.init(nrrdata);
	D.init(cmapper, *memory, *mlogic, *rom);
	D.init(project, *commentary, *mwatch, *command, *controls, *settings, *buttons, *dispatch, *iqueue, *rom,
		*supdater);
	D.init(dbg, *dispatch, *rom, *memory, *command);
	D.init(framerate, *command);
	D.init(mdumper, *lua2);
	D.init(runmode);
	D.init(supdater, *project, *mlogic, *commentary, *status, *runmode, *mdumper, *jukebox, *slotcache,
	       *framerate, *controls, *mteditor, *lua2, *rom, *mwatch, *dispatch);

	status_A->valid = false;
	status_B->valid = false;
	status_C->valid = false;
	command->add_set(lsnes_cmds);
	mapper->add_invbind_set(lsnes_invbinds);
	settings->add_set(lsnes_setgrp);
	dispatch->set_error_streams(&messages.getstream());
	random_seed_value = 0;
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

namespace
{
	uint32_t digits(size_t num)
	{
		if(num < 10) return 1;
		return 1 + digits(num / 10);
	}

	command::fnptr<> CMD_memory_use(lsnes_cmds, "show-memory", "Show memory usage",
		"show-memory\nShow memory usage",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto report = memtracker::singleton().report();
			uint32_t maxwidth_left = 0;
			uint32_t maxwidth_right = 0;
			for(auto i : report) {
				maxwidth_left = std::max(maxwidth_left, main_font.get_width(i.first));
				maxwidth_right = std::max(maxwidth_right, digits(i.second));
			}
			for(auto i : report) {
				uint32_t pad_spaces = maxwidth_left - main_font.get_width(i.first) + maxwidth_right -
					digits(i.second) + 1;
				messages << i.first << std::string(pad_spaces, ' ') << i.second << std::endl;
			}
		});
}
