#ifndef _library__keyboard_mapper__hpp__included__
#define _library__keyboard_mapper__hpp__included__

#include <set>
#include <list>
#include <stdexcept>
#include <string>
#include "keyboard.hpp"

namespace command
{
	class group;
}

namespace keyboard
{
class invbind;
class ctrlrkey;

std::pair<key*, unsigned> keymapper_lookup_subkey(keyboard& kbd, const std::string& name,
	bool axis) throw(std::bad_alloc, std::runtime_error);

/**
 * Key specifier
 */
struct keyspec
{
/**
 * Create a new key specifier (invalid).
 */
	keyspec() throw(std::bad_alloc);
/**
 * Create a new key specifier from keyspec.
 *
 * Parameter keyspec: The key specifier.
 */
	keyspec(const std::string& keyspec) throw(std::bad_alloc, std::runtime_error);
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
	bool operator==(const keyspec& keyspec);
/**
 * Compare for not-equality.
 */
	bool operator!=(const keyspec& keyspec);
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

class invbind_set;
class invbind_info;

/**
 * Set add/drop listener.
 */
class set_listener
{
public:
/**
 * Dtor.
 */
	virtual ~set_listener();
/**
 * New item in set.
 */
	virtual void create(invbind_set& s, const std::string& name, invbind_info& ibinfo) = 0;
/**
 * Deleted item from set.
 */
	virtual void destroy(invbind_set& s, const std::string& name) = 0;
/**
 * Destroyed the entiere set.
 */
	virtual void kill(invbind_set& s) = 0;
};


/**
 * Keyboard mapper. Maps keyboard keys into commands.
 */
class mapper : public event_listener
{
public:
/**
 * Create new keyboard mapper.
 */
	mapper(keyboard& kbd, command::group& domain) throw(std::bad_alloc);
/**
 * Destroy a keyboard mapper.
 */
	~mapper() throw();
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
	std::list<keyspec> get_bindings() throw(std::bad_alloc);
/**
 * Get command for key.
 */
	std::string get(const keyspec& keyspec) throw(std::bad_alloc);
/**
 * Bind command for key.
 *
 * Parameter keyspec: The key specifier to bind to.
 * Parameter cmd: The command to bind. If "", the key is unbound.
 */
	void set(const keyspec& keyspec, const std::string& cmd) throw(std::bad_alloc, std::runtime_error);
/**
 * Get set of inverse binds.
 *
 * Returns: The set of all inverses.
 */
	std::set<invbind*> get_inverses() throw(std::bad_alloc);
/**
 * Find inverse bind by command.
 *
 * Parameter command: The command.
 * Returns: The inverse bind, or NULL if none.
 */
	invbind* get_inverse(const std::string& command) throw(std::bad_alloc);
/**
 * Get set of controller keys.
 *
 * Returns: The set of all controller keys.
 */
	std::set<ctrlrkey*> get_controller_keys() throw(std::bad_alloc);
/**
 * Get specific controller key.
 */
	ctrlrkey* get_controllerkey(const std::string& command) throw(std::bad_alloc);
/**
 * Get list of controller keys for specific keyboard key.
 */
	std::list<ctrlrkey*> get_controllerkeys_kbdkey(key* kbdkey) throw(std::bad_alloc);
/**
 * Register inverse bind.
 */
	void do_register(const std::string& name, invbind& bind) throw(std::bad_alloc);
/**
 * Unregister inverse bind.
 */
	void do_unregister(const std::string& name, invbind* dummy) throw(std::bad_alloc);
/**
 * Register controller key.
 */
	void do_register(const std::string& name, ctrlrkey& ckey) throw(std::bad_alloc);
/**
 * Unregister inverse bind.
 */
	void do_unregister(const std::string& name, ctrlrkey* dummy) throw(std::bad_alloc);
/**
 * Get keyboard.
 */
	keyboard& get_keyboard() throw();
/**
 * Get command group to run commands in..
 */
	command::group& get_command_group() throw();
/**
 * Fixup command based on polarity.
 *
 * Parameter cmd: The raw command.
 * Parameter polarity: Polarity (true is rising edge, false is falling edge).
 * Returns: The fixed command, or "" if nothing should be run.
 */
	static std::string fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc);
/**
 * Add a set of inverse binds.
 */
	void add_invbind_set(invbind_set& set);
/**
 * Drop a set of inverse binds.
 */
	void drop_invbind_set(invbind_set& set);
private:
	struct triplet
	{
		triplet(modifier_set mod, modifier_set mask, key& kkey, unsigned subkey);
		triplet(key& kkey, unsigned subkey);
		triplet(keyboard& k, const keyspec& spec);
		bool operator<(const struct triplet& a) const;
		bool operator==(const struct triplet& a) const;
		bool operator<=(const struct triplet& a) const { return !(a > *this); }
		bool operator!=(const struct triplet& a) const { return !(a == *this); }
		bool operator>=(const struct triplet& a) const { return !(a < *this); }
		bool operator>(const struct triplet& a) const { return (a < *this); }
		keyspec as_keyspec() const throw(std::bad_alloc);
		bool index;
		modifier_set mod;
		modifier_set mask;
		key* _key;
		unsigned subkey;
	};
	class listener : public set_listener
	{
	public:
		listener(mapper& _grp);
		~listener();
		void create(invbind_set& s, const std::string& name, invbind_info& ibinfo);
		void destroy(invbind_set& s, const std::string& name);
		void kill(invbind_set& s);
	private:
		mapper& grp;
	} _listener;
	void change_command(const keyspec& spec, const std::string& old, const std::string& newc);
	void on_key_event(modifier_set& mods, key& key, event& event);
	void on_key_event_subkey(modifier_set& mods, key& key, unsigned skey, bool polarity);
	mapper(const mapper&);
	mapper& operator=(const mapper&);
	std::map<std::string, invbind*> ibinds;
	std::map<std::string, ctrlrkey*> ckeys;
	std::map<triplet, std::string> bindings;
	std::set<key*> listening;
	std::set<invbind_set*> invbind_set_cbs;
	keyboard& kbd;
	command::group& domain;
	bool dtor_running;
};

class invbind_info;

/**
 * Inverse bind set.
 */
class invbind_set
{
public:
/**
 * Create a set.
 */
	invbind_set();
/**
 * Destructor.
 */
	~invbind_set();
/**
 * Register a inverse bind.
 */
	void do_register(const std::string& name, invbind_info& info);
/**
 * Unregister a inverse bind.
 */
	void do_unregister(const std::string& name, invbind_info& info);
/**
 * Add a callback on new invese bind.
 */
	void add_callback(set_listener& listener) throw(std::bad_alloc);
/**
 * Drop a callback on new inverse bind.
 */
	void drop_callback(set_listener& listener);
};

/**
 * Inverse bind info.
 */
class invbind_info
{
public:
/**
 * Create inverse bind.
 *
 * Parameter set: The set to be in.
 * Parameter _command: Command this is for.
 * Parameter _name: Name of inverse key.
 */
	invbind_info(invbind_set& set, const std::string& _command, const std::string& _name)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	~invbind_info() throw();
/**
 * Make inverse bind out of this.
 */
	invbind* make(mapper& m);
/**
 * Notify set dying.
 */
	void set_died();
private:
	invbind_set* in_set;
	std::string command;
	std::string name;
};

/**
 * Inverse bind. Can map up to 2 keys to some command (and follows forward binds).
 */
class invbind
{
public:
/**
 * Create inverse bind.
 *
 * Parameter mapper: The keyboard mapper to follow.
 * Parameter command: Command this is for.
 * Parameter name: Name of inverse key.
 */
	invbind(mapper& kmapper, const std::string& command, const std::string& name, bool dynamic = false)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	~invbind() throw();
/**
 * Get keyspec.
 *
 * Parameter index: Index of the keyspec to get.
 * Returns: The keyspec.
 */
	keyspec get(unsigned index) throw(std::bad_alloc);
/**
 * Clear key (subsequent keys fill the gap).
 *
 * Parameter index: Index of key to clear.
 */
	void clear(unsigned index) throw(std::bad_alloc);
/**
 * Add key to set.
 *
 * Parameter keyspec: The new keyspec.
 */
	void append(const keyspec& keyspec) throw(std::bad_alloc);
/**
 * Get name for command.
 *
 * Returns: The name.
 */
	std::string getname() throw(std::bad_alloc);
/**
 * Notify mapper dying.
 */
	void mapper_died();
private:
	friend class mapper;
	invbind(const invbind&);
	invbind& operator=(const invbind&);
	void addkey(const keyspec& keyspec);
	mapper* _mapper;
	std::string cmd;
	std::string oname;
	std::vector<keyspec> specs;
	bool is_dynamic;
};

/**
 * A controller key.
 *
 * Can overlap with any other bind.
 */
class ctrlrkey : public event_listener
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
	ctrlrkey(mapper& kmapper, const std::string& command, const std::string& name,
		bool axis = false) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~ctrlrkey() throw();
/**
 * Get the trigger key.
 */
	std::pair<key*, unsigned> get(unsigned index) throw();
/**
 * Get the trigger key.
 */
	std::string get_string(unsigned index) throw(std::bad_alloc);
/**
 * Set the trigger key (appends).
 */
	void append(key* key, unsigned subkey) throw();
/**
 * Set the trigger key (appends).
 */
	void append(const std::string& key) throw(std::bad_alloc, std::runtime_error);
/**
 * Remove the trigger key.
 */
	void remove(key* key, unsigned subkey) throw();
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
	void on_key_event(modifier_set& mods, key& key, event& event);
	mapper& _mapper;
	std::string cmd;
	std::string oname;
	std::vector<std::pair<key*, unsigned>> keys;
	bool axis;
};
}

#endif
