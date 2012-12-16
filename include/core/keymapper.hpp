#ifndef _keymapper__hpp__included__
#define _keymapper__hpp__included__

#include <string>
#include <sstream>
#include <stdexcept>
#include <list>
#include <set>
#include <map>
#include <iostream>
#include "misc.hpp"
#include "library/keyboard.hpp"

/**
 * Takes in a raw command and returns the command that should be actually executed given the key polarity.
 *
 * parameter cmd: Raw command.
 * parameter polarity: Polarity (True => Being pressed, False => Being released).
 * returns: The fixed command, "" if no command should be executed.
 * throws std::bad_alloc: Not enough memory.
 */
std::string fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc);

/**
 * Our keyboard
 */
extern keyboard lsnes_kbd;

/**
 * This class handles internals of mapping events from keyboard buttons and pseudo-buttons.
 */
class keymapper
{
public:
/**
 * Binds a key, erroring out if binding would conflict with existing one.
 *
 * parameter mod: Modifier set to require to be pressed.
 * parameter modmask: Modifier set to take into account.
 * parameter keyname: Key to bind the action to.
 * parameter command: The command to bind.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The binding would conflict with existing one or invalid modifier/key.
 */
	static void bind(std::string mod, std::string modmask, std::string keyname, std::string command)
		throw(std::bad_alloc, std::runtime_error);
/**
 * Unbinds a key, erroring out if binding does not exist..
 *
 * parameter mod: Modifier set to require to be pressed.
 * parameter modmask: Modifier set to take into account.
 * parameter keyname: Key to bind the action to.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The binding does not exist.
 */
	static void unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Dump list of bindigns as message to console.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void dumpbindings() throw(std::bad_alloc);
/**
 * Get keys bound.
 */
	static std::set<std::string> get_bindings() throw(std::bad_alloc);
/**
 * Get command for key.
 */
	static std::string get_command_for(const std::string& keyspec) throw(std::bad_alloc);
/**
 * Bind command for key.
 */
	static void bind_for(const std::string& keyspec, const std::string& cmd) throw(std::bad_alloc,
		std::runtime_error);
};

class inverse_key
{
public:
/**
 * Create inverse key.
 *
 * Parameter command: Command this is for.
 * Parameter name: Name of inverse key.
 */
	inverse_key(const std::string& command, const std::string& name) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~inverse_key();
/**
 * Get set of inverse keys.
 *
 * Returns: The set of all inverses.
 */
	static std::set<inverse_key*> get_ikeys() throw(std::bad_alloc);
/**
 * Find by command.
 *
 * Parameter command: The command.
 * Returns: The instance.
 */
	static inverse_key* get_for(const std::string& command) throw(std::bad_alloc);
/**
 * Get keyspec.
 *
 * Parameter primary: If true, get the primary key, else secondary key.
 * Returns: The keyspec.
 */
	std::string get(bool primary) throw(std::bad_alloc);
/**
 * Clear key (if primary is cleared, secondary becomes primary).
 *
 * Parameter primary: If true, clear the primary, else the secondary.
 */
	void clear(bool primary) throw(std::bad_alloc);
/**
 * Set key.
 *
 * Parameter keyspec: The new keyspec.
 * Parameter primary: If true, set the primary, else the secondary.
 */
	void set(std::string keyspec, bool primary) throw(std::bad_alloc);
/**
 * Notify updated mapping.
 */
	static void notify_update(const std::string& keyspec, const std::string& command);
/**
 * Get name for command.
 *
 * Returns: The name.
 */
	std::string getname() throw(std::bad_alloc);
private:
	inverse_key(const inverse_key&);
	inverse_key& operator=(const inverse_key&);
	static std::set<inverse_key*>& ikeys();
	static std::map<std::string, inverse_key*>& forkey();
	void addkey(const std::string& keyspec);
	std::string cmd;
	std::string oname;
	std::string primary_spec;
	std::string secondary_spec;
};

/**
 * Translate axis calibration into mode name.
 */
std::string calibration_to_mode(keyboard_axis_calibration p);

#endif
