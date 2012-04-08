#include "core/bsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "lua/lua.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"
#include "interface/core.hpp"

#include <iomanip>
#include <fstream>

#include <boost/filesystem.hpp>

struct moviefile our_movie;
struct loaded_rom* our_rom;
bool system_corrupt;
movie_logic movb;
std::string last_save;

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
	numeric_setting savecompression("savecompression", 0, 9, 7);

	path_setting slotpath_setting("slotpath");

	class projectprefix_setting : public setting
	{
	public:
		bool _set;
		std::string prefix;
		projectprefix_setting() throw(std::bad_alloc)
			: setting("$project")
		{
			_set = false;
		}
		void blank() throw(std::bad_alloc, std::runtime_error)
		{
			_set = false;
		}
		bool is_set() throw()
		{
			return _set;
		}
		void set(const std::string& value) throw(std::bad_alloc, std::runtime_error)
		{
			prefix = value;
			_set = true;
		}
		std::string get() throw(std::bad_alloc)
		{
			return prefix;
		}
		operator std::string() throw()
		{
			if(_set)
				return prefix + "-";
			else
				return "movieslot";
		}
	} mprefix;

	function_ptr_command<> get_gamename("get-gamename", "Get the game name",
		"Syntax: get-gamename\nPrints the game name\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			messages << "Game name is '" << our_movie.gamename << "'" << std::endl;
		});

	function_ptr_command<const std::string&> set_gamename("set-gamename", "Set the game name",
		"Syntax: set-gamename <name>\nSets the game name to <name>\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			our_movie.gamename = args;
			messages << "Game name changed to '" << our_movie.gamename << "'" << std::endl;
		});

	function_ptr_command<> show_authors("show-authors", "Show the run authors",
		"Syntax: show-authors\nShows the run authors\n",
		[]() throw(std::bad_alloc, std::runtime_error)
		{
			size_t idx = 0;
			for(auto i : our_movie.authors) {
				messages << (idx++) << ": " << i.first << "|" << i.second << std::endl;
			}
			messages << "End of authors list" << std::endl;
		});

	function_ptr_command<const std::string&> add_author("add-author", "Add an author",
		"Syntax: add-author <fullname>\nSyntax: add-author |<nickname>\n"
		"Syntax: add-author <fullname>|<nickname>\nAdds a new author\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto g = split_author(t);
			our_movie.authors.push_back(g);
			messages << (our_movie.authors.size() - 1) << ": " << g.first << "|" << g.second << std::endl;
		});

	function_ptr_command<const std::string&> remove_author("remove-author", "Remove an author",
		"Syntax: remove-author <id>\nRemoves author with ID <id>\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			uint64_t index = parse_value<uint64_t>(t);
			if(index >= our_movie.authors.size())
				throw std::runtime_error("No such author");
			our_movie.authors.erase(our_movie.authors.begin() + index);
		});

	function_ptr_command<const std::string&> edit_author("edit-author", "Edit an author",
		"Syntax: edit-author <authorid> <fullname>\nSyntax: edit-author <authorid> |<nickname>\n"
		"Syntax: edit-author <authorid> <fullname>|<nickname>\nEdits author name\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]+(|[^ \t].*)", t, "Index and author required.");
			uint64_t index = parse_value<uint64_t>(r[1]);
			if(index >= our_movie.authors.size())
				throw std::runtime_error("No such author");
			auto g = split_author(r[2]);
			our_movie.authors[index] = g;
		});

	function_ptr_command<const std::string&> dump_coresave("dump-coresave", "Dump bsnes core state",
		"Syntax: dump-coresave <name>\nDumps core save to <name>\n",
		[](const std::string& name) throw(std::bad_alloc, std::runtime_error) {
			auto x = emucore_serialize();
			x.resize(x.size() - 32);
			std::ofstream y(name.c_str(), std::ios::out | std::ios::binary);
			y.write(&x[0], x.size());
			y.close();
			messages << "Saved core state to " << name << std::endl;
		});

	bool warn_hash_mismatch(const std::string& mhash, const loaded_slot& slot,
		const std::string& name, bool fatal)
	{
		if(mhash == slot.sha256)
			return true;
		if(!fatal) {
			messages << "WARNING: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha256 << std::endl;
			return true;
		} else {
			messages << "ERROR: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha256 << std::endl;
			return false;
		}
	}
}

std::string translate_name_mprefix(std::string original, bool forio)
{
	size_t prefixloc = original.find("${project}");
	if(prefixloc < original.length()) {
		std::string pprf = forio ? (slotpath_setting.get() + "/") : std::string("");
		if(prefixloc == 0)
			return pprf + static_cast<std::string>(mprefix) + original.substr(prefixloc + 10);
		else
			return original.substr(0, prefixloc) + static_cast<std::string>(mprefix) +
				original.substr(prefixloc + 10);
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


//Save state.
void do_save_state(const std::string& filename) throw(std::bad_alloc,
	std::runtime_error)
{
	std::string filename2 = translate_name_mprefix(filename, true);
	lua_callback_pre_save(filename2, true);
	try {
		uint64_t origtime = get_utime();
		if(mprefix._set)
			our_movie.prefix = sanitize_prefix(mprefix.prefix);
		our_movie.is_savestate = true;
		our_movie.sram = save_sram();
		our_movie.rom_sha256 = our_rom->rom.sha256;
		our_movie.romxml_sha256 = our_rom->rom_xml.sha256;
		our_movie.slota_sha256 = our_rom->slota.sha256;
		our_movie.slotaxml_sha256 = our_rom->slota_xml.sha256;
		our_movie.slotb_sha256 = our_rom->slotb.sha256;
		our_movie.slotbxml_sha256 = our_rom->slotb_xml.sha256;
		our_movie.savestate = emucore_serialize();
		get_framebuffer().save(our_movie.screenshot);
		movb.get_movie().save_state(our_movie.projectid, our_movie.save_frame, our_movie.lagged_frames,
			our_movie.pollcounters);
		our_movie.input = movb.get_movie().save();
		our_movie.save(filename2, savecompression);
		uint64_t took = get_utime() - origtime;
		messages << "Saved state '" << filename2 << "' in " << took << " microseconds." << std::endl;
		lua_callback_post_save(filename2, true);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		messages << "Save failed: " << e.what() << std::endl;
		lua_callback_err_save(filename2);
	}
	last_save = boost::filesystem3::absolute(boost::filesystem3::path(filename2)).string();
}

//Save movie.
void do_save_movie(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
{
	std::string filename2 = translate_name_mprefix(filename, true);
	lua_callback_pre_save(filename2, false);
	try {
		uint64_t origtime = get_utime();
		if(mprefix._set)
			our_movie.prefix = sanitize_prefix(static_cast<std::string>(mprefix.prefix));
		our_movie.is_savestate = false;
		our_movie.input = movb.get_movie().save();
		our_movie.save(filename2, savecompression);
		uint64_t took = get_utime() - origtime;
		messages << "Saved movie '" << filename2 << "' in " << took << " microseconds." << std::endl;
		lua_callback_post_save(filename2, false);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "Save failed: " << e.what() << std::endl;
		lua_callback_err_save(filename2);
	}
	last_save = boost::filesystem3::absolute(boost::filesystem3::path(filename2)).string();
}

extern time_t random_seed_value;

void do_load_beginning() throw(std::bad_alloc, std::runtime_error)
{
	SNES::config.random = false;
	SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;

	//Negative return.
	rrdata::add_internal();
	try {
		movb.get_movie().reset_state();
		random_seed_value = our_movie.movie_rtc_second;
		our_rom->load();

		load_sram(our_movie.movie_sram);
		our_movie.rtc_second = our_movie.movie_rtc_second;
		our_movie.rtc_subsecond = our_movie.movie_rtc_subsecond;
		redraw_framebuffer(screen_nosignal);
		lua_callback_do_rewind();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}
	our_movie.is_savestate = false;
	our_movie.host_memory.clear();
	messages << "Movie rewound to beginning." << std::endl;
}

//Load state from loaded movie file (does not catch errors).
void do_load_state(struct moviefile& _movie, int lmode)
{
	bool current_mode = movb.get_movie().readonly_mode();
	if(_movie.force_corrupt)
		throw std::runtime_error("Movie file invalid");
	bool will_load_state = _movie.is_savestate && lmode != LOAD_STATE_MOVIE;
	if(gtype::toromtype(_movie.gametype) != our_rom->rtype) {
		messages << "_movie.gametype = " << _movie.gametype << std::endl;
		messages << "gtype::toromtype(_movie.gametype) = " << gtype::toromtype(_movie.gametype) << std::endl;
		messages << "our_rom->rtype = " << our_rom->rtype << std::endl;
		throw std::runtime_error("ROM types of movie and loaded ROM don't match");
	}
	if(gtype::toromregion(_movie.gametype) != our_rom->orig_region && our_rom->orig_region != REGION_AUTO)
		throw std::runtime_error("NTSC/PAL select of movie and loaded ROM don't match");

	if(_movie.coreversion != emucore_get_version()) {
		if(will_load_state) {
			std::ostringstream x;
			x << "ERROR: Emulator core version mismatch!" << std::endl
				<< "\tThis version: " << emucore_get_version() << std::endl
				<< "\tFile is from: " << _movie.coreversion << std::endl;
			throw std::runtime_error(x.str());
		} else
			messages << "WARNING: Emulator core version mismatch!" << std::endl
				<< "\tThis version: " << emucore_get_version() << std::endl
				<< "\tFile is from: " << _movie.coreversion << std::endl;
	}
	bool rom_ok = true;
	rom_ok = rom_ok & warn_hash_mismatch(_movie.rom_sha256, our_rom->rom, "ROM #1", will_load_state);
	rom_ok = rom_ok & warn_hash_mismatch(_movie.romxml_sha256, our_rom->rom_xml, "XML #1", will_load_state);
	rom_ok = rom_ok & warn_hash_mismatch(_movie.slota_sha256, our_rom->slota, "ROM #2", will_load_state);
	rom_ok = rom_ok & warn_hash_mismatch(_movie.slotaxml_sha256, our_rom->slota_xml, "XML #2", will_load_state);
	rom_ok = rom_ok & warn_hash_mismatch(_movie.slotb_sha256, our_rom->slotb, "ROM #3", will_load_state);
	rom_ok = rom_ok & warn_hash_mismatch(_movie.slotbxml_sha256, our_rom->slotb_xml, "XML #3", will_load_state);
	if(!rom_ok)
		throw std::runtime_error("Incorrect ROM");

	SNES::config.random = false;
	SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;

	movie newmovie;
	if(lmode == LOAD_STATE_PRESERVE)
		newmovie = movb.get_movie();
	else
		newmovie.load(_movie.rerecords, _movie.projectid, _movie.input);

	if(will_load_state)
		newmovie.restore_state(_movie.save_frame, _movie.lagged_frames, _movie.pollcounters, true,
			(lmode == LOAD_STATE_PRESERVE) ? &_movie.input : NULL, _movie.projectid);

	//Negative return.
	rrdata::read_base(_movie.projectid);
	rrdata::read(_movie.c_rrdata);
	rrdata::add_internal();
	try {
		our_rom->region = gtype::toromregion(_movie.gametype);
		random_seed_value = _movie.rtc_second;
		our_rom->load();

		if(will_load_state) {
			//Load the savestate and movie state.
			//Set the core ports in order to avoid port state being reinitialized when loading.
			controls.set_port(0, _movie.port1, true);
			controls.set_port(1, _movie.port2, true);
			emucore_unserialize(_movie.savestate);
		} else {
			load_sram(_movie.movie_sram);
			controls.set_port(0, _movie.port1, true);
			controls.set_port(1, _movie.port2, true);
			_movie.rtc_second = _movie.movie_rtc_second;
			_movie.rtc_subsecond = _movie.movie_rtc_subsecond;
		}
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}

	//Okay, copy the movie data.
	our_movie = _movie;
	if(!our_movie.is_savestate || lmode == LOAD_STATE_MOVIE) {
		our_movie.is_savestate = false;
		our_movie.host_memory.clear();
	}
	if(our_movie.prefix != "") {
		mprefix.prefix = our_movie.prefix;
		mprefix._set = true;
	}
	movb.get_movie() = newmovie;
	//Paint the screen.
	{
		lcscreen tmp;
		if(will_load_state) {
			tmp.load(_movie.screenshot);
			redraw_framebuffer(tmp);
		} else
			redraw_framebuffer(screen_nosignal);
	}
	//Activate RW mode if needed.
	if(lmode == LOAD_STATE_RW)
		movb.get_movie().readonly_mode(false);
	if(lmode == LOAD_STATE_DEFAULT && movb.get_movie().get_frame_count() <= movb.get_movie().get_current_frame())
		movb.get_movie().readonly_mode(false);
	if(lmode == LOAD_STATE_CURRENT && !current_mode)
		movb.get_movie().readonly_mode(false);
	information_dispatch::do_mode_change(movb.get_movie().readonly_mode());
	messages << "ROM Type ";
	switch(our_rom->rtype) {
	case ROMTYPE_SNES:
		messages << "SNES";
		break;
	case ROMTYPE_BSX:
		messages << "BS-X";
		break;
	case ROMTYPE_BSXSLOTTED:
		messages << "BS-X slotted";
		break;
	case ROMTYPE_SUFAMITURBO:
		messages << "Sufami Turbo";
		break;
	case ROMTYPE_SGB:
		messages << "Super Game Boy";
		break;
	default:
		messages << "Unknown";
		break;
	}
	messages << " region ";
	switch(our_rom->region) {
	case REGION_PAL:
		messages << "PAL";
		break;
	case REGION_NTSC:
		messages << "NTSC";
		break;
	default:
		messages << "Unknown";
		break;
	}
	messages << std::endl;
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
		messages << "Rerecords " << _movie.rerecords << " length " << x.str() << " ("
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
	std::string filename2 = translate_name_mprefix(filename, true);
	uint64_t origtime = get_utime();
	lua_callback_pre_load(filename2);
	struct moviefile mfile;
	try {
		mfile = moviefile(filename2);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
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
		messages << "Can't load movie/savestate '" << filename2 << "': " << e.what() << std::endl;
		lua_callback_err_load(filename2);
		return false;
	}
	return true;
}
