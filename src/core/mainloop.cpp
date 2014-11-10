#include "lsnes.hpp"

#include "core/advdumper.hpp"
#include "core/command.hpp"
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
#include "core/mainloop.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/multitrack.hpp"
#include "core/project.hpp"
#include "core/queue.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/runmode.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/callbacks.hpp"
#include "interface/c-interface.hpp"
#include "interface/romtype.hpp"
#include "library/framebuffer.hpp"
#include "library/settingvar.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include "lua/lua.hpp"

#include <iomanip>
#include <cassert>
#include <sstream>
#include <iostream>
#include <limits>
#include <set>
#include <sys/time.h>


settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> movie_dflt_binary(lsnes_setgrp,
	"movie-default-binary", "Movie‣Saving‣Movies binary", false);
settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> save_dflt_binary(lsnes_setgrp,
	"savestate-default-binary", "Movie‣Saving‣Savestates binary", false);

namespace
{
	settingvar::supervariable<settingvar::model_int<0,999999>> SET_advance_timeout_first(lsnes_setgrp,
		"advance-timeout", "Delays‣First frame advance", 500);
	settingvar::supervariable<settingvar::model_int<0,999999>> SET_advance_timeout_subframe(lsnes_setgrp,
		"advance-subframe-timeout", "Delays‣Subframe advance", 100);
	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> SET_pause_on_end(lsnes_setgrp,
		"pause-on-end", "Movie‣Pause on end", false);

	//Mode and filename of pending load, one of LOAD_* constants.
	int loadmode;
	std::string pending_load;
	std::string pending_new_project;
	//Queued saves (all savestates).
	std::set<std::pair<std::string, int>> queued_saves;
	//Unsafe rewind.
	bool do_unsafe_rewind = false;
	void* unsafe_rewind_obj = NULL;
	//Stop at frame.
	bool stop_at_frame_active = false;
	uint64_t stop_at_frame = 0;
	//Macro hold.
	bool macro_hold_1;
	bool macro_hold_2;
}

void mainloop_signal_need_rewind(void* ptr)
{
	auto& core = CORE();
	if(ptr)
		core.runmode->start_load();
	do_unsafe_rewind = true;
	unsafe_rewind_obj = ptr;
}

portctrl::frame movie_logic::update_controls(bool subframe) throw(std::bad_alloc, std::runtime_error)
{
	auto& core = CORE();
	if(core.lua2->requests_subframe_paint)
		core.fbuf->redraw_framebuffer();

	if(subframe) {
		if(core.runmode->is_advance_subframe()) {
			//Note that platform::wait() may change value of cancel flag.
			if(!core.runmode->test_cancel()) {
				if(core.runmode->set_and_test_advanced())
					platform::wait(SET_advance_timeout_subframe(*core.settings) * 1000);
				else
					platform::wait(SET_advance_timeout_first(*core.settings) * 1000);
				core.runmode->set_and_test_advanced();
			}
			if(core.runmode->clear_and_test_cancel()) {
				stop_at_frame_active = false;
				core.runmode->set_pause();
			}
			platform::set_paused(core.runmode->is_paused());
		} else if(core.runmode->is_advance_frame()) {
			;
		} else {
			if(core.runmode->is_skiplag()) {
				stop_at_frame_active = false;
				core.runmode->set_pause();
			}
			core.runmode->clear_and_test_cancel();
		}
		platform::set_paused(core.runmode->is_paused());
		core.runmode->set_point(emulator_runmode::P_NONE);
		core.supdater->update();
	} else {
		core.runmode->decay_skiplag();
		if(core.runmode->is_advance()) {
			//Note that platform::wait() may change value of cancel flag.
			if(!core.runmode->test_cancel()) {
				uint64_t wait = 0;
				if(!core.runmode->test_advanced())
					wait = SET_advance_timeout_first(*core.settings) * 1000;
				else if(core.runmode->is_advance_subframe())
					wait = SET_advance_timeout_subframe(*core.settings) * 1000;
				else
					wait = core.framerate->to_wait_frame(framerate_regulator::get_utime());
				platform::wait(wait);
				core.runmode->set_and_test_advanced();
			}
			if(core.runmode->clear_and_test_cancel()) {
				stop_at_frame_active = false;
				core.runmode->set_pause();
			}
			platform::set_paused(core.runmode->is_paused());
		} else if(core.runmode->is_freerunning() && core.mlogic->get_movie().readonly_mode() &&
			SET_pause_on_end(*core.settings) && !stop_at_frame_active) {
			if(core.mlogic->get_movie().get_current_frame() ==
				core.mlogic->get_movie().get_frame_count()) {
				stop_at_frame_active = false;
				core.runmode->set_pause();
				platform::set_paused(true);
			}
		} else if(core.runmode->is_freerunning() && stop_at_frame_active) {
			if(core.mlogic->get_movie().get_current_frame() >= stop_at_frame) {
				stop_at_frame_active = false;
				core.runmode->set_pause();
				platform::set_paused(true);
			}
		} else {
			platform::set_paused(core.runmode->is_paused());
		}
		core.runmode->set_point(emulator_runmode::P_START);
		core.supdater->update();
	}
	platform::flush_command_queue();
	portctrl::frame tmp = core.controls->get(core.mlogic->get_movie().get_current_frame());
	core.rom->pre_emulate_frame(tmp);	//Preset controls, the lua will override if needed.
	core.lua2->callback_do_input(tmp, subframe);
	core.mteditor->process_frame(tmp);
	core.controls->commit(tmp);
	return tmp;
}

namespace
{

	//Do pending load (automatically unpauses).
	void mark_pending_load(std::string filename, int lmode)
	{
		//Convert break pause to ordinary pause.
		auto& core = CORE();
		loadmode = lmode;
		pending_load = filename;
		core.runmode->decay_break();
		core.runmode->start_load();
		platform::cancel_wait();
		platform::set_paused(false);
	}

	void mark_pending_save(std::string filename, int smode, int binary)
	{
		auto& core = CORE();
		int tmp = -1;
		if(smode == SAVE_MOVIE) {
			//Just do this immediately.
			do_save_movie(filename, binary);
			core.slotcache->flush(translate_name_mprefix(filename, tmp, -1));
			return;
		}
		if(core.runmode->get_point() == emulator_runmode::P_SAVE) {
			//We can save immediately here.
			do_save_state(filename, binary);
			core.slotcache->flush(translate_name_mprefix(filename, tmp, -1));
			return;
		}
		queued_saves.insert(std::make_pair(filename, binary));
		messages << "Pending save on '" << filename << "'" << std::endl;
	}
}


struct lsnes_callbacks : public emucore_callbacks
{
public:
	~lsnes_callbacks() throw()
	{
	}

	int16_t get_input(unsigned port, unsigned index, unsigned control)
	{
		auto& core = CORE();
		int16_t x;
		x = core.mlogic->input_poll(port, index, control);
		core.lua2->callback_snoop_input(port, index, control, x);
		return x;
	}

	int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value)
	{
		auto& core = CORE();
		if(!core.mlogic->get_movie().readonly_mode()) {
			portctrl::frame f = core.mlogic->get_movie().get_controls();
			f.axis3(port, index, control, value);
			core.mlogic->get_movie().set_controls(f);
		}
		return core.mlogic->get_movie().next_input(port, index, control);
	}

	void notify_latch(std::list<std::string>& args)
	{
		CORE().lua2->callback_do_latch(args);
	}

	void timer_tick(uint32_t increment, uint32_t per_second)
	{
		auto& core = CORE();
		if(!*core.mlogic)
			return;
		auto& m = core.mlogic->get_mfile();
		m.rtc_subsecond += increment;
		while(m.rtc_subsecond >= per_second) {
			m.rtc_second++;
			m.rtc_subsecond -= per_second;
		}
	}

	std::string get_firmware_path()
	{
		return CORE().setcache->get("firmwarepath");
	}

	std::string get_base_path()
	{
		return CORE().rom->msu1_base;
	}

	time_t get_time()
	{
		auto& core = CORE();
		return *core.mlogic ? core.mlogic->get_mfile().rtc_second : 0;
	}

	time_t get_randomseed()
	{
		return CORE().random_seed_value;
	}

	void output_frame(framebuffer::raw& screen, uint32_t fps_n, uint32_t fps_d)
	{
		auto& core = CORE();
		core.lua2->callback_do_frame_emulated();
		core.runmode->set_point(emulator_runmode::P_VIDEO);
		core.fbuf->redraw_framebuffer(screen, false, true);
		auto rate = core.rom->get_audio_rate();
		uint32_t gv = gcd(fps_n, fps_d);
		uint32_t ga = gcd(rate.first, rate.second);
		core.mdumper->on_rate_change(rate.first / ga, rate.second / ga);
		core.mdumper->on_frame(screen, fps_n / gv, fps_d / gv);
	}

	void action_state_updated()
	{
		CORE().dispatch->action_update();
	}

	void memory_read(uint64_t addr, uint64_t value)
	{
		CORE().dbg->do_callback_read(addr, value);
	}

	void memory_write(uint64_t addr, uint64_t value)
	{
		CORE().dbg->do_callback_write(addr, value);
	}

	void memory_execute(uint64_t addr, uint64_t proc)
	{
		CORE().dbg->do_callback_exec(addr, proc);
	}

	void memory_trace(uint64_t proc, const char* str, bool insn)
	{
		CORE().dbg->do_callback_trace(proc, str, insn);
	}
};

namespace
{
	lsnes_callbacks lsnes_callbacks_obj;
	command::fnptr<> CMD_segfault(lsnes_cmds, "segfault", "Trigger SIGSEGV",
		"segfault\nTrigger segmentation fault",
		[]() throw(std::bad_alloc, std::runtime_error) {
			char* ptr = (char*)0x1234;
			*ptr = 0;
		});

	command::fnptr<> CMD_div0(lsnes_cmds, "divide-by-0", "Do div0", "divide-by-0\nDo divide by 0",
		[]() throw(std::bad_alloc, std::runtime_error) {
			static int ptr = 1;
			static int ptr2 = 0;
			ptr = ptr / ptr2;
		});

	command::fnptr<const std::string&> CMD_test4(lsnes_cmds, "test4", "test", "test",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			std::list<std::string> _args;
			std::string args2 = args;
			for(auto& sym : token_iterator_foreach(args, {" ", "\t"}))
				_args.push_back(sym);
			core.lua2->callback_do_latch(_args);
		});
	command::fnptr<> CMD_count_rerecords(lsnes_cmds, "count-rerecords", "Count rerecords",
		"Syntax: count-rerecords\nCounts rerecords.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			std::vector<char> tmp;
			uint64_t x = CORE().mlogic->get_rrdata().write(tmp);
			messages << x << " rerecord(s)" << std::endl;
		});

	command::fnptr<const std::string&> CMD_quit_emulator(lsnes_cmds, "quit-emulator", "Quit the emulator",
		"Syntax: quit-emulator [/y]\nQuits emulator (/y => don't ask for confirmation).\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			CORE().runmode->set_quit();
			platform::set_paused(false);
			platform::cancel_wait();
		});

	command::fnptr<> CMD_unpause_emulator(lsnes_cmds, "unpause-emulator", "Unpause the emulator",
		"Syntax: unpause-emulator\nUnpauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			if(core.runmode->is_special())
				return;
			core.runmode->set_freerunning();
			platform::set_paused(false);
			platform::cancel_wait();
		});

	command::fnptr<> CMD_pause_emulator(lsnes_cmds, "pause-emulator", "(Un)pause the emulator",
		"Syntax: pause-emulator\n(Un)pauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			if(core.runmode->is_special())
				;
			else if(core.runmode->is_freerunning()) {
				platform::cancel_wait();
				stop_at_frame_active = false;
				core.runmode->set_pause();
			} else {
				core.runmode->set_freerunning();
				platform::set_paused(false);
				platform::cancel_wait();
			}
		});

	command::fnptr<> CMD_save_jukebox_prev(lsnes_cmds, "cycle-jukebox-backward", "Cycle save jukebox backwards",
		"Syntax: cycle-jukebox-backward\nCycle save jukebox backwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			core.jukebox->cycle_prev();
		});

	command::fnptr<> CMD_save_jukebox_next(lsnes_cmds, "cycle-jukebox-forward", "Cycle save jukebox forwards",
		"Syntax: cycle-jukebox-forward\nCycle save jukebox forwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			core.jukebox->cycle_next();
		});

	command::fnptr<const std::string&> CMD_save_jukebox_set(lsnes_cmds, "set-jukebox-slot", "Set jukebox slot",
		"Syntax: set-jukebox-slot\nSet jukebox slot\n", [](const std::string& args)
		throw(std::bad_alloc, std::runtime_error) {
			if(!regex_match("[1-9][0-9]{0,8}", args))
				throw std::runtime_error("Bad slot number");
			uint32_t slot = parse_value<uint32_t>(args);
			auto& core = CORE();
			core.jukebox->set_slot(slot - 1);
		});

	command::fnptr<> CMD_load_jukebox(lsnes_cmds, "load-jukebox", "Load save from jukebox",
		"Syntax: load-jukebox\nLoad save from jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			mark_pending_load(core.jukebox->get_slot_name(), LOAD_STATE_CURRENT);
		});

	command::fnptr<> CMD_load_jukebox_readwrite(lsnes_cmds, "load-jukebox-readwrite", "Load save from jukebox in"
		" recording mode", "Syntax: load-jukebox-readwrite\nLoad save from jukebox in recording mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			mark_pending_load(core.jukebox->get_slot_name(), LOAD_STATE_RW);
		});

	command::fnptr<> CMD_load_jukebox_readonly(lsnes_cmds, "load-jukebox-readonly", "Load save from jukebox in "
		"playback mode", "Syntax: load-jukebox-readonly\nLoad save from jukebox in playback mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			mark_pending_load(core.jukebox->get_slot_name(), LOAD_STATE_RO);
		});

	command::fnptr<> CMD_load_jukebox_preserve(lsnes_cmds, "load-jukebox-preserve", "Load save from jukebox, "
		"preserving input", "Syntax: load-jukebox-preserve\nLoad save from jukebox, preserving input\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			mark_pending_load(core.jukebox->get_slot_name(), LOAD_STATE_PRESERVE);
		});

	command::fnptr<> CMD_load_jukebox_movie(lsnes_cmds, "load-jukebox-movie", "Load save from jukebox as movie",
		"Syntax: load-jukebox-movie\nLoad save from jukebox as movie\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			mark_pending_load(core.jukebox->get_slot_name(), LOAD_STATE_MOVIE);
		});

	command::fnptr<> CMD_save_jukebox_c(lsnes_cmds, "save-jukebox", "Save save to jukebox",
		"Syntax: save-jukebox\nSave save to jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			mark_pending_save(core.jukebox->get_slot_name(), SAVE_STATE, -1);
		});

	command::fnptr<> CMD_padvance_frame(lsnes_cmds, "+advance-frame", "Advance one frame",
		"Syntax: +advance-frame\nAdvances the emulation by one frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			if(core.runmode->is_special())
				return;
			core.runmode->set_frameadvance();
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> CMD_nadvance_frame(lsnes_cmds, "-advance-frame", "Advance one frame",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().runmode->set_cancel();
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> CMD_padvance_poll(lsnes_cmds, "+advance-poll", "Advance one subframe",
		"Syntax: +advance-poll\nAdvances the emulation by one subframe.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			if(core.runmode->is_special())
				return;
			core.runmode->set_subframeadvance();
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> CMD_nadvance_poll(lsnes_cmds, "-advance-poll", "Advance one subframe",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			core.runmode->decay_break();
			core.runmode->set_cancel();
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> CMD_advance_skiplag(lsnes_cmds, "advance-skiplag", "Skip to next poll",
		"Syntax: advance-skiplag\nAdvances the emulation to the next poll.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().runmode->set_skiplag_pending();
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> CMD_reset_c(lsnes_cmds, "reset", "Reset the system",
		"Syntax: reset\nReset\nResets the system in beginning of the next frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			int sreset_action = core.rom->reset_action(false);
			if(sreset_action < 0) {
				platform::error_message("Core does not support resets");
				messages << "Emulator core does not support resets" << std::endl;
				return;
			}
			core.rom->execute_action(sreset_action, std::vector<interface_action_paramval>());
		});

	command::fnptr<> CMD_hreset_c(lsnes_cmds, "reset-hard", "Reset the system",
		"Syntax: reset-hard\nReset-hard\nHard resets the system in beginning of the next frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			int hreset_action = core.rom->reset_action(true);
			if(hreset_action < 0) {
				platform::error_message("Core does not support hard resets");
				messages << "Emulator core does not support hard resets" << std::endl;
				return;
			}
			core.rom->execute_action(hreset_action, std::vector<interface_action_paramval>());
		});

	command::fnptr<command::arg_filename> CMD_load_c(lsnes_cmds, "load", "Load savestate (current mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in current mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_CURRENT);
		});

	command::fnptr<command::arg_filename> CMD_load_smart_c(lsnes_cmds, "load-smart",
		"Load savestate (heuristic mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in heuristic mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_DEFAULT);
		});

	command::fnptr<command::arg_filename> CMD_load_state_c(lsnes_cmds, "load-state", "Load savestate (R/W)",
		"Syntax: load-state <file>\nLoads SNES state from <file> in Read/Write mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RW);
		});

	command::fnptr<command::arg_filename> CMD_load_readonly(lsnes_cmds, "load-readonly", "Load savestate (RO)",
		"Syntax: load-readonly <file>\nLoads SNES state from <file> in playback mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RO);
		});

	command::fnptr<command::arg_filename> CMD_load_preserve(lsnes_cmds, "load-preserve",
		"Load savestate (preserve input)",
		"Syntax: load-preserve <file>\nLoads SNES state from <file> preserving input\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_PRESERVE);
		});

	command::fnptr<command::arg_filename> CMD_load_movie_c(lsnes_cmds, "load-movie", "Load movie",
		"Syntax: load-movie <file>\nLoads SNES movie from <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_MOVIE);
		});

	command::fnptr<command::arg_filename> CMD_load_allbr_c(lsnes_cmds, "load-allbranches", "Load savestate "
		"(all branches)", "Syntax: load-allbranches <file>\nLoads SNES state from <file> with all "
		"branches\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_ALLBRANCH);
		});

	command::fnptr<command::arg_filename> CMD_save_state(lsnes_cmds, "save-state", "Save state",
		"Syntax: save-state <file>\nSaves SNES state to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE, -1);
		});

	command::fnptr<command::arg_filename> CMD_save_state2(lsnes_cmds, "save-state-binary", "Save state (binary)",
		"Syntax: save-state-binary <file>\nSaves binary state to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE, 1);
		});

	command::fnptr<command::arg_filename> CMD_save_state3(lsnes_cmds, "save-state-zip", "Save state (zip)",
		"Syntax: save-state-zip <file>\nSaves zip state to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE, 0);
		});

	command::fnptr<command::arg_filename> CMD_save_movie(lsnes_cmds, "save-movie", "Save movie",
		"Syntax: save-movie <file>\nSaves SNES movie to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE, -1);
		});

	command::fnptr<command::arg_filename> CMD_save_movie2(lsnes_cmds, "save-movie-binary", "Save movie (binary)",
		"Syntax: save-movie-binary <file>\nSaves binary movie to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE, 1);
		});

	command::fnptr<command::arg_filename> CMD_save_movie3(lsnes_cmds, "save-movie-zip", "Save movie (zip)",
		"Syntax: save-movie-zip <file>\nSaves zip movie to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE, 0);
		});

	command::fnptr<> CMD_set_rwmode(lsnes_cmds, "set-rwmode", "Switch to recording mode",
		"Syntax: set-rwmode\nSwitches to recording mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			core.lua2->callback_movie_lost("readwrite");
			core.mlogic->get_movie().readonly_mode(false);
			core.dispatch->mode_change(false);
			core.lua2->callback_do_readwrite();
			core.supdater->update();
		});

	command::fnptr<> CMD_set_romode(lsnes_cmds, "set-romode", "Switch to playback mode",
		"Syntax: set-romode\nSwitches to playback mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			core.mlogic->get_movie().readonly_mode(true);
			core.dispatch->mode_change(true);
			core.supdater->update();
		});

	command::fnptr<> CMD_toggle_rwmode(lsnes_cmds, "toggle-rwmode", "Toggle recording mode",
		"Syntax: toggle-rwmode\nToggles recording mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			bool c = core.mlogic->get_movie().readonly_mode();
			if(c)
				core.lua2->callback_movie_lost("readwrite");
			core.mlogic->get_movie().readonly_mode(!c);
			core.dispatch->mode_change(!c);
			if(c)
				core.lua2->callback_do_readwrite();
			core.supdater->update();
		});

	command::fnptr<> CMD_repaint(lsnes_cmds, "repaint", "Redraw the screen",
		"Syntax: repaint\nRedraws the screen\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().fbuf->redraw_framebuffer();
		});

	command::fnptr<> CMD_tpon(lsnes_cmds, "toggle-pause-on-end", "Toggle pause on end", "Toggle pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			bool tmp = SET_pause_on_end(*core.settings);
			SET_pause_on_end(*core.settings, !tmp);
			messages << "Pause-on-end is now " << (tmp ? "OFF" : "ON") << std::endl;
		});

	command::fnptr<> CMD_spon(lsnes_cmds, "set-pause-on-end", "Set pause on end", "Set pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			SET_pause_on_end(*CORE().settings, true);
			messages << "Pause-on-end is now ON" << std::endl;
		});

	command::fnptr<> CMD_cpon(lsnes_cmds, "clear-pause-on-end", "Clear pause on end", "Clear pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			SET_pause_on_end(*CORE().settings, false);
			messages << "Pause-on-end is now OFF" << std::endl;
		});

	command::fnptr<> CMD_rewind_movie(lsnes_cmds, "rewind-movie", "Rewind movie to the beginning",
		"Syntax: rewind-movie\nRewind movie to the beginning\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_BEGINNING);
		});

	command::fnptr<> CMD_cancel_save(lsnes_cmds, "cancel-saves", "Cancel all pending saves", "Syntax: "
		"cancel-save\nCancel pending saves\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			queued_saves.clear();
			messages << "Pending saves canceled." << std::endl;
		});

	command::fnptr<> CMD_flushslots(lsnes_cmds, "flush-slotinfo", "Flush slotinfo cache",
		"Flush slotinfo cache\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().slotcache->flush();
		});

	command::fnptr<> CMD_mhold1(lsnes_cmds, "+hold-macro", "Hold macro (hold)",
		"Hold macros enable\n", []() throw(std::bad_alloc, std::runtime_error) {
			macro_hold_1 = true;
		});

	command::fnptr<> CMD_mhold2(lsnes_cmds, "-hold-macro", "Hold macro (hold)",
		"Hold macros disable\n", []() throw(std::bad_alloc, std::runtime_error) {
			macro_hold_1 = false;
		});

	command::fnptr<> CMD_mhold3(lsnes_cmds, "hold-macro", "Hold macro (toggle)",
		"Hold macros toggle\n", []() throw(std::bad_alloc, std::runtime_error) {
			macro_hold_2 = !macro_hold_2;
			if(macro_hold_2)
				messages << "Macros are held for next frame." << std::endl;
			else
				messages << "Macros are not held for next frame." << std::endl;
		});

	keyboard::invbind_info IBIND_imhold1(lsnes_invbinds, "+hold-macro", "Macro‣Hold all macros");
	keyboard::invbind_info IBIND_imhold2(lsnes_invbinds, "hold-macro", "Macro‣Hold all macros (typed)");
	keyboard::invbind_info IBIND_ipause_emulator(lsnes_invbinds, "pause-emulator", "Speed‣(Un)pause");
	keyboard::invbind_info IBIND_ijback(lsnes_invbinds, "cycle-jukebox-backward", "Slot select‣Cycle backwards");
	keyboard::invbind_info IBIND_ijforward(lsnes_invbinds, "cycle-jukebox-forward", "Slot select‣Cycle forwards");
	keyboard::invbind_info IBIND_iloadj(lsnes_invbinds, "load-jukebox", "Load‣Selected slot");
	keyboard::invbind_info IBIND_iloadjrw(lsnes_invbinds, "load-jukebox-readwrite",
		"Load‣Selected slot (recording mode)");
	keyboard::invbind_info IBIND_iloadjro(lsnes_invbinds, "load-jukebox-readonly",
		"Load‣Selected slot (playback mode)");
	keyboard::invbind_info IBIND_iloadjp(lsnes_invbinds, "load-jukebox-preserve",
		"Load‣Selected slot (preserve input)");
	keyboard::invbind_info IBIND_iloadjm(lsnes_invbinds, "load-jukebox-movie", "Load‣Selected slot (as movie)");
	keyboard::invbind_info IBIND_isavej(lsnes_invbinds, "save-jukebox", "Save‣Selected slot");
	keyboard::invbind_info IBIND_iadvframe(lsnes_invbinds, "+advance-frame", "Speed‣Advance frame");
	keyboard::invbind_info IBIND_iadvsubframe(lsnes_invbinds, "+advance-poll", "Speed‣Advance subframe");
	keyboard::invbind_info IBIND_iskiplag(lsnes_invbinds, "advance-skiplag", "Speed‣Advance poll");
	keyboard::invbind_info IBIND_ireset(lsnes_invbinds, "reset", "System‣Reset");
	keyboard::invbind_info IBIND_iset_rwmode(lsnes_invbinds, "set-rwmode", "Movie‣Switch to recording");
	keyboard::invbind_info IBIND_itoggle_romode(lsnes_invbinds, "set-romode", "Movie‣Switch to playback");
	keyboard::invbind_info IBIND_itoggle_rwmode(lsnes_invbinds, "toggle-rwmode", "Movie‣Toggle playback");
	keyboard::invbind_info IBIND_irepaint(lsnes_invbinds, "repaint", "System‣Repaint screen");
	keyboard::invbind_info IBIND_itogglepause(lsnes_invbinds, "toggle-pause-on-end", "Movie‣Toggle pause-on-end");
	keyboard::invbind_info IBIND_irewind_movie(lsnes_invbinds, "rewind-movie", "Movie‣Rewind movie");
	keyboard::invbind_info IBIND_icancel_saves(lsnes_invbinds, "cancel-saves", "Save‣Cancel pending saves");
	keyboard::invbind_info IBIND_iload1(lsnes_invbinds, "load $SLOT:1", "Load‣Slot 1");
	keyboard::invbind_info IBIND_iload2(lsnes_invbinds, "load $SLOT:2", "Load‣Slot 2");
	keyboard::invbind_info IBIND_iload3(lsnes_invbinds, "load $SLOT:3", "Load‣Slot 3");
	keyboard::invbind_info IBIND_iload4(lsnes_invbinds, "load $SLOT:4", "Load‣Slot 4");
	keyboard::invbind_info IBIND_iload5(lsnes_invbinds, "load $SLOT:5", "Load‣Slot 5");
	keyboard::invbind_info IBIND_iload6(lsnes_invbinds, "load $SLOT:6", "Load‣Slot 6");
	keyboard::invbind_info IBIND_iload7(lsnes_invbinds, "load $SLOT:7", "Load‣Slot 7");
	keyboard::invbind_info IBIND_iload8(lsnes_invbinds, "load $SLOT:8", "Load‣Slot 8");
	keyboard::invbind_info IBIND_iload9(lsnes_invbinds, "load $SLOT:9", "Load‣Slot 9");
	keyboard::invbind_info IBIND_iload10(lsnes_invbinds, "load $SLOT:10", "Load‣Slot 10");
	keyboard::invbind_info IBIND_iload11(lsnes_invbinds, "load $SLOT:11", "Load‣Slot 11");
	keyboard::invbind_info IBIND_iload12(lsnes_invbinds, "load $SLOT:12", "Load‣Slot 12");
	keyboard::invbind_info IBIND_iload13(lsnes_invbinds, "load $SLOT:13", "Load‣Slot 13");
	keyboard::invbind_info IBIND_iload14(lsnes_invbinds, "load $SLOT:14", "Load‣Slot 14");
	keyboard::invbind_info IBIND_iload15(lsnes_invbinds, "load $SLOT:15", "Load‣Slot 15");
	keyboard::invbind_info IBIND_iload16(lsnes_invbinds, "load $SLOT:16", "Load‣Slot 16");
	keyboard::invbind_info IBIND_iload17(lsnes_invbinds, "load $SLOT:17", "Load‣Slot 17");
	keyboard::invbind_info IBIND_iload18(lsnes_invbinds, "load $SLOT:18", "Load‣Slot 18");
	keyboard::invbind_info IBIND_iload19(lsnes_invbinds, "load $SLOT:19", "Load‣Slot 19");
	keyboard::invbind_info IBIND_iload20(lsnes_invbinds, "load $SLOT:20", "Load‣Slot 20");
	keyboard::invbind_info IBIND_iload21(lsnes_invbinds, "load $SLOT:21", "Load‣Slot 21");
	keyboard::invbind_info IBIND_iload22(lsnes_invbinds, "load $SLOT:22", "Load‣Slot 22");
	keyboard::invbind_info IBIND_iload23(lsnes_invbinds, "load $SLOT:23", "Load‣Slot 23");
	keyboard::invbind_info IBIND_iload24(lsnes_invbinds, "load $SLOT:24", "Load‣Slot 24");
	keyboard::invbind_info IBIND_iload25(lsnes_invbinds, "load $SLOT:25", "Load‣Slot 25");
	keyboard::invbind_info IBIND_iload26(lsnes_invbinds, "load $SLOT:26", "Load‣Slot 26");
	keyboard::invbind_info IBIND_iload27(lsnes_invbinds, "load $SLOT:27", "Load‣Slot 27");
	keyboard::invbind_info IBIND_iload28(lsnes_invbinds, "load $SLOT:28", "Load‣Slot 28");
	keyboard::invbind_info IBIND_iload29(lsnes_invbinds, "load $SLOT:29", "Load‣Slot 29");
	keyboard::invbind_info IBIND_iload30(lsnes_invbinds, "load $SLOT:30", "Load‣Slot 30");
	keyboard::invbind_info IBIND_iload31(lsnes_invbinds, "load $SLOT:31", "Load‣Slot 31");
	keyboard::invbind_info IBIND_iload32(lsnes_invbinds, "load $SLOT:32", "Load‣Slot 32");
	keyboard::invbind_info IBIND_isave1(lsnes_invbinds, "save-state $SLOT:1", "Save‣Slot 1");
	keyboard::invbind_info IBIND_isave2(lsnes_invbinds, "save-state $SLOT:2", "Save‣Slot 2");
	keyboard::invbind_info IBIND_isave3(lsnes_invbinds, "save-state $SLOT:3", "Save‣Slot 3");
	keyboard::invbind_info IBIND_isave4(lsnes_invbinds, "save-state $SLOT:4", "Save‣Slot 4");
	keyboard::invbind_info IBIND_isave5(lsnes_invbinds, "save-state $SLOT:5", "Save‣Slot 5");
	keyboard::invbind_info IBIND_isave6(lsnes_invbinds, "save-state $SLOT:6", "Save‣Slot 6");
	keyboard::invbind_info IBIND_isave7(lsnes_invbinds, "save-state $SLOT:7", "Save‣Slot 7");
	keyboard::invbind_info IBIND_isave8(lsnes_invbinds, "save-state $SLOT:8", "Save‣Slot 8");
	keyboard::invbind_info IBIND_isave9(lsnes_invbinds, "save-state $SLOT:9", "Save‣Slot 9");
	keyboard::invbind_info IBIND_isave10(lsnes_invbinds, "save-state $SLOT:10", "Save‣Slot 10");
	keyboard::invbind_info IBIND_isave11(lsnes_invbinds, "save-state $SLOT:11", "Save‣Slot 11");
	keyboard::invbind_info IBIND_isave12(lsnes_invbinds, "save-state $SLOT:12", "Save‣Slot 12");
	keyboard::invbind_info IBIND_isave13(lsnes_invbinds, "save-state $SLOT:13", "Save‣Slot 13");
	keyboard::invbind_info IBIND_isave14(lsnes_invbinds, "save-state $SLOT:14", "Save‣Slot 14");
	keyboard::invbind_info IBIND_isave15(lsnes_invbinds, "save-state $SLOT:15", "Save‣Slot 15");
	keyboard::invbind_info IBIND_isave16(lsnes_invbinds, "save-state $SLOT:16", "Save‣Slot 16");
	keyboard::invbind_info IBIND_isave17(lsnes_invbinds, "save-state $SLOT:17", "Save‣Slot 17");
	keyboard::invbind_info IBIND_isave18(lsnes_invbinds, "save-state $SLOT:18", "Save‣Slot 18");
	keyboard::invbind_info IBIND_isave19(lsnes_invbinds, "save-state $SLOT:19", "Save‣Slot 19");
	keyboard::invbind_info IBIND_isave20(lsnes_invbinds, "save-state $SLOT:20", "Save‣Slot 20");
	keyboard::invbind_info IBIND_isave21(lsnes_invbinds, "save-state $SLOT:21", "Save‣Slot 21");
	keyboard::invbind_info IBIND_isave22(lsnes_invbinds, "save-state $SLOT:22", "Save‣Slot 22");
	keyboard::invbind_info IBIND_isave23(lsnes_invbinds, "save-state $SLOT:23", "Save‣Slot 23");
	keyboard::invbind_info IBIND_isave24(lsnes_invbinds, "save-state $SLOT:24", "Save‣Slot 24");
	keyboard::invbind_info IBIND_isave25(lsnes_invbinds, "save-state $SLOT:25", "Save‣Slot 25");
	keyboard::invbind_info IBIND_isave26(lsnes_invbinds, "save-state $SLOT:26", "Save‣Slot 26");
	keyboard::invbind_info IBIND_isave27(lsnes_invbinds, "save-state $SLOT:27", "Save‣Slot 27");
	keyboard::invbind_info IBIND_isave28(lsnes_invbinds, "save-state $SLOT:28", "Save‣Slot 28");
	keyboard::invbind_info IBIND_isave29(lsnes_invbinds, "save-state $SLOT:29", "Save‣Slot 29");
	keyboard::invbind_info IBIND_isave30(lsnes_invbinds, "save-state $SLOT:30", "Save‣Slot 30");
	keyboard::invbind_info IBIND_isave31(lsnes_invbinds, "save-state $SLOT:31", "Save‣Slot 31");
	keyboard::invbind_info IBIND_isave32(lsnes_invbinds, "save-state $SLOT:32", "Save‣Slot 32");
	keyboard::invbind_info IBIND_islot1(lsnes_invbinds, "set-jukebox-slot 1", "Slot select‣Slot 1");
	keyboard::invbind_info IBIND_islot2(lsnes_invbinds, "set-jukebox-slot 2", "Slot select‣Slot 2");
	keyboard::invbind_info IBIND_islot3(lsnes_invbinds, "set-jukebox-slot 3", "Slot select‣Slot 3");
	keyboard::invbind_info IBIND_islot4(lsnes_invbinds, "set-jukebox-slot 4", "Slot select‣Slot 4");
	keyboard::invbind_info IBIND_islot5(lsnes_invbinds, "set-jukebox-slot 5", "Slot select‣Slot 5");
	keyboard::invbind_info IBIND_islot6(lsnes_invbinds, "set-jukebox-slot 6", "Slot select‣Slot 6");
	keyboard::invbind_info IBIND_islot7(lsnes_invbinds, "set-jukebox-slot 7", "Slot select‣Slot 7");
	keyboard::invbind_info IBIND_islot8(lsnes_invbinds, "set-jukebox-slot 8", "Slot select‣Slot 8");
	keyboard::invbind_info IBIND_islot9(lsnes_invbinds, "set-jukebox-slot 9", "Slot select‣Slot 9");
	keyboard::invbind_info IBIND_islot10(lsnes_invbinds, "set-jukebox-slot 10", "Slot select‣Slot 10");
	keyboard::invbind_info IBIND_islot11(lsnes_invbinds, "set-jukebox-slot 11", "Slot select‣Slot 11");
	keyboard::invbind_info IBIND_islot12(lsnes_invbinds, "set-jukebox-slot 12", "Slot select‣Slot 12");
	keyboard::invbind_info IBIND_islot13(lsnes_invbinds, "set-jukebox-slot 13", "Slot select‣Slot 13");
	keyboard::invbind_info IBIND_islot14(lsnes_invbinds, "set-jukebox-slot 14", "Slot select‣Slot 14");
	keyboard::invbind_info IBIND_islot15(lsnes_invbinds, "set-jukebox-slot 15", "Slot select‣Slot 15");
	keyboard::invbind_info IBIND_islot16(lsnes_invbinds, "set-jukebox-slot 16", "Slot select‣Slot 16");
	keyboard::invbind_info IBIND_islot17(lsnes_invbinds, "set-jukebox-slot 17", "Slot select‣Slot 17");
	keyboard::invbind_info IBIND_islot18(lsnes_invbinds, "set-jukebox-slot 18", "Slot select‣Slot 18");
	keyboard::invbind_info IBIND_islot19(lsnes_invbinds, "set-jukebox-slot 19", "Slot select‣Slot 19");
	keyboard::invbind_info IBIND_islot20(lsnes_invbinds, "set-jukebox-slot 20", "Slot select‣Slot 20");
	keyboard::invbind_info IBIND_islot21(lsnes_invbinds, "set-jukebox-slot 21", "Slot select‣Slot 21");
	keyboard::invbind_info IBIND_islot22(lsnes_invbinds, "set-jukebox-slot 22", "Slot select‣Slot 22");
	keyboard::invbind_info IBIND_islot23(lsnes_invbinds, "set-jukebox-slot 23", "Slot select‣Slot 23");
	keyboard::invbind_info IBIND_islot24(lsnes_invbinds, "set-jukebox-slot 24", "Slot select‣Slot 24");
	keyboard::invbind_info IBIND_islot25(lsnes_invbinds, "set-jukebox-slot 25", "Slot select‣Slot 25");
	keyboard::invbind_info IBIND_islot26(lsnes_invbinds, "set-jukebox-slot 26", "Slot select‣Slot 26");
	keyboard::invbind_info IBIND_islot27(lsnes_invbinds, "set-jukebox-slot 27", "Slot select‣Slot 27");
	keyboard::invbind_info IBIND_islot28(lsnes_invbinds, "set-jukebox-slot 28", "Slot select‣Slot 28");
	keyboard::invbind_info IBIND_islot29(lsnes_invbinds, "set-jukebox-slot 29", "Slot select‣Slot 29");
	keyboard::invbind_info IBIND_islot30(lsnes_invbinds, "set-jukebox-slot 30", "Slot select‣Slot 30");
	keyboard::invbind_info IBIND_islot31(lsnes_invbinds, "set-jukebox-slot 31", "Slot select‣Slot 31");
	keyboard::invbind_info IBIND_islot32(lsnes_invbinds, "set-jukebox-slot 32", "Slot select‣Slot 32");

	class mywindowcallbacks : public master_dumper::notifier
	{
	public:
		mywindowcallbacks(emulator_dispatch& dispatch, emulator_runmode& runmode, status_updater& _supdater)
			: supdater(_supdater)
		{
			closenotify.set(dispatch.close, [this, &runmode]() {
				try {
					runmode.set_quit();
					platform::set_paused(false);
					platform::cancel_wait();
				} catch(...) {
				}
			});
		}
		~mywindowcallbacks() throw() {}
		void dump_status_change() throw()
		{
			supdater.update();
		}
	private:
		struct dispatch::target<> closenotify;
		status_updater& supdater;
	};

	//If there is a pending load, perform it. Return 1 on successful load, 0 if nothing to load, -1 on load
	//failing.
	int handle_load()
	{
		auto& core = CORE();
		std::string old_project = *core.mlogic ? core.mlogic->get_mfile().projectid : "";
jumpback:
		if(do_unsafe_rewind && unsafe_rewind_obj) {
			if(!*core.mlogic)
				return 0;
			uint64_t t = framerate_regulator::get_utime();
			std::vector<char> s;
			core.lua2->callback_do_unsafe_rewind(s, 0, 0, core.mlogic->get_movie(), unsafe_rewind_obj);
			core.dispatch->mode_change(false);
			do_unsafe_rewind = false;
			core.mlogic->get_mfile().is_savestate = true;
			core.runmode->set_point(emulator_runmode::P_SAVE);
			core.supdater->update();
			messages << "Rewind done in " << (framerate_regulator::get_utime() - t) << " usec."
				<< std::endl;
			return 1;
		}
		if(pending_new_project != "") {
			std::string id = pending_new_project;
			pending_new_project = "";
			project_info* old = core.project->get();
			if(old && old->id == id)
				goto nothing_to_do;
			try {
				auto& p = core.project->load(id);
				core.project->set(&p);
				if(core.project->get() != old)
					delete old;
				core.slotcache->flush();		//Wrong movie may be stale.
				core.runmode->end_load();		//Restore previous mode.
				return 1;
			} catch(std::exception& e) {
				platform::error_message(std::string("Can't switch projects: ") + e.what());
				messages << "Can't switch projects: " << e.what() << std::endl;
				goto nothing_to_do;
			}
nothing_to_do:
			core.runmode->end_load();
			platform::set_paused(core.runmode->is_paused());
			platform::flush_command_queue();
			if(core.runmode->is_load())
				goto jumpback;
			return 0;
		}
		if(pending_load != "") {
			try {
				if(loadmode != LOAD_STATE_BEGINNING && loadmode != LOAD_STATE_ROMRELOAD &&
					!do_load_state(pending_load, loadmode)) {
					core.runmode->end_load();
					pending_load = "";
					return -1;
				}
				if(loadmode == LOAD_STATE_BEGINNING)
					do_load_rewind();
				if(loadmode == LOAD_STATE_ROMRELOAD)
					do_load_rom();
				core.runmode->clear_corrupt();
			} catch(std::exception& e) {
				core.runmode->set_corrupt();
				platform::error_message(std::string("Load failed: ") + e.what());
				messages << "Load failed: " << e.what() << std::endl;
			}
			pending_load = "";
			if(!core.runmode->is_corrupt()) {
				core.runmode->end_load();
				core.runmode->set_point(emulator_runmode::P_SAVE);
				core.supdater->update();
				platform::flush_command_queue();
				if(core.runmode->is_quit())
					return -1;
				if(core.runmode->is_load())
					goto jumpback;
			}
			if(old_project != (*core.mlogic ? core.mlogic->get_mfile().projectid : ""))
				core.slotcache->flush();	//Wrong movie may be stale.
			return 1;
		}
		return 0;
	}

	//If there are pending saves, perform them.
	void handle_saves()
	{
		auto& core = CORE();
		if(!*core.mlogic)
			return;
		if(!queued_saves.empty() || (do_unsafe_rewind && !unsafe_rewind_obj)) {
			core.rom->runtosave();
			for(auto i : queued_saves) {
				do_save_state(i.first, i.second);
				int tmp = -1;
				core.slotcache->flush(translate_name_mprefix(i.first, tmp, -1));
			}
			if(do_unsafe_rewind && !unsafe_rewind_obj) {
				uint64_t t = framerate_regulator::get_utime();
				std::vector<char> s = core.rom->save_core_state(true);
				uint64_t secs = core.mlogic->get_mfile().rtc_second;
				uint64_t ssecs = core.mlogic->get_mfile().rtc_subsecond;
				core.lua2->callback_do_unsafe_rewind(s, secs, ssecs, core.mlogic->get_movie(),
					NULL);
				do_unsafe_rewind = false;
				messages << "Rewind point set in " << (framerate_regulator::get_utime() - t)
					<< " usec." << std::endl;
			}
		}
		queued_saves.clear();
	}

	bool handle_corrupt()
	{
		auto& core = CORE();
		if(!core.runmode->is_corrupt())
			return false;
		while(core.runmode->is_corrupt()) {
			platform::set_paused(true);
			platform::flush_command_queue();
			handle_load();
			if(core.runmode->is_quit())
				return true;
		}
		return true;
	}
}

void init_main_callbacks()
{
	ecore_callbacks = &lsnes_callbacks_obj;
}

void main_loop(struct loaded_rom& rom, struct moviefile& initial, bool load_has_to_succeed) throw(std::bad_alloc,
	std::runtime_error)
{
	lsnes_instance.emu_thread = threads::id();
	auto& core = CORE();
	mywindowcallbacks mywcb(*core.dispatch, *core.runmode, *core.supdater);
	core.iqueue->system_thread_available = true;
	//Basic initialization.
	core.commentary->init();
	core.fbuf->init_special_screens();
	core.jukebox->set_update([&core]() { core.supdater->update(); });
	*core.rom = rom;
	init_main_callbacks();
	initialize_all_builtin_c_cores();
	core_core::install_all_handlers();

	//Load our given movie.
	bool first_round = false;
	bool just_did_loadstate = false;
	bool used = false;
	try {
		do_load_state(initial, LOAD_STATE_INITIAL, used);
		core.runmode->set_point(emulator_runmode::P_SAVE);
		core.supdater->update();
		first_round = core.mlogic->get_mfile().is_savestate;
		just_did_loadstate = first_round;
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		if(!used)
			delete &initial;
		platform::error_message(std::string("Can't load initial state: ") + e.what());
		messages << "ERROR: Can't load initial state: " << e.what() << std::endl;
		if(load_has_to_succeed) {
			messages << "FATAL: Can't load movie" << std::endl;
			throw;
		}
		core.runmode->set_corrupt();
		core.fbuf->redraw_framebuffer(emu_framebuffer::screen_corrupt);
	}

	core.runmode->set_pause_cond(initial.start_paused);
	platform::set_paused(core.runmode->is_paused());
	stop_at_frame_active = false;

	core.lua2->run_startup_scripts();

	while(!core.runmode->is_quit() || !queued_saves.empty()) {
		if(handle_corrupt()) {
			first_round = *core.mlogic && core.mlogic->get_mfile().is_savestate;
			just_did_loadstate = first_round;
			continue;
		}
		core.framerate->ack_frame_tick(framerate_regulator::get_utime());
		core.runmode->decay_skiplag();

		if(!first_round) {
			core.controls->reset_framehold();
			if(!macro_hold_1 && !macro_hold_2) {
				core.controls->advance_macros();
			}
			macro_hold_2 = false;
			core.mlogic->get_movie().get_pollcounters().set_framepflag(false);
			core.mlogic->new_frame_starting(core.runmode->is_skiplag());
			core.mlogic->get_movie().get_pollcounters().set_framepflag(true);
			if(core.runmode->is_quit() && queued_saves.empty())
				break;
			handle_saves();
			int r = 0;
			if(queued_saves.empty())
				r = handle_load();
			if(r > 0 || core.runmode->is_corrupt()) {
				core.mlogic->get_movie().get_pollcounters().set_framepflag(
					core.mlogic->get_mfile().is_savestate);
				first_round = core.mlogic->get_mfile().is_savestate;
				stop_at_frame_active = false;
				just_did_loadstate = first_round;
				core.controls->reset_framehold();
				core.dbg->do_callback_frame(core.mlogic->get_movie().get_current_frame(), true);
				continue;
			} else if(r < 0) {
				//Not exactly desriable, but this at least won't desync.
				stop_at_frame_active = false;
				if(core.runmode->is_quit())
					goto out;
				core.runmode->set_pause();
			}
		}
		if(just_did_loadstate) {
			//If we just loadstated, we are up to date.
			if(core.runmode->is_quit())
				break;
			platform::set_paused(core.runmode->is_paused());
			platform::flush_command_queue();
			//We already have done the reset this frame if we are going to do one at all.
			core.mlogic->get_movie().set_controls(core.mlogic->update_controls(true));
			core.mlogic->get_movie().set_all_DRDY();
			just_did_loadstate = false;
		}
		core.dbg->do_callback_frame(core.mlogic->get_movie().get_current_frame(), false);
		core.rom->emulate();
		random_mix_timing_entropy();
		if(core.runmode->is_freerunning())
			platform::wait(core.framerate->to_wait_frame(framerate_regulator::get_utime()));
		first_round = false;
		core.lua2->callback_do_frame();
	}
out:
	core.jukebox->unset_update();
	core.mdumper->end_dumps();
	core_core::uninstall_all_handlers();
	core.commentary->kill();
	core.iqueue->system_thread_available = false;
	//Kill some things to avoid crashes.
	core.dbg->core_change();
	core.project->set(NULL, true);
	core.mwatch->clear_multi(core.mwatch->enumerate());
}

void set_stop_at_frame(uint64_t frame)
{
	auto& core = CORE();
	stop_at_frame = frame;
	stop_at_frame_active = (frame != 0);
	if(!core.runmode->is_special())
		core.runmode->set_freerunning();
	platform::set_paused(false);
}

void do_flush_slotinfo()
{
	CORE().slotcache->flush();
}

void switch_projects(const std::string& newproj)
{
	pending_new_project = newproj;
	CORE().runmode->start_load();
	platform::cancel_wait();
	platform::set_paused(false);
}

void load_new_rom(const romload_request& req)
{
	if(_load_new_rom(req)) {
		mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_ROMRELOAD);
	}
}

void reload_current_rom()
{
	if(reload_active_rom()) {
		mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_ROMRELOAD);
	}
}

void close_rom()
{
	if(load_null_rom()) {
		CORE().runmode->set_pause();
		mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_ROMRELOAD);
	}
}

void do_break_pause()
{
	auto& core = CORE();
	core.runmode->set_break();
	core.supdater->update();
	while(core.runmode->is_paused_break()) {
		platform::set_paused(true);
		platform::flush_command_queue();
	}
}

void convert_break_to_pause()
{
	auto& core = CORE();
	if(core.runmode->is_paused_break()) {
		core.runmode->set_pause();
		core.supdater->update();
	}
}

void debug_trash_memory(uint8_t* addr, uint8_t byte)
{
	*addr = byte;
}