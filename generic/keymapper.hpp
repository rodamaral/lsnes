#ifndef _keymapper__hpp__included__
#define _keymapper__hpp__included__

#include <string>
#include <sstream>
#include <stdexcept>
#include <list>
#include <set>
#include <iostream>
#include "misc.hpp"

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
 * Modifier.
 */
class modifier
{
public:
/**
 * Create modifier.
 */
	modifier(const std::string& name) throw(std::bad_alloc);
/**
 * Create linked modifier.
 */
	modifier(const std::string& name, const std::string& linkgroup) throw(std::bad_alloc);
/**
 * Look up a modifier.
 */
	static modifier& lookup(const std::string& name) throw(std::bad_alloc, std::runtime_error);
/**
 * Get name of modifier.
 */
	std::string name() const throw(std::bad_alloc);
private:
	
	modifier(const modifier&);
	modifier& operator=(const modifier&);
	std::string modname;
};

/**
 * Set of modifiers.
 */
class modifier_set
{
public:
/**
 * Add a modifier.
 */
	void add(const modifier& mod, bool really = true) throw(std::bad_alloc);
/**
 * Remove a modifier.
 */
	void remove(const modifier& mod, bool really = true) throw(std::bad_alloc);
/**
 * Construct set from string.
 */
	static modifier_set construct(const std::string& modifiers) throw(std::bad_alloc, std::runtime_error);
/**
 * Check modifier against its mask for validity.
 */
	static bool valid(const modifier_set& set, const modifier_set& mask) throw(std::bad_alloc);
/**
 * Check if this modifier set triggers the action.
 */
	static bool triggers(const modifier_set& set, const modifier_set& trigger, const modifier_set& mask)
		throw(std::bad_alloc);
/**
 * Equality.
 */
	bool operator==(const modifier_set& m) const throw();
/**
 * Debugging print
 */
	friend std::ostream& operator<<(std::ostream& os, const modifier_set& m);
private:
	std::set<const modifier*> set;
};

std::ostream&  operator<<(std::ostream& os, const modifier_set& m);

/**
 * Key or key group.
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
		KT_HAT
	};
/**
 * Create new key group.
 */
	keygroup(const std::string& name, enum type t) throw(std::bad_alloc);
/**
 * Change type of key group.
 */
	void change_type(enum type t);
/**
 * Change calibration (Axis pairs and pressure buttons only).
 */
	void change_calibration(short left, short center, short right, double tolerance);
/**
 * Change state of this key group.
 * 
 * For KT_KEY, value is zero/nonzero.
 * For KT_PRESSURE_* and KT_AXIS_PAIR*, value is -32768...32767.
 * For KT_HAT, 1 is up, 2 is right, 4 is down, 8 is left (may be ORed).
 */
	void set_position(short pos, const modifier_set& modifiers) throw();
/**
 * Look up key by name.
 */
	static std::pair<keygroup*, unsigned> lookup(const std::string& name) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Look up key name.
 */
	std::string name() throw(std::bad_alloc);
/**
 * Keyboard key listener.
 */
	struct key_listener
	{
/**
 * Invoked on key.
 */
		virtual void key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned subkey,
			bool polarity, const std::string& name) = 0;
	};
/**
 * Add key listener.
 */
	void add_key_listener(key_listener& l) throw(std::bad_alloc);
/**
 * Remove key listener.
 */
	void remove_key_listener(key_listener& l) throw(std::bad_alloc);
/**
 * Excelusive key listener.
 */
	static void set_exclusive_key_listener(key_listener* l) throw();
private:
	unsigned state;
	enum type ktype;
	short cal_left;
	short cal_center;
	short cal_right;
	double cal_tolerance;
	double compensate(short value);
	double compensate2(double value);
	void run_listeners(const modifier_set& modifiers, unsigned subkey, bool polarity, bool really, double x);
	std::list<key_listener*> listeners;
	std::string keyname;
	static key_listener* exclusive;
};

/**
 * This class handles internals of mapping events from keyboard buttons and pseudo-buttons.
 *
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
};

#endif

