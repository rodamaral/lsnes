#ifndef _window__hpp__included__
#define _window__hpp__included__

#include "SDL.h"
#include "render.hpp"
#include <string>
#include <map>
#include <list>
#include <stdexcept>

#define WINSTATE_NORMAL 0
#define WINSTATE_COMMAND 1
#define WINSTATE_MODAL 2
#define WINSTATE_IDENTIFY 3

class window_internal;
class window;

/**
 * \brief Handle to the graphics system.
 * 
 * This is a handle to graphics system. Note that creating multiple contexts produces undefined results.
 */
class window
{
public:
	window();
	~window();

/**
 * \brief Add messages to message queue.
 * 
 * Adds a messages to mesage queue to be shown.
 * 
 * \param msg The messages to add (split by '\n').
 * \throws std::bad_alloc Not enough memory.
 */
	void message(const std::string& msg) throw(std::bad_alloc);

/**
 * \brief Get output stream printing into message queue.
 * 
 * \return The output stream.
 * \throws std::bad_alloc Not enough memory.
 */
	std::ostream& out() throw(std::bad_alloc);

/**
 * \brief Display a modal message.
 * 
 * Displays a modal message, not returning until the message is acknowledged. Keybindings are not available, but
 * should quit be generated somehow, modal message will be closed and command callback triggered.
 * 
 * \param msg The message to show.
 * \param confirm If true, ask for Ok/cancel type input.
 * \return If confirm is true, true if ok was chosen, false if cancel was chosen. Otherwise always false.
 * \throws std::bad_alloc Not enough memory.
 */
	bool modal_message(const std::string& msg, bool confirm = false) throw(std::bad_alloc);

/**
 * \brief Signal that the emulator state is too screwed up to continue.
 * 
 * Displays fatal error message, quitting after the user acks it.
 */
	void fatal_error() throw();

/**
 * \brief Bind a key.
 * 
 * \param mod Set of modifiers.
 * \param modmask Modifier mask (set of modifiers).
 * \param keyname Name of key or pseudo-key.
 * \param command Command to run.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Invalid key or modifier name, or conflict.
 */
	void bind(std::string mod, std::string modmask, std::string keyname, std::string command)
		throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Unbind a key.
 * 
 * \param mod Set of modifiers.
 * \param modmask Modifier mask (set of modifiers).
 * \param keyname Name of key or pseudo-key.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Invalid key or modifier name, or not bound.
 */
	void unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
		std::runtime_error);

/**
 * \brief Dump bindings into this window.
 * 
 * \throws std::bad_alloc Not enough memory.
 */
	//Dump bindings.
	void dumpbindings() throw(std::bad_alloc);

/**
 * \brief Process inputs, calling command handler if needed.
 * 
 * Processes inputs. If in non-modal mode (normal mode without pause), this returns quickly. Otherwise it waits
 * for modal mode to exit.
 * 
 * \throws std::bad_alloc Not enough memory.
 */
	void poll_inputs() throw(std::bad_alloc);

/**
 * \brief Get emulator status area
 * 
 * \return Emulator status area.
 */
	std::map<std::string, std::string>& get_emustatus() throw();

/**
 * \brief Notify that the screen has been updated.
 * 
 * \param full Do full refresh.
 */
	void notify_screen_update(bool full = false) throw();

/**
 * \brief Set the screen to use as main surface.
 * 
 * \param scr The screen to use.
 */
	void set_main_surface(screen& scr) throw();

/**
 * \brief Enable/Disable pause mode.
 * 
 * \param enable Enable pause if true, disable otherwise.
 */
	void paused(bool enable) throw();

/**
 * \brief Wait specified number of milliseconds (polling for input).
 * 
 * \param msec Number of ms to wait.
 * \throws std::bad_alloc Not enough memory.
 */
	void wait_msec(uint64_t msec) throw(std::bad_alloc);

/**
 * \brief Enable or disable sound.
 * 
 * \param enable Enable sounds if true, otherwise disable sounds.
 */
	void sound_enable(bool enable) throw();

/**
 * \brief Input audio sample (at 32040.5Hz).
 * 
 * \param left Left sample.
 * \param right Right sample.
 */
	void play_audio_sample(uint16_t left, uint16_t right) throw();

/**
 * \brief Cancel pending wait, making it return now.
 */
	void cancel_wait() throw();

/**
 * \brief Set window compensation parameters.
 */
	void set_window_compensation(uint32_t xoffset, uint32_t yoffset, uint32_t hscl, uint32_t vscl);
private:
	window_internal* i;
	window(const window&);
	window& operator==(const window&);
};

/**
 * \brief Get number of msec since some undetermined epoch.
 * 
 * \return The number of milliseconds.
 */
uint64_t get_ticks_msec() throw();


#endif
