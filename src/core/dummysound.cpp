#define AUDIO_WEAK
#include "core/command.hpp"
#include "core/audioapi.hpp"

#include <cstdlib>
#include <iostream>

void audioapi_driver_init() throw()
{
	audioapi_set_dummy_cb(true);
}

void audioapi_driver_quit() throw()
{
}

void audioapi_driver_enable(bool enable) throw()
{
}

bool audioapi_driver_initialized()
{
	return true;
}

void audioapi_driver_set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error)
{
	if(dev != "null")
		throw std::runtime_error("Bad sound device '" + dev + "'");
}

std::string audioapi_driver_get_device() throw(std::bad_alloc)
{
	return "null";
}

std::map<std::string, std::string> audioapi_driver_get_devices() throw(std::bad_alloc)
{
	std::map<std::string, std::string> ret;
	ret["null"] = "NULL sound output";
	return ret;
}

const char* audioapi_driver_name = "Dummy sound plugin";
