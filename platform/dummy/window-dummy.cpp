#include "window.hpp"
#include <cstdlib>
#include <iostream>

void graphics_init() {}
void graphics_quit() {}
void window::poll_inputs() throw(std::bad_alloc) {}
void window::notify_screen_update(bool full) throw() {}
void window::set_main_surface(screen& scr) throw() {}
void window::paused(bool enable) throw() {}
void window::wait_usec(uint64_t usec) throw(std::bad_alloc) {}
void window::cancel_wait() throw() {}
void window::set_window_compensation(uint32_t xoffset, uint32_t yoffset, uint32_t hscl, uint32_t vscl) {}

bool window::modal_message(const std::string& msg, bool confirm) throw(std::bad_alloc)
{
	std::cerr << "Modal message: " << msg << std::endl;
	return confirm;
}

void window::fatal_error() throw()
{
	std::cerr << "Exiting on fatal error." << std::endl;
	exit(1);
}

void window::message(const std::string& msg) throw(std::bad_alloc)
{
	if(msg[msg.length() - 1] == '\n')
		std::cout << msg;
	else
		std::cout << msg << std::endl;
}

uint64_t get_ticks_msec() throw()
{
	static uint64_t c = 0;
	return c++;
}

const char* graphics_plugin_name = "Dummy graphics plugin";
