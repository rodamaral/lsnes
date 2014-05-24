#include "lsnes.hpp"

#include "core/advdumper.hpp"
#include "core/controller.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/mainloop.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "interface/romtype.hpp"
#include "core/loadlib.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/romloader.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/string.hpp"

#include <sys/time.h>
#include <sstream>

namespace
{
	bool hashing_in_progress = false;
	uint64_t hashing_left = 0;
	int64_t last_update = 0;

	void hash_callback(uint64_t left, uint64_t total)
	{
		if(left == 0xFFFFFFFFFFFFFFFFULL) {
			hashing_in_progress = false;
			std::cout << "Done." << std::endl;
			last_update = framerate_regulator::get_utime() - 2000000;
			return;
		}
		if(!hashing_in_progress) {
			std::cout << "Hashing disc images..." << std::flush;
		}
		hashing_in_progress = true;
		hashing_left = left;
		int64_t this_update = framerate_regulator::get_utime();
		if(this_update < last_update - 1000000 || this_update > last_update + 1000000) {
			std::cout << ((hashing_left + 524288) >> 20) << "..." << std::flush;
			last_update = this_update;
		}
	}

	void dump_what_was_loaded(loaded_rom& r, moviefile& f)
	{
		std::cout << "Core:\t" << r.rtype->get_core_identifier() << std::endl;
		std::cout << "System:\t" << r.rtype->get_hname() << std::endl;
		std::cout << "Region:\t" << r.region->get_hname() << std::endl;
		for(auto i = 0; i < ROM_SLOT_COUNT; i++) {
			std::string risha = r.romimg[i].sha_256.read();
			std::string rxsha = r.romxml[i].sha_256.read();
			std::string fisha = f.romimg_sha256[i];
			std::string fxsha = f.romxml_sha256[i];
			std::string finam = r.romimg[i].filename;
			std::string fxnam = r.romxml[i].filename;
			std::string nhint = f.namehint[i];
			if(risha != "" || rxsha != "" || fisha != "" || fxsha != "" || nhint != "") {
				std::cout << "ROM slot #" << i << ":" << std::endl;
				if(nhint != "")
					std::cout << "\tHint:\t" << nhint << std::endl;
				if(finam != "")
					std::cout << "\tFile:\t" << finam << std::endl;
				if(fxnam != "")
					std::cout << "\tXFile:\t" << fxnam << std::endl;
				if(risha != "" && fisha == risha)
					std::cout << "\tHash:\t" << risha << " (matches)" << std::endl;
				if(risha != "" && fisha != risha)
					std::cout << "\tHash:\t" << risha << " (ROM)" << std::endl;
				if(fisha != "" && fisha != risha)
					std::cout << "\tHash:\t" << risha << " (Movie)" << std::endl;
				if(rxsha != "" && fxsha == rxsha)
					std::cout << "\tXHash:\t" << rxsha << " (matches)" << std::endl;
				if(rxsha != "" && fxsha != rxsha)
					std::cout << "\tXHash:\t" << rxsha << " (ROM)" << std::endl;
				if(fxsha != "" && fxsha != rxsha)
					std::cout << "\tXHash:\t" << rxsha << " (Movie)" << std::endl;
			}
		}
	}
}

int main(int argc, char** argv)
{
	reached_main();
	std::vector<std::string> cmdline;
	for(int i = 1; i < argc; i++)
		cmdline.push_back(argv[i]);

	set_random_seed();
	platform::init();

	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;
	messages << "Command line is: ";
	for(auto k = cmdline.begin(); k != cmdline.end(); k++)
		messages << "\"" << *k << "\" ";
	messages << std::endl;

	std::string cfgpath = get_config_path();
	autoload_libraries();

	for(auto i : cmdline) {
		regex_results r;
		if(r = regex("--firmware-path=(.*)", i))
			try {
				lsnes_instance.setcache->set("firmwarepath", r[1]);
				std::cerr << "Set firmware path to '" << r[1] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set firmware path to '" << r[1] << "': " << e.what() << std::endl;
			}
		if(r = regex("--rom-path=(.*)", i))
			try {
				lsnes_instance.setcache->set("rompath", r[1]);
				std::cerr << "Set rompath path to '" << r[1] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set firmware path to '" << r[1] << "': " << e.what() << std::endl;
			}
		if(r = regex("--setting-(.*)=(.*)", i))
			try {
				lsnes_instance.setcache->set(r[1], r[2]);
				std::cerr << "Set " << r[1] << " to '" << r[2] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set " << r[1] << " to '" << r[2] << "': " << e.what()
					<< std::endl;
			}
		if(r = regex("--load-library=(.*)", i))
			try {
				with_loaded_library(*new loadlib::module(loadlib::library(r[1])));
				handle_post_loadlibrary();
			} catch(std::runtime_error& e) {
				std::cerr << "Can't load '" << r[1] << "': " << e.what() << std::endl;
				exit(1);
			}
	}
	set_hasher_callback(hash_callback);

	std::string movfn;
	for(auto i : cmdline) {
		if(i.length() > 0 && i[0] != '-') {
			movfn = i;
		}
	}

	struct loaded_rom r;
	try {
		std::map<std::string, std::string> tmp;
		r = construct_rom(movfn, cmdline);
		r.load(tmp, 1000000000, 0);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: Can't load ROM: " << e.what() << std::endl;
		fatal_error();
		exit(1);
	}

	moviefile* movie = NULL;
	try {
		if(movfn != "")
			movie = new moviefile(movfn, *r.rtype);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: Can't load movie: " << e.what() << std::endl;
		fatal_error();
		exit(1);
	}

	dump_what_was_loaded(r, *movie);

	lsnes_instance.mlogic->release_memory();
	cleanup_all_keys();
	return 0;
}
