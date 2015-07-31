#include "core/joystickapi.hpp"
#include "core/keymapper.hpp"

namespace
{
	threads::thread* joystick_thread_handle;
	void dummy_init() throw() {}
	void dummy_quit() throw() {}
	void dummy_thread_fn() throw() {}
	void dummy_signal() throw() {}
	const char* dummy_name() { return "Dummy joystick plugin"; }

	_joystick_driver driver = {
		.init = dummy_init,
		.quit = dummy_quit,
		.thread_fn = dummy_thread_fn,
		.signal = dummy_signal,
		.name = dummy_name
	};

	void* joystick_thread(int _args)
	{
		driver.thread_fn();
		return NULL;
	}
}

joystick_driver::joystick_driver(_joystick_driver drv)
{
	driver = drv;
}

void joystick_driver_init(bool soft) throw()
{
	if(!soft) lsnes_gamepads_init();
	driver.init();
	joystick_thread_handle = new threads::thread(joystick_thread, 6);
}

void joystick_driver_quit(bool soft) throw()
{
	driver.quit();
	joystick_thread_handle->join();
	joystick_thread_handle = NULL;
	if(!soft) lsnes_gamepads_deinit();
}

void joystick_driver_signal() throw()
{
	driver.signal();
}

const char* joystick_driver_name()
{
	return driver.name();
}
