#include "command.hpp"
#include "globalwrap.hpp"
#include "integer-pool.hpp"
#include "minmax.hpp"
#include "register-queue.hpp"
#include "string.hpp"
#include "zip.hpp"
#include <iostream>
#include <cstdlib>

namespace command
{
namespace
{
	struct run_script : public base
	{
		run_script(group& group, std::ostream*& _output)
			: base(group, "run-script"), in_group(group), output(_output)
		{
		}

		~run_script() throw()
		{
		}

		void invoke(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
		{
			if(filename == "") {
				(*output) << "Syntax: run-script <scriptfile>" << std::endl;
				return;
			}
			std::istream* o = NULL;
			try {
				o = &zip::openrel(filename, "");
				(*output) << "Running '" << std::string(filename) << "'" << std::endl;
				std::string line;
				while(std::getline(*o, line))
					in_group.invoke(line);
				delete o;
			} catch(std::exception& e) {
				delete o;
				throw;
			}
		}

		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Run file as a script";
		}

		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: run-script <file>\nRuns file <file> just as it would have been entered in "
				"the command line\n";
		}

		group& in_group;
		std::ostream*& output;
	};

	void default_oom_panic()
	{
		std::cerr << "PANIC: Fatal error, can't continue: Out of memory." << std::endl;
		exit(1);
	}

	struct set_callbacks
	{
		std::function<void(set& s, const std::string& name, factory_base& cmd)> ccb;
		std::function<void(set& s, const std::string& name)> dcb;
	};

	struct set_internal_state
	{
		threads::lock cb_lock;
		std::map<uint64_t, set_callbacks> callbacks;
		integer_pool pool;
	};

	
	std::map<set*, set_internal_state*>* int_state;
	threads::lock* int_state_lock;

	set_internal_state& get_set_internal(set* s)
	{
		static threads::lock olock;
		threads::alock hx(olock);
		if(!int_state_lock) int_state_lock = new threads::lock;
		if(!int_state) int_state = new std::map<set*, set_internal_state*>;
		threads::alock h(*int_state_lock);
		if(!int_state->count(s))
			(*int_state)[s] = new set_internal_state;
		return *(*int_state)[s];
	}
	void release_set_internal(set* s)
	{
		if(!int_state)
			return;
		threads::alock h(*int_state_lock);
		if(!int_state->count(s))
			return;
		delete (*int_state)[s];
		int_state->erase(s);
	}

	typedef register_queue<group, base> regqueue_t;
	typedef register_queue<set, factory_base> regqueue2_t;
}

void factory_base::_factory_base(set& _set, const std::string& cmd) throw(std::bad_alloc)
{
	in_set = &_set;
	regqueue2_t::do_register(*in_set, commandname = cmd, *this);
}

factory_base::~factory_base() throw()
{
	regqueue2_t::do_unregister(*in_set, commandname);
}

base::base(group& group, const std::string& cmd) throw(std::bad_alloc)
	: in_group(group)
{
	regqueue_t::do_register(in_group, commandname = cmd, *this);
}

base::~base() throw()
{
	regqueue_t::do_unregister(in_group, commandname);
}

std::string base::get_short_help() throw(std::bad_alloc)
{
	return "No description available";
}

std::string base::get_long_help() throw(std::bad_alloc)
{
	return "No help available on command " + commandname;
}

set::set() throw(std::bad_alloc)
{
	regqueue2_t::do_ready(*this, true);
}

set::~set() throw()
{
	regqueue2_t::do_ready(*this, false);
	release_set_internal(this);
}

void set::do_register(const std::string& name, factory_base& cmd) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	if(commands.count(name)) {
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
		return;
	}
	commands[name] = &cmd;
	auto& istate = get_set_internal(this);
	threads::alock lock2(istate.cb_lock);
	for(auto i : istate.callbacks)
		i.second.ccb(*this, name, cmd);
}

void set::do_unregister(const std::string& name, factory_base* dummy) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	commands.erase(name);
	auto& istate = get_set_internal(this);
	threads::alock lock2(istate.cb_lock);
	for(auto i : istate.callbacks)
		i.second.dcb(*this, name);
}

uint64_t set::add_callback(std::function<void(set& s, const std::string& name, factory_base& cmd)> ccb,
	std::function<void(set& s, const std::string& name)> dcb) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	set_callbacks cb;
	cb.ccb = ccb;
	cb.dcb = dcb;
	auto& istate = get_set_internal(this);
	threads::alock lock2(istate.cb_lock);
	uint64_t i = istate.pool();
	try { istate.callbacks[i] = cb; } catch(...) { istate.pool(i); throw; }
	return i;
}

void set::drop_callback(uint64_t handle) throw()
{
	threads::alock lock(int_mutex);
	auto& istate = get_set_internal(this);
	threads::alock lock2(istate.cb_lock);
	if(istate.callbacks.count(handle)) {
		istate.callbacks.erase(handle);
		istate.pool(handle);
	}
}

std::map<std::string, factory_base*> set::get_commands()
{
	threads::alock lock(int_mutex);
	return commands;
}

group::group() throw(std::bad_alloc)
{
	oom_panic_routine = default_oom_panic;
	output = &std::cerr;
	regqueue_t::do_ready(*this, true);
	//The builtin commands.
	builtin[0] = new run_script(*this, output);
}

group::~group() throw()
{
	for(size_t i = 0; i < sizeof(builtin)/sizeof(builtin[0]); i++)
		delete builtin[i];
	regqueue_t::do_ready(*this, false);
	for(auto i : set_handles)
		i.first->drop_callback(i.second);
}

void group::invoke(const std::string& cmd) throw()
{
	try {
		std::string cmd2 = strip_CR(cmd);
		if(cmd2 == "?") {
			//The special ? command.
			threads::alock lock(int_mutex);
			for(auto i : commands)
				(*output) << i.first << ": " << i.second->get_short_help() << std::endl;
			return;
		}
		if(firstchar(cmd2) == '?') {
			//?command.
			threads::alock lock(int_mutex);
			std::string rcmd = cmd2.substr(1, min(cmd2.find_first_of(" \t"), cmd2.length()));
			if(firstchar(rcmd) != '*') {
				//This may be an alias.
				if(aliases.count(rcmd)) {
					//Yup.
					(*output) << rcmd << " is an alias for: " << std::endl;
					size_t j = 0;
					for(auto i : aliases[rcmd])
						(*output) << "#" << (++j) << ": " << i << std::endl;
					return;
				}
			} else
				rcmd = rcmd.substr(1);
			if(!commands.count(rcmd))
				(*output) << "Unknown command '" << rcmd << "'" << std::endl;
			else
				(*output) << commands[rcmd]->get_long_help() << std::endl;
			return;
		}
		bool may_be_alias_expanded = true;
		if(firstchar(cmd2) == '*') {
			may_be_alias_expanded = false;
			cmd2 = cmd2.substr(1);
		}
		//Now this gets painful as command handlers must not be invoked with lock held.
		if(may_be_alias_expanded) {
			std::list<std::string> aexp;
			{
				threads::alock lock(int_mutex);
				if(!aliases.count(cmd))
					goto not_alias;
				aexp = aliases[cmd2];
			}
			for(auto i : aexp)
				invoke(i);
			return;
		}
not_alias:
		try {
			size_t split = cmd2.find_first_of(" \t");
			std::string rcmd = cmd2.substr(0, min(split, cmd2.length()));
			std::string args = cmd2.substr(min(cmd2.find_first_not_of(" \t", split), cmd2.length()));
			base* cmdh = NULL;
			{
				threads::alock lock(int_mutex);
				if(!commands.count(rcmd)) {
					(*output) << "Unknown command '" << rcmd << "'" << std::endl;
					return;
				}
				cmdh = commands[rcmd];
			}
			if(command_stack.count(cmd2))
				throw std::runtime_error("Recursive command invocation");
			command_stack.insert(cmd2);
			cmdh->invoke(args);
			command_stack.erase(cmd2);
			return;
		} catch(std::bad_alloc& e) {
			oom_panic_routine();
		} catch(std::exception& e) {
			(*output) << "Error: " << e.what() << std::endl;
			command_stack.erase(cmd2);
			return;
		}
	} catch(std::bad_alloc& e) {
		oom_panic_routine();
	}
}

std::set<std::string> group::get_aliases() throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	std::set<std::string> r;
	for(auto i : aliases)
		r.insert(i.first);
	return r;
}

std::string group::get_alias_for(const std::string& aname) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	if(!valid_alias_name(aname))
		return "";
	if(aliases.count(aname)) {
		std::string x;
		for(auto i : aliases[aname])
			x = x + i + "\n";
		return x;
	} else
		return "";
}

void group::set_alias_for(const std::string& aname, const std::string& avalue) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	if(!valid_alias_name(aname))
		return;
	std::list<std::string> newlist;
	size_t avitr = 0;
	while(avitr < avalue.length()) {
		size_t nextsplit = min(avalue.find_first_of("\n", avitr), avalue.length());
		std::string x = strip_CR(avalue.substr(avitr, nextsplit - avitr));
		if(x.length() > 0)
			newlist.push_back(x);
		avitr = nextsplit + 1;
	}
	if(newlist.empty())
		aliases.erase(aname);
	else
		aliases[aname] = newlist;
}

bool group::valid_alias_name(const std::string& aliasname) throw(std::bad_alloc)
{
	if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
		return false;
	if(aliasname.find_first_of(" \t") < aliasname.length())
		return false;
	return true;
}

void group::do_register(const std::string& name, base& cmd) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	if(commands.count(name))
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
	commands[name] = &cmd;
}

void group::do_unregister(const std::string& name, base* dummy) throw(std::bad_alloc)
{
	threads::alock lock(int_mutex);
	commands.erase(name);
}

void group::set_output(std::ostream& s)
{
	output = &s;
}

void group::set_oom_panic(void (*fn)())
{
	if(fn)
		oom_panic_routine = fn;
	else
		oom_panic_routine = default_oom_panic;
}

void group::add_set(set& s) throw(std::bad_alloc)
{
	//Add the callback first in order to avoid races.
	set_handles[&s] = s.add_callback(
		[this](set& s, const std::string& name, factory_base& cmd) {
			cmd.make(*this);
		},
		[this](set& s, const std::string& name) { 
			threads::alock lock(this->int_mutex);
			commands.erase(name);
		}
	);
	auto cmds = s.get_commands();
	for(auto i : cmds)
		i.second->make(*this);
}

void group::drop_set(set& s) throw()
{
	threads::alock lock(int_mutex);
	if(!set_handles.count(&s))
		return;
	//Drop the commands before dropping the handle.
	auto cmds = s.get_commands();
	for(auto i : cmds)
		commands.erase(i.first);
	s.drop_callback(set_handles[&s]);
	set_handles.erase(&s);
}

template<>
void invoke_fn(void (*fn)(const std::string& args), const std::string& args)
{
	fn(args);
}

template<>
void invoke_fn(void (*fn)(), const std::string& args)
{
	if(args != "")
		throw std::runtime_error("This command does not take arguments");
	fn();
}

template<>
void invoke_fn(void (*fn)(struct arg_filename a), const std::string& args)
{
	if(args == "")
		throw std::runtime_error("Filename required");
	arg_filename b;
	b.v = args;
	fn(b);
}
}
