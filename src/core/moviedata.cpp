#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/misc.hpp"
#include "core/mainloop.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/random.hpp"
#include "core/romguess.hpp"
#include "core/settings.hpp"
#include "lua/lua.hpp"
#include "library/directory.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/temporary_handle.hpp"
#include "interface/romtype.hpp"
#include <iomanip>
#include <fstream>

struct loaded_rom our_rom;
bool system_corrupt;
std::string last_save;
void update_movie_state();

namespace
{
	settingvar::supervariable<settingvar::model_int<0, 9>> savecompression(lsnes_setgrp, "savecompression",
		"Movie‣Saving‣Compression",  7);
	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> readonly_load_preserves(lsnes_setgrp,
		"preserve_on_readonly_load", "Movie‣Loading‣Preserve on readonly load", true);
	threads::lock mprefix_lock;
	std::string mprefix;
	bool mprefix_valid;

	std::string get_mprefix()
	{
		threads::alock h(mprefix_lock);
		if(!mprefix_valid)
			return "movieslot";
		else
			return mprefix + "-";
	}

	command::fnptr<const std::string&> dump_coresave(lsnes_cmds, "dump-coresave", "Dump bsnes core state",
		"Syntax: dump-coresave <name>\nDumps core save to <name>\n",
		[](const std::string& name) throw(std::bad_alloc, std::runtime_error) {
			auto x = our_rom.save_core_state();
			x.resize(x.size() - 32);
			std::ofstream y(name.c_str(), std::ios::out | std::ios::binary);
			y.write(&x[0], x.size());
			y.close();
			messages << "Saved core state to " << name << std::endl;
		});

	bool warn_hash_mismatch(const std::string& mhash, const fileimage::image& slot,
		const std::string& name, bool fatal)
	{
		if(mhash == slot.sha_256.read())
			return true;
		if(!fatal) {
			messages << "WARNING: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha_256.read() << std::endl;
			return true;
		} else {
			platform::error_message("Can't load state because hashes mismatch");
			messages << "ERROR: " << name << " hash mismatch!" << std::endl
				<< "\tMovie:   " << mhash << std::endl
				<< "\tOur ROM: " << slot.sha_256.read() << std::endl;
			return false;
		}
	}

	void set_mprefix(const std::string& pfx)
	{
		{
			threads::alock h(mprefix_lock);
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

	class _lsnes_pflag_handler : public movie::poll_flag
	{
	public:
		~_lsnes_pflag_handler()
		{
		}
		int get_pflag()
		{
			return our_rom.rtype->get_pflag();
		}
		void set_pflag(int flag)
		{
			our_rom.rtype->set_pflag(flag);
		}
	} lsnes_pflag_handler;
}

std::string get_mprefix_for_project()
{
	return get_mprefix_for_project(lsnes_instance.mlogic ? lsnes_instance.mlogic.get_mfile().projectid : "");
}

void set_mprefix_for_project(const std::string& prjid, const std::string& pfx)
{
	std::string filename = get_config_path() + "/" + safe_filename(prjid) + ".pfx";
	std::ofstream strm(filename);
	strm << pfx << std::endl;
}

void set_mprefix_for_project(const std::string& pfx)
{
	set_mprefix_for_project(lsnes_instance.mlogic ? lsnes_instance.mlogic.get_mfile().projectid : "", pfx);
	set_mprefix(pfx);
}

std::string translate_name_mprefix(std::string original, int& binary, int save)
{
	auto p = project_get();
	regex_results r = regex("\\$SLOT:(.*)", original);
	if(r) {
		if(binary < 0)
			binary = jukebox_dflt_binary(lsnes_instance.settings) ? 1 : 0;
		if(p) {
			uint64_t branch = p->get_current_branch();
			std::string branch_str;
			std::string filename;
			if(branch) branch_str = (stringfmt() << "--" << branch).str();
			filename = p->directory + "/" + p->prefix + "-" + r[1] + branch_str + ".lss";
			while(save < 0 && branch) {
				if(zip::file_exists(filename))
					break;
				branch = p->get_parent_branch(branch);
				branch_str = branch ? ((stringfmt() << "--" << branch).str()) : "";
				filename = p->directory + "/" + p->prefix + "-" + r[1] + branch_str + ".lss";
			}
			return filename;
		} else {
			std::string pprf = lsnes_instance.setcache.get("slotpath") + "/";
			return pprf + get_mprefix() + r[1] + ".lsmv";
		}
	} else {
		if(binary < 0)
			binary = (save ? save_dflt_binary(lsnes_instance.settings) :
				movie_dflt_binary(lsnes_instance.settings)) ? 1 : 0;
		return original;
	}
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
		return directory::absolute_path(path);
	} catch(...) {
		return path;
	}
}

//Save state.
void do_save_state(const std::string& filename, int binary) throw(std::bad_alloc,
	std::runtime_error)
{
	if(!lsnes_instance.mlogic || !lsnes_instance.mlogic.get_mfile().gametype) {
		platform::error_message("Can't save movie without a ROM");
		messages << "Can't save movie without a ROM" << std::endl;
		return;
	}
	auto& target = lsnes_instance.mlogic.get_mfile();
	std::string filename2 = translate_name_mprefix(filename, binary, 1);
	lua_callback_pre_save(filename2, true);
	try {
		uint64_t origtime = get_utime();
		target.is_savestate = true;
		target.sram = our_rom.rtype->save_sram();
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			target.romimg_sha256[i] = our_rom.romimg[i].sha_256.read();
			target.romxml_sha256[i] = our_rom.romxml[i].sha_256.read();
			target.namehint[i] = our_rom.romimg[i].namehint;
		}
		target.savestate = our_rom.save_core_state();
		get_framebuffer().save(target.screenshot);
		lsnes_instance.mlogic.get_movie().save_state(target.projectid, target.save_frame,
			target.lagged_frames, target.pollcounters);
		target.poll_flag = our_rom.rtype->get_pflag();
		auto prj = project_get();
		if(prj) {
			target.gamename = prj->gamename;
			target.authors = prj->authors;
		}
		target.active_macros = controls.get_macro_frames();
		target.save(filename2, savecompression(CORE().settings), binary > 0,
			lsnes_instance.mlogic.get_rrdata());
		uint64_t took = get_utime() - origtime;
		std::string kind = (binary > 0) ? "(binary format)" : "(zip format)";
		messages << "Saved state " << kind << " '" << filename2 << "' in " << took << " microseconds."
			<< std::endl;
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
		p->flush();
	}
}

//Save movie.
void do_save_movie(const std::string& filename, int binary) throw(std::bad_alloc, std::runtime_error)
{
	if(!lsnes_instance.mlogic || !lsnes_instance.mlogic.get_mfile().gametype) {
		platform::error_message("Can't save movie without a ROM");
		messages << "Can't save movie without a ROM" << std::endl;
		return;
	}
	auto& target = lsnes_instance.mlogic.get_mfile();
	std::string filename2 = translate_name_mprefix(filename, binary, 0);
	lua_callback_pre_save(filename2, false);
	try {
		uint64_t origtime = get_utime();
		target.is_savestate = false;
		auto prj = project_get();
		if(prj) {
			target.gamename = prj->gamename;
			target.authors = prj->authors;
		}
		target.active_macros.clear();
		target.save(filename2, savecompression(lsnes_instance.settings), binary > 0,
			lsnes_instance.mlogic.get_rrdata());
		uint64_t took = get_utime() - origtime;
		std::string kind = (binary > 0) ? "(binary format)" : "(zip format)";
		messages << "Saved movie " << kind << " '" << filename2 << "' in " << took << " microseconds."
			<< std::endl;
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
		p->flush();
	}
}

extern time_t random_seed_value;

namespace
{
	void populate_volatile_ram(moviefile& mf, std::list<core_vma_info>& vmas)
	{
		for(auto i : vmas) {
			//Only regions that are marked as volatile, readwrite not special are initializable.
			if(!i.volatile_flag || i.readonly || i.special)
				continue;
			if(!mf.ramcontent.count(i.name))
				continue;
			uint64_t csize = min((uint64_t)mf.ramcontent[i.name].size(), i.size);
			if(i.backing_ram)
				memcpy(i.backing_ram, &mf.ramcontent[i.name][0], csize);
			else
				for(uint64_t o = 0; o < csize; o++)
					i.write(o, mf.ramcontent[i.name][o]);
		}
	}

	std::string format_length(uint64_t mlength)
	{
		std::ostringstream x;
		if(mlength >= 3600000ULL) {
			x << mlength / 3600000ULL << ":";
			mlength %= 3600000ULL;
		}
		x << std::setfill('0') << std::setw(2) << mlength / 60000ULL << ":";
		mlength %= 60000ULL;
		x << std::setfill('0') << std::setw(2) << mlength / 1000ULL << ".";
		mlength %= 1000ULL;
		x << std::setfill('0') << std::setw(3) << mlength;
		return x.str();
	}

	void print_movie_info(moviefile& mov, loaded_rom& rom, rrdata_set& rrd)
	{
		if(rom.rtype)
			messages << "ROM Type " << rom.rtype->get_hname() << " region " << rom.region->get_hname()
				<< std::endl;
		std::string len, rerecs;
		len = format_length(mov.get_movie_length());
		std::string rerecords = mov.is_savestate ? (stringfmt() << rrd.count()).str() : mov.rerecords;
		messages << "Rerecords " << rerecords << " length " << format_length(mov.get_movie_length())
			<< " (" << mov.get_frame_count() << " frames)" << std::endl;
		if(mov.gamename != "")
			messages << "Game name: " << mov.gamename << std::endl;
		for(size_t i = 0; i < mov.authors.size(); i++)
			if(mov.authors[i].second == "")
				messages << "Author: " << mov.authors[i].first << std::endl;
			else if(mov.authors[i].first == "")
				messages << "Author: (" << mov.authors[i].second << ")" << std::endl;
			else
				messages << "Author: " << mov.authors[i].first << " ("
					<< mov.authors[i].second << ")" << std::endl;
	}

	void warn_coretype(const std::string& mov_core, loaded_rom& against, bool loadstate)
	{
		if(!against.rtype)
			return;
		std::string rom_core = against.rtype->get_core_identifier();
		if(mov_core == rom_core)
			return;
		std::ostringstream x;
		x << (loadstate ? "Error: " : "Warning: ");
		x << "Emulator core version mismatch!" << std::endl
			<< "\tCrurrent: " << rom_core << std::endl
			<< "\tMovie:    " << mov_core << std::endl;
		if(loadstate)
			throw std::runtime_error(x.str());
		else
			messages << x.str();
	}

	void warn_roms(moviefile& mov, loaded_rom& against, bool loadstate)
	{
		bool rom_ok = true;
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			if(mov.namehint[i] == "")
				mov.namehint[i] = against.romimg[i].namehint;
			if(mov.romimg_sha256[i] == "")
				mov.romimg_sha256[i] = against.romimg[i].sha_256.read();
			if(mov.romxml_sha256[i] == "")
				mov.romxml_sha256[i] = against.romxml[i].sha_256.read();
			rom_ok = rom_ok & warn_hash_mismatch(mov.romimg_sha256[i], against.romimg[i],
				(stringfmt() << "ROM #" << (i + 1)).str(), loadstate);
			rom_ok = rom_ok & warn_hash_mismatch(mov.romxml_sha256[i], against.romxml[i],
				(stringfmt() << "XML #" << (i + 1)).str(), loadstate);
		}
		if(!rom_ok)
			throw std::runtime_error("Incorrect ROM");
	}

	port_type_set& construct_movie_portset(moviefile& mov, loaded_rom& against)
	{
		auto ctrldata = against.rtype->controllerconfig(mov.settings);
		return port_type_set::make(ctrldata.ports, ctrldata.portindex());
	}

	void handle_load_core(moviefile& _movie, port_type_set& portset, bool will_load_state)
	{
		random_seed_value = _movie.movie_rtc_second;
		if(will_load_state) {
			//If settings possibly change, reload the ROM.
			if(!lsnes_instance.mlogic || lsnes_instance.mlogic.get_mfile().projectid != _movie.projectid)
				our_rom.load(_movie.settings, _movie.movie_rtc_second, _movie.movie_rtc_subsecond);
			//Load the savestate and movie state.
			//Set the core ports in order to avoid port state being reinitialized when loading.
			controls.set_ports(portset);
			our_rom.load_core_state(_movie.savestate);
			our_rom.rtype->set_pflag(_movie.poll_flag);
			controls.set_macro_frames(_movie.active_macros);
		} else {
			//Reload the ROM in order to rewind to the beginning.
			our_rom.load(_movie.settings, _movie.movie_rtc_second, _movie.movie_rtc_subsecond);
			//Load the SRAM and volatile RAM. Or anchor savestate if any.
			controls.set_ports(portset);
			_movie.rtc_second = _movie.movie_rtc_second;
			_movie.rtc_subsecond = _movie.movie_rtc_subsecond;
			if(!_movie.anchor_savestate.empty()) {
				our_rom.load_core_state(_movie.anchor_savestate);
			} else {
				our_rom.rtype->load_sram(_movie.movie_sram);
				std::list<core_vma_info> vmas = our_rom.rtype->vma_list();
				populate_volatile_ram(_movie, vmas);
			}	
			our_rom.rtype->set_pflag(0);
			controls.set_macro_frames(std::map<std::string, uint64_t>());
		}
	}
}

void do_load_rom() throw(std::bad_alloc, std::runtime_error)
{
	bool load_readwrite = !lsnes_instance.mlogic || !lsnes_instance.mlogic.get_movie().readonly_mode();
	if(lsnes_instance.mlogic) {
		port_type_set& portset = construct_movie_portset(lsnes_instance.mlogic.get_mfile(), our_rom);
		//If portset or gametype changes, force readwrite with new movie.
		if(lsnes_instance.mlogic.get_mfile().input->get_types() != portset) load_readwrite = true;
		if(our_rom.rtype != &lsnes_instance.mlogic.get_mfile().gametype->get_type()) load_readwrite = true;
	}

	if(!load_readwrite) {
		//Read-only load. This is pretty simple.
		//Force unlazying of rrdata and count a rerecord.
		if(lsnes_instance.mlogic.get_rrdata().is_lazy())
			lsnes_instance.mlogic.get_rrdata().read_base(rrdata::filename(
				lsnes_instance.mlogic.get_mfile().projectid), false);
		lsnes_instance.mlogic.get_rrdata().add(lsnes_instance.nrrdata());

		port_type_set& portset = construct_movie_portset(lsnes_instance.mlogic.get_mfile(), our_rom);

		try {
			handle_load_core(lsnes_instance.mlogic.get_mfile(), portset, false);
			lsnes_instance.mlogic.get_mfile().gametype = &our_rom.rtype->combine_region(*our_rom.region);
			for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
				lsnes_instance.mlogic.get_mfile().namehint[i] = our_rom.romimg[i].namehint;
				lsnes_instance.mlogic.get_mfile().romimg_sha256[i] = our_rom.romimg[i].sha_256.read();
				lsnes_instance.mlogic.get_mfile().romxml_sha256[i] = our_rom.romxml[i].sha_256.read();
			}
			lsnes_instance.mlogic.get_mfile().is_savestate = false;
			lsnes_instance.mlogic.get_mfile().host_memory.clear();
			lsnes_instance.mlogic.get_movie().reset_state();
			redraw_framebuffer(our_rom.rtype->draw_cover());
			lua_callback_do_rewind();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			system_corrupt = true;
			redraw_framebuffer(screen_corrupt, true);
			throw;
		}
	} else {
		//The more complicated Read-Write case.
		//We need to create a new movie and movie file.
		temporary_handle<moviefile> _movie;
		_movie.get()->force_corrupt = false;
		_movie.get()->gametype = NULL;		//Not yet known.
		_movie.get()->coreversion = our_rom.rtype->get_core_identifier();
		_movie.get()->projectid = get_random_hexstring(40);
		_movie.get()->rerecords = "0";
		_movie.get()->rerecords_mem = 0;
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			_movie.get()->namehint[i] = our_rom.romimg[i].namehint;
			_movie.get()->romimg_sha256[i] = our_rom.romimg[i].sha_256.read();
			_movie.get()->romxml_sha256[i] = our_rom.romxml[i].sha_256.read();
		}
		_movie.get()->is_savestate = false;
		_movie.get()->save_frame = 0;
		_movie.get()->lagged_frames = 0;
		_movie.get()->poll_flag = false;
		_movie.get()->movie_rtc_second = _movie.get()->rtc_second = 1000000000ULL;
		_movie.get()->movie_rtc_subsecond = _movie.get()->rtc_subsecond = 0;
		_movie.get()->start_paused = false;
		_movie.get()->lazy_project_create = true;
		port_type_set& portset2 = construct_movie_portset(*_movie.get(), our_rom);
		_movie.get()->input = NULL;
		_movie.get()->create_default_branch(portset2);

		//Wrap the input in movie.
		temporary_handle<movie> newmovie;
		newmovie.get()->set_movie_data(_movie.get()->input);
		newmovie.get()->load(_movie.get()->rerecords, _movie.get()->projectid, *(_movie.get()->input));
		newmovie.get()->set_pflag_handler(&lsnes_pflag_handler);
		newmovie.get()->readonly_mode(false);
		
		//Force new lazy rrdata and count a rerecord.
		temporary_handle<rrdata_set> rrd;
		rrd.get()->read_base(rrdata::filename(_movie.get()->projectid), true);
		rrd.get()->add(lsnes_instance.nrrdata());
		//Movie data is lost.
		lua_callback_movie_lost("reload");
		try {
			handle_load_core(*_movie.get(), portset2, false);
			_movie.get()->gametype = &our_rom.rtype->combine_region(*our_rom.region);
			redraw_framebuffer(our_rom.rtype->draw_cover());
			lua_callback_do_rewind();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			system_corrupt = true;
			redraw_framebuffer(screen_corrupt, true);
			throw;
		}

		//Set up stuff.
		lsnes_instance.mlogic.set_movie(*(newmovie()), true);
		lsnes_instance.mlogic.set_mfile(*(_movie()), true);
		lsnes_instance.mlogic.set_rrdata(*(rrd()), true);
		set_mprefix(get_mprefix_for_project(lsnes_instance.mlogic.get_mfile().projectid));
	}
	notify_mode_change(lsnes_instance.mlogic.get_movie().readonly_mode());
	notify_mbranch_change();
	messages << "ROM reloaded." << std::endl;
}

void do_load_rewind() throw(std::bad_alloc, std::runtime_error)
{
	if(!lsnes_instance.mlogic || !lsnes_instance.mlogic.get_mfile().gametype)
		throw std::runtime_error("Can't rewind movie without existing movie");

	port_type_set& portset = construct_movie_portset(lsnes_instance.mlogic.get_mfile(), our_rom);

	//Force unlazying of rrdata and count a rerecord.
	if(lsnes_instance.mlogic.get_rrdata().is_lazy())
		lsnes_instance.mlogic.get_rrdata().read_base(rrdata::filename(
			lsnes_instance.mlogic.get_mfile().projectid), false);
	lsnes_instance.mlogic.get_rrdata().add(lsnes_instance.nrrdata());

	//Enter readonly mode.
	lsnes_instance.mlogic.get_movie().readonly_mode(true);
	notify_mode_change(true);
	try {
		handle_load_core(lsnes_instance.mlogic.get_mfile(), portset, false);
		lsnes_instance.mlogic.get_mfile().is_savestate = false;
		lsnes_instance.mlogic.get_mfile().host_memory.clear();
		lsnes_instance.mlogic.get_movie().reset_state();
		redraw_framebuffer(our_rom.rtype->draw_cover());
		lua_callback_do_rewind();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}
	messages << "Movie rewound to beginning." << std::endl;
}

//Load state preserving input. Does not do checks.
void do_load_state_preserve(struct moviefile& _movie)
{
	if(!lsnes_instance.mlogic || !lsnes_instance.mlogic.get_mfile().gametype)
		throw std::runtime_error("Can't load movie preserving input without previous movie");
	if(lsnes_instance.mlogic.get_mfile().projectid != _movie.projectid)
		throw std::runtime_error("Savestate is from different movie");

	bool will_load_state = _movie.is_savestate;
	port_type_set& portset = construct_movie_portset(lsnes_instance.mlogic.get_mfile(), our_rom);

	//Construct a new movie sharing the input data.
	temporary_handle<movie> newmovie;
	newmovie.get()->set_movie_data(lsnes_instance.mlogic.get_mfile().input);
	newmovie.get()->readonly_mode(true);
	newmovie.get()->set_pflag_handler(&lsnes_pflag_handler);

	newmovie.get()->load(_movie.rerecords, _movie.projectid, *_movie.input);
	if(will_load_state)
		newmovie.get()->restore_state(_movie.save_frame, _movie.lagged_frames, _movie.pollcounters, true,
			_movie.input, _movie.projectid);

	//Count a rerecord.
	if(lsnes_instance.mlogic.get_rrdata().is_lazy() && !_movie.lazy_project_create)
		lsnes_instance.mlogic.get_rrdata().read_base(rrdata::filename(_movie.projectid), false);
	lsnes_instance.mlogic.get_rrdata().add(lsnes_instance.nrrdata());

	//Negative return.
	try {
		handle_load_core(_movie, portset, will_load_state);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}

	//Set new movie.
	lsnes_instance.mlogic.set_movie(*(newmovie()), true);

	//Some fields MUST be taken from movie or one gets desyncs.
	lsnes_instance.mlogic.get_mfile().is_savestate = _movie.is_savestate;
	lsnes_instance.mlogic.get_mfile().rtc_second = _movie.rtc_second;
	lsnes_instance.mlogic.get_mfile().rtc_subsecond = _movie.rtc_subsecond;
	std::swap(lsnes_instance.mlogic.get_mfile().host_memory, _movie.host_memory);
	if(!will_load_state)
		lsnes_instance.mlogic.get_mfile().host_memory.clear();

	try {
		//Paint the screen.
		framebuffer::raw tmp;
		if(will_load_state) {
			tmp.load(_movie.screenshot);
			redraw_framebuffer(tmp);
		} else
			redraw_framebuffer(our_rom.rtype->draw_cover());
	} catch(...) {
	}
	delete &_movie;
	notify_mode_change(lsnes_instance.mlogic.get_movie().readonly_mode());
	messages << "Loadstated at earlier point of movie." << std::endl;
}

//Load state from loaded movie file. _movie is consumed.
void do_load_state(struct moviefile& _movie, int lmode, bool& used)
{
	//Some basic sanity checks.
	bool current_mode = lsnes_instance.mlogic ? lsnes_instance.mlogic.get_movie().readonly_mode() : false;
	bool will_load_state = _movie.is_savestate && lmode != LOAD_STATE_MOVIE;

	//Load state all branches and load state initial are the same.
	if(lmode == LOAD_STATE_ALLBRANCH) lmode = LOAD_STATE_INITIAL;

	//Various checks.
	if(_movie.force_corrupt)
		throw std::runtime_error("Movie file invalid");
	if(&(_movie.gametype->get_type()) != our_rom.rtype)
		throw std::runtime_error("ROM types of movie and loaded ROM don't match");
	if(our_rom.orig_region && !our_rom.orig_region->compatible_with(_movie.gametype->get_region()))
		throw std::runtime_error("NTSC/PAL select of movie and loaded ROM don't match");
	warn_coretype(_movie.coreversion, our_rom, will_load_state);
	warn_roms(_movie, our_rom, will_load_state);

	//In certain conditions, trun LOAD_STATE_CURRENT into LOAD_STATE_PRESERVE.
	if(lmode == LOAD_STATE_CURRENT && current_mode && readonly_load_preserves(CORE().settings))
		lmode = LOAD_STATE_PRESERVE;
	//If movie file changes, turn LOAD_STATE_CURRENT into LOAD_STATE_RO
	if(lmode == LOAD_STATE_CURRENT && lsnes_instance.mlogic.get_mfile().projectid != _movie.projectid)
		lmode = LOAD_STATE_RO;

	//Handle preserving load specially.
	if(lmode == LOAD_STATE_PRESERVE) {
		do_load_state_preserve(_movie);
		used = true;
		return;
	}

	//Create a new movie, and if needed, restore the movie state.
	temporary_handle<movie> newmovie;
	newmovie.get()->set_movie_data(_movie.input);
	newmovie.get()->load(_movie.rerecords, _movie.projectid, *_movie.input);
	newmovie.get()->set_pflag_handler(&lsnes_pflag_handler);
	if(will_load_state)
		newmovie.get()->restore_state(_movie.save_frame, _movie.lagged_frames, _movie.pollcounters, true,
			NULL, _movie.projectid);

	//Copy the other branches.
	if(lmode != LOAD_STATE_INITIAL && lsnes_instance.mlogic.get_mfile().projectid == _movie.projectid) {
		newmovie.get()->set_movie_data(NULL);
		auto& oldm = lsnes_instance.mlogic.get_mfile().branches;
		auto& newm = _movie.branches;
		auto oldd = lsnes_instance.mlogic.get_mfile().input;
		auto newd = _movie.input;
		std::string dflt_name;
		//What was the old default name?
		for(auto& i : oldm)
			if(&i.second == oldd)
				dflt_name = i.first;
		//Rename the default to match with old movie if the names differ.
		if(!newm.count(dflt_name) || &newm[dflt_name] != newd)
			newm[dflt_name] = *newd;
		//Copy all other branches.
		for(auto& i : oldm)
			if(i.first != dflt_name)
				newm[i.first] = i.second;
		//Delete branches that didn't exist.
		for(auto i = newm.begin(); i != newm.end();) {
			if(oldm.count(i->first))
				i++;
			else
				i = newm.erase(i);
		}
		_movie.input = &newm[dflt_name];
		newmovie.get()->set_movie_data(_movie.input);
	}

	port_type_set& portset = construct_movie_portset(_movie, our_rom);

	temporary_handle<rrdata_set> rrd;
	bool new_rrdata = false;
	//Count a rerecord (against new or old movie).
	if(!lsnes_instance.mlogic || _movie.projectid != lsnes_instance.mlogic.get_mfile().projectid) {
		rrd.get()->read_base(rrdata::filename(_movie.projectid), _movie.lazy_project_create);
		rrd.get()->read(_movie.c_rrdata);
		rrd.get()->add(lsnes_instance.nrrdata());
		new_rrdata = true;
	} else {
		//Unlazy rrdata if needed.
		if(lsnes_instance.mlogic.get_rrdata().is_lazy() && !_movie.lazy_project_create)
			lsnes_instance.mlogic.get_rrdata().read_base(rrdata::filename(_movie.projectid), false);
		lsnes_instance.mlogic.get_rrdata().add(lsnes_instance.nrrdata());
	}
	//Negative return.
	try {
		our_rom.region = _movie.gametype ? &(_movie.gametype->get_region()) : NULL;
		handle_load_core(_movie, portset, will_load_state);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		system_corrupt = true;
		redraw_framebuffer(screen_corrupt, true);
		throw;
	}

	//If loaded a movie, clear the is savestate and rrdata.
	if(!will_load_state) {
		_movie.is_savestate = false;
		_movie.host_memory.clear();
	}

	lua_callback_movie_lost("load");

	//Copy the data.
	if(new_rrdata) lsnes_instance.mlogic.set_rrdata(*(rrd()), true);
	lsnes_instance.mlogic.set_movie(*newmovie(), true);
	lsnes_instance.mlogic.set_mfile(_movie, true);
	used = true;

	set_mprefix(get_mprefix_for_project(lsnes_instance.mlogic.get_mfile().projectid));

	//Activate RW mode if needed.
	auto& m = lsnes_instance.mlogic.get_movie();
	if(lmode == LOAD_STATE_RW)
		m.readonly_mode(false);
	if(lmode == LOAD_STATE_DEFAULT && !current_mode && m.get_frame_count() <= m.get_current_frame())
		m.readonly_mode(false);
	if(lmode == LOAD_STATE_INITIAL && m.get_frame_count() <= m.get_current_frame())
		m.readonly_mode(false);
	if(lmode == LOAD_STATE_CURRENT && !current_mode)
		m.readonly_mode(false);

	//Paint the screen.
	{
		framebuffer::raw tmp;
		if(will_load_state) {
			tmp.load(_movie.screenshot);
			redraw_framebuffer(tmp);
		} else
			redraw_framebuffer(our_rom.rtype->draw_cover());
	}

	notify_mode_change(m.readonly_mode());
	print_movie_info(_movie, our_rom, lsnes_instance.mlogic.get_rrdata());
	notify_mbranch_change();
}


void try_request_rom(const std::string& moviefile)
{
	moviefile::brief_info info(moviefile);
	auto sysregs = core_sysregion::find_matching(info.sysregion);
	rom_request req;
	req.selected = 0;
	size_t idx = 0;
	req.core_guessed = false;
	for(auto i : sysregs) {
		//FIXME: Do something with this?
		//if(i->get_type().get_biosname() != "" && info.hash[1] != "")
		//	has_bios = true;
		req.cores.push_back(&i->get_type());
		if(i->get_type().get_core_identifier() == info.corename) {
			req.selected = idx;
			req.core_guessed = true;
		}
		idx++;
	}
	if(req.cores.empty())
		throw std::runtime_error("No known core can load movie of type '" + info.sysregion + "'");
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		req.guessed[i] = false;
		req.has_slot[i] = (info.hash[i] != "");
		req.filename[i] = info.hint[i];
		req.hash[i] = info.hash[i];
		req.hashxml[i] = info.hashxml[i];
	}
	try_guess_roms(req);
	req.canceled = true;
	graphics_driver_request_rom(req);
	if(req.canceled)
		throw std::runtime_error("Canceled loading ROM");
	//Try to load the ROM using specified core.
	if(req.selected >= req.cores.size())
		throw std::runtime_error("Invalid ROM type selected");
	core_type* selected_core = req.cores[req.selected];
	loaded_rom newrom(req.filename, selected_core->get_core_identifier(), selected_core->get_iname(), "");
	our_rom = newrom;
	notify_core_change();
}

//Load state
bool do_load_state(const std::string& filename, int lmode)
{
	int tmp = -1;
	std::string filename2 = translate_name_mprefix(filename, tmp, -1);
	uint64_t origtime = get_utime();
	lua_callback_pre_load(filename2);
	struct moviefile* mfile = NULL;
	bool used = false;
	try {
		if(our_rom.rtype->isnull())
			try_request_rom(filename2);
		mfile = new moviefile(filename2, *our_rom.rtype);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		platform::error_message(std::string("Can't read movie/savestate: ") + e.what());
		messages << "Can't read movie/savestate '" << filename2 << "': " << e.what() << std::endl;
		lua_callback_err_load(filename2);
		return false;
	}
	try {
		do_load_state(*mfile, lmode, used);
		uint64_t took = get_utime() - origtime;
		messages << "Loaded '" << filename2 << "' in " << took << " microseconds." << std::endl;
		lua_callback_post_load(filename2, lsnes_instance.mlogic.get_mfile().is_savestate);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		if(!used)
			delete mfile;
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
	lsnes_instance.mlogic.get_rrdata().read_base(rrdata::filename(lsnes_instance.mlogic.get_mfile().projectid),
		false);
	lsnes_instance.mlogic.get_rrdata().add(lsnes_instance.nrrdata());
	lsnes_instance.mlogic.get_mfile().rtc_second = secs;
	lsnes_instance.mlogic.get_mfile().rtc_subsecond = ssecs;
	our_rom.load_core_state(state, true);
}

rrdata::rrdata()
	: init(false)
{
}

rrdata_set::instance rrdata::operator()()
{
	if(!init)
		next = rrdata_set::instance(get_random_hexstring(2 * RRDATA_BYTES));
	init = true;
	return next++;
}

std::string rrdata::filename(const std::string& projectid)
{
	return get_config_path() + "/" + projectid + ".rr";
}
