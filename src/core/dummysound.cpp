#include "core/command.hpp"
#include "core/audioapi.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
	void dummy_init() throw()
	{
		audioapi_voice_rate(0, 0);
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

	void dummy_set_device(const std::string& pdev, const std::string& rdev) throw(std::bad_alloc,
		std::runtime_error)
	{
		if(pdev != "null")
			throw std::runtime_error("Bad sound device '" + pdev + "'");
		if(rdev != "null")
			throw std::runtime_error("Bad sound device '" + rdev + "'");
	}

	std::string dummy_get_device(bool rec) throw(std::bad_alloc)
	{
		return "null";
	}

	std::map<std::string, std::string> dummy_get_devices(bool rec) throw(std::bad_alloc)
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

void audioapi_driver_set_device(const std::string& pdev, const std::string& rdev) throw(std::bad_alloc,
	std::runtime_error)
{
	driver.set_device(pdev, rdev);
}

std::string audioapi_driver_get_device(bool rec) throw(std::bad_alloc)
{
	return driver.get_device(rec);
}

std::map<std::string, std::string> audioapi_driver_get_devices(bool rec) throw(std::bad_alloc)
{
	return driver.get_devices(rec);
}

const char* audioapi_driver_name() throw()
{
	return driver.name();
}
