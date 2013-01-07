#include "core/loadlib.hpp"
#include "interface/romtype.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include <stdexcept>
#include <sstream>
#include <dirent.h>

namespace
{
	std::set<std::string> enumerate_directory(const std::string& dir)
	{
		std::set<std::string> x;
		DIR* d;
		dirent* d2;
		d = opendir(dir.c_str());
		if(!d) {
			messages << "Can't read directory '" << dir << "'" << std::endl;
			return x;
		}
		while(d2 = readdir(d))
			x.insert(dir + "/" + d2->d_name);
		closedir(d);
		return x;
	}
}

void handle_post_loadlibrary()
{
	if(new_core_flag) {
		new_core_flag = false;
		information_dispatch::do_new_core();
	}
}

void autoload_libraries()
{
	auto libs = enumerate_directory(get_config_path() + "/autoload");
	for(auto i : libs)
		try {
			new loaded_library(i);
			messages << "Autoloaded '" << i << "'" << std::endl;
		} catch(...) {
		}
	handle_post_loadlibrary();
}
