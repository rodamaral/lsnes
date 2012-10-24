#ifndef _library_commands__hpp__included__
#define _library_commands__hpp__included__

#include <stdexcept>
#include <string>
#include <set>
#include <map>
#include <list>
#include "library/threadtypes.hpp"

class command;

/**
 * A group of commands (with aliases).
 */
class command_group
{
public:
/**
 * Create a new command group. This also places some builtin commands in that new group.
 */
	command_group() throw(std::bad_alloc);
/**
 * Destroy a group.
 */
	~command_group() throw();
/**
 * Look up and invoke a command. The command will undergo alias expansion and recursion checking.
 *
 * parameter cmd: Command to exeucte.
 */
	void invoke(const std::string& cmd) throw();
/**
 * Get set of aliases.
 */
	std::set<std::string> get_aliases() throw(std::bad_alloc);
/**
 * Get alias
 */
	std::string get_alias_for(const std::string& aname) throw(std::bad_alloc);
/**
 * Set alias
 */
	void set_alias_for(const std::string& aname, const std::string& avalue) throw(std::bad_alloc);
/**
 * Is alias name valid.
 */
	bool valid_alias_name(const std::string& aname) throw(std::bad_alloc);
/**
 * Register a command.
 */
	void do_register(const std::string& name, command& cmd) throw(std::bad_alloc);
/**
 * Unregister a command.
 */
	void do_unregister(const std::string& name) throw(std::bad_alloc);
/**
 * Set the output stream.
 */
	void set_output(std::ostream& s);
/**
 * Set the OOM panic routine.
 */
	void set_oom_panic(void (*fn)());
private:
	std::map<std::string, command*> commands;
	std::set<std::string> command_stack;
	std::map<std::string, std::list<std::string>> aliases;
	mutex_class int_mutex;
	std::ostream* output;
	void (*oom_panic_routine)();
	command* builtin[4];
};

/**
 * A command.
 */
class command
{
public:
/**
 * Register a new command.
 *
 * parameter group: The group command will be part of.
 * parameter cmd: The command to register.
 * throws std::bad_alloc: Not enough memory.
 */
	command(command_group& group, const std::string& cmd) throw(std::bad_alloc);

/**
 * Deregister a command.
 */
	virtual ~command() throw();

/**
 * Invoke a command.
 *
 * parameter arguments: Arguments to command.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Command execution failed.
 */
	virtual void invoke(const std::string& arguments) throw(std::bad_alloc, std::runtime_error) = 0;
/**
 * Get short help for command.
 */
	virtual std::string get_short_help() throw(std::bad_alloc);

/**
 * Get long help for command.
 */
	virtual std::string get_long_help() throw(std::bad_alloc);
private:
	command(const command&);
	command& operator=(const command&);
	std::string commandname;
	command_group& in_group;
};

/**
 * Mandatory filename
 */
struct arg_filename
{
/**
 * The filename itself.
 */
	std::string v;
/**
 * Return the filename.
 *
 * returns: The filename.
 */
	operator std::string() { return v; }
};

/**
 * Run command function helper.
 *
 * parameter fn: Function pointer to invoke.
 * parameter a: The arguments to pass.
 */
template<typename... args>
void invoke_command_fn(void (*fn)(args... arguments), const std::string& a);

/**
 * Warp function pointer as command.
 */
template<typename... args>
class function_ptr_command : public command
{
public:
/**
 * Create a new command.
 *
 * parameter group: The group command will be part of.
 * parameter name: Name of the command
 * parameter description Description for the command
 * parameter help: Help for the command.
 * parameter fn: Function to call on command.
 */
	function_ptr_command(command_group& group, const std::string& name, const std::string& _description,
		const std::string& _help, void (*_fn)(args... arguments)) throw(std::bad_alloc)
		: command(group, name)
	{
		description = _description;
		help = _help;
		fn = _fn;
	}
/**
 * Destroy a commnad.
 */
	~function_ptr_command() throw()
	{
	}
/**
 * Invoke a command.
 *
 * parameter a: Arguments to function.
 */
	void invoke(const std::string& a) throw(std::bad_alloc, std::runtime_error)
	{
		invoke_command_fn(fn, a);
	}
/**
 * Get short description.
 *
 * returns: Description.
 * throw std::bad_alloc: Not enough memory.
 */
	std::string get_short_help() throw(std::bad_alloc)
	{
		return description;
	}
/**
 * Get long help.
 *
 * returns: help.
 * throw std::bad_alloc: Not enough memory.
 */
	std::string get_long_help() throw(std::bad_alloc)
	{
		return help;
	}
private:
	void (*fn)(args... arguments);
	std::string description;
	std::string help;
};

#endif
