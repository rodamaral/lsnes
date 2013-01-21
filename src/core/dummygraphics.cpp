#define GRAPHICS_WEAK
#include "core/window.hpp"

#include <cstdlib>
#include <iostream>

bool graphics_driver_is_dummy = true;

namespace
{
	uint64_t next_message_to_print = 0;
}

void graphics_driver_init() throw()
{
	platform::pausing_allowed = false;
}

void graphics_driver_quit() throw()
{
}

void graphics_driver_notify_message() throw()
{
	//Read without lock becase we run in the same thread.
	while(platform::msgbuf.get_msg_first() + platform::msgbuf.get_msg_count() > next_message_to_print) {
		if(platform::msgbuf.get_msg_first() > next_message_to_print)
			next_message_to_print = platform::msgbuf.get_msg_first();
		else
			std::cout << platform::msgbuf.get_message(next_message_to_print++) << std::endl;
	}
}

void graphics_driver_notify_status() throw()
{
}

void graphics_driver_notify_screen() throw()
{
}

bool graphics_driver_modal_message(const std::string& text, bool confirm) throw()
{
	std::cerr << "Modal message: " << text << std::endl;
	return confirm;
}

void graphics_driver_fatal_error() throw()
{
	std::cerr << "Exiting on fatal error." << std::endl;
}

const char* graphics_driver_name = "Dummy graphics plugin";
