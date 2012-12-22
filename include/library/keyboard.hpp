#ifndef _library__keyboard__hpp__included__
#define _library__keyboard__hpp__included__

#include "register-queue.hpp"
#include "threadtypes.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>

class keyboard_modifier;
class keyboard_key;
class keyboard_key_axis;
class keyboard_key_mouse;
class keyboard_event_listener;

/**
 * A group of modifiers and keys.
 *
 * Instances of this class allow registering, unregistering and looking up modifiers and keys.
 */
class keyboard
{
public:
/**
 * Create a new instance.
 */
	keyboard() throw(std::bad_alloc);
/**
 * Destroy an instance.
 *
 * The keys and modifiers in instance are not freed.
 */
	~keyboard() throw();
/**
 * Registration proxy for modifiers.
 *
 * This proxy is intended to be used together with register_queue.
 *
 * It is safe to use this proxy even in global ctor context.
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
 * Registration proxy for keys.
 *
 * This proxy is intended to be used together with register_queue.
 *
 * It is safe to use this proxy even in global ctor context.
 */
	struct _key_proxy
	{
		_key_proxy(keyboard& kbd) : _kbd(kbd) {}
		void do_register(const std::string& name, keyboard_key& key)
		{
			_kbd.do_register_key(name, key);
		}
		void do_unregister(const std::string& name)
		{
			_kbd.do_unregister_key(name);
		}
	private:
		keyboard& _kbd;
	} key_proxy;
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
/**
 * Lookup key by name.
 *
 * Parameter name: The name of the key.
 * Returns: The key.
 * Throws std::runtime_error: No such key.
 */
	keyboard_key& lookup_key(const std::string& name) throw(std::runtime_error);
/**
 * Try lookup key by name.
 *
 * Parameter name: The name of the key.
 * Returns: The key, or NULL if not found.
 */
	keyboard_key* try_lookup_key(const std::string& name) throw();
/**
 * Look up all keys.
 *
 * Returns: The set of keys.
 */
	std::list<keyboard_key*> all_keys() throw(std::bad_alloc);
/**
 * Register a key.
 *
 * Parameter name: The name of the key.
 * Parameter mod: The key.
 */
	void do_register_key(const std::string& name, keyboard_key& mod) throw(std::bad_alloc);
/**
 * Unregister a key.
 *
 * Parameter name: The name of the key.
 */
	void do_unregister_key(const std::string& name) throw();
/**
 * Set exclusive listener for all keys at once.
 */
	void set_exclusive(keyboard_event_listener* listener) throw();
private:
	keyboard(const keyboard&);
	keyboard& operator=(const keyboard&);
	std::map<std::string, keyboard_modifier*> modifiers;
	std::map<std::string, keyboard_key*> keys;
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
	bool triggers(const keyboard_modifier_set& trigger, const keyboard_modifier_set& mask) throw(std::bad_alloc);
/**
 * Stringify.
 */
	operator std::string() const throw(std::bad_alloc);
/**
 * Equality check.
 *
 * parameter m: Another set.
 * returns: True if two sets are equal, false if not.
 */
	bool operator==(const keyboard_modifier_set& m) const throw();
/**
 * Less than check.
 */
	bool operator<(const keyboard_modifier_set& m) const throw();
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

/**
 * Type of key.
 */
enum keyboard_keytype
{
/**
 * A simple key (pressed/released)
 */
	KBD_KEYTYPE_KEY,
/**
 * A joystick axis (pair of opposite directions or pressure-sensitive button).
 */
	KBD_KEYTYPE_AXIS,
/**
 * A joystick hat (directional control or a dpad).
 */
	KBD_KEYTYPE_HAT,
/**
 * A mouse axis.
 */
	KBD_KEYTYPE_MOUSE
};

/**
 * Joystick axis calibration structure.
 */
struct keyboard_axis_calibration
{
/**
 * Mode: -1 => Disabled, 0 => Pressure-sentive button, 1 => Axis.
 */
	int mode;
/**
 * Endpoint sign A (left or released).
 */
	int esign_a;
/**
 * Endpoint sign B (right or pressed).
 */
	int esign_b;
/**
 * The left limit.
 */
	int32_t left;
/**
 * The center position.
 */
	int32_t center;
/**
 * The right limit.
 */
	int32_t right;
/**
 * Relative width of null zone (greater than 0, but less than 1).
 */
	double nullwidth;
/**
 * Translate from raw value to internal value (-32767...32767).
 */
	int32_t get_calibrated_value(int32_t x) const throw();
/**
 * Translate from internal value to digital state (-1, 0, 1).
 */
	int get_digital_state(int32_t x) const throw();
};

/**
 * Mouse axis calibration structure.
 */
struct keyboard_mouse_calibration
{
/**
 * The offset from left of screen area to left of game area.
 */
	int32_t offset;
/**
 * Translate from screen coordinate to game coordinate.
 */
	int32_t get_calibrated_value(int32_t x) const throw();
};

/**
 * Superclass of key event data.
 */
class keyboard_event
{
public:
/**
 * Create a new event.
 *
 * Parameter _chngmask: The change mask.
 * Parameter _type: Type of the event.
 */
	keyboard_event(uint32_t _chngmask, keyboard_keytype _type) throw()
	{
		chngmask = _chngmask;
		type = _type;
	}
/**
 * Destructor.
 */
	virtual ~keyboard_event() throw();
/**
 * Get analog state. The format is dependent on key type.
 */
	virtual int32_t get_state() const throw() = 0;
/**
 * Get key change mask.
 *
 * Returns: A bitmask. Bit 2*n is the state of subkey n. Bit 2*n+1 is set if subkey changed state, else clear.
 */
	uint32_t get_change_mask() const throw() { return chngmask; }
/**
 * Get type of event.
 */
	keyboard_keytype get_type() const throw() { return type; }
private:
	uint32_t chngmask;
	keyboard_keytype type;
};

/**
 * A simple key event.
 */
class keyboard_event_key : public keyboard_event
{
public:
/**
 * Construct a new key event.
 *
 * Parameter chngmask: The change mask.
 */
	keyboard_event_key(uint32_t chngmask);
/**
 * Destructor.
 */
	~keyboard_event_key() throw();
/**
 * Get analog state.
 *
 * Returns: 1 if pressed, 0 if released.
 */
	int32_t get_state() const throw();
private:
	int32_t state;
};

/**
 * An axis event.
 */
class keyboard_event_axis : public keyboard_event
{
public:
/**
 * Construct a new axis event.
 *
 * Parameter state: The analog state.
 * Parameter chngmask: The change mask.
 * Parameter cal: The calibration structure.
 */
	keyboard_event_axis(int32_t state, uint32_t chngmask, const keyboard_axis_calibration& cal);
/**
 * Destructor.
 */
	~keyboard_event_axis() throw();
/**
 * Get analog state.
 *
 * Returns: Analog position of axis, -32767...32767 (0...32767 for pressure-sensitive buttons).
 */
	int32_t get_state() const throw();
/**
 * Get calibration data.
 */
	keyboard_axis_calibration get_calibration() { return cal; }
private:
	int32_t state;
	keyboard_axis_calibration cal;
};

/**
 * A hat event.
 */
class keyboard_event_hat : public keyboard_event
{
public:
/**
 * Construct a new hat event.
 *
 * Parameter chngmask: The change mask to use.
 */
	keyboard_event_hat(uint32_t chngmask);
/**
 * Destructor.
 */
	~keyboard_event_hat() throw();
/**
 * Get analog state.
 *
 * Returns: Bitmask: 1 => Up, 2 => Right, 4 => Down, 8 => Left.
 */
	int32_t get_state() const throw();
};

/**
 * A mouse event.
 */
class keyboard_event_mouse : public keyboard_event
{
public:
/**
 * Construct a new mouse event.
 *
 * Parameter state: The game-relative position to use.
 * Parameter cal: The calibration structure.
 */
	keyboard_event_mouse(int32_t state, const keyboard_mouse_calibration& cal);
/**
 * Destructor.
 */
	~keyboard_event_mouse() throw();
/**
 * Get analog state.
 *
 * Returns: Position of mouse relative to game area (with right/down positive).
 */
	int32_t get_state() const throw();
/**
 * Get calibration data.
 */
	keyboard_mouse_calibration get_calibration() { return cal; }
private:
	int32_t state;
	keyboard_mouse_calibration cal;
};

/**
 * A keyboard event listener.
 */
class keyboard_event_listener
{
public:
/**
 * Destructor.
 */
	virtual ~keyboard_event_listener() throw();
/**
 * Receive a key event.
 *
 * Parameter mods: Modifiers currently active.
 * Parameter key: The key this event is about.
 * Parameter event: The event.
 */
	virtual void on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event) = 0;
};

/**
 * A (compound) key on keyboard.
 */
class keyboard_key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 * Parameter type: The type of key.
 */
	keyboard_key(keyboard& keyb, const std::string& name, const std::string& clazz, keyboard_keytype type)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~keyboard_key() throw();
/**
 * Get class.
 */
	const std::string& get_class() { return clazz; }
/**
 * Get name.
 */
	const std::string& get_name() { return name; }
/**
 * Get keyboard this is on.
 */
	keyboard& get_keyboard() { return kbd; }
/**
 * Get key type.
 */
	keyboard_keytype get_type() const throw() { return type; }
/**
 * Add listener.
 *
 * Parameter listener: The listener.
 * Parameter analog: If true, also pass analog events.
 */
	void add_listener(keyboard_event_listener& listener, bool analog) throw(std::bad_alloc);
/**
 * Remove listener.
 *
 * Parameter listener: The listener.
 */
	void remove_listener(keyboard_event_listener& listener) throw();
/**
 * Set exclusive listener.
 *
 * Parameter listener: The listener. NULL to ungrab key.
 */
	void set_exclusive(keyboard_event_listener* listener) throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. The format is dependent on key type.
 */
	virtual void set_state(keyboard_modifier_set mods, int32_t state) throw() = 0;
/**
 * Get analog state. The format is dependent on key type.
 */
	virtual int32_t get_state() const throw() = 0;
/**
 * Get digital state. The format is dependent on key type.
 */
	virtual int32_t get_state_digital() const throw() = 0;
/**
 * Get the subkey suffixes.
 */
	virtual std::vector<std::string> get_subkeys() throw(std::bad_alloc) = 0;
/**
 * Dynamic cast to axis type.
 */
	keyboard_key_axis* cast_axis() throw();
/**
 * Dynamic cast to mouse type.
 */
	keyboard_key_mouse* cast_mouse() throw();
protected:
/**
 * Call all event listeners on this key.
 *
 * Parameter mods: The current modifiers.
 * Parameter event: The event to pass.
 */
	void call_listeners(keyboard_modifier_set& mods, keyboard_event& event);
/**
 * Mutex protecting state.
 */
	mutable mutex_class mutex;
private:
	keyboard_key(keyboard_key&);
	keyboard_key& operator=(keyboard_key&);
	keyboard& kbd;
	std::string clazz;
	std::string name;
	std::set<keyboard_event_listener*> digital_listeners;
	std::set<keyboard_event_listener*> analog_listeners;
	keyboard_event_listener* exclusive_listener;
	keyboard_keytype type;
};

/**
 * A simple key on keyboard.
 */
class keyboard_key_key : public keyboard_key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 */
	keyboard_key_key(keyboard& keyb, const std::string& name, const std::string& clazz) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~keyboard_key_key() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. 1 for pressed, 0 for released.
 */
	void set_state(keyboard_modifier_set mods, int32_t state) throw();
/**
 * Get analog state. 1 for pressed, 0 for released.
 */
	int32_t get_state() const throw();
/**
 * Get digital state. 1 for pressed, 0 for released.
 */
	int32_t get_state_digital() const throw();
/**
 * Get the subkey suffixes.
 */
	std::vector<std::string> get_subkeys() throw(std::bad_alloc);
private:
	keyboard_key_key(keyboard_key_key&);
	keyboard_key_key& operator=(keyboard_key_key&);
	int32_t state;
};

/**
 * A hat on keyboard.
 */
class keyboard_key_hat : public keyboard_key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 */
	keyboard_key_hat(keyboard& keyb, const std::string& name, const std::string& clazz) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~keyboard_key_hat() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. 1 => up, 2 => right, 4 => down, 8 => left.
 */
	void set_state(keyboard_modifier_set mods, int32_t state) throw();
/**
 * Get analog state. 1 => up, 2 => right, 4 => down, 8 => left.
 */
	int32_t get_state() const throw();
/**
 * Get digital state. 1 => up, 2 => right, 4 => down, 8 => left.
 */
	int32_t get_state_digital() const throw();
/**
 * Get the subkey suffixes.
 */
	std::vector<std::string> get_subkeys() throw(std::bad_alloc);
private:
	keyboard_key_hat(keyboard_key_hat&);
	keyboard_key_hat& operator=(keyboard_key_hat&);
	int32_t state;
};

/**
 * An axis on keyboard.
 */
class keyboard_key_axis : public keyboard_key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 * Parameter cal: Initial calibration.
 */
	keyboard_key_axis(keyboard& keyb, const std::string& name, const std::string& clazz,
		keyboard_axis_calibration cal) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~keyboard_key_axis() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. Uncalibrated analog position.
 */
	void set_state(keyboard_modifier_set mods, int32_t state) throw();
/**
 * Get analog state. -32767...32767 for axes, 0...32767 for pressure-sensitive buttons.
 */
	int32_t get_state() const throw();
/**
 * Get digital state. -1 => left, 0 => center/unpressed, 1 => Right/pressed.
 */
	int32_t get_state_digital() const throw();
/**
 * Get the subkey suffixes.
 */
	std::vector<std::string> get_subkeys() throw(std::bad_alloc);
/**
 * Get calibration.
 */
	keyboard_axis_calibration get_calibration() const throw();
/**
 * Set calibration.
 */
	void set_calibration(keyboard_axis_calibration cal) throw();
private:
	keyboard_key_axis(keyboard_key_axis&);
	keyboard_key_axis& operator=(keyboard_key_axis&);
	int32_t rawstate;
	keyboard_axis_calibration cal;
};

/**
 * A mouse axis on keyboard.
 */
class keyboard_key_mouse : public keyboard_key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 * Parameter cal: Initial calibration.
 */
	keyboard_key_mouse(keyboard& keyb, const std::string& name, const std::string& clazz,
		keyboard_mouse_calibration cal) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~keyboard_key_mouse() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. Screen-relative analog position.
 */
	void set_state(keyboard_modifier_set mods, int32_t state) throw();
/**
 * Get analog state. Game-relative analog position.
 */
	int32_t get_state() const throw();
/**
 * Get digital state. Always returns 0.
 */
	int32_t get_state_digital() const throw();
/**
 * Get the subkey suffixes. Returns empty list.
 */
	std::vector<std::string> get_subkeys() throw(std::bad_alloc);
/**
 * Get calibration.
 */
	keyboard_mouse_calibration get_calibration() const throw();
/**
 * Set calibration.
 */
	void set_calibration(keyboard_mouse_calibration cal) throw();
private:
	keyboard_key_mouse(keyboard_key_mouse&);
	keyboard_key_mouse& operator=(keyboard_key_mouse&);
	int32_t rawstate;
	keyboard_mouse_calibration cal;
};

#endif
