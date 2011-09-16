#include "mainloop.hpp"
#include "avsnoop.hpp"
#include "command.hpp"
#include "controller.hpp"
#include "framebuffer.hpp"
#include "moviedata.hpp"
#include <iomanip>
#include "framerate.hpp"
#include "memorywatch.hpp"
#include "lua.hpp"
#include "rrdata.hpp"
#include "rom.hpp"
#include "movie.hpp"
#include "moviefile.hpp"
#include "render.hpp"
#include "window.hpp"
#include "settings.hpp"
#include "rom.hpp"
#include "movie.hpp"
#include "window.hpp"
#include <cassert>
#include <sstream>
#include "memorymanip.hpp"
#include <iostream>
#include <set>
#include "lsnes.hpp"
#include <sys/time.h>
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>
#include "framerate.hpp"

#define SPECIAL_FRAME_START 0
#define SPECIAL_FRAME_VIDEO 1
#define SPECIAL_SAVEPOINT 2
#define SPECIAL_NONE 3

#define BUTTON_LEFT 0		//Gamepad
#define BUTTON_RIGHT 1		//Gamepad
#define BUTTON_UP 2		//Gamepad
#define BUTTON_DOWN 3		//Gamepad
#define BUTTON_A 4		//Gamepad
#define BUTTON_B 5		//Gamepad
#define BUTTON_X 6		//Gamepad
#define BUTTON_Y 7		//Gamepad
#define BUTTON_L 8		//Gamepad & Mouse
#define BUTTON_R 9		//Gamepad & Mouse
#define BUTTON_SELECT 10	//Gamepad
#define BUTTON_START 11		//Gamepad & Justifier
#define BUTTON_TRIGGER 12	//Superscope.
#define BUTTON_CURSOR 13	//Superscope & Justifier
#define BUTTON_PAUSE 14		//Superscope
#define BUTTON_TURBO 15		//Superscope

void update_movie_state();

namespace
{
	enum advance_mode
	{
		ADVANCE_QUIT,			//Quit the emulator.
		ADVANCE_AUTO,			//Normal (possibly slowed down play).
		ADVANCE_FRAME,			//Frame advance.
		ADVANCE_SUBFRAME,		//Subframe advance.
		ADVANCE_SKIPLAG,		//Skip lag (oneshot, reverts to normal).
		ADVANCE_SKIPLAG_PENDING,	//Activate skip lag mode at next frame.
		ADVANCE_PAUSE,			//Unconditional pause.
	};

	//Memory watches.
	std::map<std::string, std::string> memory_watches;
	//Previous mouse mask.
	int prev_mouse_mask = 0;
	//Flags related to repeating advance.
	bool advanced_once;
	bool cancel_advance;
	//Handle to the graphics system.
	window* win;
	//Emulator advance mode. Detemines pauses at start of frame / subframe, etc..
	enum advance_mode amode;
	//Mode and filename of pending load, one of LOAD_* constants.
	int loadmode;
	std::string pending_load;
	//Queued saves (all savestates).
	std::set<std::string> queued_saves;
	bool stepping_into_save;
	//Current controls.
	controls_t curcontrols;
	controls_t autoheld_controls;
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

	void send_analog_input(int32_t x, int32_t y, unsigned index)
	{
		if(controller_ismouse_by_analog(index)) {
			x -= 256;
			y -= (framebuffer.height / 2);
		} else {
			x /= 2;
			y /= 2;
		}
		int aindex = controller_index_by_analog(index);
		if(aindex < 0) {
			out(win) << "No analog controller in slot #" << (index + 1) << std::endl;
			return;
		}
		curcontrols(aindex >> 2, aindex & 3, 0) = x;
		curcontrols(aindex >> 2, aindex & 3, 1) = y;
	}

}

class firmware_path_setting : public setting
{
public:
	firmware_path_setting() : setting("firmwarepath") { _firmwarepath = "./"; default_firmware = true; }
	void blank() throw(std::bad_alloc, std::runtime_error)
	{
		_firmwarepath = "./";
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
		redraw_framebuffer(win);

	if(subframe) {
		if(amode == ADVANCE_SUBFRAME) {
			if(!cancel_advance && !advanced_once) {
				win->wait_msec(advance_timeout_first);
				advanced_once = true;
			}
			if(cancel_advance) {
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			win->paused(amode == ADVANCE_PAUSE);
		} else if(amode == ADVANCE_FRAME) {
			;
		} else {
			win->paused(amode == ADVANCE_SKIPLAG || amode == ADVANCE_PAUSE);
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
				win->wait_msec(advanced_once ? to_wait_frame(get_ticks_msec()) :
					advance_timeout_first);
				advanced_once = true;
			}
			if(cancel_advance) {
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			win->paused(amode == ADVANCE_PAUSE);
		} else {
			win->paused((amode == ADVANCE_PAUSE));
			cancel_advance = false;
		}
		location_special = SPECIAL_FRAME_START;
		update_movie_state();
	}
	win->notify_screen_update();
	win->poll_inputs();
	if(!subframe && pending_reset_cycles >= 0) {
		curcontrols(CONTROL_SYSTEM_RESET) = 1;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_HI) = pending_reset_cycles / 10000;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_LO) = pending_reset_cycles % 10000;
	} else if(!subframe) {
		curcontrols(CONTROL_SYSTEM_RESET) = 0;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_HI) = 0;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_LO) = 0;
	}
	controls_t tmp = curcontrols ^ autoheld_controls;
	lua_callback_do_input(tmp, subframe, win);
	return tmp;
}

namespace
{
	std::map<std::string, std::pair<unsigned, unsigned>> buttonmap;

	const char* buttonnames[] = {
		"left", "right", "up", "down", "A", "B", "X", "Y", "L", "R", "select", "start", "trigger", "cursor",
		"pause", "turbo"
	};

	void init_buttonmap()
	{
		static int done = 0;
		if(done)
			return;
		for(unsigned i = 0; i < 8; i++)
			for(unsigned j = 0; j < sizeof(buttonnames) / sizeof(buttonnames[0]); j++) {
				std::ostringstream x;
				x << (i + 1) << buttonnames[j];
				buttonmap[x.str()] = std::make_pair(i, j);
			}
		done = 1;
	}

	//Do button action.
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor = false)
	{
		enum devicetype_t p = controller_type_by_logical(ui_id);
		int x = controller_index_by_logical(ui_id);
		int bid = -1;
		switch(p) {
		case DT_NONE:
			out(win) << "No such controller #" << (ui_id + 1) << std::endl;
			return;
		case DT_GAMEPAD:
			switch(button) {
			case BUTTON_UP: 	bid = SNES_DEVICE_ID_JOYPAD_UP; break;
			case BUTTON_DOWN:	bid = SNES_DEVICE_ID_JOYPAD_DOWN; break;
			case BUTTON_LEFT:	bid = SNES_DEVICE_ID_JOYPAD_LEFT; break;
			case BUTTON_RIGHT:	bid = SNES_DEVICE_ID_JOYPAD_RIGHT; break;
			case BUTTON_A:		bid = SNES_DEVICE_ID_JOYPAD_A; break;
			case BUTTON_B:		bid = SNES_DEVICE_ID_JOYPAD_B; break;
			case BUTTON_X:		bid = SNES_DEVICE_ID_JOYPAD_X; break;
			case BUTTON_Y:		bid = SNES_DEVICE_ID_JOYPAD_Y; break;
			case BUTTON_L:		bid = SNES_DEVICE_ID_JOYPAD_L; break;
			case BUTTON_R:		bid = SNES_DEVICE_ID_JOYPAD_R; break;
			case BUTTON_SELECT:	bid = SNES_DEVICE_ID_JOYPAD_SELECT; break;
			case BUTTON_START:	bid = SNES_DEVICE_ID_JOYPAD_START; break;
			default:
				out(win) << "Invalid button for gamepad" << std::endl;
				return;
			};
			break;
		case DT_MOUSE:
			switch(button) {
			case BUTTON_L:		bid = SNES_DEVICE_ID_MOUSE_LEFT; break;
			case BUTTON_R:		bid = SNES_DEVICE_ID_MOUSE_RIGHT; break;
			default:
				out(win) << "Invalid button for mouse" << std::endl;
				return;
			};
			break;
		case DT_JUSTIFIER:
			switch(button) {
			case BUTTON_START:	bid = SNES_DEVICE_ID_JUSTIFIER_START; break;
			case BUTTON_TRIGGER:	bid = SNES_DEVICE_ID_JUSTIFIER_TRIGGER; break;
			default:
				out(win) << "Invalid button for justifier" << std::endl;
				return;
			};
			break;
		case DT_SUPERSCOPE:
			switch(button) {
			case BUTTON_TRIGGER:	bid = SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER; break;
			case BUTTON_CURSOR:	bid = SNES_DEVICE_ID_SUPER_SCOPE_CURSOR; break;
			case BUTTON_PAUSE:	bid = SNES_DEVICE_ID_SUPER_SCOPE_PAUSE; break;
			case BUTTON_TURBO:	bid = SNES_DEVICE_ID_SUPER_SCOPE_TURBO; break;
			default:
				out(win) << "Invalid button for superscope" << std::endl;
				return;
			};
			break;
		};
		if(do_xor)
			autoheld_controls((x & 4) ? 1 : 0, x & 3, bid) ^= newstate;
		else
			curcontrols((x & 4) ? 1 : 0, x & 3, bid) = newstate;
	}


	//Do pending load (automatically unpauses).
	void mark_pending_load(const std::string& filename, int lmode)
	{
		loadmode = lmode;
		pending_load = filename;
		amode = ADVANCE_AUTO;
		win->cancel_wait();
		win->paused(false);
	}

	//Mark pending save (movies save immediately).
	void mark_pending_save(const std::string& filename, int smode)
	{
		if(smode == SAVE_MOVIE) {
			//Just do this immediately.
			do_save_movie(win, filename);
			return;
		}
		queued_saves.insert(filename);
		win->message("Pending save on '" + filename + "'");
	}

	class dump_watch : public av_snooper::dump_notification
	{
		void dump_starting() throw()
		{
			update_movie_state();
		}
		void dump_ending() throw()
		{
			update_movie_state();
		}
	} dumpwatch;
}

void update_movie_state()
{
	auto& _status = win->get_emustatus();
	{
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
	}
	{
		std::ostringstream x;
		if(movb.get_movie().readonly_mode())
			x << "PLAY ";
		else
			x << "REC ";
		if(av_snooper::dump_in_progress())
			x << "CAP ";
		_status["Flags"] = x.str();
	}
	for(auto i = memory_watches.begin(); i != memory_watches.end(); i++) {
		try {
			_status["M[" + i->first + "]"] = evaluate_watch(i->second);
		} catch(...) {
		}
	}
	controls_t c;
	if(movb.get_movie().readonly_mode())
		c = movb.get_movie().get_controls();
	else
		c = curcontrols ^ autoheld_controls;
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
		return static_cast<std::string>(firmwarepath_setting).c_str();
	}

	void video_refresh(const uint16_t *data, bool hires, bool interlace, bool overscan)
	{
		if(stepping_into_save)
			win->message("Got video refresh in runtosave, expect desyncs!");
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
		redraw_framebuffer(win);
		uint32_t fps_n, fps_d;
		if(region) {
			fps_n = 322445;
			fps_d = 6448;
		} else {
			fps_n = 10738636;
			fps_d = 178683;
		}
		av_snooper::frame(ls, fps_n, fps_d, win);
	}

	void audio_sample(int16_t l_sample, int16_t r_sample)
	{
		uint16_t _l = l_sample;
		uint16_t _r = r_sample;
		win->play_audio_sample(_l + 32768, _r + 32768);
		av_snooper::sample(_l, _r, win);
	}

	void audio_sample(uint16_t l_sample, uint16_t r_sample)
	{
		//Yes, this interface is broken. The samples are signed but are passed as unsigned!
		win->play_audio_sample(l_sample + 32768, r_sample + 32768);
		av_snooper::sample(l_sample, r_sample, win);
	}

	int16_t input_poll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
	{
		int16_t x;
		x = movb.input_poll(port, index, id);
		//if(id == SNES_DEVICE_ID_JOYPAD_START)
		//	std::cerr << "bsnes polling for start on (" << port << "," << index << ")=" << x << std::endl;
		lua_callback_snoop_input(port ? 1 : 0, index, id, x, win);
		return x;
	}
};

namespace
{
	class quit_emulator_cmd : public command
	{
	public:
		quit_emulator_cmd() throw(std::bad_alloc) : command("quit-emulator") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "/y" || win->modal_message("Really quit?", true)) {
				amode = ADVANCE_QUIT;
				win->paused(false);
				win->cancel_wait();
			}
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Quit the emulator"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: quit-emulator [/y]\n"
				"Quits emulator (/y => don't ask for confirmation).\n";
		}
	} quitemu;

	class pause_emulator_cmd : public command
	{
	public:
		pause_emulator_cmd() throw(std::bad_alloc) : command("pause-emulator") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			if(amode != ADVANCE_AUTO) {
				amode = ADVANCE_AUTO;
				win->paused(false);
				win->cancel_wait();
				win->message("Unpaused");
			} else {
				win->cancel_wait();
				cancel_advance = false;
				amode = ADVANCE_PAUSE;
				win->message("Paused");
			}
		}
		std::string get_short_help() throw(std::bad_alloc) { return "(Un)pause the emulator"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: pause-emulator\n"
				"(Un)pauses the emulator.\n";
		}
	} pauseemu;

	class padvance_frame_cmd : public command
	{
	public:
		padvance_frame_cmd() throw(std::bad_alloc) : command("+advance-frame") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			amode = ADVANCE_FRAME;
			cancel_advance = false;
			advanced_once = false;
			win->cancel_wait();
			win->paused(false);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Advance one frame"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: +advance-frame\n"
				"Advances the emulation by one frame.\n";
		}
	} padvancef;

	class nadvance_frame_cmd : public command
	{
	public:
		nadvance_frame_cmd() throw(std::bad_alloc) : command("-advance-frame") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			cancel_advance = true;
			win->cancel_wait();
			win->paused(false);
		}
	} nadvancef;

	class padvance_poll_cmd : public command
	{
	public:
		padvance_poll_cmd() throw(std::bad_alloc) : command("+advance-poll") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			amode = ADVANCE_SUBFRAME;
			cancel_advance = false;
			advanced_once = false;
			win->cancel_wait();
			win->paused(false);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Advance one subframe"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: +advance-poll\n"
				"Advances the emulation by one subframe.\n";
		}
	} padvancep;

	class nadvance_poll_cmd : public command
	{
	public:
		nadvance_poll_cmd() throw(std::bad_alloc) : command("-advance-poll") {}

		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			cancel_advance = true;
			win->cancel_wait();
			win->paused(false);
		}
	} nadvancep;

	class advance_skiplag_cmd : public command
	{
	public:
		advance_skiplag_cmd() throw(std::bad_alloc) : command("advance-skiplag") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			amode = ADVANCE_SKIPLAG;
			win->cancel_wait();
			win->paused(false);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Skip to next poll"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: advance-skiplag\n"
				"Advances the emulation to the next poll.\n";
		}
	} skiplagc;

	class reset_cmd : public command
	{
	public:
		reset_cmd() throw(std::bad_alloc) : command("reset") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			pending_reset_cycles = 0;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Reset the SNES"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: reset\n"
				"Resets the SNES in beginning of the next frame.\n";
		}
	} resetc;

	class load_state_cmd : public command
	{
	public:
		load_state_cmd() throw(std::bad_alloc) : command("load-state") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			mark_pending_load(args, LOAD_STATE_RW);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Load state"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: load-state <file>\n"
				"Loads SNES state from <file> in Read/Write mode\n";
		}
	} loadstatec;

	class load_readonly_cmd : public command
	{
	public:
		load_readonly_cmd() throw(std::bad_alloc) : command("load-readonly") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			mark_pending_load(args, LOAD_STATE_RO);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Load state"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: load-readonly <file>\n"
				"Loads SNES state from <file> in Read-only mode\n";
		}
	} loadreadonlyc;

	class load_preserve_cmd : public command
	{
	public:
		load_preserve_cmd() throw(std::bad_alloc) : command("load-preserve") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			mark_pending_load(args, LOAD_STATE_PRESERVE);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Load state"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: load-preserve <file>\n"
				"Loads SNES state from <file> preserving input\n";
		}
	} loadpreservec;

	class load_movie_cmd : public command
	{
	public:
		load_movie_cmd() throw(std::bad_alloc) : command("load-movie") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			mark_pending_load(args, LOAD_STATE_MOVIE);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Load movie"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: load-movie <file>\n"
				"Loads movie from <file>\n";
		}
	} loadmoviec;

	class save_state_cmd : public command
	{
	public:
		save_state_cmd() throw(std::bad_alloc) : command("save-state") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			mark_pending_save(args, SAVE_STATE);
		}

		std::string get_short_help() throw(std::bad_alloc) { return "Save state"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: save-state <file>\n"
				"Saves SNES state to <file>\n";
		}
	} savestatec;

	class save_movie_cmd : public command
	{
	public:
		save_movie_cmd() throw(std::bad_alloc) : command("save-movie") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			mark_pending_save(args, SAVE_MOVIE);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Save movie"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: save-movie <file>\n"
				"Saves movie to <file>\n";
		}
	} savemoviec;

	class set_rwmode_cmd : public command
	{
	public:
		set_rwmode_cmd() throw(std::bad_alloc) : command("set-rwmode") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			movb.get_movie().readonly_mode(false);
			lua_callback_do_readwrite(win);
			update_movie_state();
			win->notify_screen_update();
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Switch to read/write mode"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: set-rwmode\n"
				"Switches to read/write mode\n";
		}
	} setrwc;

	class repainter : public command
	{
	public:
		repainter() throw(std::bad_alloc) : command("repaint") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			redraw_framebuffer(win);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Redraw the screen"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: repaint\n"
				"Redraws the screen\n";
		}
	} repaintc;

	class set_gamename_cmd : public command
	{
	public:
		set_gamename_cmd() throw(std::bad_alloc) : command("set-gamename") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			our_movie.gamename = args;
			out(win) << "Game name changed to '" << our_movie.gamename << "'" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Set the game name"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: set-gamename <name>\n"
				"Sets the game name to <name>\n";
		}
	} setnamec;

	class add_watch_command : public command
	{
	public:
		add_watch_command() throw(std::bad_alloc) : command("add-watch") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			std::string name = t;
			if(name == "" || t.tail() == "")
				throw std::runtime_error("syntax: add-watch <name> <expr>");
			std::cerr << "Add watch: '" << name << "'" << std::endl;
			memory_watches[name] = t.tail();
			update_movie_state();
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Add a memory watch"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: add-watch <name> <expression>\n"
				"Adds a new memory watch\n";
		}
	} addwatchc;

	class remove_watch_command : public command
	{
	public:
		remove_watch_command() throw(std::bad_alloc) : command("remove-watch") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			std::string name = t;
			if(name == "" || t.tail() != "") {
				out(win) << "syntax: remove-watch <name>" << std::endl;
				return;
			}
			std::cerr << "Erase watch: '" << name << "'" << std::endl;
			memory_watches.erase(name);
			auto& _status = win->get_emustatus();
			_status.erase("M[" + name + "]");
			update_movie_state();		}
		std::string get_short_help() throw(std::bad_alloc) { return "Remove a memory watch"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: remove-watch <name>\n"
				"Removes a memory watch\n";
		}
	} removewatchc;

	class test_1 : public command
	{
	public:
		test_1() throw(std::bad_alloc) : command("test-1") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			framebuffer = screen_nosignal;
			redraw_framebuffer(win);
		}
	} test1c;

	class test_2 : public command
	{
	public:
		test_2() throw(std::bad_alloc) : command("test-2") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			framebuffer = screen_corrupt;
			redraw_framebuffer(win);
		}
	} test2c;

	class test_3 : public command
	{
	public:
		test_3() throw(std::bad_alloc) : command("test-3") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			while(1);
		}
	} test3c;

	class screenshot_command : public command
	{
	public:
		screenshot_command() throw(std::bad_alloc) : command("take-screenshot") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args == "")
				throw std::runtime_error("Filename required");
			framebuffer.save_png(args);
			out(win) << "Saved PNG screenshot" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Takes a screenshot"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: take-screenshot <file>\n"
				"Saves screenshot to PNG file <file>\n";
		}
	} screenshotc;

	class mouse_button_handler : public command
	{
	public:
		mouse_button_handler() throw(std::bad_alloc) : command("mouse_button") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			std::string x = t;
			std::string y = t;
			std::string b = t;
			int _x = atoi(x.c_str());
			int _y = atoi(y.c_str());
			int _b = atoi(b.c_str());
			if(_b & ~prev_mouse_mask & 1)
				send_analog_input(_x, _y, 0);
			if(_b & ~prev_mouse_mask & 2)
				send_analog_input(_x, _y, 1);
			if(_b & ~prev_mouse_mask & 4)
				send_analog_input(_x, _y, 2);
			prev_mouse_mask = _b;
		}
	} mousebuttonh;

	class button_action : public command
	{
	public:
		button_action(const std::string& cmd, int _type, unsigned _controller, std::string _button)
			throw(std::bad_alloc)
			: command(cmd)
		{
			commandn = cmd;
			type = _type;
			controller = _controller;
			button = _button;
		}
		~button_action() throw() {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			init_buttonmap();
			if(!buttonmap.count(button))
				return;
			auto i = buttonmap[button];
			do_button_action(i.first, i.second, (type != 1) ? 1 : 0, (type == 2));
			update_movie_state();
			win->notify_screen_update();
		}
		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Press/Unpress button";
		}
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + commandn + "\n"
				"Presses/Unpresses button\n";
		}
		std::string commandn;
		unsigned controller;
		int type;
		std::string button;
	};

	class button_action_helper
	{
	public:
		button_action_helper()
		{
			for(size_t i = 0; i < sizeof(buttonnames) / sizeof(buttonnames[0]); ++i)
				for(int j = 0; j < 3; ++j)
					for(unsigned k = 0; k < 8; ++k) {
						std::ostringstream x, y;
						switch(j) {
						case 0:
							x << "+controller";
							break;
						case 1:
							x << "-controller";
							break;
						case 2:
							x << "controllerh";
							break;
						};
						x << (k + 1);
						x << buttonnames[i];
						y << (k + 1);
						y << buttonnames[i];
						new button_action(x.str(), j, k, y.str());
					}
		}
	} bah;

	//If there is a pending load, perform it.
	bool handle_load()
	{
		if(pending_load != "") {
			do_load_state(win, pending_load, loadmode);
			redraw_framebuffer(win);
			pending_load = "";
			pending_reset_cycles = -1;
			amode = ADVANCE_AUTO;
			win->cancel_wait();
			win->paused(false);
			if(!system_corrupt) {
				location_special = SPECIAL_SAVEPOINT;
				update_movie_state();
				win->notify_screen_update();
				win->poll_inputs();
			}
			return true;
		}
		return false;
	}

	//If there are pending saves, perform them.
	void handle_saves()
	{
		if(!queued_saves.empty()) {
			stepping_into_save = true;
			SNES::system.runtosave();
			stepping_into_save = false;
			for(auto i = queued_saves.begin(); i != queued_saves.end(); i++)
				do_save_state(win, *i);
		}
		queued_saves.clear();
	}

	//Do (delayed) reset. Return true if proper, false if forced at frame boundary.
	bool handle_reset(long cycles)
	{
		if(cycles == 0) {
			win->message("SNES reset");
			SNES::system.reset();
			framebuffer = screen_nosignal;
			lua_callback_do_reset(win);
			redraw_framebuffer(win);
		} else if(cycles > 0) {
			video_refresh_done = false;
			long cycles_executed = 0;
			out(win) << "Executing delayed reset... This can take some time!" << std::endl;
			while(cycles_executed < cycles && !video_refresh_done) {
				SNES::cpu.op_step();
				cycles_executed++;
			}
			if(!video_refresh_done)
				out(win) << "SNES reset (delayed " << cycles_executed << ")" << std::endl;
			else
				out(win) << "SNES reset (forced at " << cycles_executed << ")" << std::endl;
			SNES::system.reset();
			framebuffer = screen_nosignal;
			lua_callback_do_reset(win);
			redraw_framebuffer(win);
			if(video_refresh_done) {
				to_wait_frame(get_ticks_msec());
				return false;
			}
		}
		return true;
	}

	bool handle_corrupt()
	{
		if(!system_corrupt)
			return false;
		while(system_corrupt) {
			redraw_framebuffer(win);
			win->cancel_wait();
			win->paused(true);
			win->poll_inputs();
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
			out(win) << "Physical controller mapping: Logical " << (i + 1) << " is physical " <<
				controller_index_by_logical(i) << " (" << type << ")" << std::endl;
		}
	}
}

void main_loop(window* _win, struct loaded_rom& rom, struct moviefile& initial) throw(std::bad_alloc,
	std::runtime_error)
{
	//Basic initialization.
	win = _win;
	init_special_screens();
	our_rom = &rom;
	my_interface intrf;
	auto old_inteface = SNES::system.interface;
	SNES::system.interface = &intrf;
	status = &win->get_emustatus();

	//Load our given movie.
	bool first_round = false;
	bool just_did_loadstate = false;
	try {
		do_load_state(win, initial, LOAD_STATE_DEFAULT);
		first_round = our_movie.is_savestate;
		just_did_loadstate = first_round;
	} catch(std::bad_alloc& e) {
		OOM_panic(win);
	} catch(std::exception& e) {
		win->message(std::string("FATAL: Can't load initial state: ") + e.what());
		win->fatal_error();
		return;
	}

	lua_callback_startup(win);

	//print_controller_mappings();
	av_snooper::add_dump_notifier(dumpwatch);
	win->set_main_surface(main_screen);
	redraw_framebuffer(win);
	win->paused(false);
	amode = ADVANCE_PAUSE;
	while(amode != ADVANCE_QUIT) {
		if(handle_corrupt()) {
			first_round = our_movie.is_savestate;
			just_did_loadstate = true;
			continue;
		}
		long resetcycles = -1;
		ack_frame_tick(get_ticks_msec());
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;

		if(!first_round) {
			resetcycles = movb.new_frame_starting(amode == ADVANCE_SKIPLAG);
			if(amode == ADVANCE_QUIT)
				break;
			bool delayed_reset = (resetcycles > 0);
			pending_reset_cycles = -1;
			if(!handle_reset(resetcycles)) {
				continue;
			}
			if(!delayed_reset) {
				handle_saves();
			}
			if(handle_load()) {
				first_round = our_movie.is_savestate;
				amode = ADVANCE_PAUSE;
				just_did_loadstate = first_round;
				continue;
			}
		}
		if(just_did_loadstate) {
			if(amode == ADVANCE_QUIT)
				break;
			amode = ADVANCE_PAUSE;
			redraw_framebuffer(win);
			win->cancel_wait();
			win->paused(true);
			win->poll_inputs();
			just_did_loadstate = false;
		}
		SNES::system.run();
		if(amode == ADVANCE_AUTO)
			win->wait_msec(to_wait_frame(get_ticks_msec()));
		first_round = false;
	}
	av_snooper::end(win);
	SNES::system.interface = old_inteface;
}
