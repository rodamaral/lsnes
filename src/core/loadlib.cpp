#include "library/loadlib.hpp"
#include "core/command.hpp"
#include <stdexcept>
#include <sstream>

namespace {
	function_ptr_command<arg_filename> load_lib(lsnes_cmd, "load-library", "Load a library",
		"Syntax: load-library <file>\nLoad library <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			try {
				new loaded_library(args);
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Can't load '" << std::string(args) << "': " << e.what();
				throw std::runtime_error(x.str());
			}
		});
}
