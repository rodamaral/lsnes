#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/command.hpp"
#include "core/debug.hpp"
#include "core/dispatch.hpp"
#include "core/emustatus.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/inthread.hpp"
#include "core/keymapper.hpp"
#include "core/multitrack.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"
#include "core/mainloop.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/project.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/romloader.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/c-interface.hpp"
#include "interface/callbacks.hpp"
#include "interface/romtype.hpp"
#include "library/framebuffer.hpp"
#include "library/zip.hpp"

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

settingvar::variable<settingvar::model_bool<settingvar::yes_no>> jukebox_dflt_binary(lsnes_vset,
	"jukebox-default-binary", "Movie‣Saving‣Saveslots binary", true);
settingvar::variable<settingvar::model_bool<settingvar::yes_no>> movie_dflt_binary(lsnes_vset, "movie-default-binary",
	"Movie‣Saving‣Movies binary", false);
settingvar::variable<settingvar::model_bool<settingvar::yes_no>> save_dflt_binary(lsnes_vset,
	"savestate-default-binary", "Movie‣Saving‣Savestates binary", false);

namespace
{
	settingvar::variable<settingvar::model_int<0,999999>> advance_timeout_first(lsnes_vset, "advance-timeout",
		"Delays‣First frame advance", 500);
	settingvar::variable<settingvar::model_int<0,999999>> advance_timeout_subframe(lsnes_vset,
		"advance-subframe-timeout", "Delays‣Subframe advance", 100);
	settingvar::variable<settingvar::model_bool<settingvar::yes_no>> pause_on_end(lsnes_vset, "pause-on-end",
		"Movie‣Pause on end", false);
	settingvar::variable<settingvar::model_int<0,999999999>> jukebox_size(lsnes_vset, "jukebox-size",
		"Movie‣Number of save slots", 12);

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

	//Our thread.
	threads::id emulation_thread;
	//Flags related to repeating advance.
	bool advanced_once;
	bool cancel_advance;
	//Emulator advance mode. Detemines pauses at start of frame / subframe, etc..
	enum advance_mode amode;
	enum advance_mode old_mode;
	//Mode and filename of pending load, one of LOAD_* constants.
	bool load_paused;
	int loadmode;
	std::string pending_load;
	std::string pending_new_project;
	//Queued saves (all savestates).
	std::set<std::pair<std::string, int>> queued_saves;
	//Save jukebox.
	size_t save_jukebox_pointer;
	//Special subframe location. One of SPECIAL_* constants.
	int location_special;
	//Unsafe rewind.
	bool do_unsafe_rewind = false;
	void* unsafe_rewind_obj = NULL;
	//Stop at frame.
	bool stop_at_frame_active = false;
	uint64_t stop_at_frame = 0;
	//Macro hold.
	bool macro_hold_1;
	bool macro_hold_2;
	//In break pause.
	bool break_pause = false;


	std::string save_jukebox_name(size_t i)
	{
		return (stringfmt() << "$SLOT:" << (i + 1)).str();
	}

	std::map<std::string, std::string> slotinfo_cache;

	std::string vector_to_string(const std::vector<char>& x)
	{
		std::string y(x.begin(), x.end());
		while(y.length() > 0 && y[y.length() - 1] < 32)
			y = y.substr(0, y.length() - 1);
		return y;
	}

	std::string get_slotinfo(const std::string& _filename)
	{
		std::string filename = resolve_relative_path(_filename);
		if(!slotinfo_cache.count(filename)) {
			std::ostringstream out;
			try {
				moviefile::brief_info info(filename);
				if(!CORE().mlogic)
					out << "No movie";
				else if(CORE().mlogic.get_mfile().projectid == info.projectid)
					out << info.rerecords << "R/" << info.current_frame << "F";
				else
					out << "Wrong movie";
			} catch(...) {
				out << "Nonexistent";
			}
			slotinfo_cache[filename] = out.str();
		}
		return slotinfo_cache[filename];
	}

	void flush_slotinfo(const std::string& filename)
	{
		slotinfo_cache.erase(resolve_relative_path(filename));
	}

	void flush_slotinfo()
	{
		slotinfo_cache.clear();
	}
}

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
			if(!cancel_advance) {
				if(!advanced_once)
					platform::wait(advance_timeout_first * 1000);
				else
					platform::wait(advance_timeout_subframe * 1000);
				advanced_once = true;
			}
			if(cancel_advance) {
				stop_at_frame_active = false;
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			platform::set_paused(amode == ADVANCE_PAUSE);
		} else if(amode == ADVANCE_FRAME) {
			;
		} else {
			if(amode == ADVANCE_SKIPLAG) {
				stop_at_frame_active = false;
				amode = ADVANCE_PAUSE;
			}
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
				uint64_t wait = 0;
				if(!advanced_once)
					wait = advance_timeout_first * 1000;
				else if(amode == ADVANCE_SUBFRAME)
					wait = advance_timeout_subframe * 1000;
				else
					wait = to_wait_frame(get_utime());
				platform::wait(wait);
				advanced_once = true;
			}
			if(cancel_advance) {
				stop_at_frame_active = false;
				amode = ADVANCE_PAUSE;
				cancel_advance = false;
			}
			platform::set_paused(amode == ADVANCE_PAUSE);
		} else if(amode == ADVANCE_AUTO && CORE().mlogic.get_movie().readonly_mode() &&
			pause_on_end && !stop_at_frame_active) {
			if(CORE().mlogic.get_movie().get_current_frame() ==
				CORE().mlogic.get_movie().get_frame_count()) {
				stop_at_frame_active = false;
				amode = ADVANCE_PAUSE;
				platform::set_paused(true);
			}
		} else if(amode == ADVANCE_AUTO && stop_at_frame_active) {
			if(CORE().mlogic.get_movie().get_current_frame() >= stop_at_frame) {
				stop_at_frame_active = false;
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
	platform::flush_command_queue();
	controller_frame tmp = controls.get(CORE().mlogic.get_movie().get_current_frame());
	our_rom.rtype->pre_emulate_frame(tmp);	//Preset controls, the lua will override if needed.
	lua_callback_do_input(tmp, subframe);
	CORE().mteditor.process_frame(tmp);
	controls.commit(tmp);
	return tmp;
}

namespace
{

	//Do pending load (automatically unpauses).
	void mark_pending_load(std::string filename, int lmode)
	{
		if(break_pause)
			break_pause = false;
		loadmode = lmode;
		pending_load = filename;
		old_mode = amode;
		amode = ADVANCE_LOAD;
		platform::cancel_wait();
		platform::set_paused(false);
	}

	void mark_pending_save(std::string filename, int smode, int binary)
	{
		int tmp = -1;
		if(smode == SAVE_MOVIE) {
			//Just do this immediately.
			do_save_movie(filename, binary);
			flush_slotinfo(translate_name_mprefix(filename, tmp, -1));
			return;
		}
		if(location_special == SPECIAL_SAVEPOINT) {
			//We can save immediately here.
			do_save_state(filename, binary);
			flush_slotinfo(translate_name_mprefix(filename, tmp, -1));
			return;
		}
		queued_saves.insert(std::make_pair(filename, binary));
		messages << "Pending save on '" << filename << "'" << std::endl;
	}

	struct jukebox_size_listener : public settingvar::listener
	{
		jukebox_size_listener() { lsnes_vset.add_listener(*this); }
		~jukebox_size_listener() throw() {lsnes_vset.remove_listener(*this); };
		void on_setting_change(settingvar::group& grp, const settingvar::base& val)
		{
			if(val.get_iname() == "jukebox-size") {
				if(save_jukebox_pointer >= (size_t)jukebox_size)
					save_jukebox_pointer = 0;
			}
			update_movie_state();
		}
	};
}

void update_movie_state()
{
	auto p = project_get();
	bool readonly = false;
	{
		uint64_t magic[4];
		our_rom.region->fill_framerate_magic(magic);
		if(CORE().mlogic)
			CORE().commentary.frame_number(CORE().mlogic.get_movie().get_current_frame(),
				1.0 * magic[1] / magic[0]);
		else
			CORE().commentary.frame_number(0, 60.0);	//Default.
	}
	auto& _status = CORE().status.get_write();
	try {
		if(CORE().mlogic && !system_corrupt) {
			_status.movie_valid = true;
			_status.curframe = CORE().mlogic.get_movie().get_current_frame();
			_status.length = CORE().mlogic.get_movie().get_frame_count();
			_status.lag = CORE().mlogic.get_movie().get_lag_frames();
			if(location_special == SPECIAL_FRAME_START)
				_status.subframe = 0;
			else if(location_special == SPECIAL_SAVEPOINT)
				_status.subframe = _lsnes_status::subframe_savepoint;
			else if(location_special == SPECIAL_FRAME_VIDEO)
				_status.subframe = _lsnes_status::subframe_video;
			else
				_status.subframe = CORE().mlogic.get_movie().next_poll_number();
		} else {
			_status.movie_valid = false;
			_status.curframe = 0;
			_status.length = 0;
			_status.lag = 0;
			_status.subframe = 0;
		}
		_status.dumping = (information_dispatch::get_dumper_count() > 0);
		if(break_pause)
			_status.pause = _lsnes_status::pause_break;
		else if(amode == ADVANCE_PAUSE)
			_status.pause = _lsnes_status::pause_normal;
		else
			_status.pause = _lsnes_status::pause_none;
		if(CORE().mlogic) {
			auto& mo = CORE().mlogic.get_movie();
			readonly = mo.readonly_mode();
			if(system_corrupt)
				_status.mode = 'C';
			else if(!readonly)
				_status.mode = 'R';
			else if(mo.get_frame_count() >= mo.get_current_frame())
				_status.mode = 'P';
			else
				_status.mode = 'F';
		}
		if(jukebox_size > 0) {
			_status.saveslot_valid = true;
			int tmp = -1;
			std::string sfilen = translate_name_mprefix(save_jukebox_name(save_jukebox_pointer), tmp, -1);
			_status.saveslot = save_jukebox_pointer + 1;
			_status.slotinfo = utf8::to32(get_slotinfo(sfilen));
		} else {
			_status.saveslot_valid = false;
		}
		_status.branch_valid = (p != NULL);
		if(p) _status.branch = utf8::to32(p->get_branch_string());

		std::string cur_branch = CORE().mlogic ? CORE().mlogic.get_mfile().current_branch() :
			"";
		_status.mbranch_valid = (cur_branch != "");
		_status.mbranch = utf8::to32(cur_branch);

		_status.speed = (unsigned)(100 * get_realized_multiplier() + 0.5);

		if(CORE().mlogic && !system_corrupt) {
			time_t timevalue = static_cast<time_t>(CORE().mlogic.get_mfile().rtc_second);
			struct tm* time_decompose = gmtime(&timevalue);
			char datebuffer[512];
			strftime(datebuffer, 511, "%Y%m%d(%a)T%H%M%S", time_decompose);
			_status.rtc = utf8::to32(datebuffer);
			_status.rtc_valid = true;
		} else {
			_status.rtc_valid = false;
		}

		auto mset = controls.active_macro_set();
		bool mfirst = true;
		std::ostringstream mss;
		for(auto i: mset) {
			if(!mfirst) mss << ",";
			mss << i;
			mfirst = false;
		}
		_status.macros = utf8::to32(mss.str());

		controller_frame c;
		if(!CORE().mteditor.any_records())
			c = CORE().mlogic.get_movie().get_controls();
		else
			c = controls.get_committed();
		_status.inputs.clear();
		for(unsigned i = 0;; i++) {
			auto pindex = controls.lcid_to_pcid(i);
			if(pindex.first < 0 || !controls.is_present(pindex.first, pindex.second))
				break;
			char32_t buffer[MAX_DISPLAY_LENGTH];
			c.display(pindex.first, pindex.second, buffer);
			std::u32string _buffer = buffer;
			if(readonly && CORE().mteditor.is_enabled()) {
				multitrack_edit::state st = CORE().mteditor.get(pindex.first, pindex.second);
				if(st == multitrack_edit::MT_PRESERVE)
					_buffer += U" (keep)";
				else if(st == multitrack_edit::MT_OVERWRITE)
					_buffer += U" (rewrite)";
				else if(st == multitrack_edit::MT_OR)
					_buffer += U" (OR)";
				else if(st == multitrack_edit::MT_XOR)
					_buffer += U" (XOR)";
				else
					_buffer += U" (\?\?\?)";
			}
			_status.inputs.push_back(_buffer);
		}
		//Lua variables.
		_status.lvars = get_lua_watch_vars();
		//Memory watches.
		_status.mvars = CORE().mwatch.get_window_vars();

		_status.valid = true;
	} catch(...) {
	}
	CORE().status.put_write();
	notify_status_update();
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
		x = CORE().mlogic.input_poll(port, index, control);
		lua_callback_snoop_input(port, index, control, x);
		return x;
	}

	int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value)
	{
		if(!CORE().mlogic.get_movie().readonly_mode()) {
			controller_frame f = CORE().mlogic.get_movie().get_controls();
			f.axis3(port, index, control, value);
			CORE().mlogic.get_movie().set_controls(f);
		}
		return CORE().mlogic.get_movie().next_input(port, index, control);
	}

	void notify_latch(std::list<std::string>& args)
	{
		lua_callback_do_latch(args);
	}

	void timer_tick(uint32_t increment, uint32_t per_second)
	{
		if(!CORE().mlogic)
			return;
		auto& m = CORE().mlogic.get_mfile();
		m.rtc_subsecond += increment;
		while(m.rtc_subsecond >= per_second) {
			m.rtc_second++;
			m.rtc_subsecond -= per_second;
		}
	}

	std::string get_firmware_path()
	{
		return CORE().setcache.get("firmwarepath");
	}

	std::string get_base_path()
	{
		return our_rom.msu1_base;
	}

	time_t get_time()
	{
		return CORE().mlogic ? CORE().mlogic.get_mfile().rtc_second : 0;
	}

	time_t get_randomseed()
	{
		return random_seed_value;
	}

	void output_frame(framebuffer::raw& screen, uint32_t fps_n, uint32_t fps_d)
	{
		lua_callback_do_frame_emulated();
		location_special = SPECIAL_FRAME_VIDEO;
		redraw_framebuffer(screen, false, true);
		uint32_t g = gcd(fps_n, fps_d);
		fps_n /= g;
		fps_d /= g;
		information_dispatch::do_frame(screen, fps_n, fps_d);
	}

	void action_state_updated()
	{
		graphics_driver_action_updated();
	}

	void memory_read(uint64_t addr, uint64_t value)
	{
		debug_fire_callback_read(addr, value);
	}

	void memory_write(uint64_t addr, uint64_t value)
	{
		debug_fire_callback_write(addr, value);
	}

	void memory_execute(uint64_t addr, uint64_t proc)
	{
		debug_fire_callback_exec(addr, proc);
	}

	void memory_trace(uint64_t proc, const char* str, bool insn)
	{
		debug_fire_callback_trace(proc, str, insn);
	}
};

namespace
{
	lsnes_callbacks lsnes_callbacks_obj;

	command::fnptr<const std::string&> test4(lsnes_cmd, "test4", "test", "test",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			std::list<std::string> _args;
			std::string args2 = args;
			for(auto& sym : token_iterator_foreach(args, {" ", "\t"}))
				_args.push_back(sym);
			lua_callback_do_latch(_args);
		});
	command::fnptr<> count_rerecords(lsnes_cmd, "count-rerecords", "Count rerecords",
		"Syntax: count-rerecords\nCounts rerecords.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			std::vector<char> tmp;
			uint64_t x = CORE().mlogic.get_rrdata().write(tmp);
			messages << x << " rerecord(s)" << std::endl;
		});

	command::fnptr<const std::string&> quit_emulator(lsnes_cmd, "quit-emulator", "Quit the emulator",
		"Syntax: quit-emulator [/y]\nQuits emulator (/y => don't ask for confirmation).\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			amode = ADVANCE_QUIT;
			platform::set_paused(false);
			platform::cancel_wait();
		});

	command::fnptr<> unpause_emulator(lsnes_cmd, "unpause-emulator", "Unpause the emulator",
		"Syntax: unpause-emulator\nUnpauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			break_pause = false;
			amode = ADVANCE_AUTO;
			platform::set_paused(false);
			platform::cancel_wait();
		});

	command::fnptr<> pause_emulator(lsnes_cmd, "pause-emulator", "(Un)pause the emulator",
		"Syntax: pause-emulator\n(Un)pauses the emulator.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(amode != ADVANCE_AUTO || break_pause) {
				break_pause = false;
				amode = ADVANCE_AUTO;
				platform::set_paused(false);
				platform::cancel_wait();
			} else {
				platform::cancel_wait();
				cancel_advance = false;
				stop_at_frame_active = false;
				amode = ADVANCE_PAUSE;
			}
		});

	command::fnptr<> save_jukebox_prev(lsnes_cmd, "cycle-jukebox-backward", "Cycle save jukebox backwards",
		"Syntax: cycle-jukebox-backward\nCycle save jukebox backwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				return;
			if(save_jukebox_pointer == 0)
				save_jukebox_pointer = jukebox_size - 1;
			else
				save_jukebox_pointer--;
			if(save_jukebox_pointer >= (size_t)jukebox_size)
				save_jukebox_pointer = 0;
			update_movie_state();
		});

	command::fnptr<> save_jukebox_next(lsnes_cmd, "cycle-jukebox-forward", "Cycle save jukebox forwards",
		"Syntax: cycle-jukebox-forward\nCycle save jukebox forwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				return;
			if(save_jukebox_pointer + 1 >= (size_t)jukebox_size)
				save_jukebox_pointer = 0;
			else
				save_jukebox_pointer++;
			if(save_jukebox_pointer >= (size_t)jukebox_size)
				save_jukebox_pointer = 0;
			update_movie_state();
		});

	command::fnptr<const std::string&> save_jukebox_set(lsnes_cmd, "set-jukebox-slot", "Set jukebox slot",
		"Syntax: set-jukebox-slot\nSet jukebox slot\n", [](const std::string& args)
		throw(std::bad_alloc, std::runtime_error) {
			if(!regex_match("[1-9][0-9]{0,8}", args))
				throw std::runtime_error("Bad slot number");
			uint32_t slot = parse_value<uint32_t>(args);
			if(slot >= (size_t)jukebox_size)
				throw std::runtime_error("Bad slot number");
			save_jukebox_pointer = slot - 1;
			update_movie_state();
		});

	command::fnptr<> load_jukebox(lsnes_cmd, "load-jukebox", "Load save from jukebox",
		"Syntax: load-jukebox\nLoad save from jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_load(save_jukebox_name(save_jukebox_pointer), LOAD_STATE_CURRENT);
		});

	command::fnptr<> load_jukebox_readwrite(lsnes_cmd, "load-jukebox-readwrite", "Load save from jukebox in"
		" read-write mode", "Syntax: load-jukebox-readwrite\nLoad save from jukebox in read-write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_load(save_jukebox_name(save_jukebox_pointer), LOAD_STATE_RW);
		});

	command::fnptr<> load_jukebox_readonly(lsnes_cmd, "load-jukebox-readonly", "Load save from jukebox in "
		"read-only mode", "Syntax: load-jukebox-readonly\nLoad save from jukebox in read-only mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_load(save_jukebox_name(save_jukebox_pointer), LOAD_STATE_RO);
		});

	command::fnptr<> load_jukebox_preserve(lsnes_cmd, "load-jukebox-preserve", "Load save from jukebox, "
		"preserving input", "Syntax: load-jukebox-preserve\nLoad save from jukebox, preserving input\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_load(save_jukebox_name(save_jukebox_pointer), LOAD_STATE_PRESERVE);
		});

	command::fnptr<> load_jukebox_movie(lsnes_cmd, "load-jukebox-movie", "Load save from jukebox as movie",
		"Syntax: load-jukebox-movie\nLoad save from jukebox as movie\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_load(save_jukebox_name(save_jukebox_pointer), LOAD_STATE_MOVIE);
		});

	command::fnptr<> save_jukebox_c(lsnes_cmd, "save-jukebox", "Save save to jukebox",
		"Syntax: save-jukebox\nSave save to jukebox\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(jukebox_size == 0)
				throw std::runtime_error("No slot selected");
			mark_pending_save(save_jukebox_name(save_jukebox_pointer), SAVE_STATE, -1);
		});

	command::fnptr<> padvance_frame(lsnes_cmd, "+advance-frame", "Advance one frame",
		"Syntax: +advance-frame\nAdvances the emulation by one frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(break_pause)
				break_pause = false;
			amode = ADVANCE_FRAME;
			cancel_advance = false;
			advanced_once = false;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> nadvance_frame(lsnes_cmd, "-advance-frame", "Advance one frame",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			cancel_advance = true;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> padvance_poll(lsnes_cmd, "+advance-poll", "Advance one subframe",
		"Syntax: +advance-poll\nAdvances the emulation by one subframe.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(break_pause)
				break_pause = false;
			amode = ADVANCE_SUBFRAME;
			cancel_advance = false;
			advanced_once = false;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> nadvance_poll(lsnes_cmd, "-advance-poll", "Advance one subframe",
		"No help available\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(break_pause)
				break_pause = false;
			cancel_advance = true;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> advance_skiplag(lsnes_cmd, "advance-skiplag", "Skip to next poll",
		"Syntax: advance-skiplag\nAdvances the emulation to the next poll.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(break_pause)
				break_pause = false;
			amode = ADVANCE_SKIPLAG_PENDING;
			platform::cancel_wait();
			platform::set_paused(false);
		});

	command::fnptr<> reset_c(lsnes_cmd, "reset", "Reset the system",
		"Syntax: reset\nReset\nResets the system in beginning of the next frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			int sreset_action = our_rom.rtype->reset_action(false);
			if(sreset_action < 0) {
				platform::error_message("Core does not support resets");
				messages << "Emulator core does not support resets" << std::endl;
				return;
			}
			our_rom.rtype->execute_action(sreset_action, std::vector<interface_action_paramval>());
		});

	command::fnptr<> hreset_c(lsnes_cmd, "reset-hard", "Reset the system",
		"Syntax: reset-hard\nReset-hard\nHard resets the system in beginning of the next frame.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			int hreset_action = our_rom.rtype->reset_action(true);
			if(hreset_action < 0) {
				platform::error_message("Core does not support hard resets");
				messages << "Emulator core does not support hard resets" << std::endl;
				return;
			}
			our_rom.rtype->execute_action(hreset_action, std::vector<interface_action_paramval>());
		});

	command::fnptr<command::arg_filename> load_c(lsnes_cmd, "load", "Load savestate (current mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in current mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_CURRENT);
		});

	command::fnptr<command::arg_filename> load_smart_c(lsnes_cmd, "load-smart", "Load savestate (heuristic mode)",
		"Syntax: load <file>\nLoads SNES state from <file> in heuristic mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_DEFAULT);
		});

	command::fnptr<command::arg_filename> load_state_c(lsnes_cmd, "load-state", "Load savestate (R/W)",
		"Syntax: load-state <file>\nLoads SNES state from <file> in Read/Write mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RW);
		});

	command::fnptr<command::arg_filename> load_readonly(lsnes_cmd, "load-readonly", "Load savestate (RO)",
		"Syntax: load-readonly <file>\nLoads SNES state from <file> in read-only mode\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_RO);
		});

	command::fnptr<command::arg_filename> load_preserve(lsnes_cmd, "load-preserve", "Load savestate (preserve "
		"input)", "Syntax: load-preserve <file>\nLoads SNES state from <file> preserving input\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_PRESERVE);
		});

	command::fnptr<command::arg_filename> load_movie_c(lsnes_cmd, "load-movie", "Load movie",
		"Syntax: load-movie <file>\nLoads SNES movie from <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_MOVIE);
		});

	command::fnptr<command::arg_filename> load_allbr_c(lsnes_cmd, "load-allbranches", "Load savestate "
		"(all branches)", "Syntax: load-allbranches <file>\nLoads SNES state from <file> with all "
		"branches\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load(args, LOAD_STATE_ALLBRANCH);
		});

	command::fnptr<command::arg_filename> save_state(lsnes_cmd, "save-state", "Save state",
		"Syntax: save-state <file>\nSaves SNES state to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE, -1);
		});

	command::fnptr<command::arg_filename> save_state2(lsnes_cmd, "save-state-binary", "Save state (binary)",
		"Syntax: save-state-binary <file>\nSaves binary state to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE, 1);
		});

	command::fnptr<command::arg_filename> save_state3(lsnes_cmd, "save-state-zip", "Save state (zip)",
		"Syntax: save-state-zip <file>\nSaves zip state to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_STATE, 0);
		});

	command::fnptr<command::arg_filename> save_movie(lsnes_cmd, "save-movie", "Save movie",
		"Syntax: save-movie <file>\nSaves SNES movie to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE, -1);
		});

	command::fnptr<command::arg_filename> save_movie2(lsnes_cmd, "save-movie-binary", "Save movie (binary)",
		"Syntax: save-movie-binary <file>\nSaves binary movie to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE, 1);
		});

	command::fnptr<command::arg_filename> save_movie3(lsnes_cmd, "save-movie-zip", "Save movie (zip)",
		"Syntax: save-movie-zip <file>\nSaves zip movie to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			mark_pending_save(args, SAVE_MOVIE, 0);
		});

	command::fnptr<> set_rwmode(lsnes_cmd, "set-rwmode", "Switch to read/write mode",
		"Syntax: set-rwmode\nSwitches to read/write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			lua_callback_movie_lost("readwrite");
			CORE().mlogic.get_movie().readonly_mode(false);
			notify_mode_change(false);
			lua_callback_do_readwrite();
			update_movie_state();
		});

	command::fnptr<> set_romode(lsnes_cmd, "set-romode", "Switch to read-only mode",
		"Syntax: set-romode\nSwitches to read-only mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().mlogic.get_movie().readonly_mode(true);
			notify_mode_change(true);
			update_movie_state();
		});

	command::fnptr<> toggle_rwmode(lsnes_cmd, "toggle-rwmode", "Toggle read/write mode",
		"Syntax: toggle-rwmode\nToggles read/write mode\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			bool c = CORE().mlogic.get_movie().readonly_mode();
			if(c)
				lua_callback_movie_lost("readwrite");
			CORE().mlogic.get_movie().readonly_mode(!c);
			notify_mode_change(!c);
			if(c)
				lua_callback_do_readwrite();
			update_movie_state();
		});

	command::fnptr<> repaint(lsnes_cmd, "repaint", "Redraw the screen",
		"Syntax: repaint\nRedraws the screen\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			redraw_framebuffer();
		});

	command::fnptr<> tpon(lsnes_cmd, "toggle-pause-on-end", "Toggle pause on end", "Toggle pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			bool tmp = pause_on_end;
			pause_on_end.set(!tmp);
			messages << "Pause-on-end is now " << (tmp ? "OFF" : "ON") << std::endl;
		});

	command::fnptr<> spon(lsnes_cmd, "set-pause-on-end", "Set pause on end", "Set pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			pause_on_end.set(true);
			messages << "Pause-on-end is now ON" << std::endl;
		});

	command::fnptr<> cpon(lsnes_cmd, "clear-pause-on-end", "Clear pause on end", "Clear pause on end\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			pause_on_end.set(false);
			messages << "Pause-on-end is now OFF" << std::endl;
		});

	command::fnptr<> rewind_movie(lsnes_cmd, "rewind-movie", "Rewind movie to the beginning",
		"Syntax: rewind-movie\nRewind movie to the beginning\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_BEGINNING);
		});

	command::fnptr<> cancel_save(lsnes_cmd, "cancel-saves", "Cancel all pending saves", "Syntax: "
		"cancel-save\nCancel pending saves\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			queued_saves.clear();
			messages << "Pending saves canceled." << std::endl;
		});

	command::fnptr<> flushslots(lsnes_cmd, "flush-slotinfo", "Flush slotinfo cache",
		"Flush slotinfo cache\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			flush_slotinfo();
		});

	command::fnptr<> mhold1(lsnes_cmd, "+hold-macro", "Hold macro (hold)",
		"Hold macros enable\n", []() throw(std::bad_alloc, std::runtime_error) {
			macro_hold_1 = true;
		});

	command::fnptr<> mhold2(lsnes_cmd, "-hold-macro", "Hold macro (hold)",
		"Hold macros disable\n", []() throw(std::bad_alloc, std::runtime_error) {
			macro_hold_1 = false;
		});

	command::fnptr<> mhold3(lsnes_cmd, "hold-macro", "Hold macro (toggle)",
		"Hold macros toggle\n", []() throw(std::bad_alloc, std::runtime_error) {
			macro_hold_2 = !macro_hold_2;
			if(macro_hold_2)
				messages << "Macros are held for next frame." << std::endl;
			else
				messages << "Macros are not held for next frame." << std::endl;
		});

	keyboard::invbind imhold1(lsnes_mapper, "+hold-macro", "Macro‣Hold all macros");
	keyboard::invbind imhold2(lsnes_mapper, "hold-macro", "Macro‣Hold all macros (typed)");
	keyboard::invbind ipause_emulator(lsnes_mapper, "pause-emulator", "Speed‣(Un)pause");
	keyboard::invbind ijback(lsnes_mapper, "cycle-jukebox-backward", "Slot select‣Cycle backwards");
	keyboard::invbind ijforward(lsnes_mapper, "cycle-jukebox-forward", "Slot select‣Cycle forwards");
	keyboard::invbind iloadj(lsnes_mapper, "load-jukebox", "Load‣Selected slot");
	keyboard::invbind iloadjrw(lsnes_mapper, "load-jukebox-readwrite", "Load‣Selected slot (readwrite mode)");
	keyboard::invbind iloadjro(lsnes_mapper, "load-jukebox-readonly", "Load‣Selected slot (readonly mode)");
	keyboard::invbind iloadjp(lsnes_mapper, "load-jukebox-preserve", "Load‣Selected slot (preserve input)");
	keyboard::invbind iloadjm(lsnes_mapper, "load-jukebox-movie", "Load‣Selected slot (as movie)");
	keyboard::invbind isavej(lsnes_mapper, "save-jukebox", "Save‣Selected slot");
	keyboard::invbind iadvframe(lsnes_mapper, "+advance-frame", "Speed‣Advance frame");
	keyboard::invbind iadvsubframe(lsnes_mapper, "+advance-poll", "Speed‣Advance subframe");
	keyboard::invbind iskiplag(lsnes_mapper, "advance-skiplag", "Speed‣Advance poll");
	keyboard::invbind ireset(lsnes_mapper, "reset", "System‣Reset");
	keyboard::invbind iset_rwmode(lsnes_mapper, "set-rwmode", "Movie‣Switch to read/write");
	keyboard::invbind itoggle_romode(lsnes_mapper, "set-romode", "Movie‣Switch to read-only");
	keyboard::invbind itoggle_rwmode(lsnes_mapper, "toggle-rwmode", "Movie‣Toggle read-only");
	keyboard::invbind irepaint(lsnes_mapper, "repaint", "System‣Repaint screen");
	keyboard::invbind itogglepause(lsnes_mapper, "toggle-pause-on-end", "Movie‣Toggle pause-on-end");
	keyboard::invbind irewind_movie(lsnes_mapper, "rewind-movie", "Movie‣Rewind movie");
	keyboard::invbind icancel_saves(lsnes_mapper, "cancel-saves", "Save‣Cancel pending saves");
	keyboard::invbind iload1(lsnes_mapper, "load $SLOT:1", "Load‣Slot 1");
	keyboard::invbind iload2(lsnes_mapper, "load $SLOT:2", "Load‣Slot 2");
	keyboard::invbind iload3(lsnes_mapper, "load $SLOT:3", "Load‣Slot 3");
	keyboard::invbind iload4(lsnes_mapper, "load $SLOT:4", "Load‣Slot 4");
	keyboard::invbind iload5(lsnes_mapper, "load $SLOT:5", "Load‣Slot 5");
	keyboard::invbind iload6(lsnes_mapper, "load $SLOT:6", "Load‣Slot 6");
	keyboard::invbind iload7(lsnes_mapper, "load $SLOT:7", "Load‣Slot 7");
	keyboard::invbind iload8(lsnes_mapper, "load $SLOT:8", "Load‣Slot 8");
	keyboard::invbind iload9(lsnes_mapper, "load $SLOT:9", "Load‣Slot 9");
	keyboard::invbind iload10(lsnes_mapper, "load $SLOT:10", "Load‣Slot 10");
	keyboard::invbind iload11(lsnes_mapper, "load $SLOT:11", "Load‣Slot 11");
	keyboard::invbind iload12(lsnes_mapper, "load $SLOT:12", "Load‣Slot 12");
	keyboard::invbind iload13(lsnes_mapper, "load $SLOT:13", "Load‣Slot 13");
	keyboard::invbind iload14(lsnes_mapper, "load $SLOT:14", "Load‣Slot 14");
	keyboard::invbind iload15(lsnes_mapper, "load $SLOT:15", "Load‣Slot 15");
	keyboard::invbind iload16(lsnes_mapper, "load $SLOT:16", "Load‣Slot 16");
	keyboard::invbind iload17(lsnes_mapper, "load $SLOT:17", "Load‣Slot 17");
	keyboard::invbind iload18(lsnes_mapper, "load $SLOT:18", "Load‣Slot 18");
	keyboard::invbind iload19(lsnes_mapper, "load $SLOT:19", "Load‣Slot 19");
	keyboard::invbind iload20(lsnes_mapper, "load $SLOT:20", "Load‣Slot 20");
	keyboard::invbind iload21(lsnes_mapper, "load $SLOT:21", "Load‣Slot 21");
	keyboard::invbind iload22(lsnes_mapper, "load $SLOT:22", "Load‣Slot 22");
	keyboard::invbind iload23(lsnes_mapper, "load $SLOT:23", "Load‣Slot 23");
	keyboard::invbind iload24(lsnes_mapper, "load $SLOT:24", "Load‣Slot 24");
	keyboard::invbind iload25(lsnes_mapper, "load $SLOT:25", "Load‣Slot 25");
	keyboard::invbind iload26(lsnes_mapper, "load $SLOT:26", "Load‣Slot 26");
	keyboard::invbind iload27(lsnes_mapper, "load $SLOT:27", "Load‣Slot 27");
	keyboard::invbind iload28(lsnes_mapper, "load $SLOT:28", "Load‣Slot 28");
	keyboard::invbind iload29(lsnes_mapper, "load $SLOT:29", "Load‣Slot 29");
	keyboard::invbind iload30(lsnes_mapper, "load $SLOT:30", "Load‣Slot 30");
	keyboard::invbind iload31(lsnes_mapper, "load $SLOT:31", "Load‣Slot 31");
	keyboard::invbind iload32(lsnes_mapper, "load $SLOT:32", "Load‣Slot 32");
	keyboard::invbind isave1(lsnes_mapper, "save-state $SLOT:1", "Save‣Slot 1");
	keyboard::invbind isave2(lsnes_mapper, "save-state $SLOT:2", "Save‣Slot 2");
	keyboard::invbind isave3(lsnes_mapper, "save-state $SLOT:3", "Save‣Slot 3");
	keyboard::invbind isave4(lsnes_mapper, "save-state $SLOT:4", "Save‣Slot 4");
	keyboard::invbind isave5(lsnes_mapper, "save-state $SLOT:5", "Save‣Slot 5");
	keyboard::invbind isave6(lsnes_mapper, "save-state $SLOT:6", "Save‣Slot 6");
	keyboard::invbind isave7(lsnes_mapper, "save-state $SLOT:7", "Save‣Slot 7");
	keyboard::invbind isave8(lsnes_mapper, "save-state $SLOT:8", "Save‣Slot 8");
	keyboard::invbind isave9(lsnes_mapper, "save-state $SLOT:9", "Save‣Slot 9");
	keyboard::invbind isave10(lsnes_mapper, "save-state $SLOT:10", "Save‣Slot 10");
	keyboard::invbind isave11(lsnes_mapper, "save-state $SLOT:11", "Save‣Slot 11");
	keyboard::invbind isave12(lsnes_mapper, "save-state $SLOT:12", "Save‣Slot 12");
	keyboard::invbind isave13(lsnes_mapper, "save-state $SLOT:13", "Save‣Slot 13");
	keyboard::invbind isave14(lsnes_mapper, "save-state $SLOT:14", "Save‣Slot 14");
	keyboard::invbind isave15(lsnes_mapper, "save-state $SLOT:15", "Save‣Slot 15");
	keyboard::invbind isave16(lsnes_mapper, "save-state $SLOT:16", "Save‣Slot 16");
	keyboard::invbind isave17(lsnes_mapper, "save-state $SLOT:17", "Save‣Slot 17");
	keyboard::invbind isave18(lsnes_mapper, "save-state $SLOT:18", "Save‣Slot 18");
	keyboard::invbind isave19(lsnes_mapper, "save-state $SLOT:19", "Save‣Slot 19");
	keyboard::invbind isave20(lsnes_mapper, "save-state $SLOT:20", "Save‣Slot 20");
	keyboard::invbind isave21(lsnes_mapper, "save-state $SLOT:21", "Save‣Slot 21");
	keyboard::invbind isave22(lsnes_mapper, "save-state $SLOT:22", "Save‣Slot 22");
	keyboard::invbind isave23(lsnes_mapper, "save-state $SLOT:23", "Save‣Slot 23");
	keyboard::invbind isave24(lsnes_mapper, "save-state $SLOT:24", "Save‣Slot 24");
	keyboard::invbind isave25(lsnes_mapper, "save-state $SLOT:25", "Save‣Slot 25");
	keyboard::invbind isave26(lsnes_mapper, "save-state $SLOT:26", "Save‣Slot 26");
	keyboard::invbind isave27(lsnes_mapper, "save-state $SLOT:27", "Save‣Slot 27");
	keyboard::invbind isave28(lsnes_mapper, "save-state $SLOT:28", "Save‣Slot 28");
	keyboard::invbind isave29(lsnes_mapper, "save-state $SLOT:29", "Save‣Slot 29");
	keyboard::invbind isave30(lsnes_mapper, "save-state $SLOT:30", "Save‣Slot 30");
	keyboard::invbind isave31(lsnes_mapper, "save-state $SLOT:31", "Save‣Slot 31");
	keyboard::invbind isave32(lsnes_mapper, "save-state $SLOT:32", "Save‣Slot 32");
	keyboard::invbind islot1(lsnes_mapper, "set-jukebox-slot 1", "Slot select‣Slot 1");
	keyboard::invbind islot2(lsnes_mapper, "set-jukebox-slot 2", "Slot select‣Slot 2");
	keyboard::invbind islot3(lsnes_mapper, "set-jukebox-slot 3", "Slot select‣Slot 3");
	keyboard::invbind islot4(lsnes_mapper, "set-jukebox-slot 4", "Slot select‣Slot 4");
	keyboard::invbind islot5(lsnes_mapper, "set-jukebox-slot 5", "Slot select‣Slot 5");
	keyboard::invbind islot6(lsnes_mapper, "set-jukebox-slot 6", "Slot select‣Slot 6");
	keyboard::invbind islot7(lsnes_mapper, "set-jukebox-slot 7", "Slot select‣Slot 7");
	keyboard::invbind islot8(lsnes_mapper, "set-jukebox-slot 8", "Slot select‣Slot 8");
	keyboard::invbind islot9(lsnes_mapper, "set-jukebox-slot 9", "Slot select‣Slot 9");
	keyboard::invbind islot10(lsnes_mapper, "set-jukebox-slot 10", "Slot select‣Slot 10");
	keyboard::invbind islot11(lsnes_mapper, "set-jukebox-slot 11", "Slot select‣Slot 11");
	keyboard::invbind islot12(lsnes_mapper, "set-jukebox-slot 12", "Slot select‣Slot 12");
	keyboard::invbind islot13(lsnes_mapper, "set-jukebox-slot 13", "Slot select‣Slot 13");
	keyboard::invbind islot14(lsnes_mapper, "set-jukebox-slot 14", "Slot select‣Slot 14");
	keyboard::invbind islot15(lsnes_mapper, "set-jukebox-slot 15", "Slot select‣Slot 15");
	keyboard::invbind islot16(lsnes_mapper, "set-jukebox-slot 16", "Slot select‣Slot 16");
	keyboard::invbind islot17(lsnes_mapper, "set-jukebox-slot 17", "Slot select‣Slot 17");
	keyboard::invbind islot18(lsnes_mapper, "set-jukebox-slot 18", "Slot select‣Slot 18");
	keyboard::invbind islot19(lsnes_mapper, "set-jukebox-slot 19", "Slot select‣Slot 19");
	keyboard::invbind islot20(lsnes_mapper, "set-jukebox-slot 20", "Slot select‣Slot 20");
	keyboard::invbind islot21(lsnes_mapper, "set-jukebox-slot 21", "Slot select‣Slot 21");
	keyboard::invbind islot22(lsnes_mapper, "set-jukebox-slot 22", "Slot select‣Slot 22");
	keyboard::invbind islot23(lsnes_mapper, "set-jukebox-slot 23", "Slot select‣Slot 23");
	keyboard::invbind islot24(lsnes_mapper, "set-jukebox-slot 24", "Slot select‣Slot 24");
	keyboard::invbind islot25(lsnes_mapper, "set-jukebox-slot 25", "Slot select‣Slot 25");
	keyboard::invbind islot26(lsnes_mapper, "set-jukebox-slot 26", "Slot select‣Slot 26");
	keyboard::invbind islot27(lsnes_mapper, "set-jukebox-slot 27", "Slot select‣Slot 27");
	keyboard::invbind islot28(lsnes_mapper, "set-jukebox-slot 28", "Slot select‣Slot 28");
	keyboard::invbind islot29(lsnes_mapper, "set-jukebox-slot 29", "Slot select‣Slot 29");
	keyboard::invbind islot30(lsnes_mapper, "set-jukebox-slot 30", "Slot select‣Slot 30");
	keyboard::invbind islot31(lsnes_mapper, "set-jukebox-slot 31", "Slot select‣Slot 31");
	keyboard::invbind islot32(lsnes_mapper, "set-jukebox-slot 32", "Slot select‣Slot 32");

	bool on_quit_prompt = false;
	class mywindowcallbacks : public information_dispatch
	{
	public:
		mywindowcallbacks() : information_dispatch("mainloop-window-callbacks")
		{
			closenotify.set(notify_close, [this]() {
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
			});
		}
		~mywindowcallbacks() throw() {}
		void on_new_dumper(const std::string& n)
		{
			update_movie_state();
		}
		void on_destroy_dumper(const std::string& n)
		{
			update_movie_state();
		}
	private:
		struct dispatch::target<> closenotify;
	} mywcb;

	//If there is a pending load, perform it. Return 1 on successful load, 0 if nothing to load, -1 on load
	//failing.
	int handle_load()
	{
		std::string old_project = CORE().mlogic ? CORE().mlogic.get_mfile().projectid : "";
jumpback:
		if(do_unsafe_rewind && unsafe_rewind_obj) {
			if(!CORE().mlogic)
				return 0;
			uint64_t t = get_utime();
			std::vector<char> s;
			lua_callback_do_unsafe_rewind(s, 0, 0, CORE().mlogic.get_movie(), unsafe_rewind_obj);
			notify_mode_change(false);
			do_unsafe_rewind = false;
			CORE().mlogic.get_mfile().is_savestate = true;
			location_special = SPECIAL_SAVEPOINT;
			update_movie_state();
			messages << "Rewind done in " << (get_utime() - t) << " usec." << std::endl;
			return 1;
		}
		if(pending_new_project != "") {
			std::string id = pending_new_project;
			pending_new_project = "";
			project_info* old = project_get();
			if(old && old->id == id)
				goto nothing_to_do;
			try {
				auto& p = project_load(id);
				project_set(&p);
				if(project_get() != old)
					delete old;
				flush_slotinfo();	//Wrong movie may be stale.
				return 1;
			} catch(std::exception& e) {
				platform::error_message(std::string("Can't switch projects: ") + e.what());
				messages << "Can't switch projects: " << e.what() << std::endl;
				goto nothing_to_do;
			}
nothing_to_do:
			amode = old_mode;
			platform::set_paused(amode == ADVANCE_PAUSE);
			platform::flush_command_queue();
			if(amode == ADVANCE_LOAD)
				goto jumpback;
			return 0;
		}
		if(pending_load != "") {
			bool system_was_corrupt = system_corrupt;
			system_corrupt = false;
			try {
				if(loadmode != LOAD_STATE_BEGINNING && loadmode != LOAD_STATE_ROMRELOAD &&
					!do_load_state(pending_load, loadmode)) {
					if(system_was_corrupt)
						system_corrupt = system_was_corrupt;
					pending_load = "";
					return -1;
				}
				if(loadmode == LOAD_STATE_BEGINNING)
					do_load_rewind();
				if(loadmode == LOAD_STATE_ROMRELOAD)
					do_load_rom();
			} catch(std::exception& e) {
				if(!system_corrupt && system_was_corrupt)
					system_corrupt = true;
				platform::error_message(std::string("Load failed: ") + e.what());
				messages << "Load failed: " << e.what() << std::endl;
			}
			pending_load = "";
			amode = load_paused ? ADVANCE_PAUSE : ADVANCE_AUTO;
			platform::set_paused(load_paused);
			load_paused = false;
			if(!system_corrupt) {
				location_special = SPECIAL_SAVEPOINT;
				update_movie_state();
				platform::flush_command_queue();
				if(amode == ADVANCE_QUIT)
					return -1;
				if(amode == ADVANCE_LOAD)
					goto jumpback;
			}
			if(old_project != (CORE().mlogic ? CORE().mlogic.get_mfile().projectid : ""))
				flush_slotinfo();	//Wrong movie may be stale.
			return 1;
		}
		return 0;
	}

	//If there are pending saves, perform them.
	void handle_saves()
	{
		if(!CORE().mlogic)
			return;
		if(!queued_saves.empty() || (do_unsafe_rewind && !unsafe_rewind_obj)) {
			our_rom.rtype->runtosave();
			for(auto i : queued_saves) {
				do_save_state(i.first, i.second);
				int tmp = -1;
				flush_slotinfo(translate_name_mprefix(i.first, tmp, -1));
			}
			if(do_unsafe_rewind && !unsafe_rewind_obj) {
				uint64_t t = get_utime();
				std::vector<char> s = our_rom.save_core_state(true);
				uint64_t secs = CORE().mlogic.get_mfile().rtc_second;
				uint64_t ssecs = CORE().mlogic.get_mfile().rtc_subsecond;
				lua_callback_do_unsafe_rewind(s, secs, ssecs, CORE().mlogic.get_movie(),
					NULL);
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
			platform::set_paused(true);
			platform::flush_command_queue();
			handle_load();
			if(amode == ADVANCE_QUIT)
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
	platform::system_thread_available(true);
	//Basic initialization.
	dispatch_set_error_streams(&messages.getstream());
	emulation_thread = threads::this_id();
	jukebox_size_listener jlistener;
	CORE().commentary.init();
	init_special_screens();
	our_rom = rom;
	init_main_callbacks();
	initialize_all_builtin_c_cores();
	core_core::install_all_handlers();

	//Load our given movie.
	bool first_round = false;
	bool just_did_loadstate = false;
	bool used = false;
	try {
		do_load_state(initial, LOAD_STATE_INITIAL, used);
		location_special = SPECIAL_SAVEPOINT;
		update_movie_state();
		first_round = CORE().mlogic.get_mfile().is_savestate;
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
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt);
	}

	platform::set_paused(initial.start_paused);
	amode = initial.start_paused ? ADVANCE_PAUSE : ADVANCE_AUTO;
	stop_at_frame_active = false;

	lua_run_startup_scripts();

	uint64_t time_x = get_utime();
	while(amode != ADVANCE_QUIT || !queued_saves.empty()) {
		if(handle_corrupt()) {
			first_round = CORE().mlogic && CORE().mlogic.get_mfile().is_savestate;
			just_did_loadstate = first_round;
			continue;
		}
		ack_frame_tick(get_utime());
		if(amode == ADVANCE_SKIPLAG_PENDING)
			amode = ADVANCE_SKIPLAG;

		if(!first_round) {
			controls.reset_framehold();
			if(!macro_hold_1 && !macro_hold_2) {
				controls.advance_macros();
			}
			macro_hold_2 = false;
			CORE().mlogic.get_movie().get_pollcounters().set_framepflag(false);
			CORE().mlogic.new_frame_starting(amode == ADVANCE_SKIPLAG);
			CORE().mlogic.get_movie().get_pollcounters().set_framepflag(true);
			if(amode == ADVANCE_QUIT && queued_saves.empty())
				break;
			handle_saves();
			int r = 0;
			if(queued_saves.empty())
				r = handle_load();
			if(r > 0 || system_corrupt) {
				CORE().mlogic.get_movie().get_pollcounters().set_framepflag(
					CORE().mlogic.get_mfile().is_savestate);
				first_round = CORE().mlogic.get_mfile().is_savestate;
				if(system_corrupt)
					amode = ADVANCE_PAUSE;
				else
					amode = old_mode;
				stop_at_frame_active = false;
				just_did_loadstate = first_round;
				controls.reset_framehold();
				debug_fire_callback_frame(CORE().mlogic.get_movie().get_current_frame(),
					true);
				continue;
			} else if(r < 0) {
				//Not exactly desriable, but this at least won't desync.
				stop_at_frame_active = false;
				if(amode == ADVANCE_QUIT)
					return;
				amode = ADVANCE_PAUSE;
			}
		}
		if(just_did_loadstate) {
			//If we just loadstated, we are up to date.
			if(amode == ADVANCE_QUIT)
				break;
			platform::set_paused(amode == ADVANCE_PAUSE);
			platform::flush_command_queue();
			//We already have done the reset this frame if we are going to do one at all.
			CORE().mlogic.get_movie().set_controls(CORE().mlogic.update_controls(true));
			CORE().mlogic.get_movie().set_all_DRDY();
			just_did_loadstate = false;
		}
		frame_irq_time = get_utime() - time_x;
		debug_fire_callback_frame(CORE().mlogic.get_movie().get_current_frame(), false);
		our_rom.rtype->emulate();
		random_mix_timing_entropy();
		time_x = get_utime();
		if(amode == ADVANCE_AUTO)
			platform::wait(to_wait_frame(get_utime()));
		first_round = false;
		lua_callback_do_frame();
	}
	information_dispatch::do_dump_end();
	core_core::uninstall_all_handlers();
	CORE().commentary.kill();
	platform::system_thread_available(false);
	//Kill some things to avoid crashes.
	debug_core_change();
	project_set(NULL, true);
	CORE().mwatch.clear_multi(CORE().mwatch.enumerate());
}

void set_stop_at_frame(uint64_t frame)
{
	stop_at_frame = frame;
	stop_at_frame_active = (frame != 0);
	amode = ADVANCE_AUTO;
	platform::set_paused(false);
}

void do_flush_slotinfo()
{
	flush_slotinfo();
}

void switch_projects(const std::string& newproj)
{
	pending_new_project = newproj;
	amode = ADVANCE_LOAD;
	old_mode = ADVANCE_PAUSE;
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
		load_paused = true;
		mark_pending_load("SOME NONBLANK NAME", LOAD_STATE_ROMRELOAD);
	}
}

void do_break_pause()
{
	break_pause = true;
	update_movie_state();
	while(break_pause) {
		platform::set_paused(true);
		platform::flush_command_queue();
	}
}

void convert_break_to_pause()
{
	if(break_pause) {
		amode = ADVANCE_PAUSE;
		break_pause = false;
		update_movie_state();
	}
}