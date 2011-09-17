#include "moviedata.hpp"
#include "command.hpp"
#include "lua.hpp"
#include "misc.hpp"
#include "framebuffer.hpp"
#include <iomanip>
#include "lsnes.hpp"
#include "window.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>
#include "rrdata.hpp"
#include "settings.hpp"
#include "controller.hpp"

struct moviefile our_movie;
struct loaded_rom* our_rom;
bool system_corrupt;
movie_logic movb;

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


	function_ptr_command get_gamename("get-gamename", "Get the game name",
		"Syntax: get-gamename\nPrints the game name\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			messages << "Game name is '" << our_movie.gamename << "'" << std::endl;
		});

	function_ptr_command show_authors("show-authors", "Show the run authors",
		"Syntax: show-authors\nShows the run authors\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			size_t idx = 0;
			for(auto i = our_movie.authors.begin(); i != our_movie.authors.end(); i++) {
				messages << (idx++) << ": " << i->first << "|" << i->second << std::endl;
			}
			messages << "End of authors list" << std::endl;
		});

	function_ptr_command add_author("add-author", "Add an author",
		"Syntax: add-author <fullname>\nSyntax: add-author |<nickname>\n"
		"Syntax: add-author <fullname>|<nickname>\nAdds a new author\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			auto g = split_author(t.tail());
			our_movie.authors.push_back(g);
			messages << (our_movie.authors.size() - 1) << ": " << g.first << "|" << g.second << std::endl;
		});

	function_ptr_command remove_author("remove-author", "Remove an author",
		"Syntax: remove-author <id>\nRemoves author with ID <id>\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			uint64_t index = parse_value<uint64_t>(t.tail());
			if(index >= our_movie.authors.size())
				throw std::runtime_error("No such author");
			our_movie.authors.erase(our_movie.authors.begin() + index);
		});

	function_ptr_command edit_author("edit-author", "Edit an author",
		"Syntax: edit-author <authorid> <fullname>\nSyntax: edit-author <authorid> |<nickname>\n"
		"Syntax: edit-author <authorid> <fullname>|<nickname>\nEdits author name\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			uint64_t index = parse_value<uint64_t>(t);
			if(index >= our_movie.authors.size())
				throw std::runtime_error("No such author");
			auto g = split_author(t.tail());
			our_movie.authors[index] = g;
		});

	void warn_hash_mismatch(const std::string& mhash, const loaded_slot& slot,
		const std::string& name)
	{
		if(mhash != slot.sha256) {
			messages << "WARNING: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha256 << std::endl;
		}
	}
}

std::pair<std::string, std::string> split_author(const std::string& author) throw(std::bad_alloc,
	std::runtime_error)
{
	std::string _author = author;
	std::string fullname;
	std::string nickname;
	size_t split = _author.find_first_of("|");
	if(!split) {
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
	lua_callback_pre_save(filename, true);
	try {
		uint64_t origtime = get_ticks_msec();
		our_movie.is_savestate = true;
		our_movie.sram = save_sram();
		our_movie.savestate = save_core_state();
		framebuffer.save(our_movie.screenshot);
		auto s = movb.get_movie().save_state();
		our_movie.movie_state.resize(s.size());
		memcpy(&our_movie.movie_state[0], &s[0], s.size());
		our_movie.input = movb.get_movie().save();
		our_movie.save(filename, savecompression);
		uint64_t took = get_ticks_msec() - origtime;
		messages << "Saved state '" << filename << "' in " << took << "ms." << std::endl;
		lua_callback_post_save(filename, true);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		messages << "Save failed: " << e.what() << std::endl;
		lua_callback_err_save(filename);
	}
}

//Save movie.
void do_save_movie(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
{
	lua_callback_pre_save(filename, false);
	try {
		uint64_t origtime = get_ticks_msec();
		our_movie.is_savestate = false;
		our_movie.input = movb.get_movie().save();
		our_movie.save(filename, savecompression);
		uint64_t took = get_ticks_msec() - origtime;
		messages << "Saved movie '" << filename << "' in " << took << "ms." << std::endl;
		lua_callback_post_save(filename, false);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "Save failed: " << e.what() << std::endl;
		lua_callback_err_save(filename);
	}
}

//Load state from loaded movie file (does not catch errors).
void do_load_state(struct moviefile& _movie, int lmode)
{
	bool will_load_state = _movie.is_savestate && lmode != LOAD_STATE_MOVIE;
	if(gtype::toromtype(_movie.gametype) != our_rom->rtype)
		throw std::runtime_error("ROM types of movie and loaded ROM don't match");
	if(gtype::toromregion(_movie.gametype) != our_rom->orig_region && our_rom->orig_region != REGION_AUTO)
		throw std::runtime_error("NTSC/PAL select of movie and loaded ROM don't match");

	if(_movie.coreversion != bsnes_core_version) {
		if(will_load_state) {
			std::ostringstream x;
			x << "ERROR: Emulator core version mismatch!" << std::endl
				<< "\tThis version: " << bsnes_core_version << std::endl
				<< "\tFile is from: " << _movie.coreversion << std::endl;
			throw std::runtime_error(x.str());
		} else
			messages << "WARNING: Emulator core version mismatch!" << std::endl
				<< "\tThis version: " << bsnes_core_version << std::endl
				<< "\tFile is from: " << _movie.coreversion << std::endl;
	}
	warn_hash_mismatch(_movie.rom_sha256, our_rom->rom, "ROM #1");
	warn_hash_mismatch(_movie.romxml_sha256, our_rom->rom_xml, "XML #1");
	warn_hash_mismatch(_movie.slota_sha256, our_rom->slota, "ROM #2");
	warn_hash_mismatch(_movie.slotaxml_sha256, our_rom->slota_xml, "XML #2");
	warn_hash_mismatch(_movie.slotb_sha256, our_rom->slotb, "ROM #3");
	warn_hash_mismatch(_movie.slotbxml_sha256, our_rom->slotb_xml, "XML #3");

	SNES::config.random = false;
	SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;

	movie newmovie;
	if(lmode == LOAD_STATE_PRESERVE)
		newmovie = movb.get_movie();
	else
		newmovie.load(_movie.rerecords, _movie.projectid, _movie.input);

	if(will_load_state) {
		std::vector<unsigned char> tmp;
		tmp.resize(_movie.movie_state.size());
		memcpy(&tmp[0], &_movie.movie_state[0], tmp.size());
		newmovie.restore_state(tmp, true);
	}

	//Negative return.
	rrdata::read_base(_movie.projectid);
	rrdata::add_internal();
	try {
		our_rom->region = gtype::toromregion(_movie.gametype);
		our_rom->load();

		if(_movie.is_savestate && lmode != LOAD_STATE_MOVIE) {
			//Load the savestate and movie state.
			controller_set_port_type(0, _movie.port1);
			controller_set_port_type(1, _movie.port2);
			load_core_state(_movie.savestate);
			framebuffer.load(_movie.screenshot);
		} else {
			load_sram(_movie.movie_sram);
			controller_set_port_type(0, _movie.port1);
			controller_set_port_type(1, _movie.port2);
			framebuffer = screen_nosignal;
		}
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		framebuffer = screen_corrupt;
		throw;
	}

	//Okay, copy the movie data.
	our_movie = _movie;
	if(!our_movie.is_savestate || lmode == LOAD_STATE_MOVIE) {
		our_movie.is_savestate = false;
		our_movie.host_memory.clear();
	}
	movb.get_movie() = newmovie;
	//Activate RW mode if needed.
	if(lmode == LOAD_STATE_RW)
		movb.get_movie().readonly_mode(false);
	if(lmode == LOAD_STATE_DEFAULT && !(movb.get_movie().get_frame_count()))
		movb.get_movie().readonly_mode(false);
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
void do_load_state(const std::string& filename, int lmode)
{
	uint64_t origtime = get_ticks_msec();
	lua_callback_pre_load(filename);
	struct moviefile mfile;
	try {
		mfile = moviefile(filename);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "Can't read movie/savestate '" << filename << "': " << e.what() << std::endl;
		lua_callback_err_load(filename);
		return;
	}
	try {
		do_load_state(mfile, lmode);
		uint64_t took = get_ticks_msec() - origtime;
		messages << "Loaded '" << filename << "' in " << took << "ms." << std::endl;
		lua_callback_post_load(filename, our_movie.is_savestate);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "Can't load movie/savestate '" << filename << "': " << e.what() << std::endl;
		lua_callback_err_load(filename);
		return;
	}
}
