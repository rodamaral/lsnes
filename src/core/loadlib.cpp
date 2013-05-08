#include "core/loadlib.hpp"
#include "interface/romtype.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "library/directory.hpp"
#include <stdexcept>
#include <sstream>
#include <dirent.h>

void handle_post_loadlibrary()
{
	if(new_core_flag) {
		new_core_flag = false;
		information_dispatch::do_new_core();
	}
}

void autoload_libraries()
{
	try {
		auto libs = enumerate_directory(get_config_path() + "/autoload", ".*");
		for(auto i : libs)
			try {
				new loaded_library(i);
				messages << "Autoloaded '" << i << "'" << std::endl;
			} catch(...) {
			}
		handle_post_loadlibrary();
	} catch(std::exception& e) {
		messages << e.what() << std::endl;
	}
}
