#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/avsnoop.hpp"
#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/window.hpp"

#include <sys/time.h>
#include <sstream>

namespace
{
	class myavsnoop : public av_snooper
	{
	public:
		myavsnoop(uint64_t frames_to_dump)
			: av_snooper("myavsnoop-monitor")
		{
			frames_dumped = 0;
			total = frames_to_dump;
		}

		~myavsnoop()
		{
		}

		void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, const uint32_t* raw, bool hires,
			bool interlaced, bool overscan, unsigned region) throw(std::bad_alloc, std::runtime_error)
		{
			frames_dumped++;
			if(frames_dumped % 100 == 0) {
				std::cout << "Dumping frame " << frames_dumped << "/" << total << " ("
					<< (100 * frames_dumped / total) << "%)" << std::endl;
			}
			if(frames_dumped == total) {
				//Rough way to end it.
				av_snooper::_end();
				exit(1);
			}
		}

		void end() throw(std::bad_alloc, std::runtime_error)
		{
			std::cout << "Finished!" << std::endl;
		}
	private:
		uint64_t frames_dumped;
		uint64_t total;
	};

	void dumper_startup(const std::vector<std::string>& cmdline)
	{
		unsigned level = 7;
		std::string prefix = "avidump";
		uint64_t length = 0;
		bool jmd = false;
		bool sdmp = false;
		bool ssflag = false;
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string a = *i;
			if(a == "--jmd")
				jmd = true;
			if(a == "--sdmp")
				sdmp = true;
			if(a == "--ss")
				ssflag = true;
			if(a.length() > 9 && a.substr(0, 9) == "--prefix=")
				prefix = a.substr(9);
			if(a.length() > 8 && a.substr(0, 8) == "--level=")
				try {
					level = boost::lexical_cast<unsigned>(a.substr(8));
					if(level < 0 || level > 18)
						throw std::runtime_error("Level out of range (0-18)");
				} catch(std::exception& e) {
					std::cerr << "Bad --level: " << e.what() << std::endl;
					exit(1);
				}
			if(a.length() > 9 && a.substr(0, 9) == "--length=")
				try {
					length = boost::lexical_cast<uint64_t>(a.substr(9));
					if(!length)
						throw std::runtime_error("Length out of range (1-)");
				} catch(std::exception& e) {
					std::cerr << "Bad --length: " << e.what() << std::endl;
					exit(1);
				}
		}
		if(!length) {
			std::cerr << "--length=<frames> has to be specified" << std::endl;
			exit(1);
		}
		if(jmd && sdmp) {
			std::cerr << "--jmd and --sdmp are mutually exclusive" << std::endl;
			exit(1);
		}
		std::cout << "Invoking dumper" << std::endl;
		std::ostringstream cmd;
		if(sdmp && ssflag)
			cmd << "dump-sdmpss " << prefix;
		else if(sdmp)
			cmd << "dump-sdmp " << prefix;
		else if(jmd)
			cmd << "dump-jmd " << level << " " << prefix;
		else
			cmd << "dump-avi " << level << " " << prefix;
		command::invokeC(cmd.str());
		if(av_snooper::dump_in_progress()) {
			std::cout << "Dumper attach confirmed" << std::endl;
		} else {
			std::cout << "Can't start dumper!" << std::endl;
			exit(1);
		}
		myavsnoop* s = new myavsnoop(length);
	}

	void startup_lua_scripts(const std::vector<std::string>& cmdline)
	{
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string a = *i;
			if(a.length() > 6 && a.substr(0, 6) == "--lua=") {
				command::invokeC("run-lua " + a.substr(6));
			}
		}
	}
}

class my_interfaced : public SNES::Interface
{
	string path(SNES::Cartridge::Slot slot, const string &hint)
	{
		return "./";
	}
};


int main(int argc, char** argv)
{
	std::vector<std::string> cmdline;
	for(int i = 1; i < argc; i++)
		cmdline.push_back(argv[i]);
	my_interfaced intrf;
	SNES::interface = &intrf;

	set_random_seed();

	{
		std::ostringstream x;
		x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
		bsnes_core_version = x.str();
	}
	init_lua();

	messages << "BSNES version: " << bsnes_core_version << std::endl;
	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;
	messages << "Command line is: ";
	for(auto k = cmdline.begin(); k != cmdline.end(); k++)
		messages << "\"" << *k << "\" ";
	messages << std::endl;

	std::string cfgpath = get_config_path();

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
	try {
		bool tried = false;
		bool loaded = false;
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
			throw std::runtime_error("Specifying movie is required");
		if(!loaded)
			throw std::runtime_error("Can't load any of the movies specified");
		//Load ROM before starting the dumper.
		our_rom = &r;
		our_rom->region = gtype::toromregion(movie.gametype);
		our_rom->load();
		dumper_startup(cmdline);
		startup_lua_scripts(cmdline);
		main_loop(r, movie, true);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: " << e.what() << std::endl;
		fatal_error();
		return 1;
	}
	rrdata::close();
	return 0;
}
