#ifndef _library__keymapper__hpp__included__
#define _library__keymapper__hpp__included__

#include "commands.hpp"
#include <set>
#include <list>
#include <stdexcept>
#include <string>
#include "keyboard.hpp"

class inverse_bind;
class controller_key;

std::pair<keyboard_key*, unsigned> keymapper_lookup_subkey(keyboard& kbd, const std::string& name, bool axis)
	throw(std::bad_alloc, std::runtime_error);

/**
 * Key specifier
 */
struct key_specifier
{
/**
 * Create a new key specifier (invalid).
 */
	key_specifier() throw(std::bad_alloc);
/**
 * Create a new key specifier from keyspec.
 *
 * Parameter keyspec: The key specifier.
 */
	key_specifier(const std::string& keyspec) throw(std::bad_alloc, std::runtime_error);
/**
 * Get the key specifier as a keyspec.
 */
	operator std::string() throw(std::bad_alloc);
/**
 * Is valid?
 */
	operator bool() throw();
/**
 * Is not valid?
 */
	bool operator!() throw();
/**
 * Clear the keyspec.
 */
	void clear() throw();
/**
 * Compare for equality.
 */
	bool operator==(const key_specifier& keyspec);
/**
 * Compare for not-equality.
 */
	bool operator!=(const key_specifier& keyspec);
/**
 * The modifier.
 */
	std::string mod;
/**
 * The mask.
 */
	std::string mask;
/**
 * The key itself.
 */
	std::string key;
};


/**
 * Keyboard mapper. Maps keyboard keys into commands.
 */
class keyboard_mapper : public keyboard_event_listener
{
public:
/**
 * Create new keyboard mapper.
 */
	keyboard_mapper(keyboard& kbd, command_group& domain) throw(std::bad_alloc);
/**
 * Destroy a keyboard mapper.
 */
	~keyboard_mapper() throw();
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
	void bind(std::string mod, std::string modmask, std::string keyname, std::string command)
		throw(std::bad_alloc, std::runtime_error);
/**
 * Unbinds a key, erroring out if binding does not exist.
 *
 * parameter mod: Modifier set to require to be pressed.
 * parameter modmask: Modifier set to take into account.
 * parameter keyname: Key to bind the action to.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The binding does not exist.
 */
	void unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Get keys bound.
 *
 * Returns: The set of keyspecs that are bound.
 */
	std::list<key_specifier> get_bindings() throw(std::bad_alloc);
/**
 * Get command for key.
 */
	std::string get(const key_specifier& keyspec) throw(std::bad_alloc);
/**
 * Bind command for key.
 *
 * Parameter keyspec: The key specifier to bind to.
 * Parameter cmd: The command to bind. If "", the key is unbound.
 */
	void set(const key_specifier& keyspec, const std::string& cmd) throw(std::bad_alloc, std::runtime_error);
/**
 * Get set of inverse binds.
 *
 * Returns: The set of all inverses.
 */
	std::set<inverse_bind*> get_inverses() throw(std::bad_alloc);
/**
 * Find inverse bind by command.
 *
 * Parameter command: The command.
 * Returns: The inverse bind, or NULL if none.
 */
	inverse_bind* get_inverse(const std::string& command) throw(std::bad_alloc);
/**
 * Get set of controller keys.
 *
 * Returns: The set of all controller keys.
 */
	std::set<controller_key*> get_controller_keys() throw(std::bad_alloc);
/**
 * Get specific controller key.
 */
	controller_key* get_controllerkey(const std::string& command) throw(std::bad_alloc);
/**
 * Get list of controller keys for specific keyboard key.
 */
	std::list<controller_key*> get_controllerkeys_kbdkey(keyboard_key* kbdkey) throw(std::bad_alloc);
/**
 * Proxy for inverse bind registrations.
 */
	struct _inverse_proxy
	{
		_inverse_proxy(keyboard_mapper& mapper) : _mapper(mapper) {}
		void do_register(const std::string& name, inverse_bind& ibind)
		{
			_mapper.do_register_inverse(name, ibind);
		}
		void do_unregister(const std::string& name)
		{
			_mapper.do_unregister_inverse(name);
		}
	private:
		keyboard_mapper& _mapper;
	} inverse_proxy;
/**
 * Proxy for controller key registrations.
 */
	struct _controllerkey_proxy
	{
		_controllerkey_proxy(keyboard_mapper& mapper) : _mapper(mapper) {}
		void do_register(const std::string& name, controller_key& ckey)
		{
			_mapper.do_register_ckey(name, ckey);
		}
		void do_unregister(const std::string& name)
		{
			_mapper.do_unregister_ckey(name);
		}
	private:
		keyboard_mapper& _mapper;
	} controllerkey_proxy;
/**
 * Register inverse bind.
 */
	void do_register_inverse(const std::string& name, inverse_bind& bind) throw(std::bad_alloc);
/**
 * Unregister inverse bind.
 */
	void do_unregister_inverse(const std::string& name) throw(std::bad_alloc);
/**
 * Register controller key.
 */
	void do_register_ckey(const std::string& name, controller_key& ckey) throw(std::bad_alloc);
/**
 * Unregister inverse bind.
 */
	void do_unregister_ckey(const std::string& name) throw(std::bad_alloc);
/**
 * Get keyboard.
 */
	keyboard& get_keyboard() throw();
/**
 * Get command group to run commands in..
 */
	command_group& get_command_group() throw();
/**
 * Fixup command based on polarity.
 *
 * Parameter cmd: The raw command.
 * Parameter polarity: Polarity (true is rising edge, false is falling edge).
 * Returns: The fixed command, or "" if nothing should be run.
 */
	static std::string fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc);
private:
	struct triplet
	{
		triplet(keyboard_modifier_set mod, keyboard_modifier_set mask, keyboard_key& key, unsigned subkey);
		triplet(keyboard_key& key, unsigned subkey);
		triplet(keyboard& k, const key_specifier& spec);
		bool operator<(const struct triplet& a) const;
		bool operator==(const struct triplet& a) const;
		bool operator<=(const struct triplet& a) const { return !(a > *this); }
		bool operator!=(const struct triplet& a) const { return !(a == *this); }
		bool operator>=(const struct triplet& a) const { return !(a < *this); }
		bool operator>(const struct triplet& a) const { return (a < *this); }
		key_specifier as_keyspec() const throw(std::bad_alloc);
		bool index;
		keyboard_modifier_set mod;
		keyboard_modifier_set mask;
		keyboard_key* key;
		unsigned subkey;
	};
	void change_command(const key_specifier& spec, const std::string& old, const std::string& newc);
	void on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event);
	void on_key_event_subkey(keyboard_modifier_set& mods, keyboard_key& key, unsigned skey, bool polarity);
	keyboard_mapper(const keyboard_mapper&);
	keyboard_mapper& operator=(const keyboard_mapper&);
	std::map<std::string, inverse_bind*> ibinds;
	std::map<std::string, controller_key*> ckeys;
	std::map<triplet, std::string> bindings;
	std::set<keyboard_key*> listening;
	keyboard& kbd;
	command_group& domain;
	mutex_class mutex;
};

/**
 * Inverse bind. Can map up to 2 keys to some command (and follows forward binds).
 */
class inverse_bind
{
public:
/**
 * Create inverse bind.
 *
 * Parameter mapper: The keyboard mapper to follow.
 * Parameter command: Command this is for.
 * Parameter name: Name of inverse key.
 */
	inverse_bind(keyboard_mapper& mapper, const std::string& command, const std::string& name)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	~inverse_bind() throw();
/**
 * Get keyspec.
 *
 * Parameter primary: If true, get the primary key, else secondary key.
 * Returns: The keyspec.
 */
	key_specifier get(bool primary) throw(std::bad_alloc);
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
	void set(const key_specifier& keyspec, bool primary) throw(std::bad_alloc);
/**
 * Get name for command.
 *
 * Returns: The name.
 */
	std::string getname() throw(std::bad_alloc);
private:
	friend class keyboard_mapper;
	inverse_bind(const inverse_bind&);
	inverse_bind& operator=(const inverse_bind&);
	void addkey(const key_specifier& keyspec);
	keyboard_mapper& mapper;
	std::string cmd;
	std::string oname;
	key_specifier primary_spec;
	key_specifier secondary_spec;
	mutex_class mutex;
};

/**
 * A controller key.
 *
 * Can overlap with any other bind.
 */
class controller_key : public keyboard_event_listener
{
public:
/**
 * Create a new controller key.
 *
 * Parameter mapper: The keyboard mapper to follow.
 * Parameter command: Command to run.
 * Parameter name: Name of controller key.
 * Parameter axis: If true, create a axis-type key.
 */
	controller_key(keyboard_mapper& mapper, const std::string& command, const std::string& name,
		bool axis = false) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~controller_key() throw();
/**
 * Get the trigger key.
 */
	std::pair<keyboard_key*, unsigned> get() throw();
/**
 * Get the trigger key.
 */
	std::string get_string() throw(std::bad_alloc);
/**
 * Set the trigger key.
 */
	void set(keyboard_key* key, unsigned subkey) throw();
/**
 * Set the trigger key.
 */
	void set(const std::string& key) throw(std::bad_alloc, std::runtime_error);
/**
 * Get the command.
 */
	const std::string& get_command() const throw() { return cmd; }
/**
 * Get the name.
 */
	const std::string& get_name() const throw() { return oname; }
/**
 * Is axis-type?
 */
	bool is_axis() const throw() { return axis; }
private:
	void on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event);
	keyboard_mapper& mapper;
	std::string cmd;
	std::string oname;
	keyboard_key* key;
	unsigned subkey;
	bool axis;
	mutex_class mutex;
};

#endif
