#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/avsnoop.hpp"
#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/lua.hpp"
#include "core/mainloop.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/render.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"

#include <iomanip>
#include <cassert>
#include <sstream>
#include <iostream>
#include <set>
#include <sys/time.h>

#define SPECIAL_FRAME_START 0
#define SPECIAL_FRAME_VIDEO 1
#define SPECIAL_SAVEPOINT 2
#define SPECIAL_NONE 3

void update_movie_state();

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
	bool stepping_into_save;
	//Save jukebox.
	std::vector<std::string> save_jukebox;
	size_t save_jukebox_pointer;
	//Emulator status area.
	std::map<std::string, std::string>* status;
	//Pending reset cycles. -1 if no reset pending, otherwise, cycle count for reset.
	long pending_reset_cycles = -1;
	//Set by every video refresh.
	bool video_refresh_done;
	//Special subframe location. One of SPECIAL_* constants.
	int location_special;
	//Few settings.
	numeric_setting advance_timeout_first("advance-timeout", 0, 999999999, 500);
	//Last frame params.
	bool last_hires = false;
	bool last_interlace = false;
}

class firmware_path_setting : public setting
{
public:
	firmware_path_setting() : setting("firmwarepath") { _firmwarepath = "."; default_firmware = true; }
	void blank() throw(std::bad_alloc, std::runtime_error)
	{
		_firmwarepath = ".";
		default_firmware = true;
	}

	bool is_set() throw()
	{
		return !default_firmware;
	}

	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error)
	{
		_firmwarepath = value;
		default_firmware = false;
	}

	std::string get() throw(std::bad_alloc)
	{
		return _firmwarepath;
	}

	operator std::string() throw(std::bad_alloc)
	{
		return _firmwarepath;
	}
private:
	std::string _firmwarepath;
	bool default_firmware;
} firmwarepath_setting;

controls_t movie_logic::update_controls(bool subframe) throw(std::bad_alloc, std::runtime_error)
{
	if(lua_requests_subframe_paint)
		redraw_framebuffer();

	if(subframe) {
		if(amode == ADVANCE_SUBFRAME) {
			if(!cancel_advance && !advanced_once) {
				window::wait_usec(advance_timeout_first * 1000);
				advanced_once = true;
			}
			if(cancel_advance) {
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			window::paused(amode == ADVANCE_PAUSE);
		} else if(amode == ADVANCE_FRAME) {
			;
		} else {
			window::paused(amode == ADVANCE_SKIPLAG || amode == ADVANCE_PAUSE);
			cancel_advance = false;
		}
		if(amode == ADVANCE_SKIPLAG)
			amode = ADVANCE_AUTO;
		location_special = SPECIAL_NONE;
		update_movie_state();
	} else {
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;
		if(amode == ADVANCE_FRAME || amode == ADVANCE_SUBFRAME) {
			if(!cancel_advance) {
				window::wait_usec(advanced_once ? to_wait_frame(get_utime()) :
					(advance_timeout_first * 1000));
				advanced_once = true;
			}
			if(cancel_advance) {
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			window::paused(amode == ADVANCE_PAUSE);
		} else {
			window::paused((amode == ADVANCE_PAUSE));
			cancel_advance = false;
		}
		location_special = SPECIAL_FRAME_START;
		update_movie_state();

	}
	window::notify_screen_update();
	window::poll_inputs();
	if(!subframe && pending_reset_cycles >= 0)
		set_curcontrols_reset(pending_reset_cycles);
	else if(!subframe)
		set_curcontrols_reset(-1);
	controls_t tmp = get_current_controls(movb.get_movie().get_current_frame());
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
		amode = ADVANCE_LOAD;
		window::cancel_wait();
		window::paused(false);
	}

	void mark_pending_save(const std::string& filename, int smode)
	{
		if(smode == SAVE_MOVIE) {
			//Just do this immediately.
			do_save_movie(filename);
			return;
		}
		queued_saves.insert(filename);
		window::message("Pending save on '" + filename + "'");
	}

	class dump_watch : public av_snooper::dump_notification
	{
		void dump_starting(const std::string& n) throw()
		{
			update_movie_state();
		}
		void dump_ending(const std::string& n) throw()
		{
			update_movie_state();
		}
	} dumpwatch;

	uint32_t lpalette[0x80000];
	void init_palette()
	{
		static bool palette_init = false;
		if(palette_init)
			return;
		palette_init = true;
		for(unsigned i = 0; i < 0x80000; i++) {
			unsigned l = (i >> 15) & 0xF;
			unsigned r = (i >> 0) & 0x1F;
			unsigned g = (i >> 5) & 0x1F;
			unsigned b = (i >> 10) & 0x1F;
			double _l = static_cast<double>(l);
			double m = 17.0 / 31.0;
			r = floor(m * r * _l + 0.5);
			g = floor(m * g * _l + 0.5);
			b = floor(m * b * _l + 0.5);
			lpalette[i] = r * 65536 + g * 256 + b;
		}
	}
}

void update_movie_state()
{
	auto& _status = window::get_emustatus();
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
		_status["Frame"] = x.str();
	} else
		_status["Frame"] = "N/A";
#ifndef NO_TIME_INTERCEPT
	if(!system_corrupt) {
		time_t timevalue = static_cast<time_t>(our_movie.rtc_second);
		struct tm* time_decompose = gmtime(&timevalue);
		char datebuffer[512];
		strftime(datebuffer, 511, "%Y%m%d(%a)T%H%M%S", time_decompose);
		_status["RTC"] = datebuffer;
	} else {
		_status["RTC"] = "N/A";
	}
#endif
	{
		std::ostringstream x;
		x << (system_corrupt ? "C" : "-");
		x << (av_snooper::dump_in_progress() ? "D" : "-");
		x << (last_hires ? "H" : "-");
		x << (last_interlace ? "I" : "-");
		if(!system_corrupt)
			x << (movb.get_movie().readonly_mode() ? "P" : "R");
		else
			x << "-";
		_status["Flags"] = x.str();
	}
	if(save_jukebox.size() > 0)
		_status["Saveslot"] = save_jukebox[save_jukebox_pointer];
	else
		_status.erase("Saveslot");
	do_watch_memory();

	controls_t c;
	if(movb.get_movie().readonly_mode())
		c = movb.get_movie().get_controls();
	else
		c = get_current_controls(movb.get_movie().get_current_frame());
	for(unsigned i = 0; i < 8; i++) {
		unsigned pindex = controller_index_by_logical(i);
		unsigned port = pindex >> 2;
		unsigned dev = pindex & 3;
		auto ctype = controller_type_by_logical(i);
		std::ostringstream x;
		switch(ctype) {
		case DT_GAMEPAD:
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_LEFT) ? "l" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_RIGHT) ? "r" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_UP) ? "u" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_DOWN) ? "d" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_A) ? "A" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_B) ? "B" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_X) ? "X" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_Y) ? "Y" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_L) ? "L" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_R) ? "R" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_START) ? "S" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JOYPAD_SELECT) ? "s" : " ");
			break;
		case DT_MOUSE:
			x << c(port, dev, SNES_DEVICE_ID_MOUSE_X) << " ";
			x << c(port, dev, SNES_DEVICE_ID_MOUSE_Y) << " ";
			x << (c(port, dev, SNES_DEVICE_ID_MOUSE_LEFT) ? "L" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_MOUSE_RIGHT) ? "R" : " ");
			break;
		case DT_SUPERSCOPE:
			x << c(port, dev, SNES_DEVICE_ID_SUPER_SCOPE_X) << " ";
			x << c(port, dev, SNES_DEVICE_ID_SUPER_SCOPE_Y) << " ";
			x << (c(port, dev, SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER) ? "T" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_SUPER_SCOPE_CURSOR) ? "C" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_SUPER_SCOPE_TURBO) ? "t" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_SUPER_SCOPE_PAUSE) ? "P" : " ");
			break;
		case DT_JUSTIFIER:
			x << c(port, dev, SNES_DEVICE_ID_JUSTIFIER_X) << " ";
			x << c(port, dev, SNES_DEVICE_ID_JUSTIFIER_Y) << " ";
			x << (c(port, dev, SNES_DEVICE_ID_JUSTIFIER_START) ? "T" : " ");
			x << (c(port, dev, SNES_DEVICE_ID_JUSTIFIER_TRIGGER) ? "S" : " ");
			break;
		case DT_NONE:
			continue;
		}
		char y[3] = {'P', 0, 0};
		y[1] = 49 + i;
		_status[std::string(y)] = x.str();
	}
}


class my_interface : public SNES::Interface
{
	string path(SNES::Cartridge::Slot slot, const string &hint)
	{
		const char* _hint = hint;
		std::string _hint2 = _hint;
		std::string fwp = firmwarepath_setting;
		std::string finalpath = fwp + "/" + _hint2;
		return finalpath.c_str();
	}

	void videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan)
	{
		last_hires = hires;
		last_interlace = interlace;
		init_palette();
		if(stepping_into_save)
			window::message("Got video refresh in runtosave, expect desyncs!");
		video_refresh_done = true;
		bool region = (SNES::system.region() == SNES::System::Region::PAL);
		//std::cerr << "Frame: hires     flag is " << (hires ? "  " : "un") << "set." << std::endl;
		//std::cerr << "Frame: interlace flag is " << (interlace ? "  " : "un") << "set." << std::endl;
		//std::cerr << "Frame: overscan  flag is " << (overscan ? "  " : "un") << "set." << std::endl;
		//std::cerr << "Frame: region    flag is " << (region ? "  " : "un") << "set." << std::endl;
		lcscreen ls(data, hires, interlace, overscan, region);
		framebuffer = ls;
		location_special = SPECIAL_FRAME_VIDEO;
		update_movie_state();
		redraw_framebuffer();
		uint32_t fps_n, fps_d;
		uint32_t fclocks;
		if(region)
			fclocks = interlace ? DURATION_PAL_FIELD : DURATION_PAL_FRAME;
		else
			fclocks = interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
		fps_n = SNES::system.cpu_frequency();
		fps_d = fclocks;
		uint32_t g = gcd(fps_n, fps_d);
		fps_n /= g;
		fps_d /= g;
		av_snooper::_frame(ls, fps_n, fps_d, data, hires, interlace, overscan, region ? SNOOP_REGION_PAL :
			SNOOP_REGION_NTSC);
	}

	void audioSample(int16_t l_sample, int16_t r_sample)
	{
		uint16_t _l = l_sample;
		uint16_t _r = r_sample;
		window::play_audio_sample(_l + 32768, _r + 32768);
		av_snooper::_sample(l_sample, r_sample);
		//The SMP emits a sample every 768 ticks of its clock. Use this in order to keep track of time.
		our_movie.rtc_subsecond += 768;
		while(our_movie.rtc_subsecond >= SNES::system.apu_frequency()) {
			our_movie.rtc_second++;
			our_movie.rtc_subsecond -= SNES::system.apu_frequency();
		}
	}

	int16_t inputPoll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
	{
		int16_t x;
		x = movb.input_poll(port, index, id);
		//if(id == SNES_DEVICE_ID_JOYPAD_START)
		//	std::cerr << "bsnes polling for start on (" << port << "," << index << ")=" << x << std::endl;
		lua_callback_snoop_input(port ? 1 : 0, index, id, x);
		return x;
	}
};

namespace
{
	function_ptr_command<> count_rerecords("count-rerecords", "Count rerecords",
		"Syntax: count-rerecords\nCounts rerecords.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			std::vector<char> tmp;
			uint64_t x = rrdata::write(tmp);
			messages << x << " rerecord(s)" << std::endl;
		});

	function_ptr_command<const std::string&> quit_emulator("quit-emulator", "Quit the emulator",
		"Syntax: quit-emulator [/y]\nQuits emulator (/y => don't ask for confirmation).\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "/y" || window::modal_message("Really quit?", true)) {
				amode = ADVANCE_QUIT;
				window::paused(false);
				window::cancel_wait();
			}
		});

	function_ptr_command<> pause_emulator("pause-emulator", "(Un)pause the emulator",
		"Syntax: pause-emulator\n(Un)pauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(amode != ADVANCE_AUTO) {
				amode = ADVANCE_AUTO;
				window::paused(false);
				window::cancel_wait();
				window::message("Unpaused");
			} else {
				window::cancel_wait();
				cancel_advance = false;
				amode = ADVANCE_PAUSE;
				window::message("Paused");
			}
		});

	function_ptr_command<> save_jukebox_prev("cycle-jukebox-backward", "Cycle save jukebox backwards",
		"Syntax: cycle-jukebox-backwards\nCycle save jukebox backwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(save_jukebox_pointer == 0)
				save_jukebox_pointer = save_jukebox.size() - 1;
			else
				save_jukebox_pointer--;
			if(save_jukebox_pointer >= save_jukebox.size())
				save_jukebox_pointer = 0;
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<> save_jukebox_next("cycle-jukebox-forward", "Cycle save jukebox forwards",
		"Syntax: cycle-jukebox-forwards\nCycle save jukebox forwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(save_jukebox_pointer == save_jukebox.size() - 1)
				save_jukebox_pointer = 0;
			else
				save_jukebox_pointer++;
			if(save_jukebox_pointer >= save_jukebox.size())
				save_jukebox_pointer = 0;
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<arg_filename> add_jukebox("add-jukebox-save", "Add save to jukebox",
		"Syntax: add-jukebox-save\nAdd save to jukebox\n",
		[](arg_filename filename) throw(std::bad_alloc, std::runtime_error) {
			save_jukebox.push_back(filename);
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<> load_jukebox("load-jukebox", "Load save from jukebox",
		"Syntax: load-jukebox\nLoad save from jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(!save_jukebox.size())
				throw std::runtime_error("No saves in jukebox");
			mark_pending_load(save_jukebox[save_jukebox_pointer], LOAD_STATE_CURRENT);
		});

	function_ptr_command<> save_jukebox_c("save-jukebox", "Save save to jukebox",
		"Syntax: save-jukebox\nSave save to jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(!save_jukebox.size())
				throw std::runtime_error("No saves in jukebox");
			mark_pending_save(save_jukebox[save_jukebox_pointer], SAVE_STATE);
		});

	function_ptr_command<> padvance_frame("+advance-frame", "Advance one frame",
		"Syntax: +advance-frame\nAdvances the emulation by one frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_FRAME;
			cancel_advance = false;
			advanced_once = false;
			window::cancel_wait();
			window::paused(false);
		});

	function_ptr_command<> nadvance_frame("-advance-frame", "Advance one frame",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			cancel_advance = true;
			window::cancel_wait();
			window::paused(false);
		});

	function_ptr_command<> padvance_poll("+advance-poll", "Advance one subframe",
		"Syntax: +advance-poll\nAdvances the emulation by one subframe.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_SUBFRAME;
			cancel_advance = false;
			advanced_once = false;
			window::cancel_wait();
			window::paused(false);
		});

	function_ptr_command<> nadvance_poll("-advance-poll", "Advance one subframe",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			cancel_advance = true;
			window::cancel_wait();
			window::paused(false);
		});

	function_ptr_command<> advance_skiplag("advance-skiplag", "Skip to next poll",
		"Syntax: advance-skiplag\nAdvances the emulation to the next poll.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_SKIPLAG;
			window::cancel_wait();
			window::paused(false);
		});

	function_ptr_command<> reset_c("reset", "Reset the SNES",
		"Syntax: reset\nResets the SNES in beginning of the next frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			pending_reset_cycles = 0;
		});

	function_ptr_command<arg_filename> load_c("load", "Load savestate (current mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in current mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_CURRENT);
		});

	function_ptr_command<arg_filename> load_state_c("load-state", "Load savestate (R/W)",
		"Syntax: load-state <file>\nLoads SNES state from <file> in Read/Write mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RW);
		});

	function_ptr_command<arg_filename> load_readonly("load-readonly", "Load savestate (RO)",
		"Syntax: load-readonly <file>\nLoads SNES state from <file> in read-only mode\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RO);
		});

	function_ptr_command<arg_filename> load_preserve("load-preserve", "Load savestate (preserve input)",
		"Syntax: load-preserve <file>\nLoads SNES state from <file> preserving input\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_PRESERVE);
		});

	function_ptr_command<arg_filename> load_movie_c("load-movie", "Load movie",
		"Syntax: load-movie <file>\nLoads SNES movie from <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_MOVIE);
		});


	function_ptr_command<arg_filename> save_state("save-state", "Save state",
		"Syntax: save-state <file>\nSaves SNES state to <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE);
		});

	function_ptr_command<arg_filename> save_movie("save-movie", "Save movie",
		"Syntax: save-movie <file>\nSaves SNES movie to <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE);
		});

	function_ptr_command<> set_rwmode("set-rwmode", "Switch to read/write mode",
		"Syntax: set-rwmode\nSwitches to read/write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			movb.get_movie().readonly_mode(false);
			window_callback::do_mode_change(false);
			lua_callback_do_readwrite();
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<> set_romode("set-romode", "Switch to read-only mode",
		"Syntax: set-romode\nSwitches to read-only mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			movb.get_movie().readonly_mode(true);
			window_callback::do_mode_change(true);
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<> toggle_rwmode("toggle-rwmode", "Toggle read/write mode",
		"Syntax: toggle-rwmode\nToggles read/write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			bool c = movb.get_movie().readonly_mode();
			movb.get_movie().readonly_mode(!c);
			window_callback::do_mode_change(!c);
			if(c)
				lua_callback_do_readwrite();
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<> repaint("repaint", "Redraw the screen",
		"Syntax: repaint\nRedraws the screen\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			redraw_framebuffer();
		});



	function_ptr_command<> test1("test-1", "no description available", "No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			framebuffer = screen_nosignal;
			redraw_framebuffer();
		});

	function_ptr_command<> test2("test-2", "no description available", "No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			framebuffer = screen_corrupt;
			redraw_framebuffer();
		});

	function_ptr_command<> test3("test-3", "no description available", "No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			while(1);
		});


	bool on_quit_prompt = false;
	class mywindowcallbacks : public window_callback
	{
	public:
		void on_close() throw()
		{
			if(on_quit_prompt) {
				amode = ADVANCE_QUIT;
				window::paused(false);
				window::cancel_wait();
				return;
			}
			on_quit_prompt = true;
			try {
				if(window::modal_message("Really quit?", true)) {
					amode = ADVANCE_QUIT;
					window::paused(false);
					window::cancel_wait();
				}
			} catch(...) {
			}
			on_quit_prompt = false;
		}

		void on_click(int32_t x, int32_t y, uint32_t buttonmask) throw()
		{
			if(buttonmask & ~prev_mouse_mask & 1)
				send_analog_input(x, y, 0);
			if(buttonmask & ~prev_mouse_mask & 2)
				send_analog_input(x, y, 1);
			if(buttonmask & ~prev_mouse_mask & 4)
				send_analog_input(x, y, 2);
			prev_mouse_mask = buttonmask;
		}
	} mywcb;

	//If there is a pending load, perform it. Return 1 on successful load, 0 if nothing to load, -1 on load
	//failing.
	int handle_load()
	{
		if(pending_load != "") {
			system_corrupt = false;
			if(!do_load_state(pending_load, loadmode)) {
				pending_load = "";
				return -1;
			}
			redraw_framebuffer();
			pending_load = "";
			pending_reset_cycles = -1;
			amode = ADVANCE_AUTO;
			window::cancel_wait();
			window::paused(false);
			if(!system_corrupt) {
				location_special = SPECIAL_SAVEPOINT;
				update_movie_state();
				window::notify_screen_update();
				window::poll_inputs();
			}
			return 1;
		}
		return 0;
	}

	//If there are pending saves, perform them.
	void handle_saves()
	{
		if(!queued_saves.empty()) {
			stepping_into_save = true;
			SNES::system.runtosave();
			stepping_into_save = false;
			for(auto i : queued_saves)
				do_save_state(i);
		}
		queued_saves.clear();
	}

	//Do (delayed) reset. Return true if proper, false if forced at frame boundary.
	bool handle_reset(long cycles)
	{
		if(cycles < 0)
			return true;
		video_refresh_done = false;
		if(cycles == 0)
			window::message("SNES reset");
		else if(cycles > 0) {
			window::message("SNES delayed reset not implemented (doing immediate reset)");
			/* ... This code is just too buggy.
			long cycles_executed = 0;
			messages << "Executing delayed reset... This can take some time!" << std::endl;
			while(cycles_executed < cycles && !video_refresh_done) {
				//Poll inputs once in a while to prevent activating watchdog.
				if(cycles_executed % 100 == 0)
					window::poll_inputs();
				SNES::cpu.op_step();
				cycles_executed++;
			}
			if(!video_refresh_done)
				messages << "SNES reset (delayed " << cycles_executed << ")" << std::endl;
			else
				messages << "SNES reset (forced at " << cycles_executed << ")" << std::endl;
			*/
		}
		SNES::system.reset();
		framebuffer = screen_nosignal;
		lua_callback_do_reset();
		redraw_framebuffer();
		if(video_refresh_done) {
			to_wait_frame(get_utime());
			return false;
		}
		return true;
	}

	bool handle_corrupt()
	{
		if(!system_corrupt)
			return false;
		while(system_corrupt) {
			redraw_framebuffer();
			window::cancel_wait();
			window::paused(true);
			window::poll_inputs();
			handle_load();
			if(amode == ADVANCE_QUIT)
				return true;
		}
		return true;
	}

	void print_controller_mappings()
	{
		for(unsigned i = 0; i < 8; i++) {
			std::string type = "unknown";
			if(controller_type_by_logical(i) == DT_NONE)
				type = "disconnected";
			if(controller_type_by_logical(i) == DT_GAMEPAD)
				type = "gamepad";
			if(controller_type_by_logical(i) == DT_MOUSE)
				type = "mouse";
			if(controller_type_by_logical(i) == DT_SUPERSCOPE)
				type = "superscope";
			if(controller_type_by_logical(i) == DT_JUSTIFIER)
				type = "justifier";
			messages << "Physical controller mapping: Logical " << (i + 1) << " is physical " <<
				controller_index_by_logical(i) << " (" << type << ")" << std::endl;
		}
	}
}

void main_loop(struct loaded_rom& rom, struct moviefile& initial, bool load_has_to_succeed) throw(std::bad_alloc,
	std::runtime_error)
{
	//Basic initialization.
	init_special_screens();
	our_rom = &rom;
	my_interface intrf;
	auto old_inteface = SNES::interface;
	SNES::interface = &intrf;
	intrf.initialize(&intrf);
	status = &window::get_emustatus();

	//Load our given movie.
	bool first_round = false;
	bool just_did_loadstate = false;
	try {
		do_load_state(initial, LOAD_STATE_DEFAULT);
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
			window::fatal_error();
		}
		system_corrupt = true;
		update_movie_state();
		framebuffer = screen_corrupt;
		redraw_framebuffer();
	}

	lua_callback_startup();

	//print_controller_mappings();
	window::set_main_surface(main_screen);
	redraw_framebuffer();
	window::paused(false);
	amode = ADVANCE_PAUSE;
	while(amode != ADVANCE_QUIT || !queued_saves.empty()) {
		if(handle_corrupt()) {
			first_round = our_movie.is_savestate;
			just_did_loadstate = first_round;
			continue;
		}
		long resetcycles = -1;
		ack_frame_tick(get_utime());
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;

		if(!first_round) {
			resetcycles = movb.new_frame_starting(amode == ADVANCE_SKIPLAG);
			if(amode == ADVANCE_QUIT && queued_saves.empty())
				break;
			bool delayed_reset = (resetcycles > 0);
			pending_reset_cycles = -1;
			if(!handle_reset(resetcycles)) {
				continue;
			}
			if(!delayed_reset) {
				handle_saves();
			}
			int r = 0;
			if(queued_saves.empty())
				r = handle_load();
			if(r > 0 || system_corrupt) {
				first_round = our_movie.is_savestate;
				amode = ADVANCE_PAUSE;
				just_did_loadstate = first_round;
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
			amode = ADVANCE_PAUSE;
			redraw_framebuffer();
			window::cancel_wait();
			window::paused(true);
			window::poll_inputs();
			//We already have done the reset this frame if we are going to do one at all.
			movb.get_movie().set_controls(get_current_controls(movb.get_movie().get_current_frame()));
			just_did_loadstate = false;
		}
		SNES::system.run();
		if(amode == ADVANCE_AUTO)
			window::wait_usec(to_wait_frame(get_utime()));
		first_round = false;
	}
	av_snooper::_end();
	SNES::interface = old_inteface;
}
