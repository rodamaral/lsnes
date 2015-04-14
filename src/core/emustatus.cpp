#include "cmdhelp/loadsave.hpp"
#include "core/advdumper.hpp"
#include "core/audioapi.hpp"
#include "core/dispatch.hpp"
#include "core/emustatus.hpp"
#include "core/framerate.hpp"
#include "core/inthread.hpp"
#include "core/jukebox.hpp"
#include "core/memorywatch.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/multitrack.hpp"
#include "core/project.hpp"
#include "core/rom.hpp"
#include "core/runmode.hpp"
#include "lua/lua.hpp"

#include <sstream>

const int _lsnes_status::pause_none = 0;
const int _lsnes_status::pause_normal = 1;
const int _lsnes_status::pause_break = 2;
const uint64_t _lsnes_status::subframe_savepoint = 0xFFFFFFFFFFFFFFFEULL;
const uint64_t _lsnes_status::subframe_video = 0xFFFFFFFFFFFFFFFFULL;

slotinfo_cache::slotinfo_cache(movie_logic& _mlogic, command::group& _cmd)
	: mlogic(_mlogic), cmd(_cmd),
	flushcmd(cmd, CLOADSAVE::flushslots, [this]() { this->flush(); })
{
}

std::string slotinfo_cache::get(const std::string& _filename)
{
	std::string filename = resolve_relative_path(_filename);
	if(!cache.count(filename)) {
		std::ostringstream out;
		try {
			moviefile::brief_info info(filename);
			if(!mlogic)
				out << "No movie";
			else if(mlogic.get_mfile().projectid == info.projectid)
				out << info.rerecords << "R/" << info.current_frame << "F";
			else
				out << "Wrong movie";
		} catch(...) {
			out << "Nonexistent";
		}
		cache[filename] = out.str();
	}
	return cache[filename];
}

void slotinfo_cache::flush(const std::string& _filename)
{
	cache.erase(resolve_relative_path(_filename));
}

void slotinfo_cache::flush()
{
	cache.clear();
}

status_updater::status_updater(project_state& _project, movie_logic& _mlogic, voice_commentary& _commentary,
	triplebuffer::triplebuffer<_lsnes_status>& _status, emulator_runmode& _runmode, master_dumper& _mdumper,
	save_jukebox& _jukebox, slotinfo_cache& _slotcache, framerate_regulator& _framerate,
	controller_state& _controls, multitrack_edit& _mteditor, lua_state& _lua2, loaded_rom& _rom,
	memwatch_set& _mwatch, emulator_dispatch& _dispatch)
	: project(_project), mlogic(_mlogic), commentary(_commentary), status(_status), runmode(_runmode),
	mdumper(_mdumper), jukebox(_jukebox), slotcache(_slotcache), framerate(_framerate), controls(_controls),
	mteditor(_mteditor), lua2(_lua2), rom(_rom), mwatch(_mwatch), dispatch(_dispatch)
{
}


void status_updater::update()
{
	auto p = project.get();
	bool readonly = false;
	{
		uint64_t magic[4];
		rom.region_fill_framerate_magic(magic);
		if(mlogic)
			commentary.frame_number(mlogic.get_movie().get_current_frame(),
				1.0 * magic[1] / magic[0]);
		else
			commentary.frame_number(0, 60.0);	//Default.
	}
	auto& _status = status.get_write();
	try {
		if(mlogic && !runmode.is_corrupt()) {
			_status.movie_valid = true;
			_status.curframe = mlogic.get_movie().get_current_frame();
			_status.length = mlogic.get_movie().get_frame_count();
			_status.lag = mlogic.get_movie().get_lag_frames();
			if(runmode.get_point() == emulator_runmode::P_START)
				_status.subframe = 0;
			else if(runmode.get_point() == emulator_runmode::P_SAVE)
				_status.subframe = _lsnes_status::subframe_savepoint;
			else if(runmode.get_point() == emulator_runmode::P_VIDEO)
				_status.subframe = _lsnes_status::subframe_video;
			else
				_status.subframe = mlogic.get_movie().next_poll_number();
		} else {
			_status.movie_valid = false;
			_status.curframe = 0;
			_status.length = 0;
			_status.lag = 0;
			_status.subframe = 0;
		}
		_status.dumping = (mdumper.get_dumper_count() > 0);
		if(runmode.is_paused_break())
			_status.pause = _lsnes_status::pause_break;
		else if(runmode.is_paused_normal())
			_status.pause = _lsnes_status::pause_normal;
		else
			_status.pause = _lsnes_status::pause_none;
		if(mlogic) {
			auto& mo = mlogic.get_movie();
			readonly = mo.readonly_mode();
			if(runmode.is_corrupt())
				_status.mode = 'C';
			else if(!readonly)
				_status.mode = 'R';
			else if(mo.get_frame_count() >= mo.get_current_frame())
				_status.mode = 'P';
			else
				_status.mode = 'F';
		}
		try {
			_status.saveslot_valid = true;
			int tmp = -1;
			std::string sfilen = translate_name_mprefix(jukebox.get_slot_name(), tmp, -1);
			_status.saveslot = jukebox.get_slot() + 1;
			_status.slotinfo = utf8::to32(slotcache.get(sfilen));
		} catch(...) {
			_status.saveslot_valid = false;
		}
		_status.branch_valid = (p != NULL);
		if(p) _status.branch = utf8::to32(p->get_branch_string());

		std::string cur_branch = mlogic ? mlogic.get_mfile().current_branch() :
			"";
		_status.mbranch_valid = (cur_branch != "");
		_status.mbranch = utf8::to32(cur_branch);

		_status.speed = (unsigned)(100 * framerate.get_realized_multiplier() + 0.5);

		if(mlogic && !runmode.is_corrupt()) {
			time_t timevalue = static_cast<time_t>(mlogic.get_mfile().dyn.rtc_second);
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

		portctrl::frame c;
		if(!mteditor.any_records())
			c = mlogic.get_movie().get_controls();
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
			if(readonly && mteditor.is_enabled()) {
				multitrack_edit::state st = mteditor.get(pindex.first, pindex.second);
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
		_status.lvars = lua2.get_watch_vars();
		//Memory watches.
		_status.mvars = mwatch.get_window_vars();

		_status.valid = true;
	} catch(...) {
	}
	status.put_write();
	dispatch.status_update();
}
