#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/instance.hpp"
#include "core/rom.hpp"
#include "core/moviedata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/crandom.hpp"
#include "library/directory.hpp"
#include "library/hex.hpp"
#include "library/loadlib.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/skein.hpp"
#include "library/serialization.hpp"
#include "library/arch-detect.hpp"

#include <cstdlib>
#include <csignal>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>
#include <boost/filesystem.hpp>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#ifdef USE_LIBGCRYPT_SHA256
#include <gcrypt.h>
#endif

namespace
{
	bool reached_main_flag;

	char endian_char(int e)
	{
		if(e < 0)
			return 'L';
		if(e > 0)
			return 'B';
		return 'N';
	}

	void fatal_signal_handler(int sig)
	{
		write(2, "Caught fatal signal!\n", 21);
		if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic.get_mfile(),
			lsnes_instance.mlogic.get_rrdata());
		signal(sig, SIG_DFL);
		raise(sig);
	}

	void terminate_handler()
	{
		write(2, "Terminating abnormally!\n", 24);
		if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic.get_mfile(),
			lsnes_instance.mlogic.get_rrdata());
		std::cerr << "Exiting on fatal error" << std::endl;
		exit(1);
	}

	command::fnptr<const std::string&> test4(lsnes_cmds, "panicsave-movie", "", "",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
		if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic.get_mfile(),
			lsnes_instance.mlogic.get_rrdata());
	});

	//% is intentionally missing.
	const char* allowed_filename_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
		"^&'@{}[],$?!-#().+~_";
}

std::string safe_filename(const std::string& str)
{
	std::ostringstream o;
	for(size_t i = 0; i < str.length(); i++) {
		unsigned char ch = static_cast<unsigned char>(str[i]);
		if(strchr(allowed_filename_chars, ch))
			o << str[i];
		else
			o << "%" << hex::to8(ch);
	}
	return o.str();
}

struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline) throw(std::bad_alloc,
	std::runtime_error)
{
	std::string f;
	regex_results optp;
	for(auto i : cmdline) {
		if(!(optp = regex("--rom=(.+)", i)))
			continue;
		f = optp[1];
	}

	struct loaded_rom r;
	try {
		r = loaded_rom(f);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		throw std::runtime_error(std::string("Can't load ROM: ") + e.what());
	}

	std::string not_present = "N/A";
	for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
		std::string romname = "UNKNOWN ROM";
		std::string xmlname = "UNKNOWN XML";
		if(i < r.rtype->get_image_count()) {
			romname = (r.rtype->get_image_info(i).hname == "ROM") ? std::string("ROM") :
				(r.rtype->get_image_info(i).hname + " ROM");
			xmlname = r.rtype->get_image_info(i).hname + " XML";
		}
		if(r.romimg[i].data)	messages << romname << " hash: " << r.romimg[i].sha_256.read() << std::endl;
		if(r.romxml[i].data)	messages << xmlname << " hash: " << r.romxml[i].sha_256.read() << std::endl;
	}
	return r;
}

void dump_region_map() throw(std::bad_alloc)
{
	std::list<struct memory_region*> regions = lsnes_instance.memory.get_regions();
	for(auto i : regions) {
		std::ostringstream x;
		x << hex::to(i->base) << "-" << hex::to(i->last_address()) << " " << hex::to(i->size) << " ";
		messages << x.str() << (i->readonly ? "R-" : "RW") << endian_char(i->endian)
			<< (i->special ? 'I' : 'M') << " " << i->name << std::endl;
	}
}

void fatal_error() throw()
{
	platform::fatal_error();
	std::cout << "PANIC: Fatal error, can't continue." << std::endl;
	exit(1);
}

std::string get_config_path() throw(std::bad_alloc)
{
	const char* tmp;
	std::string basedir;
	if((tmp = getenv("APPDATA"))) {
		//If $APPDATA exists, it is the base directory
		basedir = tmp;
	} else if((tmp = getenv("XDG_CONFIG_HOME"))) {
		//If $XDG_CONFIG_HOME exists, it is the base directory
		basedir = tmp;
	} else if((tmp = getenv("HOME"))) {
		//If $HOME exists, the base directory is '.config' there.
		basedir = std::string(tmp) + "/.config";
	} else {
		//Last chance: Return current directory.
		return ".";
	}
	//Try to create 'lsnes'. If it exists (or is created) and is directory, great. Otherwise error out.
	std::string lsnes_path = basedir + "/lsnes";
	if(!directory::ensure_exists(lsnes_path)) {
		messages << "FATAL: Can't create configuration directory '" << lsnes_path << "'" << std::endl;
		fatal_error();
	}
	//Yes, this is racy, but portability is more important than being absolutely correct...
	std::string tfile = lsnes_path + "/test";
	remove(tfile.c_str());
	FILE* x;
	if(!(x = fopen(tfile.c_str(), "w+"))) {
		messages << "FATAL: Configuration directory '" << lsnes_path << "' is not writable" << std::endl;
		fatal_error();
	}
	fclose(x);
	remove(tfile.c_str());
	return lsnes_path;
}

void OOM_panic()
{
	if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic.get_mfile(),
		lsnes_instance.mlogic.get_rrdata());
	messages << "FATAL: Out of memory!" << std::endl;
	fatal_error();
}

std::ostream& messages_relay_class::getstream() { return platform::out(); }
messages_relay_class messages;

uint32_t gcd(uint32_t a, uint32_t b) throw()
{
	if(b == 0)
		return a;
	else
		return gcd(b, a % b);
}

std::string format_address(void* addr)
{
	return hex::to((uint64_t)addr);
}

bool in_global_ctors()
{
	return !reached_main_flag;
}

void reached_main()
{
	crandom::init();
	new_core_flag = false;	//We'll process the static cores anyway.
	reached_main_flag = true;
	lsnes_instance.command.set_oom_panic(OOM_panic);
	lsnes_instance.command.set_output(platform::out());
	loadlib::module::run_initializers();
	std::set_terminate(terminate_handler);
#ifdef SIGHUP
	signal(SIGHUP, fatal_signal_handler);
#endif
#ifdef SIGINT
	signal(SIGINT, fatal_signal_handler);
#endif
#ifdef SIGQUIT
	signal(SIGQUIT, fatal_signal_handler);
#endif
#ifdef SIGILL
	signal(SIGILL, fatal_signal_handler);
#endif
#ifdef SIGABRT
	signal(SIGABRT, fatal_signal_handler);
#endif
#ifdef SIGSEGV
	signal(SIGSEGV, fatal_signal_handler);
#endif
#ifdef SIGFPE
	signal(SIGFPE, fatal_signal_handler);
#endif
#ifdef SIGPIPE
	signal(SIGPIPE, fatal_signal_handler);
#endif
#ifdef SIGBUS
	signal(SIGBUS, fatal_signal_handler);
#endif
#ifdef SIGTRAP
	signal(SIGTRAP, fatal_signal_handler);
#endif
#ifdef SIGTERM
	signal(SIGTERM, fatal_signal_handler);
#endif
}

std::string mangle_name(const std::string& orig)
{
	std::ostringstream out;
	for(auto i : orig) {
		if(i == '(')
			out << "[";
		else if(i == ')')
			out << "]";
		else if(i == '|')
			out << "\xE2\x8F\xBF";
		else if(i == '/')
			out << "\xE2\x8B\xBF";
		else
			out << i;
	}
	return out.str();
}

std::string get_temp_file()
{
#if !defined(_WIN32) && !defined(_WIN64)
	char tname[512];
	strcpy(tname, "/tmp/lsnestmp_XXXXXX");
	int h = mkstemp(tname);
	if(h < 0)
		throw std::runtime_error("Failed to get new tempfile name");
	close(h);
	return tname;
#else
	char tpath[512];
	char tname[512];
	if(!GetTempPathA(512, tpath))
		throw std::runtime_error("Failed to get new tempfile name");
	if(!GetTempFileNameA(tpath, "lsn", 0, tname))
		throw std::runtime_error("Failed to get new tempfile name");
	return tname;
#endif
}

command::fnptr<const std::string&> macro_test(lsnes_cmds, "test-macro", "", "",
	[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
		regex_results r = regex("([0-9]+)[ \t](.*)", args);
		if(!r) {
			messages << "Bad syntax" << std::endl;
			return;
		}
		unsigned ctrl = parse_value<unsigned>(r[1]);
		auto pcid = controls.lcid_to_pcid(ctrl);
		if(pcid.first < 0) {
			messages << "Bad controller" << std::endl;
			return;
		}
		try {
			const port_controller* _ctrl = controls.get_blank().porttypes().port_type(pcid.first).
				controller_info->get(pcid.second);
			if(!_ctrl) {
				messages << "No controller data for controller" << std::endl;
				return;
			}
			controller_macro_data mdata(r[2].c_str(), controller_macro_data::make_descriptor(*_ctrl), 0);
			messages << "Macro: " << mdata.dump(*_ctrl) << std::endl;
		} catch(std::exception& e) {
			messages << "Exception: " << e.what() << std::endl;
		}
	});
