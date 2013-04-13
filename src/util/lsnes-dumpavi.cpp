#include "lsnes.hpp"

#include "core/advdumper.hpp"
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
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/string.hpp"

#include <sys/time.h>
#include <sstream>

namespace
{
	class myavsnoop : public information_dispatch
	{
	public:
		myavsnoop(adv_dumper& _dumper, uint64_t frames_to_dump)
			: information_dispatch("myavsnoop-monitor"), dumper(_dumper)
		{
			frames_dumped = 0;
			total = frames_to_dump;
		}

		~myavsnoop() throw()
		{
		}

		void on_frame(struct framebuffer_raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			frames_dumped++;
			if(frames_dumped % 100 == 0) {
				std::cout << "Dumping frame " << frames_dumped << "/" << total << " ("
					<< (100 * frames_dumped / total) << "%)" << std::endl;
			}
			if(frames_dumped == total) {
				//Rough way to end it.
				dumper.end();
				information_dispatch::do_dump_end();
				exit(0);
			}
		}

		void on_dump_end()
		{
			std::cout << "Finished!" << std::endl;
		}
	private:
		uint64_t frames_dumped;
		uint64_t total;
		adv_dumper& dumper;
	};

	void dumper_startup(adv_dumper& dumper, const std::string& mode, const std::string& prefix, uint64_t length)
	{
		std::cout << "Invoking dumper" << std::endl;
		try {
			dumper.start(mode, prefix);
		} catch(std::exception& e) {
			std::cerr << "Can't start dumper: " << e.what() << std::endl;
			exit(1);
		}
		if(information_dispatch::get_dumper_count()) {
			std::cout << "Dumper attach confirmed" << std::endl;
		} else {
			std::cout << "Can't start dumper!" << std::endl;
			exit(1);
		}
		new myavsnoop(dumper, length);
	}

	void startup_lua_scripts(const std::vector<std::string>& cmdline)
	{
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string a = *i;
			if(a.length() > 6 && a.substr(0, 6) == "--lua=") {
				lsnes_cmd.invoke("run-lua " + a.substr(6));
			}
		}
	}

	struct adv_dumper& locate_dumper(const std::string& name)
	{
		adv_dumper* _dumper = NULL;
		std::set<adv_dumper*> dumpers = adv_dumper::get_dumper_set();
		for(auto i : dumpers)
			if(i->id() == name)
				_dumper = i;
		if(!_dumper) {
			std::cerr << "No such dumper '" << name << "' found (try --dumper=list)" << std::endl;
			exit(1);
		}
		return *_dumper;
	}

	std::string format_details(unsigned detail)
	{
		std::string r;
		if((detail & adv_dumper::target_type_mask) == adv_dumper::target_type_file)
			r = r + "TARGET_FILE";
		else if((detail & adv_dumper::target_type_mask) == adv_dumper::target_type_prefix)
			r = r + "TARGET_PREFIX";
		else if((detail & adv_dumper::target_type_mask) == adv_dumper::target_type_special)
			r = r + "TARGET_SPECIAL";
		else 
			r = r + "TARGET_UNKNOWN";
		return r;
	}
	
	adv_dumper& get_dumper(const std::vector<std::string>& cmdline, std::string& mode, std::string& prefix,
		uint64_t& length)
	{
		bool dumper_given = false;
		std::string dumper;
		bool mode_given = false;
		prefix = "avidump";
		length = 0;
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string a = *i;
			if(a.length() >= 7 && a.substr(0, 7) == "--core=") {
				preferred_core_default = a.substr(7);
			} else if(a.length() >= 9 && a.substr(0, 9) == "--dumper=") {
				dumper_given = true;
				dumper = a.substr(9);
			} else if(a.length() >= 7 && a.substr(0, 7) == "--mode=") {
				mode_given = true;
				mode = a.substr(7);
			} else if(a.length() >= 9 && a.substr(0, 9) == "--prefix=")
				prefix = a.substr(9);
			else if(a.length() >= 9 && a.substr(0, 9) == "--length=")
				try {
					length = boost::lexical_cast<uint64_t>(a.substr(9));
					if(!length)
						throw std::runtime_error("Length out of range (1-)");
				} catch(std::exception& e) {
					std::cerr << "Bad --length: " << e.what() << std::endl;
					exit(1);
				}
			else if(a.length() >= 9 && a.substr(0, 9) == "--option=") {
				std::string nameval = a.substr(9);
				size_t s = nameval.find_first_of("=");
				if(s >= nameval.length()) {
					std::cerr << "Invalid option syntax (expected --option=foo=bar)" << std::endl;
					exit(1);
				}
				std::string name = nameval.substr(0, s);
				std::string val = nameval.substr(s + 1);
				try {
					lsnes_vset[name].str(val);
				} catch(std::exception& e) {
					std::cerr << "Can't set '" << name << "' to '" << val << "': " << e.what()
						<< std::endl;
					exit(1);
				}
			} else if(a.length() >= 12 && a.substr(0, 15) == "--load-library=")
				try {
					new loaded_library(a.substr(15));
					handle_post_loadlibrary();
				} catch(std::runtime_error& e) {
					std::cerr << "Can't load '" << a.substr(15) << "': " << e.what() << std::endl;
					exit(1);
				}
		}
		if(preferred_core_default == "list") {
			//Help on cores.
			std::set<std::pair<std::string, std::string>> cores;
			for(auto i : core_type::get_core_types())
				cores.insert(std::make_pair(i->get_core_shortname(), i->get_core_identifier()));
			for(auto i : cores)
				std::cout << i.first << " -> " << i.second << std::endl;
			exit(0);
		}
		if(dumper == "list") {
			//Help on dumpers.
			std::set<adv_dumper*> dumpers = adv_dumper::get_dumper_set();
			std::cout << "Dumpers available:" << std::endl;
			for(auto i : dumpers)
				std::cout << i->id() << "\t" << i->name() << std::endl;
			exit(0);
		}
		if(!dumper_given) {
			std::cerr << "Dumper required (--dumper=foo)" << std::endl;
			exit(1);
		}
		if(mode == "list") {
			//Help on modes.
			adv_dumper& _dumper = locate_dumper(dumper);
			std::set<std::string> modes = _dumper.list_submodes();
			if(modes.empty()) {
				unsigned d = _dumper.mode_details("");
				std::cout << "No modes available for " << dumper << " (" << format_details(d) << ")"
					<< std::endl;
				exit(0);
			}
			std::cout << "Modes available for " << dumper << ":" << std::endl;
			for(auto i : modes) {
				unsigned d = _dumper.mode_details(i);
				std::cout << i << "\t" << _dumper.modename(i) << "\t(" << format_details(d) << ")"
					<< std::endl;
			}
			exit(0);
		}
		adv_dumper& _dumper = locate_dumper(dumper);
		if(!mode_given && !_dumper.list_submodes().empty()) {
			std::cerr << "Mode required for this dumper" << std::endl;
			exit(1);
		}
		if(mode_given && _dumper.list_submodes().empty()) {
			std::cerr << "This dumper does not have mode select" << std::endl;
			exit(1);
		}
		if(mode_given && !_dumper.list_submodes().count(mode)) {
			std::cerr << "'" << mode << "' is not a valid mode for '" << dumper << "'" << std::endl;
			exit(1);
		}
		if(!length) {
			std::cerr << "--length=<frames> has to be specified" << std::endl;
			exit(1);
		}
		return locate_dumper(dumper);
	}
}

int main(int argc, char** argv)
{
	reached_main();
	std::vector<std::string> cmdline;
	for(int i = 1; i < argc; i++)
		cmdline.push_back(argv[i]);
	uint64_t length;
	std::string mode, prefix;
	
	adv_dumper& dumper = get_dumper(cmdline, mode, prefix, length);

	set_random_seed();
	platform::init();
	init_lua();

	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;
	messages << "Command line is: ";
	for(auto k = cmdline.begin(); k != cmdline.end(); k++)
		messages << "\"" << *k << "\" ";
	messages << std::endl;

	std::string cfgpath = get_config_path();
	autoload_libraries();

	for(auto i : cmdline) {
		regex_results r;
		if(r = regex("--firmware-path=(.*)", i)) {
			try {
				lsnes_vsetc.set("firmwarepath", r[1]);
				std::cerr << "Set firmware path to '" << r[1] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set firmware path to '" << r[1] << "': " << e.what() << std::endl;
			}
		}
		if(r = regex("--setting-(.*)=(.*)", i)) {
			try {
				lsnes_vset[r[1]].str(r[2]);
				std::cerr << "Set " << r[1] << " to '" << r[2] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set " << r[1] << " to '" << r[2] << "': " << e.what() << std::endl;
			}
		}
	}

	messages << "--- Loading ROM ---" << std::endl;
	struct loaded_rom r;
	try {
		std::map<std::string, std::string> tmp;
		r = load_rom_from_commandline(cmdline);
		r.load(tmp, 1000000000, 0);
		messages << "Using core: " << r.rtype->get_core_identifier() << std::endl;
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: Can't load ROM: " << e.what() << std::endl;
		fatal_error();
		exit(1);
	}
	messages << "Detected region: " << r.rtype->combine_region(*r.region).get_name() << std::endl;
	set_nominal_framerate(r.region->approx_framerate());

	messages << "--- Internal memory mappings ---" << std::endl;
	dump_region_map();
	messages << "--- End of Startup --- " << std::endl;

	moviefile movie;
	try {
		bool tried = false;
		bool loaded = false;
		for(auto i = cmdline.begin(); i != cmdline.end(); i++)
			if(i->length() > 0 && (*i)[0] != '-') {
				try {
					tried = true;
					movie = moviefile(*i, *r.rtype);
					loaded = true;
				} catch(std::bad_alloc& e) {
					OOM_panic();
				} catch(std::exception& e) {
					messages << "Error loading '" << *i << "': " << e.what() << std::endl;
				}
			}
		if(!tried)
			throw std::runtime_error("Specifying movie is required");
		if(!loaded)
			throw std::runtime_error("Can't load any of the movies specified");
		//Load ROM before starting the dumper.
		our_rom = &r;
		messages << "Using core: " << our_rom->rtype->get_core_identifier() << std::endl;
		our_rom->region = &movie.gametype->get_region();
		our_rom->load(movie.settings, movie.movie_rtc_second, movie.movie_rtc_subsecond);
		startup_lua_scripts(cmdline);
		dumper_startup(dumper, mode, prefix, length);
		main_loop(r, movie, true);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: " << e.what() << std::endl;
		fatal_error();
		return 1;
	}
	information_dispatch::do_dump_end();
	rrdata::close();
	quit_lua();
	return 0;
}
