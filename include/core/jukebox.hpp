#ifndef _jukebox__hpp__included__
#define _jukebox__hpp__included__

#include <functional>
#include <cstdlib>
#include <string>

namespace settingvar { class group; }

class save_jukebox_listener;

/**
 * Save jukebox.
 */
class save_jukebox
{
public:
/**
 * Ctor.
 */
	save_jukebox(settingvar::group& _settings, command::group& _cmd);
/**
 * Dtor.
 */
	~save_jukebox();
/**
 * Get current slot.
 *
 * Throws std::runtime_exception: No slot selected.
 */
	size_t get_slot();
/**
 * Set current slot.
 *
 * Parameter slot: The slot to select.
 * Throws std::runtime_exception: Slot out of range.
 */
	void set_slot(size_t slot);
/**
 * Cycle next slot.
 *
 * Throws std::runtime_exception: No slot selected.
 */
	void cycle_next();
/**
 * Cycle previous slot.
 *
 * Throws std::runtime_exception: No slot selected.
 */
	void cycle_prev();
/**
 * Get save as binary flag.
 */
	bool save_binary();
/**
 * Get name of current jukebox slot.
 *
 * Throws std::runtime_exception: No slot selected.
 */
	std::string get_slot_name();
/**
 * Set size of jukebox.
 *
 * Parameter size: The new size.
 */
	void set_size(size_t size);
/**
 * Set update function.
 */
	void set_update(std::function<void()> _update);
/**
 * Unset update function.
 */
	void unset_update();
private:
	void do_slotsel(const std::string& arg);
	settingvar::group& settings;
	size_t current_slot;
	size_t current_size;
	std::function<void()> update;
	save_jukebox_listener* listener;
	command::group& cmd;
	command::_fnptr<const std::string&> slotsel;
	command::_fnptr<> cycleprev;
	command::_fnptr<> cyclenext;
};

#endif
