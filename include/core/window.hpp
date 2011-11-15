#ifndef _window__hpp__included__
#define _window__hpp__included__

#include "render.hpp"
#include "messagebuffer.hpp"
#include <string>
#include <map>
#include <list>
#include <stdexcept>

#define WINSTATE_NORMAL 0
#define WINSTATE_COMMAND 1
#define WINSTATE_MODAL 2
#define WINSTATE_IDENTIFY 3

class window;

/**
 * Sound/Graphics init/quit functions. Sound init is called after graphics init, and vice versa for quit.
 *
 * These need to be implemented by the corresponding plugins.
 */
void graphics_init();
void sound_init();
void sound_quit();
void graphics_quit();
void joystick_init();
void joystick_quit();

/**
 * This is a handle to graphics system. Note that creating multiple contexts produces undefined results.
 */
class window
{
public:
/**
 * Window constructor.
 */
	window() throw() {}

/**
 * Initialize the graphics system.
 *
 * Implemented by generic window code.
 */
	static void init();

/**
 * Shut down the graphics system.
 *
 * Implemented by generic window code.
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
 * Implemented by the generic window code.
 *
 * returns: Emulator status area.
 */
	static std::map<std::string, std::string>& get_emustatus() throw();

/**
 * Message buffer.
 *
 * Implemented by the generic window code.
 */
	static messagebuffer msgbuf;

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
 *
 * Implemented by the generic window code.
 */
	static void set_sound_device(const std::string& dev) throw();

/******************************** GRAPHICS PLUGIN **********************************/
/**
 * Notification when messages get updated.
 *
 * Needs to be implemented by the graphics plugin.
 */
	static void notify_message() throw(std::bad_alloc, std::runtime_error);

/**
 * Displays a modal message, not returning until the message is acknowledged. Keybindings are not available, but
 * should quit be generated somehow, modal message will be closed and command callback triggered.
 *
 * Needs to be implemented by the graphics plugin.
 *
 * parameter msg: The message to show.
 * parameter confirm: If true, ask for Ok/cancel type input.
 * returns: If confirm is true, true if ok was chosen, false if cancel was chosen. Otherwise always false.
 * throws std::bad_alloc: Not enough memory.
 */
	static bool modal_message(const std::string& msg, bool confirm = false) throw(std::bad_alloc);

/**
 * Displays fatal error message, quitting after the user acks it (called by fatal_error()).
 *
 * Needs to be implemented by the graphics plugin.
 */
	static void fatal_error2() throw();

/**
 * Processes inputs. If in non-modal mode (normal mode without pause), this returns quickly. Otherwise it waits
 * for modal mode to exit. Also needs to call window::poll_joysticks().
 *
 * Needs to be implemented by the graphics plugin.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void poll_inputs() throw(std::bad_alloc);

/**
 * Enable/Disable pause mode.
 *
 * Needs to be implemented by the graphics plugin.
 *
 * parameter enable: Enable pause if true, disable otherwise.
 */
	static void paused(bool enable) throw();

/**
 * Wait specified number of microseconds (polling for input).
 *
 * Needs to be implemented by the graphics plugin.
 *
 * parameter usec: Number of us to wait.
 * throws std::bad_alloc: Not enough memory.
 */
	static void wait_usec(uint64_t usec) throw(std::bad_alloc);

/**
 * Cancel pending wait_usec, making it return now.
 *
 * Needs to be implemented by the graphics plugin.
 */
	static void cancel_wait() throw();

/******************************** SOUND PLUGIN **********************************/
/**
 * Enable or disable sound.
 *
 * Needs to be implemented by the sound plugin.
 *
 * parameter enable: Enable sounds if true, otherwise disable sounds.
 */
	static void _sound_enable(bool enable) throw();

/**
 * Input audio sample (at specified rate).
 *
 * Needs to be implemented by the sound plugin.
 *
 * parameter left: Left sample.
 * parameter right: Right sample.
 */
	static void play_audio_sample(uint16_t left, uint16_t right) throw();

/**
 * Has the sound system been successfully initialized?
 *
 * Needs to be implemented by the sound plugin.
 */
	static bool sound_initialized();

/**
 * Set sound device.
 *
 * Needs to be implemented by the sound plugin.
 */
	static void _set_sound_device(const std::string& dev);

/**
 * Get current sound device.
 *
 * Needs to be implemented by the sound plugin.
 */
	static std::string get_current_sound_device();

/**
 * Get available sound devices.
 *
 * Needs to be implemented by the sound plugin.
 */
	static std::map<std::string, std::string> get_sound_devices();

/******************************** JOYSTICK PLUGIN **********************************/
/**
 * Poll joysticks.
 *
 * Needs to be implemented by the joystick plugin.
 */
	static void poll_joysticks();
private:
	window(const window&);
	window& operator==(const window&);
};


/**
 * Names of plugins.
 */
extern const char* sound_plugin_name;
extern const char* graphics_plugin_name;
extern const char* joystick_plugin_name;

#endif
