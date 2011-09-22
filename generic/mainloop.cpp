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
	std::vector<controls_t> autofire_pattern;
	size_t autofire_position;
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
			messages << "No analog controller in slot #" << (index + 1) << std::endl;
			return;
		}
		curcontrols(aindex >> 2, aindex & 3, 0) = x;
		curcontrols(aindex >> 2, aindex & 3, 1) = y;
	}

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
				window::wait_msec(advance_timeout_first);
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
		autofire_position = (autofire_position + 1) % autofire_pattern.size();
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;
		if(amode == ADVANCE_FRAME || amode == ADVANCE_SUBFRAME) {
			if(!cancel_advance) {
				window::wait_msec(advanced_once ? to_wait_frame(get_ticks_msec()) :
					advance_timeout_first);
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
	if(!subframe && pending_reset_cycles >= 0) {
		curcontrols(CONTROL_SYSTEM_RESET) = 1;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_HI) = pending_reset_cycles / 10000;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_LO) = pending_reset_cycles % 10000;
	} else if(!subframe) {
		curcontrols(CONTROL_SYSTEM_RESET) = 0;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_HI) = 0;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_LO) = 0;
	}
	controls_t tmp = curcontrols ^ autoheld_controls ^ autofire_pattern[autofire_position];
	lua_callback_do_input(tmp, subframe);
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
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor, controls_t& c)
	{
		enum devicetype_t p = controller_type_by_logical(ui_id);
		int x = controller_index_by_logical(ui_id);
		int bid = -1;
		switch(p) {
		case DT_NONE:
			messages << "No such controller #" << (ui_id + 1) << std::endl;
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
				messages << "Invalid button for gamepad" << std::endl;
				return;
			};
			break;
		case DT_MOUSE:
			switch(button) {
			case BUTTON_L:		bid = SNES_DEVICE_ID_MOUSE_LEFT; break;
			case BUTTON_R:		bid = SNES_DEVICE_ID_MOUSE_RIGHT; break;
			default:
				messages << "Invalid button for mouse" << std::endl;
				return;
			};
			break;
		case DT_JUSTIFIER:
			switch(button) {
			case BUTTON_START:	bid = SNES_DEVICE_ID_JUSTIFIER_START; break;
			case BUTTON_TRIGGER:	bid = SNES_DEVICE_ID_JUSTIFIER_TRIGGER; break;
			default:
				messages << "Invalid button for justifier" << std::endl;
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
				messages << "Invalid button for superscope" << std::endl;
				return;
			};
			break;
		};
		if(do_xor)
			c((x & 4) ? 1 : 0, x & 3, bid) ^= newstate;
		else
			c((x & 4) ? 1 : 0, x & 3, bid) = newstate;
	}

	//Do button action.
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor = false)
	{
		if(do_xor)
			do_button_action(ui_id, button, newstate, do_xor, autoheld_controls);
		else
			do_button_action(ui_id, button, newstate, do_xor, curcontrols);
	}


	//Do pending load (automatically unpauses).
	void mark_pending_load(const std::string& filename, int lmode)
	{
		loadmode = lmode;
		pending_load = filename;
		amode = ADVANCE_AUTO;
		window::cancel_wait();
		window::paused(false);
	}

	//Mark pending save (movies save immediately).
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
	} else {
		_status["Frame"] = "N/A";
	}
	{
		std::ostringstream x;
		if(system_corrupt)
			x << "CORRUPT ";
		else if(movb.get_movie().readonly_mode())
			x << "PLAY ";
		else
			x << "REC ";
		if(av_snooper::dump_in_progress())
			x << "CAP ";
		_status["Flags"] = x.str();
	}
	if(save_jukebox.size() > 0)
		_status["Saveslot"] = save_jukebox[save_jukebox_pointer];
	else
		_status.erase("Saveslot");
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
		const char* _hint = hint;
		std::string _hint2 = _hint;
		std::string fwp = firmwarepath_setting;
		std::string finalpath = fwp + "/" + _hint2;
		return finalpath.c_str();
	}

	void video_refresh(const uint16_t *data, bool hires, bool interlace, bool overscan)
	{
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
		if(region) {
			fps_n = 322445;
			fps_d = 6448;
		} else {
			fps_n = 10738636;
			fps_d = 178683;
		}
		av_snooper::frame(ls, fps_n, fps_d, true);
	}

	void audio_sample(int16_t l_sample, int16_t r_sample)
	{
		uint16_t _l = l_sample;
		uint16_t _r = r_sample;
		window::play_audio_sample(_l + 32768, _r + 32768);
		av_snooper::sample(_l, _r, true);
	}

	void audio_sample(uint16_t l_sample, uint16_t r_sample)
	{
		//Yes, this interface is broken. The samples are signed but are passed as unsigned!
		window::play_audio_sample(l_sample + 32768, r_sample + 32768);
		av_snooper::sample(l_sample, r_sample, true);
	}

	int16_t input_poll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
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
			mark_pending_load(save_jukebox[save_jukebox_pointer], LOAD_STATE_RW);
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
			lua_callback_do_readwrite();
			update_movie_state();
			window::notify_screen_update();
		});

	function_ptr_command<> repaint("repaint", "Redraw the screen",
		"Syntax: repaint\nRedraws the screen\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			redraw_framebuffer();
		});

	function_ptr_command<tokensplitter&> add_watch("add-watch", "Add a memory watch",
		"Syntax: add-watch <name> <expression>\nAdds a new memory watch\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			std::string name = t;
			if(name == "" || t.tail() == "")
				throw std::runtime_error("syntax: add-watch <name> <expr>");
			std::cerr << "Add watch: '" << name << "'" << std::endl;
			memory_watches[name] = t.tail();
			update_movie_state();
		});

	function_ptr_command<tokensplitter&> remove_watch("remove-watch", "Remove a memory watch",
		"Syntax: remove-watch <name>\nRemoves a memory watch\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			std::string name = t;
			if(name == "" || t.tail() != "") {
				messages << "syntax: remove-watch <name>" << std::endl;
				return;
			}
			std::cerr << "Erase watch: '" << name << "'" << std::endl;
			memory_watches.erase(name);
			auto& _status = window::get_emustatus();
			_status.erase("M[" + name + "]");
			update_movie_state();
		});

	function_ptr_command<tokensplitter&> autofire("autofire", "Set autofire pattern",
		"Syntax: autofire <buttons|->...\nSet autofire pattern\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			if(!t)
				throw std::runtime_error("Need at least one frame for autofire");
			std::vector<controls_t> new_autofire_pattern;
			init_buttonmap();
			while(t) {
				std::string fpattern = t;
				if(fpattern == "-")
					new_autofire_pattern.push_back(controls_t());
				else {
					controls_t c;
					while(fpattern != "") {
						size_t split = fpattern.find_first_of(",");
						std::string button = fpattern;
						std::string rest;
						if(split < fpattern.length()) {
							button = fpattern.substr(0, split);
							rest = fpattern.substr(split + 1);
						}
						if(!buttonmap.count(button)) {
							std::ostringstream x;
							x << "Invalid button '" << button << "'";
							throw std::runtime_error(x.str());
						}
						auto g = buttonmap[button];
						do_button_action(g.first, g.second, 1, false, c);
						fpattern = rest;
					}
					new_autofire_pattern.push_back(c);
				}
			}
			autofire_pattern = new_autofire_pattern;
			autofire_position = 0;
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
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			init_buttonmap();
			if(!buttonmap.count(button))
				return;
			auto i = buttonmap[button];
			do_button_action(i.first, i.second, (type != 1) ? 1 : 0, (type == 2));
			update_movie_state();
			window::notify_screen_update();
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
			for(auto i = queued_saves.begin(); i != queued_saves.end(); i++)
				do_save_state(*i);
		}
		queued_saves.clear();
	}

	//Do (delayed) reset. Return true if proper, false if forced at frame boundary.
	bool handle_reset(long cycles)
	{
		if(cycles == 0) {
			window::message("SNES reset");
			SNES::system.reset();
			framebuffer = screen_nosignal;
			lua_callback_do_reset();
			redraw_framebuffer();
		} else if(cycles > 0) {
			video_refresh_done = false;
			long cycles_executed = 0;
			messages << "Executing delayed reset... This can take some time!" << std::endl;
			while(cycles_executed < cycles && !video_refresh_done) {
				SNES::cpu.op_step();
				cycles_executed++;
			}
			if(!video_refresh_done)
				messages << "SNES reset (delayed " << cycles_executed << ")" << std::endl;
			else
				messages << "SNES reset (forced at " << cycles_executed << ")" << std::endl;
			SNES::system.reset();
			framebuffer = screen_nosignal;
			lua_callback_do_reset();
			redraw_framebuffer();
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
	auto old_inteface = SNES::system.interface;
	SNES::system.interface = &intrf;
	status = &window::get_emustatus();
	autofire_pattern.push_back(controls_t());
	window_callback::set_callback_handler(mywcb);

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
	av_snooper::add_dump_notifier(dumpwatch);
	window::set_main_surface(main_screen);
	redraw_framebuffer();
	window::paused(false);
	amode = ADVANCE_PAUSE;
	while(amode != ADVANCE_QUIT) {
		if(handle_corrupt()) {
			first_round = our_movie.is_savestate;
			just_did_loadstate = first_round;
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
			int r = handle_load();
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
			if(amode == ADVANCE_QUIT)
				break;
			amode = ADVANCE_PAUSE;
			redraw_framebuffer();
			window::cancel_wait();
			window::paused(true);
			window::poll_inputs();
			just_did_loadstate = false;
		}
		SNES::system.run();
		if(amode == ADVANCE_AUTO)
			window::wait_msec(to_wait_frame(get_ticks_msec()));
		first_round = false;
	}
	av_snooper::end(true);
	SNES::system.interface = old_inteface;
}
