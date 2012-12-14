#ifndef _library__keyboard__hpp__included__
#define _library__keyboard__hpp__included__

#include "register-queue.hpp"
#include "threadtypes.hpp"
#include <map>
#include <string>

class keyboard_modifier;

/**
 * A group of modifiers and keys
 */
class keyboard
{
public:
/**
 * Create a new group of keys.
 */
	keyboard() throw(std::bad_alloc);
/**
 * Destructor.
 */
	~keyboard() throw();
/**
 * Registration proxy for modifiers.
 */
	struct _modifier_proxy
	{
		_modifier_proxy(keyboard& kbd) : _kbd(kbd) {}
		void do_register(const std::string& name, keyboard_modifier& mod)
		{
			_kbd.do_register_modifier(name, mod);
		}
		void do_unregister(const std::string& name)
		{
			_kbd.do_unregister_modifier(name);
		}
	private:
		keyboard& _kbd;
	} modifier_proxy;
/**
 * Lookup modifier by name.
 *
 * Parameter name: The name of the modifier.
 * Returns: The modifier.
 * Throws std::runtime_error: No such modifier.
 */
	keyboard_modifier& lookup_modifier(const std::string& name) throw(std::runtime_error);
/**
 * Try lookup modifier by name.
 *
 * Parameter name: The name of the modifier.
 * Returns: The modifier, or NULL if not found.
 */
	keyboard_modifier* try_lookup_modifier(const std::string& name) throw();
/**
 * Look up all modifiers.
 *
 * Returns: The set of modifiers.
 */
	std::list<keyboard_modifier*> all_modifiers() throw(std::bad_alloc);
/**
 * Register a modifier.
 *
 * Parameter name: The name of the modifier.
 * Parameter mod: The modifier.
 */
	void do_register_modifier(const std::string& name, keyboard_modifier& mod) throw(std::bad_alloc);
/**
 * Unregister a modifier.
 *
 * Parameter name: The name of the modifier.
 */
	void do_unregister_modifier(const std::string& name) throw();
private:
	keyboard(const keyboard&);
	keyboard& operator=(const keyboard&);
	std::map<std::string, keyboard_modifier*> modifiers;
	mutex_class mutex;
};

/**
 * A modifier or group of modifiers.
 */
class keyboard_modifier
{
public:
/**
 * Create a (group of) modifiers.
 *
 * Parameter keyb: The keyboard these will be on.
 * Parameter _name: The name of the modifier.
 */
	keyboard_modifier(keyboard& keyb, const std::string& _name) throw(std::bad_alloc)
		: kbd(keyb), name(_name)
	{
		register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_register(kbd.modifier_proxy, name,
			*this);
	}
/**
 * Create a linked modifier in group.
 *
 * Parameter keyb: The keyboard these will be on.
 * Parameter _name: The name of the modifier.
 * Parameter _link: The name of the modifier group this is in.
 */
	keyboard_modifier(keyboard& keyb, const std::string& _name, const std::string& _link) throw(std::bad_alloc)
		: kbd(keyb), name(_name), link(_link)
	{
		register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_register(kbd.modifier_proxy, name,
			*this);
	}
/**
 * Destructor.
 */
	~keyboard_modifier() throw()
	{
		register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_unregister(kbd.modifier_proxy, name);
	}
/**
 * Get associated keyboard.
 */
	keyboard& get_keyboard() const throw() { return kbd; }
/**
 * Get name of the modifier.
 */
	const std::string& get_name() const throw() { return name; }
/**
 * Get linked name of the modifier.
 *
 * Returns: The linked name, or "" if none.
 */
	const std::string& get_link_name() const throw() { return link; }
/**
 * Get the linked modifier.
 *
 * Returns: The linked modifier, or NULL if none (or not initialized yet).
 */
	keyboard_modifier* get_link() { return kbd.try_lookup_modifier(link); }
private:
	keyboard& kbd;
	std::string name;
	std::string link;
};

/**
 * A set of modifier keys.
 */
class keyboard_modifier_set
{
public:
/**
 * Add a modifier into the set.
 *
 * parameter mod: The modifier to add.
 * parameter really: If true, actually add the key. If false, do nothing.
 * throws std::bad_alloc: Not enough memory.
 */
	void add(keyboard_modifier& mod, bool really = true) throw(std::bad_alloc);
/**
 * Remove a modifier from the set.
 *
 * parameter mod: The modifier to remove.
 * parameter really: If true, actually remove the key. If false, do nothing.
 * throws std::bad_alloc: Not enough memory.
 */
	void remove(keyboard_modifier& mod, bool really = true) throw(std::bad_alloc);
/**
 * Construct modifier set from comma-separated string.
 *
 * parameter kbd: The keyboard to take the modifiers from.
 * parameter modifiers: The modifiers as string
 * returns: The constructed modifier set.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Illegal modifier or wrong syntax.
 */
	static keyboard_modifier_set construct(keyboard& kbd, const std::string& modifiers) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Check modifier against its mask for validity.
 *
 * This method checks that:
 * - for each modifier in set, either that or its linkage group is in mask.
 * - Both modifier and its linkage group isn't in either set or mask.
 *
 * parameter mask: The mask to check against.
 * returns: True if set is valid, false if not.
 * throws std::bad_alloc: Not enough memory.
 */
	bool valid(keyboard_modifier_set& mask) throw(std::bad_alloc);
/**
 * Check if this modifier set triggers the action.
 *
 * Modifier set triggers another if for each modifier or linkage group in mask:
 * - Modifier appears in both set and trigger.
 * - At least one modifier with this linkage group appears in both set and trigger.
 * - Modifiers with this linkage group do not appear in either set nor trigger.
 *
 */
	bool triggers(keyboard_modifier_set& trigger, keyboard_modifier_set& mask) throw(std::bad_alloc);
/**
 * Equality check.
 *
 * parameter m: Another set.
 * returns: True if two sets are equal, false if not.
 */
	bool operator==(const keyboard_modifier_set& m) const throw();

private:
	friend std::ostream& operator<<(std::ostream& os, const keyboard_modifier_set& m);
	std::set<keyboard_modifier*> set;
};

/**
 * Debugging print. Prints textual version of set into stream.
 *
 * parameter os: The stream to print to.
 * parameter m: The modifier set to print.
 * returns: reference to os.
 */
std::ostream&  operator<<(std::ostream& os, const keyboard_modifier_set& m);

#endif
