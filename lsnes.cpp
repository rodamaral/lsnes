#ifndef PLATFORM_STARTUP
#include <sstream>
#include "mainloop.hpp"
#include "command.hpp"
#include "lua.hpp"
#include "moviedata.hpp"
#include "rrdata.hpp"
#include "lsnes.hpp"
#include "rom.hpp"
#include "keymapper.hpp"
#include "misc.hpp"
#include "window.hpp"
#include <sys/time.h>
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>
#include "framerate.hpp"
#if defined(_WIN32) || defined(_WIN64)
#include "SDL_main.h"
#endif


class my_interfaced : public SNES::Interface
{
	string path(SNES::Cartridge::Slot slot, const string &hint)
	{
		return "./";
	}
};

struct moviefile generate_movie_template(std::vector<std::string> cmdline, loaded_rom& r)
{
	struct moviefile movie;
	movie.gametype = gtype::togametype(r.rtype, r.region);
	movie.rom_sha256 = r.rom.sha256;
	movie.romxml_sha256 = r.rom_xml.sha256;
	movie.slota_sha256 = r.slota.sha256;
	movie.slotaxml_sha256 = r.slota_xml.sha256;
	movie.slotb_sha256 = r.slotb.sha256;
	movie.slotbxml_sha256 = r.slotb_xml.sha256;
	movie.movie_sram = load_sram_commandline(cmdline);
	for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
		std::string o = *i;
		if(o.length() >= 8 && o.substr(0, 8) == "--port1=")
			movie.port1 = port_type::lookup(o.substr(8), false).ptype;
		if(o.length() >= 8 && o.substr(0, 8) == "--port2=")
			movie.port2 = port_type::lookup(o.substr(8), true).ptype;
		if(o.length() >= 11 && o.substr(0, 11) == "--gamename=")
			movie.gamename = o.substr(11);
		if(o.length() >= 9 && o.substr(0, 9) == "--author=") {
			std::string line = o.substr(9);
			auto g = split_author(line);
			movie.authors.push_back(g);
		}

	}


	return movie;
}

namespace
{
	void run_extra_scripts(const std::vector<std::string>& cmdline)
	{
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string o = *i;
			if(o.length() >= 6 && o.substr(0, 6) == "--run=") {
				std::string file = o.substr(6);
				messages << "--- Running " << file << " --- " << std::endl;
				command::invokeC("run-script " + file);
				messages << "--- End running " << file << " --- " << std::endl;
			}
		}
	}
}

#if defined(_WIN32) || defined(_WIN64)
int SDL_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	std::vector<std::string> cmdline;
	for(int i = 1; i < argc; i++)
		cmdline.push_back(argv[i]);
	my_interfaced intrf;
	SNES::system.interface = &intrf;

	set_random_seed();

	{
		std::ostringstream x;
		x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
		bsnes_core_version = x.str();
	}
	window::init();
	init_lua();

	messages << "BSNES version: " << bsnes_core_version << std::endl;
	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;
	messages << "Command line is: ";
	for(auto k = cmdline.begin(); k != cmdline.end(); k++)
		messages << "\"" << *k << "\" ";
	messages << std::endl;

	std::string cfgpath = get_config_path();
	create_lsnesrc();
	messages << "Saving per-user data to: " << get_config_path() << std::endl;
	messages << "--- Running lsnesrc --- " << std::endl;
	command::invokeC("run-script " + cfgpath + "/lsnes.rc");
	messages << "--- End running lsnesrc --- " << std::endl;

	run_extra_scripts(cmdline);

	messages << "--- Loading ROM ---" << std::endl;
	struct loaded_rom r;
	try {
		r = load_rom_from_commandline(cmdline);
		r.load();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: Can't load ROM: " << e.what() << std::endl;
		fatal_error();
		exit(1);
	}
	messages << "Detected region: " << gtype::tostring(r.rtype, r.region) << std::endl;
	if(r.region == REGION_PAL)
		set_nominal_framerate(322445.0/6448.0);
	else if(r.region == REGION_NTSC)
		set_nominal_framerate(10738636.0/178683.0);

	messages << "--- Internal memory mappings ---" << std::endl;
	dump_region_map();
	messages << "--- End of Startup --- " << std::endl;
	moviefile movie;
	movie.force_corrupt = true;
	try {
		bool loaded = false;
		bool tried = false;
		for(auto i = cmdline.begin(); i != cmdline.end(); i++)
			if(i->length() > 0 && (*i)[0] != '-') {
				try {
					tried = true;
					movie = moviefile(*i);
					loaded = true;
				} catch(std::bad_alloc& e) {
					OOM_panic();
				} catch(std::exception& e) {
					messages << "Error loading '" << *i << "': " << e.what() << std::endl;
				}
			}
		if(!tried)
			movie = generate_movie_template(cmdline, r);
		main_loop(r, movie);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: " << e.what() << std::endl;
		fatal_error();
		return 1;
	}
	rrdata::close();
	window::quit();
	return 0;
}
#endif