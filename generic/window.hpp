#ifndef _window__hpp__included__
#define _window__hpp__included__

#include "render.hpp"
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
 * This is a handle to graphics system. Note that creating multiple contexts produces undefined results.
 */
class window
{
public:
	window() throw() {}
	
/**
 * Initialize the graphics system.
 */
	static void init();

/**
 * Shut down the graphics system.
 */
	static void quit();

/**
 * Adds a messages to mesage queue to be shown.
 *
 * parameter msg: The messages to add (split by '\n').
 * throws std::bad_alloc: Not enough memory.
 */
	static void message(const std::string& msg) throw(std::bad_alloc);

/**
 * Get output stream printing into message queue.
 *
 * Note that lines printed there should be terminated by '\n'.
 *
 * returns: The output stream.
 * throws std::bad_alloc: Not enough memory.
 */
	static std::ostream& out() throw(std::bad_alloc);

/**
 * Displays a modal message, not returning until the message is acknowledged. Keybindings are not available, but
 * should quit be generated somehow, modal message will be closed and command callback triggered.
 *
 * parameter msg: The message to show.
 * parameter confirm: If true, ask for Ok/cancel type input.
 * returns: If confirm is true, true if ok was chosen, false if cancel was chosen. Otherwise always false.
 * throws std::bad_alloc: Not enough memory.
 */
	static bool modal_message(const std::string& msg, bool confirm = false) throw(std::bad_alloc);

/**
 * Displays fatal error message, quitting after the user acks it.
 */
	static void fatal_error() throw();

/**
 * Processes inputs. If in non-modal mode (normal mode without pause), this returns quickly. Otherwise it waits
 * for modal mode to exit.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void poll_inputs() throw(std::bad_alloc);

/**
 * Get emulator status area
 *
 * returns: Emulator status area.
 */
	static std::map<std::string, std::string>& get_emustatus() throw();

/**
 * Notify that the screen has been updated.
 *
 * parameter full: Do full refresh if true.
 */
	static void notify_screen_update(bool full = false) throw();

/**
 * Set the screen to use as main surface.
 *
 * parameter scr: The screen to use.
 */
	static void set_main_surface(screen& scr) throw();

/**
 * Enable/Disable pause mode.
 *
 * parameter enable: Enable pause if true, disable otherwise.
 */
	static void paused(bool enable) throw();

/**
 * Wait specified number of milliseconds (polling for input).
 *
 * parameter msec: Number of ms to wait.
 * throws std::bad_alloc: Not enough memory.
 */
	static void wait_msec(uint64_t msec) throw(std::bad_alloc);

/**
 * Cancel pending wait_msec, making it return now.
 */
	static void cancel_wait() throw();

/**
 * Enable or disable sound.
 *
 * parameter enable: Enable sounds if true, otherwise disable sounds.
 */
	static void sound_enable(bool enable) throw();

/**
 * Input audio sample (at 32040.5Hz).
 *
 * parameter left: Left sample.
 * parameter right: Right sample.
 */
	static void play_audio_sample(uint16_t left, uint16_t right) throw();

/**
 * Set window main screen compensation parameters. This is used for mouse click reporting.
 *
 * parameter xoffset: X coordinate of origin.
 * parameter yoffset: Y coordinate of origin.
 * parameter hscl: Horizontal scaling factor.
 * parameter vscl: Vertical scaling factor.
 */
	static void set_window_compensation(uint32_t xoffset, uint32_t yoffset, uint32_t hscl, uint32_t vscl);
private:
	window(const window&);
	window& operator==(const window&);
};

/**
 * Get number of msec since some undetermined epoch.
 *
 * returns: The number of milliseconds.
 */
uint64_t get_ticks_msec() throw();

#endif
