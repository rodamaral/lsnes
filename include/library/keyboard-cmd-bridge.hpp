#ifndef _library__keyboard_cmd_bridge__hpp__
#define _library__keyboard_cmd_bridge__hpp__

#include "keyboard.hpp"
#include "commands.hpp"

class inverse_bind;

/**
 * Bridge between keyboard and command group.
 */
class keyboard_command_bridge
{
public:
/**
 * Create a bridge.
 */
	keyboard_command_bridge(keyboard& kgroup, command_group& cgroup);
/**
 * Destroy a bridge.
 */
	~keyboard_command_bridge() throw();
/**
 * Bind a key.
 *
 * Parameter mod: The modifiers.
 * Parameter mask: The modifier mask.
 * Parameter key: The key.
 * Parameter cmd: The command to execute.
 */
	void bind(const std::string& mod, const std::string& mask, const std::string& key, const std::string& cmd)
		throw(std::bad_alloc);
/**
 * Unbind a key.
 *
 * Parameter mod: The modifiers.
 * Parameter mask: The modifier mask.
 * Parameter key: The key.
 */
	void unbind(const std::string& mod, const std::string& mask, const std::string& key) throw(std::runtime_error);
/**
 * Get set of used keyspecs.
 */
	std::set<std::string> all_keyspecs() throw(std::bad_alloc);
/**
 * Get command for key.
 *
 * Parameter keyspec: The keyspec (<mod>/<mask>|<key>).
 * Returns: The command bound.
 */
	std::string get_command(const std::string& keyspec) throw(std::bad_alloc);
/**
 * Set command for key.
 *
 * Parameter keyspec: The keyspec (<mod>/<mask>|<key>).
 * Parameter cmd: The command to bind.
 */
	void set_command(const std::string& keyspec, const std::string& cmd) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Get set of all inverse binds.
 *
 * Returns: The set of all inverses.
 */
	std::set<inverse_bind*> all_inverses() throw(std::bad_alloc);
/**
 * Find inverse by command.
 *
 * Parameter command: The command.
 * Returns: The instance, or NULL if there is none.
 */
	inverse_bind* get_inverse(const std::string& command) throw(std::bad_alloc);
/**
 * Register an inverse.
 */
	void do_register(const std::string& name, inverse_bind& inv);
/**
 * Unregister an inverse.
 */
	void do_unregister(const std::string& name);
private:
	keyboard_command_bridge(keyboard_command_bridge&);
	keyboard_command_bridge& operator=(keyboard_command_bridge&);
	std::map<std::string, inverse_bind*> inverses;
};

/**
 * Inverse key binding.
 */
class inverse_bind
{
/**
 * Create a new inverse binding.
 *
 * Parameter bridge: The bridge to associate with.
 * Parameter name: The name of the command.
 * Parameter cmd: The command to create inverse for.
 */
	inverse_bind(keyboard_command_bridge& bridge, const std::string& name, const std::string& cmd)
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
 * Get name for command.
 */
	std::string get_name() throw() { return name; }
/**
 * Get command for inverse.
 */
	std::string get_command() throw() { return cmd; }
private:
	friend class keyboard_command_bridge;
	inverse_bind(inverse_bind&);
	inverse_bind& operator=(inverse_bind&);
	keyboard_command_bridge& bridge;
	std::string cmd;
	std::string name;
	std::string primary;
	std::string secondary;
};

#endif
