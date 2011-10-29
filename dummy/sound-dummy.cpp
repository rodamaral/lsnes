#include "window.hpp"
#include <cstdlib>
#include <iostream>
#include "command.hpp"

void sound_init() {}
void sound_quit() {}
void window::sound_enable(bool enable) throw() {}
void window::play_audio_sample(uint16_t left, uint16_t right) throw() {}
void window::set_sound_rate(uint32_t rate_n, uint32_t rate_d) {}

bool window::sound_initialized()
{
	return true;
}

void window::set_sound_device(const std::string& dev)
{
	if(dev != "null")
		throw std::runtime_error("Bad sound device '" + dev + "'");
}

std::string window::get_current_sound_device()
{
	return "null";
}

std::map<std::string, std::string> window::get_sound_devices()
{
	std::map<std::string, std::string> ret;
	ret["null"] = "NULL sound output";
	return ret;
}

const char* sound_plugin_name = "Dummy sound plugin";
