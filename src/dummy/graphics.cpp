#include "core/window.hpp"

#include <cstdlib>
#include <iostream>

bool dummy_interface = true;

namespace
{
	uint64_t next_message_to_print = 0;
}

void graphics_plugin::init() throw()
{
	platform::pausing_allowed = false;
}

void graphics_plugin::quit() throw()
{
}

void graphics_plugin::notify_message() throw()
{
	//Read without lock becase we run in the same thread.
	while(platform::msgbuf.get_msg_first() + platform::msgbuf.get_msg_count() > next_message_to_print) {
		if(platform::msgbuf.get_msg_first() > next_message_to_print)
			next_message_to_print = platform::msgbuf.get_msg_first();
		else
			std::cout << platform::msgbuf.get_message(next_message_to_print++) << std::endl;
	}
}

void graphics_plugin::notify_status() throw()
{
}

void graphics_plugin::notify_screen() throw()
{
}

bool graphics_plugin::modal_message(const std::string& text, bool confirm) throw()
{
	std::cerr << "Modal message: " << text << std::endl;
	return confirm;
}

void graphics_plugin::fatal_error() throw()
{
	std::cerr << "Exiting on fatal error." << std::endl;
}

std::string graphics_plugin::request_rom(core_type& coretype)
{
	throw std::runtime_error("Headless does not support ROM preloading");
}

const char* graphics_plugin::name = "Dummy graphics plugin";
