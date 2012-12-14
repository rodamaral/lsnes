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
 * Key or key group.
 *
 * Each object of this type is either one key or group of keys.
 */
class keygroup
{
public:
/**
 * Key group type.
 */
	enum type
	{
/**
 * Disabled.
 */
		KT_DISABLED,
/**
 * Singular button.
 */
		KT_KEY,
/**
 * Pressure-sensitive button
 */
		KT_PRESSURE_PM,
		KT_PRESSURE_MP,
		KT_PRESSURE_0P,
		KT_PRESSURE_0M,
		KT_PRESSURE_P0,
		KT_PRESSURE_M0,
/**
 * Axis key pair.
 */
		KT_AXIS_PAIR,
		KT_AXIS_PAIR_INVERSE,
/**
 * Hat.
 */
		KT_HAT,
/**
 * Mouse axis (this is not a real axis!).
 */
		KT_MOUSE
	};
/**
 * Create a new key group.
 *
 * parameter name: Name of the key group.
 * parameter _clazz: The key class.
 * parameter t: Initial type of the key group.
 * throws std::bad_alloc: Not enough memory.
 */
	keygroup(const std::string& name, const std::string& _clazz, enum type t) throw(std::bad_alloc);
/**
 * Destructor
 */
	~keygroup() throw();
/**
 * Lookup key group by name.
 *
 * Parameter name: The key group name.
 * Returns: The looked up key group, or NULL if not found.
 */
	static keygroup* lookup_by_name(const std::string& name) throw();
/**
 * Get the set of axes.
 *
 * Returns: The axis set (all axes).
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::set<std::string> get_axis_set() throw(std::bad_alloc);
/**
 * Change type of key group.
 *
 * parameter t: New type for the key group.
 */
	void change_type(enum type t) throw();
/**
 * Change calibration (Axis pairs and pressure buttons only).
 *
 * parameter left: The control value at extreme negative position.
 * parameter center: The control value at center position.
 * parameter right: The control value at extreme positive position.
 * parameter tolerance: How wide is the neutral zone (must be larger than 0 and smaller than 1).
 */
	void change_calibration(short left, short center, short right, double tolerance);
/**
 * Change state of this key group.
 *
 * For KT_KEY, value is zero/nonzero.
 * For KT_PRESSURE_* and KT_AXIS_PAIR*, value is -32768...32767.
 * For KT_HAT, 1 is up, 2 is right, 4 is down, 8 is left (may be ORed).
 * For KT_MOUSE, value is -32768...32767.
 *
 * parameter pos: New position.
 * parameter modifiers: The modifier set that was pressed during the change.
 */
	void set_position(short pos, keyboard_modifier_set& modifiers) throw();
/**
 * Look up individual key by name.
 *
 * parameter name: The name of the key to look up.
 * returns: First element is pointer to key group, second is key index within the group.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: No such key known.
 */
	static std::pair<keygroup*, unsigned> lookup(const std::string& name) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Look up key group name.
 *
 * returns: The name of the key group.
 * throws std::bad_alloc: Not enough memory.
 */
	std::string name() throw(std::bad_alloc);
/**
 * Get set of all keys (including subkeys).
 */
	static std::set<std::string> get_keys() throw(std::bad_alloc);

/**
 * Key group parameters.
 */
	struct parameters
	{
/**
 * Type
 */
		enum type ktype;
/**
 * Last known raw value.
 */
		short last_rawval;
/**
 * Calibration left.
 */
		short cal_left;
/**
 * Calibration center.
 */
		short cal_center;
/**
 * Calibration right.
 */
		short cal_right;
/**
 * Calibration tolerance.
 */
		double cal_tolerance;
	};
/**
 * Get parameters.
 */
	struct parameters get_parameters();
/**
 * Get all key parameters.
 */
	static std::map<std::string, struct parameters> get_all_parameters();
/**
 * Set callback requests on/off
 */
	void request_hook_callback(bool state);
/**
 * Get status value.
 */
	signed get_value();
/**
 * Get class.
 */
	const std::string& get_class();
private:
	signed state;
	enum type ktype;
	short last_rawval;
	short cal_left;
	short cal_center;
	short cal_right;
	double cal_tolerance;
	double compensate(short value);
	double compensate2(double value);
	void run_listeners(keyboard_modifier_set& modifiers, unsigned subkey, bool polarity, bool really, double x);
	std::string keyname;
	std::string clazz;
	bool requests_hook;
};

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

#endif
