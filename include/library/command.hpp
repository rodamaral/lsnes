#ifndef _library_command__hpp__included__
#define _library_command__hpp__included__

#include <stdexcept>
#include <string>
#include <functional>
#include <set>
#include <map>
#include <list>

namespace command
{
class set;
class base;
class factory_base;

/**
 * A set of commands.
 */
class set
{
public:
/**
 * Set add/drop listener.
 */
	class listener
	{
	public:
/**
 * Dtor.
 */
		virtual ~listener();
/**
 * New item in set.
 */
		virtual void create(set& s, const std::string& name, factory_base& cmd) = 0;
/**
 * Deleted item from set.
 */
		virtual void destroy(set& s, const std::string& name) = 0;
/**
 * Destroyed the entiere set.
 */
		virtual void kill(set& s) = 0;
	};
/**
 * Create a new set.
 */
	set() throw(std::bad_alloc);
/**
 * Destroy a set.
 */
	~set() throw();
/**
 * Add a command to set.
 */
	void do_register(const std::string& name, factory_base& cmd) throw(std::bad_alloc);
/**
 * Remove a command from set.
 */
	void do_unregister(const std::string& name, factory_base& cmd) throw(std::bad_alloc);
/**
 * Add a notification callback and call ccb on all.
 *
 * Parameter listener: The listener to add.
 */
	void add_callback(listener& listener) throw(std::bad_alloc);
/**
 * Drop a notification callback and call dcb on all.
 *
 * Parameter listener: The listener to drop.
 */
	void drop_callback(listener& listener) throw();
private:
	char dummy;
};

/**
 * A group of commands (with aliases).
 */
class group
{
public:
/**
 * Create a new command group. This also places some builtin commands in that new group.
 */
	group() throw(std::bad_alloc);
/**
 * Destroy a group.
 */
	~group() throw();
/**
 * Look up and invoke a command. The command will undergo alias expansion and recursion checking.
 *
 * parameter cmd: Command to exeucte.
 */
	void invoke(const std::string& cmd) throw();
/**
 * Look up and invoke a command. No alias expansion is performed, but recursion checking is.
 *
 * parameter cmd: Command to execute.
 * parameter args: The parameters for command.
 */
	void invoke(const std::string& cmd, const std::string& args) throw();
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
	void do_register(const std::string& name, base& cmd) throw(std::bad_alloc);
/**
 * Unregister a command.
 */
	void do_unregister(const std::string& name, base& cmd) throw(std::bad_alloc);
/**
 * Add all commands (including future ones) in given set.
 */
	void add_set(set& s) throw(std::bad_alloc);
/**
 * Drop a set of commands.
 */
	void drop_set(set& s) throw();
/**
 * Set the output stream.
 */
	void set_output(std::ostream& s);
/**
 * Set the OOM panic routine.
 */
	void set_oom_panic(void (*fn)());
private:
	class listener : public set::listener
	{
	public:
		listener(group& _grp);
		~listener();
		void create(set& s, const std::string& name, factory_base& cmd);
		void destroy(set& s, const std::string& name);
		void kill(set& s);
	private:
		group& grp;
	} _listener;
	std::set<std::string> command_stack;
	std::map<std::string, std::list<std::string>> aliases;
	std::ostream* output;
	void (*oom_panic_routine)();
	base* builtin[1];
};

/**
 * A command.
 */
class base
{
public:
/**
 * Register a new command.
 *
 * parameter group: The group command will be part of.
 * parameter cmd: The command to register.
 * parameter dynamic: Should the object be freed when its parent group dies?
 * throws std::bad_alloc: Not enough memory.
 */
	base(group& group, const std::string& cmd, bool dynamic) throw(std::bad_alloc);

/**
 * Deregister a command.
 */
	virtual ~base() throw();

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
/**
 * Get name of command.
 */
	const std::string& get_name() { return commandname; }
/**
 * Notify that the parent group died.
 *
 * Note: Assumed to be called with global lock held.
 */
	void group_died() throw();
private:
	base(const base&);
	base& operator=(const base&);
	std::string commandname;
	group* in_group;
	bool is_dynamic;
};

/**
 * A command factory.
 */
class factory_base
{
public:
	factory_base() throw() {}
/**
 * Register a new command.
 *
 * parameter _set: The set command will be part of.
 * parameter cmd: The command to register.
 * throws std::bad_alloc: Not enough memory.
 */
	void _factory_base(set& _set, const std::string& cmd) throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~factory_base() throw();
/**
 * Make a new command.
 */
	virtual base* make(group& grp) = 0;
/**
 * Notify that the parent set died.
 *
 * Note: Assumed to be called with global lock held.
 */
	void set_died() throw();
private:
	factory_base(const factory_base&);
	factory_base& operator=(const factory_base&);
	std::string commandname;
	set* in_set;
};

/**
 * Name, Description and help for command.
 */
struct stub
{
/**
 * The command name.
 */
	const char* name;
/**
 * The short description.
 */
	const char* desc;
/**
 * The long help.
 */
	const char* help;
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
void invoke_fn(std::function<void(args... arguments)> fn, const std::string& a);

/**
 * Warp function pointer as command.
 */
template<typename... args>
class _fnptr : public base
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
 * parameter dynamic: Should the object be freed when its parent group dies?
 */
	_fnptr(group& group, const std::string& name, const std::string& _description,
		const std::string& _help, void (*_fn)(args... arguments), bool dynamic = false) throw(std::bad_alloc)
		: base(group, name, dynamic)
	{
		shorthelp = _description;
		help = _help;
		fn = _fn;
	}
/**
 * Create a new command.
 *
 * parameter _set: The set command will be part of.
 * parameter name: Name of the command
 * parameter fn: Function to call on command.
 * parameter description Description&Help for the command
 */
	_fnptr(group& _group, stub _name, std::function<void(args... arguments)> _fn) throw(std::bad_alloc)
		: base(_group, _name.name, false)
	{
		shorthelp = _name.desc;
		help = _name.help;
		fn = _fn;
	}
/**
 * Destroy a commnad.
 */
	~_fnptr() throw()
	{
	}
/**
 * Invoke a command.
 *
 * parameter a: Arguments to function.
 */
	void invoke(const std::string& a) throw(std::bad_alloc, std::runtime_error)
	{
		invoke_fn(fn, a);
	}
/**
 * Get short description.
 *
 * returns: Description.
 * throw std::bad_alloc: Not enough memory.
 */
	std::string get_short_help() throw(std::bad_alloc)
	{
		return shorthelp;
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
	std::function<void(args... arguments)> fn;
	std::string shorthelp;
	std::string help;
};

/**
 * Function pointer command factory.
 */
template<typename... args>
class fnptr : public factory_base
{
public:
/**
 * Create a new command.
 *
 * parameter _set: The set command will be part of.
 * parameter desc: Command descriptor.
 * parameter fn: Function to call on command.
 */
	fnptr(set& _set, stub _name, void (*_fn)(args... arguments)) throw(std::bad_alloc)
	{
		shorthelp = _name.desc;
		name = _name.name;
		help = _name.help;
		fn = _fn;
		_factory_base(_set, name);
	}
/**
 * Create a new command.
 *
 * parameter _set: The set command will be part of.
 * parameter name: Name of the command
 * parameter description Description for the command
 * parameter help: Help for the command.
 * parameter fn: Function to call on command.
 */
	fnptr(set& _set, const std::string& _name, const std::string& _description,
		const std::string& _help, void (*_fn)(args... arguments)) throw(std::bad_alloc)
	{
		shorthelp = _description;
		name = _name;
		help = _help;
		fn = _fn;
		_factory_base(_set, name);
	}
/**
 * Destroy a commnad.
 */
	~fnptr() throw()
	{
	}
/**
 * Make a command.
 */
	base* make(group& grp) throw(std::bad_alloc)
	{
		return new _fnptr<args...>(grp, name, shorthelp, help, fn, true);
	}
private:
	void (*fn)(args... arguments);
	std::string shorthelp;
	std::string help;
	std::string name;
};

/**
 * Generic byname factory.
 */
template<typename T>
class byname_factory : public factory_base
{
public:
/**
 * Create a new factory.
 */
	byname_factory(set& s, const std::string& _name)
	{
		name = _name;
		_factory_base(s, name);
	}
/**
 * Destroy.
 */
	~byname_factory() throw()
	{
	}
/**
 * Make a command.
 */
	base* make(group& grp) throw(std::bad_alloc)
	{
		return new T(grp, name);
	}
private:
	std::string name;
};
}
#endif
