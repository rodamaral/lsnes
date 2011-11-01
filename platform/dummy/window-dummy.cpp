#include "window.hpp"
#include <cstdlib>
#include <iostream>

namespace
{
	uint64_t next_message_to_print = 0;
}

void graphics_init() {}
void graphics_quit() {}
void window::poll_inputs() throw(std::bad_alloc) {}
void window::notify_screen_update(bool full) throw() {}
void window::set_main_surface(screen& scr) throw() {}
void window::paused(bool enable) throw() {}
void window::wait_usec(uint64_t usec) throw(std::bad_alloc) {}
void window::cancel_wait() throw() {}

bool window::modal_message(const std::string& msg, bool confirm) throw(std::bad_alloc)
{
	std::cerr << "Modal message: " << msg << std::endl;
	return confirm;
}

void window::fatal_error2() throw()
{
	std::cerr << "Exiting on fatal error." << std::endl;
	exit(1);
}

void window::notify_message() throw(std::bad_alloc, std::runtime_error)
{
	while(msgbuf.get_msg_first() + msgbuf.get_msg_count() > next_message_to_print) {
		if(msgbuf.get_msg_first() > next_message_to_print)
			next_message_to_print = msgbuf.get_msg_first();
		else
			std::cout << msgbuf.get_message(next_message_to_print++) << std::endl;
	}
}

const char* graphics_plugin_name = "Dummy graphics plugin";
