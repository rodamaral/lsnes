#include "core/loadlib.hpp"
#include "interface/romtype.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include <stdexcept>
#include <sstream>

void handle_post_loadlibrary()
{
	if(new_core_flag) {
		new_core_flag = false;
		information_dispatch::do_new_core();
	}
}
