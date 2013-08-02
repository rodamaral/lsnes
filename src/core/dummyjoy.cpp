#include "core/joystickapi.hpp"
#include "core/keymapper.hpp"

namespace
{
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
}

joystick_driver::joystick_driver(_joystick_driver drv)
{
	driver = drv;
}

void joystick_driver_init() throw()
{
	lsnes_gamepads_init();
	driver.init();
}

void joystick_driver_quit() throw()
{
	driver.quit();
	lsnes_gamepads_deinit();
}

void joystick_driver_thread_fn() throw()
{
	driver.thread_fn();
}

void joystick_driver_signal() throw()
{
	driver.signal();
}

const char* joystick_driver_name()
{
	return driver.name();
}
