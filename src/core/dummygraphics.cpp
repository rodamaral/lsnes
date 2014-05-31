#include "core/window.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
	uint64_t next_message_to_print = 0;
	bool is_dummy = true;

	void dummy_init() throw()
	{
		platform::pausing_allowed = false;
	}

	void dummy_quit() throw()
	{
	}

	void dummy_notify_message() throw()
	{
		//Read without lock becase we run in the same thread.
		while(platform::msgbuf.get_msg_first() + platform::msgbuf.get_msg_count() > next_message_to_print) {
			if(platform::msgbuf.get_msg_first() > next_message_to_print)
				next_message_to_print = platform::msgbuf.get_msg_first();
			else
				std::cout << platform::msgbuf.get_message(next_message_to_print++) << std::endl;
		}
	}

	void dummy_error_message(const std::string& text) throw()
	{
		std::cerr << "Error message: " << text << std::endl;
	}

	void dummy_fatal_error() throw()
	{
		std::cerr << "Exiting on fatal error." << std::endl;
	}

	void dummy_request_rom(rom_request& req)
	{
		throw std::runtime_error("Headless build does not support ROM requests");
	}

	const char* dummy_name() { return "Dummy graphics plugin"; }

	struct _graphics_driver driver = {
		.init = dummy_init,
		.quit = dummy_quit,
		.notify_message = dummy_notify_message,
		.error_message = dummy_error_message,
		.fatal_error = dummy_fatal_error,
		.name = dummy_name,
		.request_rom = dummy_request_rom
	};

}

graphics_driver::graphics_driver(_graphics_driver drv)
{
	driver = drv;
	is_dummy = false;
}

bool graphics_driver_is_dummy()
{
	return is_dummy;
}

void graphics_driver_init() throw()
{
	driver.init();
}

void graphics_driver_quit() throw()
{
	driver.quit();
}

void graphics_driver_notify_message() throw()
{
	driver.notify_message();
}

void graphics_driver_error_message(const std::string& text) throw()
{
	driver.error_message(text);
}

void graphics_driver_fatal_error() throw()
{
	driver.fatal_error();
}

const char* graphics_driver_name()
{
	return driver.name();
}

void graphics_driver_request_rom(rom_request& req)
{
	driver.request_rom(req);
}
