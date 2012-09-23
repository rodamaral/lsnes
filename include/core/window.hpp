#ifndef _window__hpp__included__
#define _window__hpp__included__

#include "core/keymapper.hpp"
#include "core/messagebuffer.hpp"
#include "core/status.hpp"
#include "library/framebuffer.hpp"
#include <string>
#include <map>
#include <list>
#include <stdexcept>

class emulator_status;

/**
 * Mutex.
 */
struct mutex
{
/**
 * Hold mutex RAII-style.
 */
	struct holder
	{
		holder(mutex& m) throw();
		~holder() throw();
	private:
		mutex& mut;
	};
/**
 * Create a mutex. The returned mutex can be deleted using delete.
 */
	static mutex& aquire() throw(std::bad_alloc);
/**
 * Create a recursive mutex. The returned mutex can be deleted using delete.
 */
	static mutex& aquire_rec() throw(std::bad_alloc);
/**
 * Destroy a mutex.
 */
	virtual ~mutex() throw();
/**
 * Lock a mutex.
 */
	virtual void lock() throw() = 0;
/**
 * Lock a mutex.
 */
	virtual void unlock() throw() = 0;
protected:
	mutex() throw();
};

/**
 * Condition variable.
 */
struct condition
{
/**
 * Create a condition variable. The returned condition can be freed using delete.
 */
	static condition& aquire(mutex& m) throw(std::bad_alloc);
/**
 * Destroy a condition.
 */
	virtual ~condition() throw();
/**
 * Return associated mutex.
 */
	mutex& associated() throw();
/**
 * Wait for condition. The associate mutex must be locked.
 */
	virtual bool wait(uint64_t max_usec) throw() = 0;
/**
 * Signal a condition. The associated mutex should be locked.
 */
	virtual void signal() throw() = 0;
protected:
	condition(mutex& m);
	mutex& assoc;
};

/**
 * Thread ID.
 */
struct thread_id
{
/**
 * Return thread id for this thread. Can be freed with delete.
 */
	static thread_id& me() throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~thread_id() throw();
/**
 * Is this thread me?
 */
	virtual bool is_me() throw() = 0;
protected:
	thread_id() throw();
};

/**
 * Thread.
 */
struct thread
{
/**
 * Create a thread, jumping to specified starting point.
 */
	static thread& create(void* (*entrypoint)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error);
/**
 * Destroy a thread, first joining it (if not already joined).
 */
	virtual ~thread() throw();
/**
 * Is this thread still alive?
 */
	bool is_alive() throw();
/**
 * Join a thread.
 */
	void* join() throw();
protected:
	thread() throw();
/**
 * Notify that this thread has quit.
 */
	void notify_quit(void* retval) throw();
/**
 * Join a thread.
 */
	virtual void _join() throw() = 0;
private:
	bool alive;
	bool joined;
	void* returns;
};

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
	keypress(modifier_set mod, keygroup& _key, short _value);
/**
 * Create new keypress (two keys).
 */
	keypress(modifier_set mod, keygroup& _key, keygroup& _key2, short _value);
/**
 * Modifier set.
 */
	modifier_set modifiers;
/**
 * The actual key (first)
 */
	keygroup* key1;
/**
 * The actual key (second)
 */
	keygroup* key2;
/**
 * Value for the press
 */
	short value;
};

/**
 * Functions implemented by the graphics plugin.
 *
 * Unless explicitly noted otherwise, all the methods are to be called from emulation thread if that exists, otherwise
 * from the main thread.
 */
struct graphics_plugin
{
/**
 * Graphics initialization function.
 *
 * - The first initialization function to be called by platform::init().
 */
	static void init() throw();
/**
 * Graphics quit function.
 *
 * - The last quit function to be called by platform::quit().
 */
	static void quit() throw();
/**
 * Notification when messages get updated.
 */
	static void notify_message() throw();
/**
 * Notification when status gets updated.
 */
	static void notify_status() throw();
/**
 * Notification when main screen gets updated.
 */
	static void notify_screen() throw();
/**
 * Show modal message dialog.
 *
 * Parameter text: The text for dialog.
 * Parameter confirm: If true, display confirmation dialog, if false, display notification dialog.
 * Returns: True if confirmation dialog was confirmed, otherwise false.
 */
	static bool modal_message(const std::string& text, bool confirm = false) throw();
/**
 * Displays fatal error message.
 *
 * - After this routine returns, the program will quit.
 * - The call can occur in any thread.
 */
	static void fatal_error() throw();
/**
 * Identification for graphics plugin.
 */
	static const char* name;
};

/**
 * Functions implemented by the joystick plugin.
 *
 * Unless explicitly noted otherwise, all the methods are to be called from emulation thread if that exists, otherwise
 * from the main thread.
 */
struct joystick_plugin
{
/**
 * Joystick initialization function.
 *
 * - The third initialization function to be called by window_init().
 * - The call occurs in the main thread.
 * - Implemented by the joystick plugin.
 */
	static void init() throw();
/**
 * Joystick quit function.
 *
 * - The third last quit function to be called by window_quit().
 * - The call occurs in the main thread.
 * - Implemented by the joystick plugin.
 */
	static void quit() throw();
/**
 * This thread becomes the joystick polling thread.
 *
 * - Called in joystick polling thread.
 */
	static void thread_fn() throw();
/**
 * Signal the joystick thread to quit.
 */
	static void signal() throw();
/**
 * Identification for joystick plugin.
 */
	static const char* name;
};

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
	static mutex& msgbuf_lock() throw();
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
	static void set_sound_device(const std::string& dev) throw();
/**
 * Show modal message dialog.
 *
 * Parameter text: The text for dialog.
 * Parameter confirm: If true, display confirmation dialog, if false, display notification dialog.
 * Returns: True, if confirmation dialog was confirmed, otherwise false.
 */
	static bool modal_message(const std::string& text, bool confirm = false) throw()
	{
		return graphics_plugin::modal_message(text, confirm);
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
		graphics_plugin::notify_message();
	}
/**
 * Notify changed status.
 */
	static void notify_status() throw()
	{
		graphics_plugin::notify_status();
	}
/**
 * Notify changed screen.
 */
	static void notify_screen() throw()
	{
		graphics_plugin::notify_screen();
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
