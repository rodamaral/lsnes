#include "lsnes.hpp"

#include "cmdhelp/moviedata.hpp"
#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/jukebox.hpp"
#include "core/mainloop.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/runmode.hpp"
#include "core/settings.hpp"
#include "interface/romtype.hpp"
#include "library/directory.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/temporary_handle.hpp"
#include "lua/lua.hpp"

#include <iomanip>
#include <fstream>

std::string last_save;

namespace
{
	settingvar::supervariable<settingvar::model_int<0, 9>> SET_savecompression(lsnes_setgrp, "savecompression",
		"Movie‣Saving‣Compression",  7);
	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> SET_readonly_load_preserves(
		lsnes_setgrp, "preserve_on_readonly_load", "Movie‣Loading‣Preserve on readonly load", true);
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

	command::fnptr<const std::string&> test4(lsnes_cmds, CMOVIEDATA::panic,
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
		auto& core = CORE();
		if(*core.mlogic) emerg_save_movie(core.mlogic->get_mfile(), core.mlogic->get_rrdata());
	});

	command::fnptr<const std::string&> CMD_dump_coresave(lsnes_cmds, CMOVIEDATA::dumpcore,
		[](const std::string& name) throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			auto x = core.rom->save_core_state();
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
		auto& core = CORE();
		{
			threads::alock h(mprefix_lock);
			mprefix_valid = (pfx != "");
			mprefix = pfx;
		}
		core.supdater->update();
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

	void set_gameinfo(moviefile& mfile)
	{
		master_dumper::gameinfo gi;
		gi.gamename = mfile.gamename;
		gi.length = mfile.get_movie_length() / 1000.0;
		gi.rerecords = mfile.rerecords;
		gi.authors = mfile.authors;
		CORE().mdumper->on_gameinfo_change(gi);
	}

	class _lsnes_pflag_handler : public movie::poll_flag
	{
	public:
		~_lsnes_pflag_handler()
		{
		}
		int get_pflag()
		{
			return CORE().rom->get_pflag();
		}
		void set_pflag(int flag)
		{
			CORE().rom->set_pflag(flag);
		}
	} lsnes_pflag_handler;
}

std::string get_mprefix_for_project()
{
	auto& core = CORE();
	return get_mprefix_for_project(*core.mlogic ? core.mlogic->get_mfile().projectid : "");
}

void set_mprefix_for_project(const std::string& prjid, const std::string& pfx)
{
	std::string filename = get_config_path() + "/" + safe_filename(prjid) + ".pfx";
	std::ofstream strm(filename);
	strm << pfx << std::endl;
}

void set_mprefix_for_project(const std::string& pfx)
{
	auto& core = CORE();
	set_mprefix_for_project(*core.mlogic ? core.mlogic->get_mfile().projectid : "", pfx);
	set_mprefix(pfx);
}

std::string translate_name_mprefix(std::string original, int& binary, int save)
{
	auto& core = CORE();
	auto p = core.project->get();
	regex_results r = regex("\\$SLOT:(.*)", original);
	if(r) {
		if(binary < 0)
			binary = core.jukebox->save_binary() ? 1 : 0;
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
			std::string pprf = SET_slotpath(*core.settings) + "/";
			return pprf + get_mprefix() + r[1] + ".lsmv";
		}
	} else {
		if(binary < 0)
			binary = (save ? save_dflt_binary(*core.settings) :
				movie_dflt_binary(*core.settings)) ? 1 : 0;
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
	auto& core = CORE();
	if(!*core.mlogic || !core.mlogic->get_mfile().gametype) {
		platform::error_message("Can't save movie without a ROM");
		messages << "Can't save movie without a ROM" << std::endl;
		return;
	}
	auto& target = core.mlogic->get_mfile();
	std::string filename2 = translate_name_mprefix(filename, binary, 1);
	core.lua2->callback_pre_save(filename2, true);
	try {
		uint64_t origtime = framerate_regulator::get_utime();
		target.dyn.sram = core.rom->save_sram();
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			auto& img = core.rom->get_rom(i);
			auto& xml = core.rom->get_markup(i);
			target.romimg_sha256[i] = img.sha_256.read();
			target.romxml_sha256[i] = xml.sha_256.read();
			target.namehint[i] = img.namehint;
		}
		target.dyn.savestate = core.rom->save_core_state();
		core.fbuf->get_framebuffer().save(target.dyn.screenshot);
		core.mlogic->get_movie().save_state(target.projectid, target.dyn.save_frame,
			target.dyn.lagged_frames, target.dyn.pollcounters);
		target.dyn.poll_flag = core.rom->get_pflag();
		auto prj = core.project->get();
		if(prj) {
			target.gamename = prj->gamename;
			target.authors = prj->authors;
		}
		target.dyn.active_macros = core.controls->get_macro_frames();
		target.save(filename2, SET_savecompression(*core.settings), binary > 0,
			core.mlogic->get_rrdata(), true);
		uint64_t took = framerate_regulator::get_utime() - origtime;
		std::string kind = (binary > 0) ? "(binary format)" : "(zip format)";
		messages << "Saved state " << kind << " '" << filename2 << "' in " << took << " microseconds."
			<< std::endl;
		core.lua2->callback_post_save(filename2, true);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		platform::error_message(std::string("Save failed: ") + e.what());
		messages << "Save failed: " << e.what() << std::endl;
		core.lua2->callback_err_save(filename2);
	}
	last_save = resolve_relative_path(filename2);
	auto p = core.project->get();
	if(p) {
		p->last_save = last_save;
		p->flush();
	}
}

//Save movie.
void do_save_movie(const std::string& filename, int binary) throw(std::bad_alloc, std::runtime_error)
{
	auto& core = CORE();
	if(!*core.mlogic || !core.mlogic->get_mfile().gametype) {
		platform::error_message("Can't save movie without a ROM");
		messages << "Can't save movie without a ROM" << std::endl;
		return;
	}
	auto& target = core.mlogic->get_mfile();
	std::string filename2 = translate_name_mprefix(filename, binary, 0);
	core.lua2->callback_pre_save(filename2, false);
	try {
		uint64_t origtime = framerate_regulator::get_utime();
		auto prj = core.project->get();
		if(prj) {
			target.gamename = prj->gamename;
			target.authors = prj->authors;
		}
		target.save(filename2, SET_savecompression(*core.settings), binary > 0,
			core.mlogic->get_rrdata(), false);
		uint64_t took = framerate_regulator::get_utime() - origtime;
		std::string kind = (binary > 0) ? "(binary format)" : "(zip format)";
		messages << "Saved movie " << kind << " '" << filename2 << "' in " << took << " microseconds."
			<< std::endl;
		core.lua2->callback_post_save(filename2, false);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		platform::error_message(std::string("Save failed: ") + e.what());
		messages << "Save failed: " << e.what() << std::endl;
		core.lua2->callback_err_save(filename2);
	}
	last_save = resolve_relative_path(filename2);
	auto p = core.project->get();
	if(p) {
		p->last_save = last_save;
		p->flush();
	}
}

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
		if(!rom.isnull())
			messages << "ROM Type " << rom.get_hname() << " region " << rom.region_get_hname()
				<< std::endl;
		std::string len, rerecs;
		len = format_length(mov.get_movie_length());
		std::string rerecords = mov.dyn.save_frame ? (stringfmt() << rrd.count()).str() : mov.rerecords;
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
		if(against.isnull())
			return;
		std::string rom_core = against.get_core_identifier();
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
			auto& img = against.get_rom(i);
			auto& xml = against.get_markup(i);
			if(mov.namehint[i] == "")
				mov.namehint[i] = img.namehint;
			if(mov.romimg_sha256[i] == "")
				mov.romimg_sha256[i] = img.sha_256.read();
			if(mov.romxml_sha256[i] == "")
				mov.romxml_sha256[i] = xml.sha_256.read();
			rom_ok = rom_ok & warn_hash_mismatch(mov.romimg_sha256[i], img,
				(stringfmt() << "ROM #" << (i + 1)).str(), loadstate);
			rom_ok = rom_ok & warn_hash_mismatch(mov.romxml_sha256[i], xml,
				(stringfmt() << "XML #" << (i + 1)).str(), loadstate);
		}
		if(!rom_ok)
			throw std::runtime_error("Incorrect ROM");
	}

	portctrl::type_set& construct_movie_portset(moviefile& mov, loaded_rom& against)
	{
		auto ctrldata = against.controllerconfig(mov.settings);
		return portctrl::type_set::make(ctrldata.ports, ctrldata.portindex());
	}

	void handle_load_core(moviefile& _movie, portctrl::type_set& portset, bool will_load_state,
		bool force = false);

	void handle_load_core(moviefile& _movie, portctrl::type_set& portset, bool will_load_state, bool force)
	{
		auto& core = CORE();
		core.random_seed_value = _movie.movie_rtc_second;
		if(will_load_state) {
			//If settings possibly change, reload the ROM.
			if(force || !*core.mlogic || core.mlogic->get_mfile().projectid != _movie.projectid)
				core.rom->load(_movie.settings, _movie.movie_rtc_second, _movie.movie_rtc_subsecond);
			//Load the savestate and movie state.
			//Set the core ports in order to avoid port state being reinitialized when loading.
			core.controls->set_ports(portset);
			core.rom->load_core_state(_movie.dyn.savestate);
			core.rom->set_pflag(_movie.dyn.poll_flag);
			core.controls->set_macro_frames(_movie.dyn.active_macros);
		} else {
			//If settings possibly change, reload the ROM. Otherwise rewind to beginning.
			if(force || !*core.mlogic || core.mlogic->get_mfile().projectid != _movie.projectid)
				core.rom->load(_movie.settings, _movie.movie_rtc_second, _movie.movie_rtc_subsecond);
			else
				core.rom->reset_to_load();
			//Load the SRAM and volatile RAM. Or anchor savestate if any.
			core.controls->set_ports(portset);
			_movie.dyn.rtc_second = _movie.movie_rtc_second;
			_movie.dyn.rtc_subsecond = _movie.movie_rtc_subsecond;
			if(!_movie.anchor_savestate.empty()) {
				core.rom->load_core_state(_movie.anchor_savestate);
			} else {
				core.rom->load_sram(_movie.movie_sram);
				std::list<core_vma_info> vmas = core.rom->vma_list();
				populate_volatile_ram(_movie, vmas);
			}
			core.rom->set_pflag(0);
			core.controls->set_macro_frames(std::map<std::string, uint64_t>());
		}
	}
}

void do_load_rom() throw(std::bad_alloc, std::runtime_error)
{
	auto& core = CORE();
	bool load_readwrite = !*core.mlogic || !core.mlogic->get_movie().readonly_mode();
	if(*core.mlogic) {
		portctrl::type_set& portset = construct_movie_portset(core.mlogic->get_mfile(), *core.rom);
		//If portset or gametype changes, force readwrite with new movie.
		if(core.mlogic->get_mfile().input->get_types() != portset) load_readwrite = true;
		else if(!core.rom->is_of_type(core.mlogic->get_mfile().gametype->get_type())) load_readwrite = true;
		else if(!core.rom->region_compatible_with(core.mlogic->get_mfile().gametype->get_region()))
			load_readwrite = true;
	}

	if(!load_readwrite) {
		//Read-only load. This is pretty simple.
		//Force unlazying of rrdata and count a rerecord.
		if(core.mlogic->get_rrdata().is_lazy())
			core.mlogic->get_rrdata().read_base(rrdata::filename(
				core.mlogic->get_mfile().projectid), false);
		core.mlogic->get_rrdata().add((*core.nrrdata)());

		portctrl::type_set& portset = construct_movie_portset(core.mlogic->get_mfile(), *core.rom);

		try {
			//Force game's region to run's region. We already checked this is possible above.
			core.rom->set_internal_region(core.mlogic->get_mfile().gametype->get_region());

			handle_load_core(core.mlogic->get_mfile(), portset, false, true);
			core.mlogic->get_mfile().gametype = &core.rom->get_sysregion();
			for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
				auto& img = core.rom->get_rom(i);
				auto& xml = core.rom->get_markup(i);
				core.mlogic->get_mfile().namehint[i] = img.namehint;
				core.mlogic->get_mfile().romimg_sha256[i] = img.sha_256.read();
				core.mlogic->get_mfile().romxml_sha256[i] = xml.sha_256.read();
			}
			core.mlogic->get_mfile().clear_dynstate();
			core.mlogic->get_movie().reset_state();
			core.fbuf->redraw_framebuffer(core.rom->draw_cover());
			core.lua2->callback_do_rewind();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			core.runmode->set_corrupt();
			core.fbuf->redraw_framebuffer(emu_framebuffer::screen_corrupt, true);
			throw;
		}
	} else {
		//The more complicated Read-Write case.
		//We need to create a new movie and movie file.
		temporary_handle<moviefile> _movie;
		_movie.get()->force_corrupt = false;
		_movie.get()->gametype = NULL;		//Not yet known.
		_movie.get()->coreversion = core.rom->get_core_identifier();
		_movie.get()->projectid = get_random_hexstring(40);
		_movie.get()->rerecords = "0";
		_movie.get()->rerecords_mem = 0;
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			auto& img = core.rom->get_rom(i);
			auto& xml = core.rom->get_markup(i);
			_movie.get()->namehint[i] = img.namehint;
			_movie.get()->romimg_sha256[i] = img.sha_256.read();
			_movie.get()->romxml_sha256[i] = xml.sha_256.read();
		}
		_movie.get()->movie_rtc_second = _movie.get()->dyn.rtc_second = 1000000000ULL;
		_movie.get()->movie_rtc_subsecond = _movie.get()->dyn.rtc_subsecond = 0;
		_movie.get()->start_paused = false;
		_movie.get()->lazy_project_create = true;
		portctrl::type_set& portset2 = construct_movie_portset(*_movie.get(), *core.rom);
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
		rrd.get()->add((*core.nrrdata)());
		//Movie data is lost.
		core.lua2->callback_movie_lost("reload");
		try {
			handle_load_core(*_movie.get(), portset2, false, true);
			_movie.get()->gametype = &core.rom->get_sysregion();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			core.runmode->set_corrupt();
			core.fbuf->redraw_framebuffer(emu_framebuffer::screen_corrupt, true);
			throw;
		}

		//Set up stuff.
		core.mlogic->set_movie(*(newmovie()), true);
		core.mlogic->set_mfile(*(_movie()), true);
		core.mlogic->set_rrdata(*(rrd()), true);
		set_mprefix(get_mprefix_for_project(core.mlogic->get_mfile().projectid));
		set_gameinfo(core.mlogic->get_mfile());

		core.fbuf->redraw_framebuffer(core.rom->draw_cover());
		core.lua2->callback_do_rewind();
	}
	core.dispatch->mode_change(core.mlogic->get_movie().readonly_mode());
	core.dispatch->mbranch_change();
	messages << "ROM reloaded." << std::endl;
}

void do_load_rewind() throw(std::bad_alloc, std::runtime_error)
{
	auto& core = CORE();
	if(!*core.mlogic || !core.mlogic->get_mfile().gametype)
		throw std::runtime_error("Can't rewind movie without existing movie");

	portctrl::type_set& portset = construct_movie_portset(core.mlogic->get_mfile(), *core.rom);

	//Force unlazying of rrdata and count a rerecord.
	if(core.mlogic->get_rrdata().is_lazy())
		core.mlogic->get_rrdata().read_base(rrdata::filename(
			core.mlogic->get_mfile().projectid), false);
	core.mlogic->get_rrdata().add((*core.nrrdata)());

	//Enter readonly mode.
	core.mlogic->get_movie().readonly_mode(true);
	core.dispatch->mode_change(true);
	try {
		handle_load_core(core.mlogic->get_mfile(), portset, false);
		core.mlogic->get_mfile().clear_dynstate();
		core.mlogic->get_movie().reset_state();
		core.fbuf->redraw_framebuffer(core.rom->draw_cover());
		core.lua2->callback_do_rewind();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		core.runmode->set_corrupt();
		core.fbuf->redraw_framebuffer(emu_framebuffer::screen_corrupt, true);
		throw;
	}
	messages << "Movie rewound to beginning." << std::endl;
}

//Load state preserving input. Does not do checks.
void do_load_state_preserve(struct moviefile& _movie)
{
	auto& core = CORE();
	if(!*core.mlogic || !core.mlogic->get_mfile().gametype)
		throw std::runtime_error("Can't load movie preserving input without previous movie");
	if(core.mlogic->get_mfile().projectid != _movie.projectid)
		throw std::runtime_error("Savestate is from different movie");

	bool will_load_state = _movie.dyn.save_frame;
	portctrl::type_set& portset = construct_movie_portset(core.mlogic->get_mfile(), *core.rom);

	//Construct a new movie sharing the input data.
	temporary_handle<movie> newmovie;
	newmovie.get()->set_movie_data(core.mlogic->get_mfile().input);
	newmovie.get()->readonly_mode(true);
	newmovie.get()->set_pflag_handler(&lsnes_pflag_handler);

	newmovie.get()->load(_movie.rerecords, _movie.projectid, *_movie.input);
	if(will_load_state)
		newmovie.get()->restore_state(_movie.dyn.save_frame, _movie.dyn.lagged_frames,
			_movie.dyn.pollcounters, true, _movie.input, _movie.projectid);

	//Count a rerecord.
	if(core.mlogic->get_rrdata().is_lazy() && !_movie.lazy_project_create)
		core.mlogic->get_rrdata().read_base(rrdata::filename(_movie.projectid), false);
	core.mlogic->get_rrdata().add((*core.nrrdata)());

	//Negative return.
	try {
		handle_load_core(_movie, portset, will_load_state);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		core.runmode->set_corrupt();
		core.fbuf->redraw_framebuffer(emu_framebuffer::screen_corrupt, true);
		throw;
	}

	//Set new movie.
	core.mlogic->set_movie(*(newmovie()), true);

	//If not loading a state, clear the state, so state swap will swap in state at beginning.
	if(!will_load_state)
		_movie.clear_dynstate();
	//Swap the dynamic state to movie dynamic state (will desync otherwise).
	core.mlogic->get_mfile().dyn.swap(_movie.dyn);

	try {
		//Paint the screen.
		framebuffer::raw tmp;
		if(will_load_state) {
			tmp.load(core.mlogic->get_mfile().dyn.screenshot);
			core.fbuf->redraw_framebuffer(tmp);
		} else
			core.fbuf->redraw_framebuffer(core.rom->draw_cover());
	} catch(...) {
	}
	delete &_movie;
	core.dispatch->mode_change(core.mlogic->get_movie().readonly_mode());
	messages << "Loadstated at earlier point of movie." << std::endl;
}

//Load state from loaded movie file. _movie is consumed.
void do_load_state(struct moviefile& _movie, int lmode, bool& used)
{
	auto& core = CORE();
	//Some basic sanity checks.
	bool current_mode = *core.mlogic ? core.mlogic->get_movie().readonly_mode() : false;
	bool will_load_state = _movie.dyn.save_frame && lmode != LOAD_STATE_MOVIE;

	//Load state all branches and load state initial are the same.
	if(lmode == LOAD_STATE_ALLBRANCH) lmode = LOAD_STATE_INITIAL;

	//Various checks.
	if(_movie.force_corrupt)
		throw std::runtime_error("Movie file invalid");
	if(!core.rom->is_of_type(_movie.gametype->get_type()))
		throw std::runtime_error("ROM types of movie and loaded ROM don't match");
	if(!core.rom->region_compatible_with(_movie.gametype->get_region()))
		throw std::runtime_error("NTSC/PAL select of movie and loaded ROM don't match");
	warn_coretype(_movie.coreversion, *core.rom, will_load_state);
	warn_roms(_movie, *core.rom, will_load_state);

	//In certain conditions, trun LOAD_STATE_CURRENT into LOAD_STATE_PRESERVE.
	if(lmode == LOAD_STATE_CURRENT && current_mode && SET_readonly_load_preserves(*core.settings))
		lmode = LOAD_STATE_PRESERVE;
	//If movie file changes, turn LOAD_STATE_CURRENT into LOAD_STATE_RO
	if(lmode == LOAD_STATE_CURRENT && core.mlogic->get_mfile().projectid != _movie.projectid)
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
		newmovie.get()->restore_state(_movie.dyn.save_frame, _movie.dyn.lagged_frames,
			_movie.dyn.pollcounters, true, NULL, _movie.projectid);

	//Copy the other branches.
	if(lmode != LOAD_STATE_INITIAL && core.mlogic->get_mfile().projectid == _movie.projectid) {
		newmovie.get()->set_movie_data(NULL);
		auto& oldm = core.mlogic->get_mfile().branches;
		auto& newm = _movie.branches;
		auto oldd = core.mlogic->get_mfile().input;
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

	portctrl::type_set& portset = construct_movie_portset(_movie, *core.rom);

	temporary_handle<rrdata_set> rrd;
	bool new_rrdata = false;
	//Count a rerecord (against new or old movie).
	if(!*core.mlogic || _movie.projectid != core.mlogic->get_mfile().projectid) {
		rrd.get()->read_base(rrdata::filename(_movie.projectid), _movie.lazy_project_create);
		rrd.get()->read(_movie.c_rrdata);
		rrd.get()->add((*core.nrrdata)());
		new_rrdata = true;
	} else {
		//Unlazy rrdata if needed.
		if(core.mlogic->get_rrdata().is_lazy() && !_movie.lazy_project_create)
			core.mlogic->get_rrdata().read_base(rrdata::filename(_movie.projectid), false);
		core.mlogic->get_rrdata().add((*core.nrrdata)());
	}
	//Negative return.
	try {
		core.rom->set_internal_region(_movie.gametype->get_region());
		handle_load_core(_movie, portset, will_load_state);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		core.runmode->set_corrupt();
		core.fbuf->redraw_framebuffer(emu_framebuffer::screen_corrupt, true);
		throw;
	}

	//If loaded a movie, clear the is savestate and rrdata.
	if(!will_load_state)
		_movie.clear_dynstate();

	core.lua2->callback_movie_lost("load");

	//Copy the data.
	if(new_rrdata) core.mlogic->set_rrdata(*(rrd()), true);
	core.mlogic->set_movie(*newmovie(), true);
	core.mlogic->set_mfile(_movie, true);
	used = true;

	set_mprefix(get_mprefix_for_project(core.mlogic->get_mfile().projectid));

	//Activate RW mode if needed.
	auto& m = core.mlogic->get_movie();
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
			tmp.load(_movie.dyn.screenshot);
			core.fbuf->redraw_framebuffer(tmp);
		} else
			core.fbuf->redraw_framebuffer(core.rom->draw_cover());
	}

	core.dispatch->mode_change(m.readonly_mode());
	print_movie_info(_movie, *core.rom, core.mlogic->get_rrdata());
	core.dispatch->mbranch_change();
	set_gameinfo(core.mlogic->get_mfile());
}


void try_request_rom(const std::string& moviefile)
{
	auto& core = CORE();
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
	loaded_rom newrom(new rom_image(req.filename, selected_core->get_core_identifier(),
		selected_core->get_iname(), ""));
	*core.rom = newrom;
	core.dispatch->core_change();
}

//Load state
bool do_load_state(const std::string& filename, int lmode)
{
	auto& core = CORE();
	int tmp = -1;
	std::string filename2 = translate_name_mprefix(filename, tmp, -1);
	uint64_t origtime = framerate_regulator::get_utime();
	core.lua2->callback_pre_load(filename2);
	struct moviefile* mfile = NULL;
	bool used = false;
	try {
		if(core.rom->isnull())
			try_request_rom(filename2);
		mfile = new moviefile(filename2, core.rom->get_internal_rom_type());
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		platform::error_message(std::string("Can't read movie/savestate: ") + e.what());
		messages << "Can't read movie/savestate '" << filename2 << "': " << e.what() << std::endl;
		core.lua2->callback_err_load(filename2);
		return false;
	}
	try {
		do_load_state(*mfile, lmode, used);
		uint64_t took = framerate_regulator::get_utime() - origtime;
		messages << "Loaded '" << filename2 << "' in " << took << " microseconds." << std::endl;
		core.lua2->callback_post_load(filename2, core.mlogic->get_mfile().dyn.save_frame);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		if(!used)
			delete mfile;
		platform::error_message(std::string("Can't load movie/savestate: ") + e.what());
		messages << "Can't load movie/savestate '" << filename2 << "': " << e.what() << std::endl;
		core.lua2->callback_err_load(filename2);
		return false;
	}
	return true;
}

void mainloop_restore_state(const dynamic_state& state)
{
	auto& core = CORE();
	//Force unlazy rrdata.
	core.mlogic->get_rrdata().read_base(rrdata::filename(core.mlogic->get_mfile().projectid),
		false);
	core.mlogic->get_rrdata().add((*core.nrrdata)());
	core.rom->load_core_state(state.savestate, true);
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
