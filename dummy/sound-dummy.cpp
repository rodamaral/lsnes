#include "window.hpp"
#include <cstdlib>
#include <iostream>
#include "command.hpp"

namespace
{
	function_ptr_command<const std::string&> enable_sound("enable-sound", "Enable/Disable sound",
		"Syntax: enable-sound <on/off>\nEnable or disable sound.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			std::string s = args;
			if(s == "on" || s == "true" || s == "1" || s == "enable" || s == "enabled")
				;
			else if(s == "off" || s == "false" || s == "0" || s == "disable" || s == "disabled")
				;
			else
				throw std::runtime_error("Bad sound setting");
		});
}

void sound_init() {}
void sound_quit() {}
void window::sound_enable(bool enable) throw() {}
void window::play_audio_sample(uint16_t left, uint16_t right) throw() {}
void window::set_sound_rate(uint32_t rate_n, uint32_t rate_d) {}

const char* sound_plugin_name = "Dummy sound plugin";
