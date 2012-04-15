#include "lsnes.hpp"
#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/window.hpp"
#include "interface/core.hpp"

#include "platform/sdl/platform.hpp"

#include <sys/time.h>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE) || defined(__APPLE__)
#include "SDL_main.h"
#endif


struct moviefile generate_movie_template(std::vector<std::string> cmdline, loaded_rom& r)
{
	struct moviefile movie;
	movie.port1 = PT_GAMEPAD;
	movie.port2 = PT_NONE;
	movie.coreversion = emucore_get_version();
	movie.projectid = get_random_hexstring(40);
	copy_romdata_to_movie(movie, r);
	movie.movie_sram = load_sram_commandline(cmdline);
	for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
		std::string o = *i;
		if(o.length() >= 9 && o.substr(0, 9) == "--prefix=")
			movie.prefix = sanitize_prefix(o.substr(9));
		if(o.length() >= 8 && o.substr(0, 8) == "--port1=")
			movie.port1 = porttype_info::lookup(o.substr(8)).value;
		if(o.length() >= 8 && o.substr(0, 8) == "--port2=")
			movie.port2 = porttype_info::lookup(o.substr(8)).value;
		if(o.length() >= 11 && o.substr(0, 11) == "--gamename=")
			movie.gamename = o.substr(11);
		if(o.length() >= 9 && o.substr(0, 9) == "--author=") {
			std::string line = o.substr(9);
			auto g = split_author(line);
			movie.authors.push_back(g);
		}
		if(o.length() >= 13 && o.substr(0, 13) == "--rtc-second=") {
			movie.rtc_second = movie.movie_rtc_second = parse_value<int64_t>(o.substr(13));
		}
		if(o.length() >= 16 && o.substr(0, 16) == "--rtc-subsecond=") {
			movie.rtc_subsecond = movie.movie_rtc_subsecond = parse_value<int64_t>(o.substr(16));
			if(movie.rtc_subsecond < 0 || movie.rtc_subsecond > 3462619485019ULL)
				throw std::runtime_error("Bad RTC subsecond value (range is 0-3462619485019)");
		}
	}
	movie.input.clear(movie.port1, movie.port2);

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

	void sdl_main_loop(struct loaded_rom& rom, struct moviefile& initial, bool load_has_to_succeed = false)
		throw(std::bad_alloc, std::runtime_error);

	struct emu_args
	{
		struct loaded_rom* rom;
		struct moviefile* initial;
		bool load_has_to_succeed;
	};

	void* emulator_thread(void* _args)
	{
		struct emu_args* args = reinterpret_cast<struct emu_args*>(_args);
		try {
			main_loop(*args->rom, *args->initial, args->load_has_to_succeed);
			notify_emulator_exit();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			messages << "FATAL: " << e.what() << std::endl;
			platform::fatal_error();
		}
	}

	void* joystick_thread(void* _args)
	{
		joystick_plugin::thread_fn();
	}

	void sdl_main_loop(struct loaded_rom& rom, struct moviefile& initial, bool load_has_to_succeed)
		throw(std::bad_alloc, std::runtime_error)
	{
		try {
			struct emu_args args;
			args.rom = &rom;
			args.initial = &initial;
			args.load_has_to_succeed = load_has_to_succeed;
			thread* t;
			thread* t2;
			t = &thread::create(emulator_thread, &args);
			t2 = &thread::create(joystick_thread, &args);
			ui_loop();
			joystick_plugin::signal();
			t2->join();
			t->join();
			delete t;
			delete t2;
		} catch(std::bad_alloc& e) {
			OOM_panic();
		}
	}
}

int main(int argc, char** argv)
{
	reached_main();
	std::vector<std::string> cmdline;
	for(int i = 1; i < argc; i++)
		cmdline.push_back(argv[i]);
	if(cmdline.size() == 1 && cmdline[0] == "--version") {
		std::cout << "lsnes rr" << lsnes_version << " (" << lsnes_git_revision << ")" << std::endl;
		std::cout << emucore_get_version() << std::endl;
		return 0;
	}
	emucore_basic_init();

	set_random_seed();

	platform::init();
	init_lua();

	messages << "BSNES version: " << emucore_get_version() << std::endl;
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
	messages << "Detected region: " << r.rtype->get_sysregion(r.region->get_iname())->get_iname() << std::endl;
	auto vrate = emucore_get_video_rate();
	set_nominal_framerate(1.0 * vrate.first / vrate.second);

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
		sdl_main_loop(r, movie);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: " << e.what() << std::endl;
		fatal_error();
		return 1;
	}
	rrdata::close();
	platform::quit();
	quit_lua();
	return 0;
}
