#include "core/loadlib.hpp"
#include "interface/romtype.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "library/directory.hpp"
#include "library/loadlib.hpp"
#include "library/opus.hpp"
#include <stdexcept>
#include <sstream>
#include <dirent.h>

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

void autoload_libraries(void(*on_error)(const std::string& err))
{
	try {
		std::string extension = loadlib::library::extension();
		auto libs = enumerate_directory(get_config_path() + "/autoload", ".*");
		for(auto i : libs) {
			if(i.length() < extension.length() + 1)
				continue;
			if(i[i.length() - extension.length() - 1] != '.')
				continue;
			std::string tmp = i;
			if(tmp.substr(i.length() - extension.length()) != extension)
				continue;
			try {
				with_loaded_library(*new loadlib::module(loadlib::library(i)));
			} catch(std::exception& e) {
				std::string x = "Can't load '" + i + "': " + e.what();
				if(on_error)
					on_error(x);
				messages << x << std::endl;
			}
		}
		handle_post_loadlibrary();
	} catch(std::exception& e) {
		messages << e.what() << std::endl;
	}
}
