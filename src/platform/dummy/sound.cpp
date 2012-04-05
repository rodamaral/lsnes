#include "core/command.hpp"
#include "core/window.hpp"

#include <cstdlib>
#include <iostream>

void sound_plugin::init() throw()
{
}

void sound_plugin::quit() throw()
{
}

void sound_plugin::enable(bool enable) throw()
{
}

void sound_plugin::sample(uint16_t left, uint16_t right) throw()
{
}

bool sound_plugin::initialized()
{
	return true;
}

void sound_plugin::set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error)
{
	if(dev != "null")
		throw std::runtime_error("Bad sound device '" + dev + "'");
}

std::string sound_plugin::get_device() throw(std::bad_alloc)
{
	return "null";
}

std::map<std::string, std::string> sound_plugin::get_devices() throw(std::bad_alloc)
{
	std::map<std::string, std::string> ret;
	ret["null"] = "NULL sound output";
	return ret;
}

const char* sound_plugin::name = "Dummy sound plugin";
