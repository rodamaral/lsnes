#ifndef _library__keyboard__hpp__included__
#define _library__keyboard__hpp__included__

#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>
#include <stdexcept>
#include "text.hpp"

namespace keyboard
{
class modifier;
class key;
class key_axis;
class key_mouse;
class event_listener;

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
 * Lookup modifier by name.
 *
 * Parameter name: The name of the modifier.
 * Returns: The modifier.
 * Throws std::runtime_error: No such modifier.
 */
	modifier& lookup_modifier(const text& name) throw(std::runtime_error);
/**
 * Try lookup modifier by name.
 *
 * Parameter name: The name of the modifier.
 * Returns: The modifier, or NULL if not found.
 */
	modifier* try_lookup_modifier(const text& name) throw();
/**
 * Look up all modifiers.
 *
 * Returns: The set of modifiers.
 */
	std::list<modifier*> all_modifiers() throw(std::bad_alloc);
/**
 * Register a modifier.
 *
 * Parameter name: The name of the modifier.
 * Parameter mod: The modifier.
 */
	void do_register(const text& name, modifier& mod) throw(std::bad_alloc);
/**
 * Unregister a modifier.
 *
 * Parameter name: The name of the modifier.
 */
	void do_unregister(const text& name, modifier& mod) throw();
/**
 * Lookup key by name.
 *
 * Parameter name: The name of the key.
 * Returns: The key.
 * Throws std::runtime_error: No such key.
 */
	key& lookup_key(const text& name) throw(std::runtime_error);
/**
 * Try lookup key by name.
 *
 * Parameter name: The name of the key.
 * Returns: The key, or NULL if not found.
 */
	key* try_lookup_key(const text& name) throw();
/**
 * Look up all keys.
 *
 * Returns: The set of keys.
 */
	std::list<key*> all_keys() throw(std::bad_alloc);
/**
 * Register a key.
 *
 * Parameter name: The name of the key.
 * Parameter mod: The key.
 */
	void do_register(const text& name, key& mod) throw(std::bad_alloc);
/**
 * Unregister a key.
 *
 * Parameter name: The name of the key.
 */
	void do_unregister(const text& name, key& mod) throw();
/**
 * Set exclusive listener for all keys at once.
 */
	void set_exclusive(event_listener* listener) throw();
/**
 * Set current key.
 */
	void set_current_key(key* key) throw();
/**
 * Get current key.
 */
	key* get_current_key() throw();
private:
	keyboard(const keyboard&);
	keyboard& operator=(const keyboard&);
	key* current_key;
};

/**
 * A modifier or group of modifiers.
 */
class modifier
{
public:
/**
 * Create a (group of) modifiers.
 *
 * Parameter keyb: The keyboard these will be on.
 * Parameter _name: The name of the modifier.
 */
	modifier(keyboard& keyb, const text& _name) throw(std::bad_alloc)
		: kbd(keyb), name(_name)
	{
		keyb.do_register(name, *this);
	}
/**
 * Create a linked modifier in group.
 *
 * Parameter keyb: The keyboard these will be on.
 * Parameter _name: The name of the modifier.
 * Parameter _link: The name of the modifier group this is in.
 */
	modifier(keyboard& keyb, const text& _name, const text& _link) throw(std::bad_alloc)
		: kbd(keyb), name(_name), link(_link)
	{
		keyb.do_register(name, *this);
	}
/**
 * Destructor.
 */
	~modifier() throw()
	{
		kbd.do_unregister(name, *this);
	}
/**
 * Get associated keyboard.
 */
	keyboard& get_keyboard() const throw() { return kbd; }
/**
 * Get name of the modifier.
 */
	const text& get_name() const throw() { return name; }
/**
 * Get linked name of the modifier.
 *
 * Returns: The linked name, or "" if none.
 */
	const text& get_link_name() const throw() { return link; }
/**
 * Get the linked modifier.
 *
 * Returns: The linked modifier, or NULL if none (or not initialized yet).
 */
	modifier* get_link() { return kbd.try_lookup_modifier(link); }
private:
	keyboard& kbd;
	text name;
	text link;
};

/**
 * A set of modifier keys.
 */
class modifier_set
{
public:
/**
 * Add a modifier into the set.
 *
 * parameter mod: The modifier to add.
 * parameter really: If true, actually add the key. If false, do nothing.
 * throws std::bad_alloc: Not enough memory.
 */
	void add(modifier& mod, bool really = true) throw(std::bad_alloc);
/**
 * Remove a modifier from the set.
 *
 * parameter mod: The modifier to remove.
 * parameter really: If true, actually remove the key. If false, do nothing.
 * throws std::bad_alloc: Not enough memory.
 */
	void remove(modifier& mod, bool really = true) throw(std::bad_alloc);
/**
 * Construct modifier set from comma-separated string.
 *
 * parameter kbd: The keyboard to take the modifiers from.
 * parameter modifiers: The modifiers as string
 * returns: The constructed modifier set.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Illegal modifier or wrong syntax.
 */
	static modifier_set construct(keyboard& kbd, const text& modifiers) throw(std::bad_alloc,
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
	bool valid(modifier_set& mask) throw(std::bad_alloc);
/**
 * Check if this modifier set triggers the action.
 *
 * Modifier set triggers another if for each modifier or linkage group in mask:
 * - Modifier appears in both set and trigger.
 * - At least one modifier with this linkage group appears in both set and trigger.
 * - Modifiers with this linkage group do not appear in either set nor trigger.
 *
 */
	bool triggers(const modifier_set& trigger, const modifier_set& mask) throw(std::bad_alloc);
/**
 * Stringify.
 */
	operator text() const throw(std::bad_alloc);
/**
 * Equality check.
 *
 * parameter m: Another set.
 * returns: True if two sets are equal, false if not.
 */
	bool operator==(const modifier_set& m) const throw();
/**
 * Less than check.
 */
	bool operator<(const modifier_set& m) const throw();
private:
	friend std::ostream& operator<<(std::ostream& os, const modifier_set& m);
	std::set<modifier*> set;
};

/**
 * Debugging print. Prints textual version of set into stream.
 *
 * parameter os: The stream to print to.
 * parameter m: The modifier set to print.
 * returns: reference to os.
 */
std::ostream&  operator<<(std::ostream& os, const modifier_set& m);

/**
 * Type of key.
 */
enum keytype
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
struct axis_calibration
{
/**
 * Mode: -1 => Disabled, 0 => Pressure-sentive button, 1 => Axis.
 */
	int mode;
};

/**
 * Mouse axis calibration structure.
 */
struct mouse_calibration
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
class event
{
public:
/**
 * Create a new event.
 *
 * Parameter _chngmask: The change mask.
 * Parameter _type: Type of the event.
 */
	event(uint32_t _chngmask, keytype _type) throw()
	{
		chngmask = _chngmask;
		type = _type;
	}
/**
 * Destructor.
 */
	virtual ~event() throw();
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
	keytype get_type() const throw() { return type; }
private:
	uint32_t chngmask;
	keytype type;
};

/**
 * A simple key event.
 */
class event_key : public event
{
public:
/**
 * Construct a new key event.
 *
 * Parameter chngmask: The change mask.
 */
	event_key(uint32_t chngmask);
/**
 * Destructor.
 */
	~event_key() throw();
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
class event_axis : public event
{
public:
/**
 * Construct a new axis event.
 *
 * Parameter state: The analog state.
 * Parameter chngmask: The change mask.
 * Parameter cal: The calibration structure.
 */
	event_axis(int32_t state, uint32_t chngmask);
/**
 * Destructor.
 */
	~event_axis() throw();
/**
 * Get analog state.
 *
 * Returns: Analog position of axis, -32768...32767 (0...32767 for pressure-sensitive buttons).
 */
	int32_t get_state() const throw();
private:
	int32_t state;
	axis_calibration cal;
};

/**
 * A hat event.
 */
class event_hat : public event
{
public:
/**
 * Construct a new hat event.
 *
 * Parameter chngmask: The change mask to use.
 */
	event_hat(uint32_t chngmask);
/**
 * Destructor.
 */
	~event_hat() throw();
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
class event_mouse : public event
{
public:
/**
 * Construct a new mouse event.
 *
 * Parameter state: The game-relative position to use.
 * Parameter cal: The calibration structure.
 */
	event_mouse(int32_t state, const mouse_calibration& cal);
/**
 * Destructor.
 */
	~event_mouse() throw();
/**
 * Get analog state.
 *
 * Returns: Position of mouse relative to game area (with right/down positive).
 */
	int32_t get_state() const throw();
/**
 * Get calibration data.
 */
	mouse_calibration get_calibration() { return cal; }
private:
	int32_t state;
	mouse_calibration cal;
};

/**
 * A keyboard event listener.
 */
class event_listener
{
public:
/**
 * Destructor.
 */
	virtual ~event_listener() throw();
/**
 * Receive a key event.
 *
 * Parameter mods: Modifiers currently active.
 * Parameter key: The key this event is about.
 * Parameter event: The event.
 */
	virtual void on_key_event(modifier_set& mods, key& key, event& event) = 0;
};

/**
 * A (compound) key on keyboard.
 */
class key
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
	key(keyboard& keyb, const text& name, const text& clazz, keytype type)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~key() throw();
/**
 * Get class.
 */
	const text& get_class() { return clazz; }
/**
 * Get name.
 */
	const text& get_name() { return name; }
/**
 * Get keyboard this is on.
 */
	keyboard& get_keyboard() { return kbd; }
/**
 * Get key type.
 */
	keytype get_type() const throw() { return type; }
/**
 * Add listener.
 *
 * Parameter listener: The listener.
 * Parameter analog: If true, also pass analog events.
 */
	void add_listener(event_listener& listener, bool analog) throw(std::bad_alloc);
/**
 * Remove listener.
 *
 * Parameter listener: The listener.
 */
	void remove_listener(event_listener& listener) throw();
/**
 * Set exclusive listener.
 *
 * Parameter listener: The listener. NULL to ungrab key.
 */
	void set_exclusive(event_listener* listener) throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. The format is dependent on key type.
 */
	virtual void set_state(modifier_set mods, int32_t state) throw() = 0;
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
	virtual std::vector<text> get_subkeys() throw(std::bad_alloc) = 0;
/**
 * Dynamic cast to axis type.
 */
	key_axis* cast_axis() throw();
/**
 * Dynamic cast to mouse type.
 */
	key_mouse* cast_mouse() throw();
protected:
/**
 * Call all event listeners on this key.
 *
 * Parameter mods: The current modifiers.
 * Parameter event: The event to pass.
 */
	void call_listeners(modifier_set& mods, event& event);
private:
	key(key&);
	key& operator=(key&);
	keyboard& kbd;
	text clazz;
	text name;
	std::set<event_listener*> digital_listeners;
	std::set<event_listener*> analog_listeners;
	event_listener* exclusive_listener;
	keytype type;
};

/**
 * A simple key on keyboard.
 */
class key_key : public key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 */
	key_key(keyboard& keyb, const text& name, const text& clazz) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~key_key() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. 1 for pressed, 0 for released.
 */
	void set_state(modifier_set mods, int32_t state) throw();
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
	std::vector<text> get_subkeys() throw(std::bad_alloc);
private:
	key_key(key_key&);
	key_key& operator=(key_key&);
	int32_t state;
};

/**
 * A hat on keyboard.
 */
class key_hat : public key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 */
	key_hat(keyboard& keyb, const text& name, const text& clazz) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~key_hat() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. 1 => up, 2 => right, 4 => down, 8 => left.
 */
	void set_state(modifier_set mods, int32_t state) throw();
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
	std::vector<text> get_subkeys() throw(std::bad_alloc);
private:
	key_hat(key_hat&);
	key_hat& operator=(key_hat&);
	int32_t state;
};

/**
 * An axis on keyboard.
 */
class key_axis : public key
{
public:
/**
 * Constructor.
 *
 * Parameter keyb: The keyboard this is on.
 * Parameter name: The base name of the key.
 * Parameter clazz: The class of the key.
 * Parameter mode: Initial mode: -1 => disabled, 0 => axis, 1 => pressure
 */
	key_axis(keyboard& keyb, const text& name, const text& clazz, int mode)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	~key_axis() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. Uncalibrated analog position.
 */
	void set_state(modifier_set mods, int32_t state) throw();
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
	std::vector<text> get_subkeys() throw(std::bad_alloc);
/**
 * Get mode.
 */
	int get_mode() const throw();
/**
 * Set mode.
 */
	void set_mode(int mode, double tolerance) throw();
private:
	key_axis(key_axis&);
	key_axis& operator=(key_axis&);
	int32_t rawstate;
	int digitalstate;
	double last_tolerance;
	int _mode;
};

/**
 * A mouse axis on keyboard.
 */
class key_mouse : public key
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
	key_mouse(keyboard& keyb, const text& name, const text& clazz,
		mouse_calibration cal) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~key_mouse() throw();
/**
 * Set analog state.
 *
 * Parameter mods: The current modifiers.
 * Parameter state: The new state. Screen-relative analog position.
 */
	void set_state(modifier_set mods, int32_t state) throw();
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
	std::vector<text> get_subkeys() throw(std::bad_alloc);
/**
 * Get calibration.
 */
	mouse_calibration get_calibration() const throw();
/**
 * Set calibration.
 */
	void set_calibration(mouse_calibration cal) throw();
private:
	key_mouse(key_mouse&);
	key_mouse& operator=(key_mouse&);
	int32_t rawstate;
	mouse_calibration cal;
};
}
#endif
