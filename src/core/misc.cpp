#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"
#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/window.hpp"
#include "library/crandom.hpp"
#include "library/directory.hpp"
#include "library/hex.hpp"
#include "library/loadlib.hpp"
#include "library/string.hpp"

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
	bool crashing = false;

	void fatal_signal_handler(int sig)
	{
		if(crashing) {
			write(2, "Double fault, exiting!\n", 23);
			signal(sig, SIG_DFL);
			raise(sig);
		}
		crashing = true;
		write(2, "Caught fatal signal!\n", 21);
		if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic->get_mfile(),
			lsnes_instance.mlogic->get_rrdata());
		signal(sig, SIG_DFL);
		raise(sig);
	}

	void terminate_handler()
	{
		if(crashing) {
			write(2, "Double fault, exiting!\n", 23);
			exit(1);
		}
		crashing = true;
		write(2, "Terminating abnormally!\n", 24);
		if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic->get_mfile(),
			lsnes_instance.mlogic->get_rrdata());
		std::cerr << "Exiting on fatal error" << std::endl;
		exit(1);
	}

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
	if(lsnes_instance.mlogic) emerg_save_movie(lsnes_instance.mlogic->get_mfile(),
		lsnes_instance.mlogic->get_rrdata());
	messages << "FATAL: Out of memory!" << std::endl;
	fatal_error();
}

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
	notify_new_core.errors_to(&messages.getstream());
	crandom::init();
	new_core_flag = false;	//We'll process the static cores anyway.
	reached_main_flag = true;
	lsnes_instance.command->set_oom_panic(OOM_panic);
	lsnes_instance.command->set_output(platform::out());
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
