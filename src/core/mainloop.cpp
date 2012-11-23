#include "lsnes.hpp"
#include "core/emucore.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/inthread.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"
#include "core/mainloop.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/framebuffer.hpp"
#include "library/pixfmt-lrgb.hpp"

#include <iomanip>
#include <cassert>
#include <sstream>
#include <iostream>
#include <limits>
#include <set>
#include <sys/time.h>

#define SPECIAL_FRAME_START 0
#define SPECIAL_FRAME_VIDEO 1
#define SPECIAL_SAVEPOINT 2
#define SPECIAL_NONE 3

void update_movie_state();
time_t random_seed_value = 0;

namespace
{
	enum advance_mode
	{
		ADVANCE_QUIT,			//Quit the emulator.
		ADVANCE_AUTO,			//Normal (possibly slowed down play).
		ADVANCE_LOAD,			//Loading a state.
		ADVANCE_FRAME,			//Frame advance.
		ADVANCE_SUBFRAME,		//Subframe advance.
		ADVANCE_SKIPLAG,		//Skip lag (oneshot, reverts to normal).
		ADVANCE_SKIPLAG_PENDING,	//Activate skip lag mode at next frame.
		ADVANCE_PAUSE,			//Unconditional pause.
	};

	//Previous mouse mask.
	int prev_mouse_mask = 0;
	//Flags related to repeating advance.
	bool advanced_once;
	bool cancel_advance;
	//Emulator advance mode. Detemines pauses at start of frame / subframe, etc..
	enum advance_mode amode;
	//Mode and filename of pending load, one of LOAD_* constants.
	int loadmode;
	std::string pending_load;
	//Queued saves (all savestates).
	std::set<std::string> queued_saves;
	//Save jukebox.
	numeric_setting jukebox_size(lsnes_set, "jukebox-size", 0, 999, 12);
	size_t save_jukebox_pointer;
	//Special subframe location. One of SPECIAL_* constants.
	int location_special;
	//Few settings.
	numeric_setting advance_timeout_first(lsnes_set, "advance-timeout", 0, 999999999, 500);
	boolean_setting pause_on_end(lsnes_set, "pause-on-end", false);
	//Last frame params.
	bool last_hires = false;
	bool last_interlace = false;
	//Unsafe rewind.
	bool do_unsafe_rewind = false;
	void* unsafe_rewind_obj = NULL;

	enum advance_mode old_mode;

	std::string save_jukebox_name(size_t i)
	{
		return (stringfmt() << "${project}" << (i + 1) << ".lsmv").str();
	}

	class _lsnes_pflag_handler : public movie::poll_flag
	{
	public:
		~_lsnes_pflag_handler()
		{
		}
		int get_pflag()
		{
			return core_get_poll_flag();
		}
		void set_pflag(int flag)
		{
			core_set_poll_flag(flag);
		}
	} lsnes_pflag_handler;

}

path_setting firmwarepath_setting(lsnes_set, "firmwarepath");

void mainloop_signal_need_rewind(void* ptr)
{
	if(ptr) {
		old_mode = amode;
		amode = ADVANCE_LOAD;
	}
	do_unsafe_rewind = true;
	unsafe_rewind_obj = ptr;
}

controller_frame movie_logic::update_controls(bool subframe) throw(std::bad_alloc, std::runtime_error)
{
	if(lua_requests_subframe_paint)
		redraw_framebuffer();

	if(subframe) {
		if(amode == ADVANCE_SUBFRAME) {
			if(!cancel_advance && !advanced_once) {
				platform::wait(advance_timeout_first * 1000);
				advanced_once = true;
			}
			if(cancel_advance) {
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			platform::set_paused(amode == ADVANCE_PAUSE);
		} else if(amode == ADVANCE_FRAME) {
			;
		} else {
			if(amode == ADVANCE_SKIPLAG)
				amode = ADVANCE_PAUSE;
			platform::set_paused(amode == ADVANCE_PAUSE);
			cancel_advance = false;
		}
		location_special = SPECIAL_NONE;
		update_movie_state();
	} else {
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;
		if(amode == ADVANCE_FRAME || amode == ADVANCE_SUBFRAME) {
			if(!cancel_advance) {
				platform::wait(advanced_once ? to_wait_frame(get_utime()) :
					(advance_timeout_first * 1000));
				advanced_once = true;
			}
			if(cancel_advance) {
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			platform::set_paused(amode == ADVANCE_PAUSE);
		} else if(amode == ADVANCE_AUTO && movb.get_movie().readonly_mode() && pause_on_end) {
			if(movb.get_movie().get_current_frame() == movb.get_movie().get_frame_count()) {
				amode = ADVANCE_PAUSE;
				platform::set_paused(true);
			}
		} else {
			platform::set_paused((amode == ADVANCE_PAUSE));
			cancel_advance = false;
		}
		location_special = SPECIAL_FRAME_START;
		update_movie_state();

	}
	information_dispatch::do_status_update();
	platform::flush_command_queue();
	controller_frame tmp = controls.commit(movb.get_movie().get_current_frame());
	lua_callback_do_input(tmp, subframe);
	return tmp;
}

namespace
{

	//Do pending load (automatically unpauses).
	void mark_pending_load(const std::string& filename, int lmode)
	{
		loadmode = lmode;
		pending_load = filename;
		old_mode = amode;
		amode = ADVANCE_LOAD;
		platform::cancel_wait();
		platform::set_paused(false);
	}

	void mark_pending_save(const std::string& filename, int smode)
	{
		if(smode == SAVE_MOVIE) {
			//Just do this immediately.
			do_save_movie(filename);
			return;
		}
		queued_saves.insert(filename);
		messages << "Pending save on '" << filename << "'" << std::endl;
	}

	bool reload_rom(const std::string& filename)
	{
		std::string filenam = filename;
		if(filenam == "")
			filenam = our_rom->load_filename;
		if(filenam == "") {
			messages << "No ROM loaded" << std::endl;
			return false;
		}
		try {
			messages << "Loading ROM " << filenam << std::endl;
			loaded_rom newrom(filenam);
			*our_rom = newrom;
			for(size_t i = 0; i < sizeof(our_rom->romimg)/sizeof(our_rom->romimg[0]); i++) {
				our_movie.romimg_sha256[i] = our_rom->romimg[i].sha256;
				our_movie.romxml_sha256[i] = our_rom->romxml[i].sha256;
			}
		} catch(std::exception& e) {
			messages << "Can't reload ROM: " << e.what() << std::endl;
			return false;
		}
		return true;
	}
}

void update_movie_state()
{
	{
		uint64_t magic[4];
		core_get_region().fill_framerate_magic(magic);
		voice_frame_number(movb.get_movie().get_current_frame(), 1.0 * magic[1] / magic[0]);
	}
	auto& _status = platform::get_emustatus();
	if(!system_corrupt) {
		std::ostringstream x;
		x << movb.get_movie().get_current_frame() << "(";
		if(location_special == SPECIAL_FRAME_START)
			x << "0";
		else if(location_special == SPECIAL_SAVEPOINT)
			x << "S";
		else if(location_special == SPECIAL_FRAME_VIDEO)
			x << "V";
		else
			x << movb.get_movie().next_poll_number();
		x << ";" << movb.get_movie().get_lag_frames() << ")/" << movb.get_movie().get_frame_count();
		_status.set("Frame", x.str());
	} else
		_status.set("Frame", "N/A");
	if(!system_corrupt) {
		time_t timevalue = static_cast<time_t>(our_movie.rtc_second);
		struct tm* time_decompose = gmtime(&timevalue);
		char datebuffer[512];
		strftime(datebuffer, 511, "%Y%m%d(%a)T%H%M%S", time_decompose);
		_status.set("RTC", datebuffer);
	} else {
		_status.set("RTC", "N/A");
	}
	{
		std::ostringstream x;
		auto& mo = movb.get_movie();
		x << (information_dispatch::get_dumper_count() ? "D" : "-");
		x << (last_hires ? "H" : "-");
		x << (last_interlace ? "I" : "-");
		if(system_corrupt)
			x << "C";
		else if(!mo.readonly_mode())
			x << "R";
		else if(mo.get_frame_count() >= mo.get_current_frame())
			x << "P";
		else
			x << "F";
		_status.set("Flags", x.str());
	}
	if(jukebox_size > 0)
		_status.set("Saveslot", translate_name_mprefix(save_jukebox_name(save_jukebox_pointer)));
	else
		_status.erase("Saveslot");
	{
		std::ostringstream x;
		x << get_framerate();
		_status.set("SPD%", x.str());
	}
	do_watch_memory();

	controller_frame c;
	if(movb.get_movie().readonly_mode())
		c = movb.get_movie().get_controls();
	else
		c = controls.get_committed();
	auto lim = get_core_logical_controller_limits();
	for(unsigned i = 0; i < lim.first; i++) {
		auto pindex = controls.lcid_to_pcid(i);
		if(pindex.first < 0 || !controls.is_present(pindex.first, pindex.second)) {
			_status.erase((stringfmt() << "P" << (i + 1)).str());
			continue;
		}
		char buffer[MAX_DISPLAY_LENGTH];
		c.display(pindex.first, pindex.second, buffer);
		_status.set((stringfmt() << "P" << (i + 1)).str(), buffer);
	}
}

uint64_t audio_irq_time;
uint64_t controller_irq_time;
uint64_t frame_irq_time;

struct lsnes_callbacks : public emucore_callbacks
{
public:
	~lsnes_callbacks() throw()
	{
	}
	
	int16_t get_input(unsigned port, unsigned index, unsigned control)
	{
		int16_t x;
		x = movb.input_poll(port, index, control);
		lua_callback_snoop_input(port, index, control, x);
		return x;
	}

	int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value)
	{
		if(!movb.get_movie().readonly_mode()) {
			controller_frame f = movb.get_movie().get_controls();
			f.axis3(port, index, control, value);
			movb.get_movie().set_controls(f);
		}
		return movb.get_movie().next_input(port, index, control);
	}

	void timer_tick(uint32_t increment, uint32_t per_second)
	{
		our_movie.rtc_subsecond += increment;
		while(our_movie.rtc_subsecond >= per_second) {
			our_movie.rtc_second++;
			our_movie.rtc_subsecond -= per_second;
		}
	}

	std::string get_firmware_path()
	{
		return firmwarepath_setting;
	}
	
	std::string get_base_path()
	{
		return our_rom->msu1_base;
	}

	time_t get_time()
	{
		return our_movie.rtc_second;
	}

	time_t get_randomseed()
	{
		return random_seed_value;
	}

	void output_frame(framebuffer_raw& screen, uint32_t fps_n, uint32_t fps_d)
	{
		lua_callback_do_frame_emulated();
		location_special = SPECIAL_FRAME_VIDEO;
		update_movie_state();
		redraw_framebuffer(screen, false, true);
		uint32_t g = gcd(fps_n, fps_d);
		fps_n /= g;
		fps_d /= g;
		information_dispatch::do_frame(screen, fps_n, fps_d);
	}
};

namespace
{
	class jukebox_size_listener : public setting_listener
	{
	public:
		jukebox_size_listener() {}
		~jukebox_size_listener() throw() {}
		void blanked(setting_group& group, const std::string& setting) {}
		void changed(setting_group& group, const std::string& setting, const std::string& value)
		{
			if(setting == "jukebox-size") {
				if(save_jukebox_pointer >= jukebox_size)
					save_jukebox_pointer = 0;
				update_movie_state();
			}
		}
	} _jukebox_size_listener;

	function_ptr_command<> count_rerecords(lsnes_cmd, "count-rerecords", "Count rerecords",
		"Syntax: count-rerecords\nCounts rerecords.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			std::vector<char> tmp;
			uint64_t x = rrdata::write(tmp);
			messages << x << " rerecord(s)" << std::endl;
		});

	function_ptr_command<const std::string&> quit_emulator(lsnes_cmd, "quit-emulator", "Quit the emulator",
		"Syntax: quit-emulator [/y]\nQuits emulator (/y => don't ask for confirmation).\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_QUIT;
			platform::set_paused(false);
			platform::cancel_wait();
		});

	function_ptr_command<> unpause_emulator(lsnes_cmd, "unpause-emulator", "Unpause the emulator",
		"Syntax: unpause-emulator\nUnpauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_AUTO;
			platform::set_paused(false);
			platform::cancel_wait();
			messages << "Unpaused" << std::endl;
		});

	function_ptr_command<> pause_emulator(lsnes_cmd, "pause-emulator", "(Un)pause the emulator",
		"Syntax: pause-emulator\n(Un)pauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(amode != ADVANCE_AUTO) {
				amode = ADVANCE_AUTO;
				platform::set_paused(false);
				platform::cancel_wait();
				messages << "Unpaused" << std::endl;
			} else {
				platform::cancel_wait();
				cancel_advance = false;
				amode = ADVANCE_PAUSE;
				messages << "Paused" << std::endl;
			}
		});

	function_ptr_command<> save_jukebox_prev(lsnes_cmd, "cycle-jukebox-backward", "Cycle save jukebox backwards",
		"Syntax: cycle-jukebox-backward\nCycle save jukebox backwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				return;
			if(save_jukebox_pointer == 0)
				save_jukebox_pointer = jukebox_size - 1;
			else
				save_jukebox_pointer--;
			if(save_jukebox_pointer >= jukebox_size)
				save_jukebox_pointer = 0;
			update_movie_state();
			information_dispatch::do_status_update();
		});

	function_ptr_command<> save_jukebox_next(lsnes_cmd, "cycle-jukebox-forward", "Cycle save jukebox forwards",
		"Syntax: cycle-jukebox-forward\nCycle save jukebox forwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				return;
			if(save_jukebox_pointer == jukebox_size - 1)
				save_jukebox_pointer = 0;
			else
				save_jukebox_pointer++;
			if(save_jukebox_pointer >= jukebox_size)
				save_jukebox_pointer = 0;
			update_movie_state();
			information_dispatch::do_status_update();
		});

	function_ptr_command<> load_jukebox(lsnes_cmd, "load-jukebox", "Load save from jukebox",
		"Syntax: load-jukebox\nLoad save from jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_load(save_jukebox_name(save_jukebox_pointer), LOAD_STATE_CURRENT);
		});

	function_ptr_command<> save_jukebox_c(lsnes_cmd, "save-jukebox", "Save save to jukebox",
		"Syntax: save-jukebox\nSave save to jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_save(save_jukebox_name(save_jukebox_pointer), SAVE_STATE);
		});

	function_ptr_command<> padvance_frame(lsnes_cmd, "+advance-frame", "Advance one frame",
		"Syntax: +advance-frame\nAdvances the emulation by one frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_FRAME;
			cancel_advance = false;
			advanced_once = false;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	function_ptr_command<> nadvance_frame(lsnes_cmd, "-advance-frame", "Advance one frame",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			cancel_advance = true;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	function_ptr_command<> padvance_poll(lsnes_cmd, "+advance-poll", "Advance one subframe",
		"Syntax: +advance-poll\nAdvances the emulation by one subframe.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_SUBFRAME;
			cancel_advance = false;
			advanced_once = false;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	function_ptr_command<> nadvance_poll(lsnes_cmd, "-advance-poll", "Advance one subframe",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			cancel_advance = true;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	function_ptr_command<> advance_skiplag(lsnes_cmd, "advance-skiplag", "Skip to next poll",
		"Syntax: advance-skiplag\nAdvances the emulation to the next poll.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_SKIPLAG_PENDING;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	function_ptr_command<const std::string&> reset_c(lsnes_cmd, "reset", "Reset the system",
		"Syntax: reset\nReset <delay>\nResets the system in beginning of the next frame.\n",
		[](const std::string& x) throw(std::bad_alloc, std::runtime_error) {
			if(!core_supports_reset) {
				messages << "Emulator core does not support resets" << std::endl;
				return;
			}
			if(!core_supports_dreset && x != "") {
				messages << "Emulator core does not support delayed resets" << std::endl;
				return;
			}
			if(x == "")
				core_request_reset(0);
			else
				core_request_reset(parse_value<uint32_t>(x));
		});

	function_ptr_command<arg_filename> load_c(lsnes_cmd, "load", "Load savestate (current mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in current mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_CURRENT);
		});

	function_ptr_command<arg_filename> load_smart_c(lsnes_cmd, "load-smart", "Load savestate (heuristic mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in heuristic mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_DEFAULT);
		});

	function_ptr_command<arg_filename> load_state_c(lsnes_cmd, "load-state", "Load savestate (R/W)",
		"Syntax: load-state <file>\nLoads SNES state from <file> in Read/Write mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RW);
		});

	function_ptr_command<arg_filename> load_readonly(lsnes_cmd, "load-readonly", "Load savestate (RO)",
		"Syntax: load-readonly <file>\nLoads SNES state from <file> in read-only mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RO);
		});

	function_ptr_command<arg_filename> load_preserve(lsnes_cmd, "load-preserve", "Load savestate (preserve "
		"input)", "Syntax: load-preserve <file>\nLoads SNES state from <file> preserving input\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_PRESERVE);
		});

	function_ptr_command<arg_filename> load_movie_c(lsnes_cmd, "load-movie", "Load movie",
		"Syntax: load-movie <file>\nLoads SNES movie from <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_MOVIE);
		});


	function_ptr_command<arg_filename> save_state(lsnes_cmd, "save-state", "Save state",
		"Syntax: save-state <file>\nSaves SNES state to <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE);
		});

	function_ptr_command<arg_filename> save_movie(lsnes_cmd, "save-movie", "Save movie",
		"Syntax: save-movie <file>\nSaves SNES movie to <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE);
		});

	function_ptr_command<> set_rwmode(lsnes_cmd, "set-rwmode", "Switch to read/write mode",
		"Syntax: set-rwmode\nSwitches to read/write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			movb.get_movie().readonly_mode(false);
			information_dispatch::do_mode_change(false);
			lua_callback_do_readwrite();
			update_movie_state();
			information_dispatch::do_status_update();
		});

	function_ptr_command<> set_romode(lsnes_cmd, "set-romode", "Switch to read-only mode",
		"Syntax: set-romode\nSwitches to read-only mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			movb.get_movie().readonly_mode(true);
			information_dispatch::do_mode_change(true);
			update_movie_state();
			information_dispatch::do_status_update();
		});

	function_ptr_command<> toggle_rwmode(lsnes_cmd, "toggle-rwmode", "Toggle read/write mode",
		"Syntax: toggle-rwmode\nToggles read/write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			bool c = movb.get_movie().readonly_mode();
			movb.get_movie().readonly_mode(!c);
			information_dispatch::do_mode_change(!c);
			if(c)
				lua_callback_do_readwrite();
			update_movie_state();
			information_dispatch::do_status_update();
		});

	function_ptr_command<> repaint(lsnes_cmd, "repaint", "Redraw the screen",
		"Syntax: repaint\nRedraws the screen\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			redraw_framebuffer();
		});

	function_ptr_command<> tpon(lsnes_cmd, "toggle-pause-on-end", "Toggle pause on end", "Toggle pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			bool newstate = !static_cast<bool>(pause_on_end);
			pause_on_end.set(newstate ? "1" : "0");
			messages << "Pause-on-end is now " << (newstate ? "ON" : "OFF") << std::endl;
		});

	function_ptr_command<> rewind_movie(lsnes_cmd, "rewind-movie", "Rewind movie to the beginning",
		"Syntax: rewind-movie\nRewind movie to the beginning\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_BEGINNING);
		});

	function_ptr_command<const std::string&> reload_rom2(lsnes_cmd, "reload-rom", "Reload the ROM image",
		"Syntax: reload-rom [<file>]\nReload the ROM image from <file>\n",
		[](const std::string& filename) throw(std::bad_alloc, std::runtime_error) {
			if(reload_rom(filename))
				mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_ROMRELOAD);
		});

	function_ptr_command<> cancel_save(lsnes_cmd, "cancel-saves", "Cancel all pending saves", "Syntax: "
		"cancel-save\nCancel pending saves\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			queued_saves.clear();
			messages << "Pending saves canceled." << std::endl;
		});

	function_ptr_command<> test1(lsnes_cmd, "test-1", "no description available", "No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			redraw_framebuffer(screen_nosignal);
		});

	function_ptr_command<> test2(lsnes_cmd, "test-2", "no description available", "No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			redraw_framebuffer(screen_corrupt);
		});

	function_ptr_command<> test3(lsnes_cmd, "test-3", "no description available", "No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			while(1);
		});

	inverse_key ipause_emulator("pause-emulator", "Speed‣(Un)pause");
	inverse_key ijback("cycle-jukebox-backward", "Slot select‣Cycle backwards");
	inverse_key ijforward("cycle-jukebox-forward", "Slot select‣Cycle forwards");
	inverse_key iloadj("load-jukebox", "Load‣Selected slot");
	inverse_key isavej("save-jukebox", "Save‣Selected slot");
	inverse_key iadvframe("+advance-frame", "Speed‣Advance frame");
	inverse_key iadvsubframe("+advance-poll", "Speed‣Advance subframe");
	inverse_key iskiplag("advance-skiplag", "Speed‣Advance poll");
	inverse_key ireset("reset", "System‣Reset");
	inverse_key iset_rwmode("set-rwmode", "Movie‣Switch to read/write");
	inverse_key itoggle_romode("set-romode", "Movie‣Switch to read-only");
	inverse_key itoggle_rwmode("toggle-rwmode", "Movie‣Toggle read-only");
	inverse_key irepaint("repaint", "System‣Repaint screen");
	inverse_key itogglepause("toggle-pause-on-end", "Movie‣Toggle pause-on-end");
	inverse_key irewind_movie("rewind-movie", "Movie‣Rewind movie");
	inverse_key icancel_saves("cancel-saves", "Save‣Cancel pending saves");
	inverse_key iload1("load ${project}1.lsmv", "Load‣Slot 1");
	inverse_key iload2("load ${project}2.lsmv", "Load‣Slot 2");
	inverse_key iload3("load ${project}3.lsmv", "Load‣Slot 3");
	inverse_key iload4("load ${project}4.lsmv", "Load‣Slot 4");
	inverse_key iload5("load ${project}5.lsmv", "Load‣Slot 5");
	inverse_key iload6("load ${project}6.lsmv", "Load‣Slot 6");
	inverse_key iload7("load ${project}7.lsmv", "Load‣Slot 7");
	inverse_key iload8("load ${project}8.lsmv", "Load‣Slot 8");
	inverse_key iload9("load ${project}9.lsmv", "Load‣Slot 9");
	inverse_key iload10("load ${project}10.lsmv", "Load‣Slot 10");
	inverse_key iload11("load ${project}11.lsmv", "Load‣Slot 11");
	inverse_key iload12("load ${project}12.lsmv", "Load‣Slot 12");
	inverse_key iload13("load ${project}13.lsmv", "Load‣Slot 13");
	inverse_key iload14("load ${project}14.lsmv", "Load‣Slot 14");
	inverse_key iload15("load ${project}15.lsmv", "Load‣Slot 15");
	inverse_key iload16("load ${project}16.lsmv", "Load‣Slot 16");
	inverse_key iload17("load ${project}17.lsmv", "Load‣Slot 17");
	inverse_key iload18("load ${project}18.lsmv", "Load‣Slot 18");
	inverse_key iload19("load ${project}19.lsmv", "Load‣Slot 19");
	inverse_key iload20("load ${project}20.lsmv", "Load‣Slot 20");
	inverse_key iload21("load ${project}21.lsmv", "Load‣Slot 21");
	inverse_key iload22("load ${project}22.lsmv", "Load‣Slot 22");
	inverse_key iload23("load ${project}23.lsmv", "Load‣Slot 23");
	inverse_key iload24("load ${project}24.lsmv", "Load‣Slot 24");
	inverse_key iload25("load ${project}25.lsmv", "Load‣Slot 25");
	inverse_key iload26("load ${project}26.lsmv", "Load‣Slot 26");
	inverse_key iload27("load ${project}27.lsmv", "Load‣Slot 27");
	inverse_key iload28("load ${project}28.lsmv", "Load‣Slot 28");
	inverse_key iload29("load ${project}29.lsmv", "Load‣Slot 29");
	inverse_key iload30("load ${project}30.lsmv", "Load‣Slot 30");
	inverse_key iload31("load ${project}31.lsmv", "Load‣Slot 31");
	inverse_key iload32("load ${project}32.lsmv", "Load‣Slot 32");
	inverse_key isave1("save-state ${project}1.lsmv", "Save‣Slot 1");
	inverse_key isave2("save-state ${project}2.lsmv", "Save‣Slot 2");
	inverse_key isave3("save-state ${project}3.lsmv", "Save‣Slot 3");
	inverse_key isave4("save-state ${project}4.lsmv", "Save‣Slot 4");
	inverse_key isave5("save-state ${project}5.lsmv", "Save‣Slot 5");
	inverse_key isave6("save-state ${project}6.lsmv", "Save‣Slot 6");
	inverse_key isave7("save-state ${project}7.lsmv", "Save‣Slot 7");
	inverse_key isave8("save-state ${project}8.lsmv", "Save‣Slot 8");
	inverse_key isave9("save-state ${project}9.lsmv", "Save‣Slot 9");
	inverse_key isave10("save-state ${project}10.lsmv", "Save‣Slot 10");
	inverse_key isave11("save-state ${project}11.lsmv", "Save‣Slot 11");
	inverse_key isave12("save-state ${project}12.lsmv", "Save‣Slot 12");
	inverse_key isave13("save-state ${project}13.lsmv", "Save‣Slot 13");
	inverse_key isave14("save-state ${project}14.lsmv", "Save‣Slot 14");
	inverse_key isave15("save-state ${project}15.lsmv", "Save‣Slot 15");
	inverse_key isave16("save-state ${project}16.lsmv", "Save‣Slot 16");
	inverse_key isave17("save-state ${project}17.lsmv", "Save‣Slot 17");
	inverse_key isave18("save-state ${project}18.lsmv", "Save‣Slot 18");
	inverse_key isave19("save-state ${project}19.lsmv", "Save‣Slot 19");
	inverse_key isave20("save-state ${project}20.lsmv", "Save‣Slot 20");
	inverse_key isave21("save-state ${project}21.lsmv", "Save‣Slot 21");
	inverse_key isave22("save-state ${project}22.lsmv", "Save‣Slot 22");
	inverse_key isave23("save-state ${project}23.lsmv", "Save‣Slot 23");
	inverse_key isave24("save-state ${project}24.lsmv", "Save‣Slot 24");
	inverse_key isave25("save-state ${project}25.lsmv", "Save‣Slot 25");
	inverse_key isave26("save-state ${project}26.lsmv", "Save‣Slot 26");
	inverse_key isave27("save-state ${project}27.lsmv", "Save‣Slot 27");
	inverse_key isave28("save-state ${project}28.lsmv", "Save‣Slot 28");
	inverse_key isave29("save-state ${project}29.lsmv", "Save‣Slot 29");
	inverse_key isave30("save-state ${project}30.lsmv", "Save‣Slot 30");
	inverse_key isave31("save-state ${project}31.lsmv", "Save‣Slot 31");
	inverse_key isave32("save-state ${project}32.lsmv", "Save‣Slot 32");

	bool on_quit_prompt = false;
	class mywindowcallbacks : public information_dispatch
	{
	public:
		mywindowcallbacks() : information_dispatch("mainloop-window-callbacks") {}
		void on_new_dumper(const std::string& n)
		{
			update_movie_state();
		}
		void on_destroy_dumper(const std::string& n)
		{
			update_movie_state();
		}
		void on_close() throw()
		{
			if(on_quit_prompt) {
				amode = ADVANCE_QUIT;
				platform::set_paused(false);
				platform::cancel_wait();
				return;
			}
			on_quit_prompt = true;
			try {
				amode = ADVANCE_QUIT;
				platform::set_paused(false);
				platform::cancel_wait();
			} catch(...) {
			}
			on_quit_prompt = false;
		}
	} mywcb;

	//If there is a pending load, perform it. Return 1 on successful load, 0 if nothing to load, -1 on load
	//failing.
	int handle_load()
	{
		if(do_unsafe_rewind && unsafe_rewind_obj) {
			uint64_t t = get_utime();
			std::vector<char> s;
			lua_callback_do_unsafe_rewind(s, 0, 0, movb.get_movie(), unsafe_rewind_obj);
			information_dispatch::do_mode_change(false);
			do_unsafe_rewind = false;
			our_movie.is_savestate = true;
			location_special = SPECIAL_SAVEPOINT;
			update_movie_state();
			messages << "Rewind done in " << (get_utime() - t) << " usec." << std::endl;
			return 1;
		}
		if(pending_load != "") {
			system_corrupt = false;
			if(loadmode != LOAD_STATE_BEGINNING && loadmode != LOAD_STATE_ROMRELOAD &&
				!do_load_state(pending_load, loadmode)) {
				movb.get_movie().set_pflag_handler(&lsnes_pflag_handler);
				pending_load = "";
				return -1;
			}
			try {
				if(loadmode == LOAD_STATE_BEGINNING)
					do_load_beginning(false);
				if(loadmode == LOAD_STATE_ROMRELOAD)
					do_load_beginning(true);
			} catch(std::exception& e) {
				messages << "Load failed: " << e.what() << std::endl;
			}
			movb.get_movie().set_pflag_handler(&lsnes_pflag_handler);
			pending_load = "";
			amode = ADVANCE_AUTO;
			platform::cancel_wait();
			platform::set_paused(false);
			if(!system_corrupt) {
				location_special = SPECIAL_SAVEPOINT;
				update_movie_state();
				information_dispatch::do_status_update();
				platform::flush_command_queue();
			}
			return 1;
		}
		return 0;
	}

	//If there are pending saves, perform them.
	void handle_saves()
	{
		if(!queued_saves.empty() || (do_unsafe_rewind && !unsafe_rewind_obj)) {
			core_runtosave();
			for(auto i : queued_saves)
				do_save_state(i);
			if(do_unsafe_rewind && !unsafe_rewind_obj) {
				uint64_t t = get_utime();
				std::vector<char> s = save_core_state(true);
				uint64_t secs = our_movie.rtc_second;
				uint64_t ssecs = our_movie.rtc_subsecond;
				lua_callback_do_unsafe_rewind(s, secs, ssecs, movb.get_movie(), NULL);
				do_unsafe_rewind = false;
				messages << "Rewind point set in " << (get_utime() - t) << " usec." << std::endl;
			}
		}
		queued_saves.clear();
	}

	bool handle_corrupt()
	{
		if(!system_corrupt)
			return false;
		while(system_corrupt) {
			platform::cancel_wait();
			platform::set_paused(true);
			platform::flush_command_queue();
			handle_load();
			if(amode == ADVANCE_QUIT)
				return true;
		}
		return true;
	}
}

void main_loop(struct loaded_rom& rom, struct moviefile& initial, bool load_has_to_succeed) throw(std::bad_alloc,
	std::runtime_error)
{
	//Init listners.
	lsnes_set.add_listener(_jukebox_size_listener);
	//Basic initialization.
	voicethread_task();
	init_special_screens();
	our_rom = &rom;
	lsnes_callbacks lsnes_callbacks_obj;
	ecore_callbacks = &lsnes_callbacks_obj;
	movb.get_movie().set_pflag_handler(&lsnes_pflag_handler);
	core_install_handler();

	//Load our given movie.
	bool first_round = false;
	bool just_did_loadstate = false;
	try {
		do_load_state(initial, LOAD_STATE_INITIAL);
		location_special = SPECIAL_SAVEPOINT;
		update_movie_state();
		first_round = our_movie.is_savestate;
		just_did_loadstate = first_round;
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "ERROR: Can't load initial state: " << e.what() << std::endl;
		if(load_has_to_succeed) {
			messages << "FATAL: Can't load movie" << std::endl;
			platform::fatal_error();
		}
		system_corrupt = true;
		update_movie_state();
		redraw_framebuffer(screen_corrupt);
	}

	lua_callback_startup();

	platform::set_paused(initial.start_paused);
	amode = initial.start_paused ? ADVANCE_PAUSE : ADVANCE_AUTO;
	uint64_t time_x = get_utime();
	while(amode != ADVANCE_QUIT || !queued_saves.empty()) {
		if(handle_corrupt()) {
			first_round = our_movie.is_savestate;
			just_did_loadstate = first_round;
			continue;
		}
		ack_frame_tick(get_utime());
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;

		if(!first_round) {
			controls.reset_framehold();
			movb.new_frame_starting(amode == ADVANCE_SKIPLAG);
			if(amode == ADVANCE_QUIT && queued_saves.empty())
				break;
			handle_saves();
			int r = 0;
			if(queued_saves.empty())
				r = handle_load();
			if(r > 0 || system_corrupt) {
				first_round = our_movie.is_savestate;
				if(system_corrupt)
					amode = ADVANCE_PAUSE;
				else
					amode = old_mode;
				just_did_loadstate = first_round;
				controls.reset_framehold();
				continue;
			} else if(r < 0) {
				//Not exactly desriable, but this at least won't desync.
				amode = ADVANCE_PAUSE;
			}
		}
		if(just_did_loadstate) {
			//If we just loadstated, we are up to date.
			if(amode == ADVANCE_QUIT)
				break;
			platform::cancel_wait();
			platform::set_paused(amode == ADVANCE_PAUSE);
			platform::flush_command_queue();
			//We already have done the reset this frame if we are going to do one at all.
			movb.get_movie().set_controls(movb.update_controls(true));
			movb.get_movie().set_all_DRDY();
			just_did_loadstate = false;
		}
		frame_irq_time = get_utime() - time_x;
		core_emulate_frame();
		time_x = get_utime();
		if(amode == ADVANCE_AUTO)
			platform::wait(to_wait_frame(get_utime()));
		first_round = false;
		lua_callback_do_frame();
	}
	information_dispatch::do_dump_end();
	core_uninstall_handler();
	voicethread_kill();
}
