#include "window.hpp"
#include <cstdlib>
#include <iostream>

namespace
{
	std::map<std::string, std::string> status;
}

void window::init() {}
void window::quit() {}
void window::poll_inputs() throw(std::bad_alloc) {}
void window::notify_screen_update(bool full) throw() {}
void window::set_main_surface(screen& scr) throw() {}
void window::paused(bool enable) throw() {}
void window::wait_usec(uint64_t usec) throw(std::bad_alloc) {}
void window::cancel_wait() throw() {}
void window::sound_enable(bool enable) throw() {}
void window::play_audio_sample(uint16_t left, uint16_t right) throw() {}
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

std::map<std::string, std::string>& window::get_emustatus() throw()
{
	return status;
}

uint64_t get_ticks_msec() throw()
{
	static uint64_t c = 0;
	return c++;
}

void window::set_sound_rate(uint32_t rate_n, uint32_t rate_d)
{
}
