#ifndef _window__hpp__included__
#define _window__hpp__included__

#include "core/keymapper.hpp"
#include "core/rom.hpp"
#include "interface/romtype.hpp"
#include "library/messagebuffer.hpp"
#include "library/emustatus.hpp"
#include "library/framebuffer.hpp"
#include <string>
#include <map>
#include <list>
#include <stdexcept>

class emulator_status;

/**
 * Information about keypress.
 */
struct keypress
{
/**
 * Create null keypress (no modifiers, NULL key and released).
 */
	keypress();
/**
 * Create new keypress.
 */
	keypress(keyboard::modifier_set mod, keyboard::key& _key, short _value);
/**
 * Create new keypress (two keys).
 */
	keypress(keyboard::modifier_set mod, keyboard::key& _key, keyboard::key& _key2, short _value);
/**
 * Modifier set.
 */
	keyboard::modifier_set modifiers;
/**
 * The actual key (first)
 */
	keyboard::key* key1;
/**
 * The actual key (second)
 */
	keyboard::key* key2;
/**
 * Value for the press
 */
	short value;
};

//ROM request.
struct rom_request
{
	//List of core types.
	std::vector<core_type*> cores;
	//Selected core (default core on call).
	bool core_guessed;
	size_t selected;
	//Filename selected (on entry, filename hint).
	bool has_slot[ROM_SLOT_COUNT];
	bool guessed[ROM_SLOT_COUNT];
	std::string filename[ROM_SLOT_COUNT];
	std::string hash[ROM_SLOT_COUNT];
	std::string hashxml[ROM_SLOT_COUNT];
	//Canceled flag.
	bool canceled;
};


//Various methods corresponding to graphics_driver_*
struct _graphics_driver
{
	void (*init)();
	void (*quit)();
	void (*notify_message)();
	void (*notify_status)();
	void (*notify_screen)();
	void (*error_message)(const std::string& text);
	void (*fatal_error)();
	const char* (*name)();
	void (*action_updated)();
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
 * Notification when status gets updated.
 */
void graphics_driver_notify_status() throw();
/**
 * Notification when main screen gets updated.
 */
void graphics_driver_notify_screen() throw();
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
 * Enable/Disable an action.
 */
void graphics_driver_action_updated();
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
 * Get emulator status area
 *
 * returns: Emulator status area.
 */
	static emulator_status& get_emustatus() throw();
/**
 * Message buffer.
 */
	static messagebuffer msgbuf;
/**
 * Get message buffer lock.
 */
	static mutex_class& msgbuf_lock() throw();
/**
 * Set palette used on screen.
 */
	static void screen_set_palette(unsigned rshift, unsigned gshift, unsigned bshift) throw();
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
 * Notify changed status.
 */
	static void notify_status() throw()
	{
		graphics_driver_notify_status();
	}
/**
 * Notify changed screen.
 */
	static void notify_screen() throw()
	{
		graphics_driver_notify_screen();
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
 * Queue keypress.
 *
 * - Can be called from any thread.
 *
 * Parameter k: The keypress to queue.
 */
	static void queue(const keypress& k) throw(std::bad_alloc);
/**
 * Queue command.
 *
 * - Can be called from any thread.
 *
 * Parameter c: The command to queue.
 */
	static void queue(const std::string& c) throw(std::bad_alloc);
/**
 * Queue function to be called in emulation thread.
 *
 * - Can be called from any thread (exception: Synchronous mode can not be used from emulation nor main threads).
 *
 * Parameter f: The function to execute.
 * Parameter arg: Argument to pass to the function.
 * Parameter sync: If true, execute function call synchronously, else asynchronously.
 */
	static void queue(void (*f)(void* arg), void* arg, bool sync) throw(std::bad_alloc);
/**
 * Run all queues.
 */
	static void run_queues() throw();
/**
 * Set availablinty of system thread.
 */
	static void system_thread_available(bool av) throw();

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

template<typename T>
void functor_call_helper(void* args)
{
	(*reinterpret_cast<T*>(args))();
}

template<typename T>
void runemufn(T fn)
{
	platform::queue(functor_call_helper<T>, &fn, true);
}

/**
 * If set, queueing synchronous function produces a warning.
 */
extern volatile bool queue_synchronous_fn_warning;

#endif
