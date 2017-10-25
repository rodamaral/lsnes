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
#include "core/filedownload.hpp"
#include "core/mainloop.hpp"
#include "core/messages.hpp"
#include "core/misc.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/directory.hpp"
#include "library/crandom.hpp"
#include "library/string.hpp"

#include <sys/time.h>
#include <sstream>

namespace
{
	bool hashing_in_progress = false;
	uint64_t hashing_left = 0;
	int64_t last_update = 0;

	std::string do_download_movie(const std::string& origname)
	{
		if(!regex_match("[A-Za-z][A-Za-z0-9+.-]*:.+", origname))
			return origname;	//File.
		if(directory::is_regular(origname))
			return origname;	//Even exists.
		//Okay, we need to download this.
		auto download_in_progress = new file_download();
		download_in_progress->url = lsnes_uri_rewrite(origname);
		if(download_in_progress->url != origname)
			messages << "Internally redirecting to " << download_in_progress->url << std::endl;
		download_in_progress->target_slot = "dumpavi_download_tmp";
		download_in_progress->do_async(*lsnes_instance.rom);
		messages << "Downloading " << download_in_progress->url << ":" << std::endl;
		while(!download_in_progress->finished) {
			messages << download_in_progress->statusmsg() << std::endl;
			usleep(1000000);
		}
		if(download_in_progress->errormsg != "") {
			std::string err = download_in_progress->errormsg;
			delete download_in_progress;
			throw std::runtime_error(err);
		}
		delete download_in_progress;
		messages << "Download finished." << std::endl;
		return "$MEMORY:dumpavi_download_tmp";
	}

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

	class myavsnoop : public dumper_base
	{
	public:
		myavsnoop(dumper_base& _dumper, uint64_t frames_to_dump)
			: dumper(_dumper)
		{
			frames_dumped = 0;
			total = frames_to_dump;
			lsnes_instance.mdumper->add_dumper(*this);
		}

		~myavsnoop() throw()
		{
			lsnes_instance.mdumper->drop_dumper(*this);
		}

		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			frames_dumped++;
			if(frames_dumped % 100 == 0) {
				std::cout << "Dumping frame " << frames_dumped << "/" << total << " ("
					<< (100 * frames_dumped / total) << "%)" << std::endl;
			}
			if(frames_dumped >= total) {
				//Rough way to end it.
				CORE().command->invoke("quit-emulator");
			}
		}
		void on_sample(short l, short r)
		{
			//We aren't interested in samples.
		}
		void on_rate_change(uint32_t n, uint32_t d)
		{
			//We aren't interested in samples.
		}
		void on_gameinfo_change(const master_dumper::gameinfo& gi)
		{
			std::cout << "Game:" << gi.gamename << std::endl;
			std::cout << "Length:" << gi.get_readable_time(3) << std::endl;
			std::cout << "Rerecords:" << gi.get_rerecords() << std::endl;
			for(unsigned i = 0; i < gi.get_author_count(); i++)
				std::cout << "Author: " << gi.get_author_long(i) << std::endl;
		}
		void on_end()
		{
			std::cout << "Finished!" << std::endl;
			delete this;
		}
	private:
		uint64_t frames_dumped;
		uint64_t total;
		dumper_base& dumper;
	};

	void dumper_startup(dumper_factory_base& dumper, const std::string& mode, const std::string& prefix,
		uint64_t length)
	{
		dumper_base* _dumper;
		std::cout << "Invoking dumper" << std::endl;
		try {
			_dumper = lsnes_instance.mdumper->start(dumper, mode, prefix);
		} catch(std::exception& e) {
			std::cerr << "Can't start dumper: " << e.what() << std::endl;
			exit(1);
		}
		if(lsnes_instance.mdumper->get_dumper_count()) {
			std::cout << "Dumper attach confirmed" << std::endl;
		} else {
			std::cout << "Can't start dumper!" << std::endl;
			exit(1);
		}
		auto d = new myavsnoop(*_dumper, length);
		d->on_gameinfo_change(lsnes_instance.mdumper->get_gameinfo());
	}

	void startup_lua_scripts(const std::vector<std::string>& cmdline)
	{
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string a = *i;
			if(a.length() > 6 && a.substr(0, 6) == "--lua=") {
				lsnes_instance.lua2->add_startup_script(a.substr(6));
			}
		}
	}

	struct dumper_factory_base& locate_dumper(const std::string& name)
	{
		dumper_factory_base* _dumper = NULL;
		std::set<dumper_factory_base*> dumpers = dumper_factory_base::get_dumper_set();
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
		if((detail & dumper_factory_base::target_type_mask) == dumper_factory_base::target_type_file)
			r = r + "TARGET_FILE";
		else if((detail & dumper_factory_base::target_type_mask) == dumper_factory_base::target_type_prefix)
			r = r + "TARGET_PREFIX";
		else if((detail & dumper_factory_base::target_type_mask) == dumper_factory_base::target_type_special)
			r = r + "TARGET_SPECIAL";
		else
			r = r + "TARGET_UNKNOWN";
		return r;
	}

	dumper_factory_base& get_dumper(const std::vector<std::string>& cmdline, std::string& mode,
		std::string& prefix, uint64_t& length, bool& overdump_mode, uint64_t& overdump_length)
	{
		bool dumper_given = false;
		std::string dumper;
		bool mode_given = false;
		prefix = "avidump";
		length = 0;
		overdump_mode = false;
		overdump_length = 0;
		for(auto i = cmdline.begin(); i != cmdline.end(); i++) {
			std::string a = *i;
			if(a.length() >= 9 && a.substr(0, 9) == "--dumper=") {
				dumper_given = true;
				dumper = a.substr(9);
			} else if(a.length() >= 7 && a.substr(0, 7) == "--mode=") {
				mode_given = true;
				mode = a.substr(7);
			} else if(a.length() >= 9 && a.substr(0, 9) == "--prefix=")
				prefix = a.substr(9);
			else if(a.length() >= 9 && a.substr(0, 9) == "--length=")
				try {
					length = raw_lexical_cast<uint64_t>(a.substr(9));
					if(!length)
						throw std::runtime_error("Length out of range (1-)");
					if(overdump_mode)
						throw std::runtime_error("--length and --overdump-length are "
							"mutually exclusive.");
				} catch(std::exception& e) {
					std::cerr << "Bad --length: " << e.what() << std::endl;
					exit(1);
				}
			else if(a.length() >= 18 && a.substr(0, 18) == "--overdump-length=")
				try {
					overdump_length = raw_lexical_cast<uint64_t>(a.substr(18));
					overdump_mode = true;
					if(length)
						throw std::runtime_error("--length and --overdump-length are "
							"mutually exclusive.");
				} catch(std::exception& e) {
					std::cerr << "Bad --overdump-length: " << e.what() << std::endl;
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
					lsnes_instance.setcache->set(name, val);
				} catch(std::exception& e) {
					std::cerr << "Can't set '" << name << "' to '" << val << "': " << e.what()
						<< std::endl;
					exit(1);
				}
			} else if(a.length() >= 12 && a.substr(0, 15) == "--load-library=")
				try {
					with_loaded_library(*new loadlib::module(loadlib::library(a.substr(15))));
					handle_post_loadlibrary();
				} catch(std::runtime_error& e) {
					std::cerr << "Can't load '" << a.substr(15) << "': " << e.what() << std::endl;
					exit(1);
				}
		}
		if(dumper == "list") {
			//Help on dumpers.
			std::set<dumper_factory_base*> dumpers = dumper_factory_base::get_dumper_set();
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
			dumper_factory_base& _dumper = locate_dumper(dumper);
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
		dumper_factory_base& _dumper = locate_dumper(dumper);
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
		if(!length && !overdump_mode) {
			std::cerr << "--length=<frames> or --overdump-length=<frames> has to be specified"
				<< std::endl;
			exit(1);
		}
		return locate_dumper(dumper);
	}
}

int main(int argc, char** argv)
{
	try {
		crandom::init();
	} catch(std::exception& e) {
		std::cerr << "Error initializing system RNG" << std::endl;
		return 1;
	}

	reached_main();
	std::vector<std::string> cmdline;
	for(int i = 1; i < argc; i++)
		cmdline.push_back(argv[i]);
	uint64_t length, overdump_length;
	bool overdump_mode;
	std::string mode, prefix;

	dumper_factory_base& dumper = get_dumper(cmdline, mode, prefix, length, overdump_mode, overdump_length);

	set_random_seed();
	platform::init();
	init_lua(lsnes_instance);
	lsnes_instance.mdumper->set_output(&messages.getstream());
	set_hasher_callback(hash_callback);

	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;
	messages << "Command line is: ";
	for(auto k = cmdline.begin(); k != cmdline.end(); k++)
		messages << "\"" << *k << "\" ";
	messages << std::endl;

	std::string cfgpath = get_config_path();
	autoload_libraries();

	lsnes_uri_rewrite.load(cfgpath + "/lsnesurirewrite.cfg");

	for(auto i : cmdline) {
		regex_results r;
		if(r = regex("--firmware-path=(.*)", i)) {
			try {
				lsnes_instance.setcache->set("firmwarepath", r[1]);
				std::cerr << "Set firmware path to '" << r[1] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set firmware path to '" << r[1] << "': " << e.what() << std::endl;
			}
		}
		if(r = regex("--setting-(.*)=(.*)", i)) {
			try {
				lsnes_instance.setcache->set(r[1], r[2]);
				std::cerr << "Set " << r[1] << " to '" << r[2] << "'" << std::endl;
			} catch(std::exception& e) {
				std::cerr << "Can't set " << r[1] << " to '" << r[2] << "': " << e.what()
					<< std::endl;
			}
		}
	}

	std::string movfn;
	for(auto i : cmdline) {
		if(i.length() > 0 && i[0] != '-') {
			movfn = i;
		}
	}
	if(movfn == "") {
		messages << "Movie filename required" << std::endl;
		return 0;
	}

	try {
		movfn = do_download_movie(movfn);
	} catch(std::exception& e) {
		messages << "FATAL: Can't download movie: " << e.what() << std::endl;
		quit_lua(lsnes_instance);
		fatal_error();
		exit(1);
	}

	init_main_callbacks();
	messages << "--- Loading ROM ---" << std::endl;
	struct loaded_rom r;
	try {
		std::map<std::string, std::string> tmp;
		r = construct_rom(movfn, cmdline);
		r.load(tmp, 1000000000, 0);
		messages << "Using core: " << r.get_core_identifier() << std::endl;
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: Can't load ROM: " << e.what() << std::endl;
		quit_lua(lsnes_instance);
		fatal_error();
		exit(1);
	}
	messages << "Detected region: " << r.get_sysregion().get_name() << std::endl;
	lsnes_instance.framerate->set_nominal_framerate(r.region_approx_framerate());

	messages << "--- End of Startup --- " << std::endl;

	moviefile* movie;
	try {
		movie = new moviefile(movfn, r.get_internal_rom_type());
		//Load ROM before starting the dumper.
		*lsnes_instance.rom = r;
		messages << "Using core: " << lsnes_instance.rom->get_core_identifier() << std::endl;
		lsnes_instance.rom->set_internal_region(movie->gametype->get_region());
		lsnes_instance.rom->load(movie->settings, movie->movie_rtc_second, movie->movie_rtc_subsecond);
		startup_lua_scripts(cmdline);
		if(overdump_mode)
			length = overdump_length + movie->get_frame_count();
		dumper_startup(dumper, mode, prefix, length);
		main_loop(r, *movie, true);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		messages << "FATAL: " << e.what() << std::endl;
		quit_lua(lsnes_instance);
		fatal_error();
		return 1;
	}
	quit_lua(lsnes_instance);
	lsnes_instance.mlogic->release_memory();
	lsnes_instance.buttons->cleanup();
	return 0;
}
