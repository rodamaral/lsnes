#include <sstream>
#include "mainloop.hpp"
#include "command.hpp"
#include "lua.hpp"
#include "rrdata.hpp"
#include "lsnes.hpp"
#include "rom.hpp"
#include "keymapper.hpp"
#include "misc.hpp"
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
			fieldsplitter f(line);
			std::string full = f;
			std::string nick = f;
			if(full == "" && nick == "")
				throw std::runtime_error("Bad author name, one of full or nickname must be present");
			movie.authors.push_back(std::make_pair(full, nick));
		}

	}


	return movie;
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

	window::out() << "BSNES version: " << bsnes_core_version << std::endl;
	window::out() << "lsnes version: lsnes rr" << lsnes_version << std::endl;
	window::out() << "Command line is: ";
	for(auto k = cmdline.begin(); k != cmdline.end(); k++)
		window::out() << "\"" << *k << "\" ";
	window::out() << std::endl;

	std::string cfgpath = get_config_path();
	create_lsnesrc();
	window::out() << "Saving per-user data to: " << get_config_path() << std::endl;
	window::out() << "--- Running lsnesrc --- " << std::endl;
	command::invokeC("run-script " + cfgpath + "/lsnes.rc");
	window::out() << "--- End running lsnesrc --- " << std::endl;

	window::out() << "--- Loading ROM ---" << std::endl;
	struct loaded_rom r;
	try {
		r = load_rom_from_commandline(cmdline);
		r.load();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		window::out() << "FATAL: Can't load ROM: " << e.what() << std::endl;
		window::fatal_error();
		exit(1);
	}
	window::out() << "Detected region: " << gtype::tostring(r.rtype, r.region) << std::endl;
	if(r.region == REGION_PAL)
		set_nominal_framerate(322445.0/6448.0);
	else if(r.region == REGION_NTSC)
		set_nominal_framerate(10738636.0/178683.0);

	window::out() << "--- Internal memory mappings ---" << std::endl;
	dump_region_map();
	window::out() << "--- End of Startup --- " << std::endl;
	try {
		moviefile movie;
		bool loaded = false;
		for(auto i = cmdline.begin(); i != cmdline.end(); i++)
			if(i->length() > 0 && (*i)[0] != '-') {
				movie = moviefile(*i);
				loaded = true;
			}
		if(!loaded)
			movie = generate_movie_template(cmdline, r);
		main_loop(r, movie);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		window::message(std::string("Fatal: ") + e.what());
		window::fatal_error();
		return 1;
	}
	rrdata::close();
	window::quit();
	return 0;
}