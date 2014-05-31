#ifndef _window__hpp__included__
#define _window__hpp__included__

#include "interface/romtype.hpp"
#include "library/keyboard.hpp"
#include "library/messagebuffer.hpp"
#include "library/framebuffer.hpp"
#include <string>
#include <map>
#include <list>
#include <stdexcept>

class rom_request;

//Various methods corresponding to graphics_driver_*
struct _graphics_driver
{
	void (*init)();
	void (*quit)();
	void (*notify_message)();
	void (*error_message)(const std::string& text);
	void (*fatal_error)();
	const char* (*name)();
	void (*request_rom)(rom_request& req);
};

struct graphics_driver
{
	graphics_driver(_graphics_driver drv);
};

//Is dummy graphics plugin.
bool graphics_driver_is_dummy();
/**
 * Graphics initialization function.
 *
 * - The first initialization function to be called by platform::init().
 */
void graphics_driver_init() throw();
/**
 * Graphics quit function.
 *
 * - The last quit function to be called by platform::quit().
 */
void graphics_driver_quit() throw();
/**
 * Notification when messages get updated.
 */
void graphics_driver_notify_message() throw();
/**
 * Show error message dialog when UI thread is free.
 *
 * Parameter text: The text for dialog.
 */
void graphics_driver_error_message(const std::string& text) throw();
/**
 * Displays fatal error message.
 *
 * - After this routine returns, the program will quit.
 * - The call can occur in any thread.
 */
void graphics_driver_fatal_error() throw();
/**
 * Identification for graphics plugin.
 */
const char* graphics_driver_name();
/**
 * Request a ROM image.
 */
void graphics_driver_request_rom(rom_request& req);

/**
 * Platform-specific-related functions.
 */
struct platform
{
/**
 * Initialize the system.
 */
	static void init();

/**
 * Shut down the system.
 */
	static void quit();
/**
 * Get output stream printing into message queue.
 *
 * Note that lines printed there should be terminated by '\n'.
 *
 * Implemented by the generic window code.
 *
 * returns: The output stream.
 * throws std::bad_alloc: Not enough memory.
 */
	static std::ostream& out() throw(std::bad_alloc);
/**
 * Message buffer.
 */
	static messagebuffer msgbuf;
/**
 * Get message buffer lock.
 */
	static threads::lock& msgbuf_lock() throw();
/**
 * Adds a messages to mesage queue to be shown.
 *
 * Implemented by the generic window code.
 *
 * parameter msg: The messages to add (split by '\n').
 * throws std::bad_alloc: Not enough memory.
 */
	static void message(const std::string& msg) throw(std::bad_alloc);
/**
 * Displays fatal error message, quitting after the user acks it (called by fatal_error()).
 *
 * Needs to be implemented by the graphics plugin.
 */
	static void fatal_error() throw();
/**
 * Enable or disable sound.
 *
 * Implemented by the generic window code.
 *
 * parameter enable: Enable sounds if true, otherwise disable sounds.
 */
	static void sound_enable(bool enable) throw();
/**
 * Are sounds enabled?
 */
	static bool is_sound_enabled() throw();
/**
 * Set sound device.
 */
	static void set_sound_device(const std::string& pdev, const std::string& rdev) throw();
/**
 * Show error message dialog after UI thread becomes free.
 *
 * Parameter text: The text for dialog.
 */
	static void error_message(const std::string& text) throw()
	{
		return graphics_driver_error_message(text);
	}
/**
 * Process command and keypress queues.
 *
 * - If emulating normally, this routine returns fast.
 * - If emulator is in pause mode, this routine will block until emulator has left pause mode.
 * - If emulator is in some special mode, this routine can block until said mode is left.
 */
	static void flush_command_queue() throw();
/**
 * Enable/Disable pause mode.
 *
 * - This function doesn't actually block. For actual paused blocking, use flush_command_queue().
 *
 * Parameter enable: Enable pause mode if true, disable pause mode if false.
 */
	static void set_paused(bool enable) throw();
/**
 * Wait specified number of milliseconds before returning.
 *
 * - The command and keypresses queues are processed while waiting.
 *
 * Parameter usec: The number of microseconds to wait.
 */
	static void wait(uint64_t usec) throw();
/**
 * Cause call to wait() to return immediately.
 */
	static void cancel_wait() throw();
/**
 * Notify received message.
 */
	static void notify_message() throw()
	{
		graphics_driver_notify_message();
	}
/**
 * Set modal pause mode.
 *
 * - Modal pause works like ordinary pause, except it uses a separate flag.
 *
 * Parameter enable: If true, enable modal pause, else disable it.
 */
	static void set_modal_pause(bool enable) throw();
/**
 * Run all queues.
 */
	static void run_queues() throw();

	static bool pausing_allowed;
	static double global_volume;
	static volatile bool do_exit_dummy_event_loop;
	static void dummy_event_loop() throw();
	static void exit_dummy_event_loop() throw();
};

class modal_pause_holder
{
public:
	modal_pause_holder();
	~modal_pause_holder();
private:
	modal_pause_holder(const modal_pause_holder&);
	modal_pause_holder& operator=(const modal_pause_holder&);
};

/**
 * If set, queueing synchronous function produces a warning.
 */
extern volatile bool queue_synchronous_fn_warning;

#endif
