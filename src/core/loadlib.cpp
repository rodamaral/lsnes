#include "core/loadlib.hpp"
#include "interface/romtype.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "library/directory.hpp"
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

void with_loaded_library(loaded_library* l)
{
	try {
		if(!opus::libopus_loaded())
			opus::load_libopus(*l);
	} catch(...) {
		//This wasn't libopus.
	}
}

void autoload_libraries()
{
	try {
		auto libs = enumerate_directory(get_config_path() + "/autoload", ".*");
		for(auto i : libs)
			with_loaded_library(new loaded_library(i));
		handle_post_loadlibrary();
	} catch(std::exception& e) {
		messages << e.what() << std::endl;
	}
}
