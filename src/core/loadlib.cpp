#include "core/loadlib.hpp"
#include "interface/romtype.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "library/directory.hpp"
#include "library/filelist.hpp"
#include "library/running-executable.hpp"
#include "library/loadlib.hpp"
#include "library/opus.hpp"
#include <stdexcept>
#include <sstream>
#include <dirent.h>

namespace
{
	std::string get_name(std::string path)
	{
#if defined(_WIN32) || defined(_WIN64)
		const char* sep = "\\/";
#else
		const char* sep = "/";
#endif
		size_t p = path.find_last_of(sep);
		std::string name;
		if(p == std::string::npos)
			name = path;
		else
			name = path.substr(p + 1);
		return name;
	}

	std::string get_user_library_dir()
	{
		return get_config_path() + "/autoload";
	}

	std::string get_system_library_dir()
	{
		std::string path;
		try {
			path = running_executable();
		} catch(...) {
			return "";
		}
#if __WIN32__ || __WIN64__
		const char* sep = "\\/";
#else
		const char* sep = "/";
#endif
		size_t p = path.find_last_of(sep);
		if(p >= path.length())
			path = ".";
		else if(p == 0)
			path = "/";
		else
			path = path.substr(0, p);
#if !__WIN32__ && !__WIN64__
		//If executable is in /bin, translate library path to corresponding /lib/lsnes.
		regex_results r = regex("(.*)/bin", path);
		if(r) path = r[1] + "/lib/lsnes";
		else
#endif
			path = path + "/plugins";
		return path;
	}
}

void handle_post_loadlibrary()
{
	if(new_core_flag) {
		new_core_flag = false;
		core_core::initialize_new_cores();
		notify_new_core();
	}
}

void with_loaded_library(const loadlib::module& l)
{
	try {
		if(!opus::libopus_loaded())
			opus::load_libopus(l);
	} catch(...) {
		//This wasn't libopus.
	}
}

namespace
{
	void load_libraries(std::set<std::string> libs, bool system,
		void(*on_error)(const std::string& libname, const std::string& err, bool system))
	{
		std::set<std::string> blacklist;
		std::set<std::string> killlist;
		//System plugins can't be killlisted nor blacklisted.
		if(!system) {
			filelist _blacklist(get_user_library_dir() + "/blacklist", get_user_library_dir());
			filelist _killlist(get_user_library_dir() + "/killlist", get_user_library_dir());
			killlist = _killlist.enumerate();
			blacklist = _blacklist.enumerate();
			//Try to kill the libs that don't exist anymore.
			for(auto i : libs)
				if(killlist.count(get_name(i)))
					remove(i.c_str());
			killlist = _killlist.enumerate();
			//All killlisted plugins are automatically blacklisted.
			for(auto i : killlist)
				blacklist.insert(i);
		}

		std::string extension = loadlib::library::extension();
		for(auto i : libs) {
			if(i.length() < extension.length() + 1)
				continue;
			if(i[i.length() - extension.length() - 1] != '.')
				continue;
			std::string tmp = i;
			if(tmp.substr(i.length() - extension.length()) != extension)
				continue;
			if(blacklist.count(get_name(i)))
				continue;
			try {
				with_loaded_library(*new loadlib::module(loadlib::library(i)));
			} catch(std::exception& e) {
				std::string x = "Can't load '" + i + "': " + e.what();

				if(on_error)
					on_error(get_name(i), e.what(), system);
				messages << x << std::endl;
			}
		}
	}
}

void autoload_libraries(void(*on_error)(const std::string& libname, const std::string& err, bool system))
{
	try {
		auto libs = enumerate_directory(get_user_library_dir(), ".*");
		load_libraries(libs, false, on_error);
	} catch(std::exception& e) {
		messages << e.what() << std::endl;
	}
	try {
		auto libs = enumerate_directory(get_system_library_dir(), ".*");
		load_libraries(libs, true, on_error);
	} catch(std::exception& e) {
		messages << e.what() << std::endl;
	}
	handle_post_loadlibrary();
}
