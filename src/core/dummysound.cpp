#define AUDIO_WEAK
#include "core/command.hpp"
#include "core/audioapi.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
	void dummy_init() throw()
	{
		audioapi_set_dummy_cb(true);
	}

	void dummy_quit() throw()
	{
	}

	void dummy_enable(bool enable) throw()
	{
	}

	bool dummy_initialized()
	{
		return true;
	}

	void dummy_set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error)
	{
		if(dev != "null")
			throw std::runtime_error("Bad sound device '" + dev + "'");
	}

	std::string dummy_get_device() throw(std::bad_alloc)
	{
		return "null";
	}

	std::map<std::string, std::string> dummy_get_devices() throw(std::bad_alloc)
	{
		std::map<std::string, std::string> ret;
		ret["null"] = "NULL sound output";
		return ret;
	}

	const char* dummy_name() { return "Dummy sound plugin"; }

	_audioapi_driver driver = {
		.init = dummy_init,
		.quit = dummy_quit,
		.enable = dummy_enable,
		.initialized = dummy_initialized,
		.set_device = dummy_set_device,
		.get_device = dummy_get_device,
		.get_devices = dummy_get_devices,
		.name = dummy_name
	};
}

audioapi_driver::audioapi_driver(struct _audioapi_driver _driver)
{
	driver = _driver;
}

void audioapi_driver_init() throw()
{
	driver.init();
}

void audioapi_driver_quit() throw()
{
	driver.quit();
}

void audioapi_driver_enable(bool _enable) throw()
{
	driver.enable(_enable);
}

bool audioapi_driver_initialized()
{
	return driver.initialized();
}

void audioapi_driver_set_device(const std::string& dev) throw(std::bad_alloc, std::runtime_error)
{
	driver.set_device(dev);
}

std::string audioapi_driver_get_device() throw(std::bad_alloc)
{
	return driver.get_device();
}

std::map<std::string, std::string> audioapi_driver_get_devices() throw(std::bad_alloc)
{
	return driver.get_devices();
}

const char* audioapi_driver_name() throw()
{
	return driver.name();
}