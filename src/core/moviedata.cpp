#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "lua/lua.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"
#include "interface/romtype.hpp"

#include <iomanip>
#include <fstream>

#include <boost/filesystem.hpp>

#ifdef BOOST_FILESYSTEM3
namespace boost_fs = boost::filesystem3;
#else
namespace boost_fs = boost::filesystem;
#endif

struct moviefile our_movie;
struct loaded_rom* our_rom;
bool system_corrupt;
movie_logic movb;
std::string last_save;
void update_movie_state();

extern "C"
{
	time_t __wrap_time(time_t* t)
	{
		time_t v = static_cast<time_t>(our_movie.rtc_second);
		if(t)
			*t = v;
		return v;
	}
}

std::vector<char>& get_host_memory()
{
	return our_movie.host_memory;
}

movie& get_movie()
{
	return movb.get_movie();
}

namespace
{
	setting_var<setting_var_model_int<0, 9>> savecompression(lsnes_vset, "savecompression", "Movie‣Compression",
		7);
	setting_var<setting_var_model_bool<setting_yes_no>> readonly_load_preserves(lsnes_vset,
		"preserve_on_readonly_load", "Movie‣Preserve on readonly load", true);
	mutex_class mprefix_lock;
	std::string mprefix;
	bool mprefix_valid;

	std::string get_mprefix()
	{
		umutex_class h(mprefix_lock);
		if(!mprefix_valid)
			return "movieslot";
		else
			return mprefix + "-";
	}

	function_ptr_command<const std::string&> dump_coresave(lsnes_cmd, "dump-coresave", "Dump bsnes core state",
		"Syntax: dump-coresave <name>\nDumps core save to <name>\n",
		[](const std::string& name) throw(std::bad_alloc, std::runtime_error) {
			auto x = our_rom->save_core_state();
			x.resize(x.size() - 32);
			std::ofstream y(name.c_str(), std::ios::out | std::ios::binary);
			y.write(&x[0], x.size());
			y.close();
			messages << "Saved core state to " << name << std::endl;
		});

	bool warn_hash_mismatch(const std::string& mhash, const loaded_slot& slot,
		const std::string& name, bool fatal)
	{
		if(mhash == slot.sha_256)
			return true;
		if(!fatal) {
			messages << "WARNING: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha_256 << std::endl;
			return true;
		} else {
			platform::error_message("Can't load state because hashes mismatch");
			messages << "ERROR: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha_256 << std::endl;
			return false;
		}
	}

	void set_mprefix(const std::string& pfx)
	{
		{
			umutex_class h(mprefix_lock);
			mprefix_valid = (pfx != "");
			mprefix = pfx;
		}
		update_movie_state();
	}

	std::string get_mprefix_for_project(const std::string& prjid)
	{
		std::string filename = get_config_path() + "/" + safe_filename(prjid) + ".pfx";
		std::ifstream strm(filename);
		if(!strm)
			return "";
		std::string pfx;
		std::getline(strm, pfx);
		return strip_CR(pfx);
	}
}

std::string get_mprefix_for_project()
{
	return get_mprefix_for_project(our_movie.projectid);
}

void set_mprefix_for_project(const std::string& prjid, const std::string& pfx)
{
	std::string filename = get_config_path() + "/" + safe_filename(prjid) + ".pfx";
	std::ofstream strm(filename);
	strm << pfx << std::endl;
}

void set_mprefix_for_project(const std::string& pfx)
{
	set_mprefix_for_project(our_movie.projectid, pfx);
	set_mprefix(pfx);
}

std::string translate_name_mprefix(std::string original)
{
	auto p = project_get();
	regex_results r;
	if(p && (r = regex("\\$\\{project\\}([0-9]+).lsmv", original))) {
		return p->directory + "/" + p->prefix + "-" + r[1] + ".lss";
	}
	size_t prefixloc = original.find("${project}");
	if(prefixloc < original.length()) {
		std::string pprf = lsnes_vset["slotpath"].str() + "/";
		if(prefixloc == 0)
			return pprf + get_mprefix() + original.substr(prefixloc + 10);
		else
			return original.substr(0, prefixloc) + get_mprefix() + original.substr(prefixloc + 10);
	} else
		return original;
}

std::pair<std::string, std::string> split_author(const std::string& author) throw(std::bad_alloc,
	std::runtime_error)
{
	std::string _author = author;
	std::string fullname;
	std::string nickname;
	size_t split = _author.find_first_of("|");
	if(split >= _author.length()) {
		fullname = _author;
	} else {
		fullname = _author.substr(0, split);
		nickname = _author.substr(split + 1);
	}
	if(fullname == "" && nickname == "")
		throw std::runtime_error("Bad author name");
	return std::make_pair(fullname, nickname);
}

//Resolve relative path.
std::string resolve_relative_path(const std::string& path)
{
	try {
		return boost_fs::absolute(boost_fs::path(path)).string();
	} catch(...) {
		return path;
	}
}

//Save state.
void do_save_state(const std::string& filename) throw(std::bad_alloc,
	std::runtime_error)
{
	if(!our_movie.gametype) {
		platform::error_message("Can't save movie without a ROM");
		messages << "Can't save movie without a ROM" << std::endl;
		return;
	}
	std::string filename2 = translate_name_mprefix(filename);
	lua_callback_pre_save(filename2, true);
	try {
		uint64_t origtime = get_utime();
		our_movie.is_savestate = true;
		our_movie.sram = our_rom->rtype->save_sram();
		for(size_t i = 0; i < sizeof(our_rom->romimg)/sizeof(our_rom->romimg[0]); i++) {
			our_movie.romimg_sha256[i] = our_rom->romimg[i].sha_256;
			our_movie.romxml_sha256[i] = our_rom->romxml[i].sha_256;
		}
		our_movie.savestate = our_rom->save_core_state();
		get_framebuffer().save(our_movie.screenshot);
		movb.get_movie().save_state(our_movie.projectid, our_movie.save_frame, our_movie.lagged_frames,
			our_movie.pollcounters);
		our_movie.input = movb.get_movie().save();
		our_movie.poll_flag = our_rom->rtype->get_pflag();
		auto prj = project_get();
		if(prj) {
			our_movie.gamename = prj->gamename;
			our_movie.authors = prj->authors;
		}
		our_movie.active_macros = controls.get_macro_frames();
		our_movie.save(filename2, savecompression);
		uint64_t took = get_utime() - origtime;
		messages << "Saved state '" << filename2 << "' in " << took << " microseconds." << std::endl;
		lua_callback_post_save(filename2, true);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		platform::error_message(std::string("Save failed: ") + e.what());
		messages << "Save failed: " << e.what() << std::endl;
		lua_callback_err_save(filename2);
	}
	last_save = resolve_relative_path(filename2);
	auto p = project_get();
	if(p) {
		p->last_save = last_save;
		project_flush(p);
	}
}

//Save movie.
void do_save_movie(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
{
	if(!our_movie.gametype) {
		platform::error_message("Can't save movie without a ROM");
		messages << "Can't save movie without a ROM" << std::endl;
		return;
	}
	std::string filename2 = translate_name_mprefix(filename);
	lua_callback_pre_save(filename2, false);
	try {
		uint64_t origtime = get_utime();
		our_movie.is_savestate = false;
		our_movie.input = movb.get_movie().save();
		auto prj = project_get();
		if(prj) {
			our_movie.gamename = prj->gamename;
			our_movie.authors = prj->authors;
		}
		our_movie.active_macros.clear();
		our_movie.save(filename2, savecompression);
		uint64_t took = get_utime() - origtime;
		messages << "Saved movie '" << filename2 << "' in " << took << " microseconds." << std::endl;
		lua_callback_post_save(filename2, false);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		platform::error_message(std::string("Save failed: ") + e.what());
		messages << "Save failed: " << e.what() << std::endl;
		lua_callback_err_save(filename2);
	}
	last_save = resolve_relative_path(filename2);
	auto p = project_get();
	if(p) {
		p->last_save = last_save;
		project_flush(p);
	}
}

extern time_t random_seed_value;

void do_load_beginning(bool reload) throw(std::bad_alloc, std::runtime_error)
{
	bool force_rw = false;
	if(!our_movie.gametype && !reload) {
		platform::error_message("Can't load movie without a ROM");
		messages << "Can't load movie without a ROM" << std::endl;
		return;
	}

	//Negative return.
	if(!reload) {
		//Force unlazy rrdata.
		rrdata::read_base(our_movie.projectid, false);
		rrdata::add_internal();
	} else {
		auto ctrldata = our_rom->rtype->controllerconfig(our_movie.settings);
		port_type_set& portset = port_type_set::make(ctrldata.ports, ctrldata.portindex);
		controls.set_ports(portset);
		if(our_movie.input.get_types() != portset) {
			//The input type changes, so set the types.
			force_rw = true;
			our_movie.input.clear(portset);
			movb.get_movie().load(our_movie.rerecords, our_movie.projectid, our_movie.input);
		}
	}
	try {
		bool ro = movb.get_movie().readonly_mode() && !force_rw;
		movb.get_movie().reset_state();
		random_seed_value = our_movie.movie_rtc_second;
		our_rom->load(our_movie.settings, our_movie.movie_rtc_second, our_movie.movie_rtc_subsecond);
		our_movie.gametype = &our_rom->rtype->combine_region(*our_rom->region);
		if(reload)
			movb.get_movie().readonly_mode(ro);

		our_rom->rtype->load_sram(our_movie.movie_sram);
		our_movie.rtc_second = our_movie.movie_rtc_second;
		our_movie.rtc_subsecond = our_movie.movie_rtc_subsecond;
		if(!our_movie.anchor_savestate.empty())
			our_rom->load_core_state(our_movie.anchor_savestate);
		our_rom->rtype->set_pflag(0);
		controls.set_macro_frames(std::map<std::string, uint64_t>());
		redraw_framebuffer(our_rom->rtype->draw_cover());
		lua_callback_do_rewind();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}
	notify_mode_change(movb.get_movie().readonly_mode());
	our_movie.is_savestate = false;
	our_movie.host_memory.clear();
	if(reload)
		messages << "ROM reloaded." << std::endl;
	else
		messages << "Movie rewound to beginning." << std::endl;
}

//Load state from loaded movie file (does not catch errors).
void do_load_state(struct moviefile& _movie, int lmode)
{
	bool current_mode = movb.get_movie().readonly_mode();
	if(_movie.force_corrupt)
		throw std::runtime_error("Movie file invalid");
	bool will_load_state = _movie.is_savestate && lmode != LOAD_STATE_MOVIE;
	if(our_rom->rtype && &(_movie.gametype->get_type()) != our_rom->rtype)
		throw std::runtime_error("ROM types of movie and loaded ROM don't match");
	if(our_rom->orig_region && !our_rom->orig_region->compatible_with(_movie.gametype->get_region()))
		throw std::runtime_error("NTSC/PAL select of movie and loaded ROM don't match");
	auto _hostmemory = _movie.host_memory;

	if(our_rom->rtype && _movie.coreversion != our_rom->rtype->get_core_identifier()) {
		if(will_load_state) {
			std::ostringstream x;
			x << "ERROR: Emulator core version mismatch!" << std::endl
				<< "\tThis version: " << our_rom->rtype->get_core_identifier() << std::endl
				<< "\tFile is from: " << _movie.coreversion << std::endl;
			throw std::runtime_error(x.str());
		} else
			messages << "WARNING: Emulator core version mismatch!" << std::endl
				<< "\tThis version: " << our_rom->rtype->get_core_identifier() << std::endl
				<< "\tFile is from: " << _movie.coreversion << std::endl;
	}
	bool rom_ok = true;
	for(size_t i = 0; i < sizeof(our_rom->romimg)/sizeof(our_rom->romimg[0]); i++) {
		rom_ok = rom_ok & warn_hash_mismatch(_movie.romimg_sha256[i], our_rom->romimg[i],
			(stringfmt() << "ROM #" << (i + 1)).str(), will_load_state);
		rom_ok = rom_ok & warn_hash_mismatch(_movie.romxml_sha256[i], our_rom->romxml[i],
			(stringfmt() << "XML #" << (i + 1)).str(), will_load_state);
	}
	if(!rom_ok)
		throw std::runtime_error("Incorrect ROM");

	if(lmode == LOAD_STATE_CURRENT && movb.get_movie().readonly_mode() && readonly_load_preserves)
		lmode = LOAD_STATE_PRESERVE;

	movie newmovie;
	if(lmode == LOAD_STATE_PRESERVE)
		newmovie = movb.get_movie();
	else
		newmovie.load(_movie.rerecords, _movie.projectid, _movie.input);

	if(will_load_state)
		newmovie.restore_state(_movie.save_frame, _movie.lagged_frames, _movie.pollcounters, true,
			(lmode == LOAD_STATE_PRESERVE) ? &_movie.input : NULL, _movie.projectid);

	auto ctrldata = our_rom->rtype->controllerconfig(_movie.settings);
	port_type_set& portset = port_type_set::make(ctrldata.ports, ctrldata.portindex);

	//Negative return.
	rrdata::read_base(_movie.projectid, _movie.lazy_project_create);
	rrdata::read(_movie.c_rrdata);
	rrdata::add_internal();
	try {
		our_rom->region = _movie.gametype ? &(_movie.gametype->get_region()) : NULL;
		random_seed_value = _movie.movie_rtc_second;
		our_rom->load(_movie.settings, _movie.movie_rtc_second, _movie.movie_rtc_subsecond);

		if(will_load_state) {
			//Load the savestate and movie state.
			//Set the core ports in order to avoid port state being reinitialized when loading.
			controls.set_ports(portset);
			our_rom->load_core_state(_movie.savestate);
			our_rom->rtype->set_pflag(_movie.poll_flag);
			controls.set_macro_frames(_movie.active_macros);
		} else {
			our_rom->rtype->load_sram(_movie.movie_sram);
			controls.set_ports(portset);
			_movie.rtc_second = _movie.movie_rtc_second;
			_movie.rtc_subsecond = _movie.movie_rtc_subsecond;
			if(!_movie.anchor_savestate.empty())
				our_rom->load_core_state(_movie.anchor_savestate);
			our_rom->rtype->set_pflag(0);
			controls.set_macro_frames(std::map<std::string, uint64_t>());
		}
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}

	//Okay, copy the movie data.
	if(lmode != LOAD_STATE_PRESERVE)
		our_movie = _movie;
	else {
		//Some fields MUST be taken from movie or one gets desyncs.
		our_movie.is_savestate = _movie.is_savestate;
		our_movie.rtc_second = _movie.rtc_second;
		our_movie.rtc_subsecond = _movie.rtc_subsecond;
	}
	if(!our_movie.is_savestate || lmode == LOAD_STATE_MOVIE) {
		our_movie.is_savestate = false;
		our_movie.host_memory.clear();
	} else {
		//Hostmemory must be unconditionally reloaded, even in preserve mode.
		our_movie.host_memory = _hostmemory;
	}
	if(lmode != LOAD_STATE_PRESERVE) {
		set_mprefix(get_mprefix_for_project(our_movie.projectid));
	}
	movb.get_movie() = newmovie;
	//Activate RW mode if needed.
	if(lmode == LOAD_STATE_RW)
		movb.get_movie().readonly_mode(false);
	if(lmode == LOAD_STATE_DEFAULT && !current_mode &&
		movb.get_movie().get_frame_count() <= movb.get_movie().get_current_frame())
		movb.get_movie().readonly_mode(false);
	if(lmode == LOAD_STATE_INITIAL && movb.get_movie().get_frame_count() <= movb.get_movie().get_current_frame())
		movb.get_movie().readonly_mode(false);
	if(lmode == LOAD_STATE_CURRENT && !current_mode)
		movb.get_movie().readonly_mode(false);
	//Paint the screen.
	{
		framebuffer_raw tmp;
		if(will_load_state) {
			tmp.load(_movie.screenshot);
			redraw_framebuffer(tmp);
		} else
			redraw_framebuffer(our_rom->rtype->draw_cover());
	}
	notify_mode_change(movb.get_movie().readonly_mode());
	if(our_rom->rtype)
		messages << "ROM Type " << our_rom->rtype->get_hname() << " region " << our_rom->region->get_hname()
			<< std::endl;
	uint64_t mlength = _movie.get_movie_length();
	{
		mlength += 999999;
		std::ostringstream x;
		if(mlength > 3600000000000) {
			x << mlength / 3600000000000 << ":";
			mlength %= 3600000000000;
		}
		x << std::setfill('0') << std::setw(2) << mlength / 60000000000 << ":";
		mlength %= 60000000000;
		x << std::setfill('0') << std::setw(2) << mlength / 1000000000 << ".";
		mlength %= 1000000000;
		x << std::setfill('0') << std::setw(3) << mlength / 1000000;
		std::string rerecords = _movie.rerecords;
		if(our_movie.is_savestate)
			rerecords = (stringfmt() << rrdata::count()).str();
		messages << "Rerecords " << rerecords << " length " << x.str() << " ("
			<< _movie.get_frame_count() << " frames)" << std::endl;
	}
	if(_movie.gamename != "")
		messages << "Game Name: " << _movie.gamename << std::endl;
	for(size_t i = 0; i < _movie.authors.size(); i++)
		messages << "Author: " << _movie.authors[i].first << "(" << _movie.authors[i].second << ")"
			<< std::endl;
}

//Load state
bool do_load_state(const std::string& filename, int lmode)
{
	std::string filename2 = translate_name_mprefix(filename);
	uint64_t origtime = get_utime();
	lua_callback_pre_load(filename2);
	struct moviefile mfile;
	try {
		mfile = moviefile(filename2, *our_rom->rtype);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		platform::error_message(std::string("Can't read movie/savestate: ") + e.what());
		messages << "Can't read movie/savestate '" << filename2 << "': " << e.what() << std::endl;
		lua_callback_err_load(filename2);
		return false;
	}
	try {
		do_load_state(mfile, lmode);
		uint64_t took = get_utime() - origtime;
		messages << "Loaded '" << filename2 << "' in " << took << " microseconds." << std::endl;
		lua_callback_post_load(filename2, our_movie.is_savestate);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		platform::error_message(std::string("Can't load movie/savestate: ") + e.what());
		messages << "Can't load movie/savestate '" << filename2 << "': " << e.what() << std::endl;
		lua_callback_err_load(filename2);
		return false;
	}
	return true;
}

void mainloop_restore_state(const std::vector<char>& state, uint64_t secs, uint64_t ssecs)
{
	//Force unlazy rrdata.
	rrdata::read_base(our_movie.projectid, false);
	rrdata::add_internal();
	our_movie.rtc_second = secs;
	our_movie.rtc_subsecond = ssecs;
	our_rom->load_core_state(state, true);
}
