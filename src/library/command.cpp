#include "command.hpp"
#include "globalwrap.hpp"
#include "integer-pool.hpp"
#include "minmax.hpp"
#include "stateobject.hpp"
#include "string.hpp"
#include "threads.hpp"
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
			: base(group, "run-script", true), in_group(group), output(_output)
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

	threads::rlock* global_lock;
	threads::rlock& get_cmd_lock()
	{
		if(!global_lock) global_lock = new threads::rlock;
		return *global_lock;
	}

	struct set_internal
	{
		std::set<set::listener*> callbacks;
		std::map<std::string, factory_base*> commands;
	};

	struct group_internal
	{
		std::map<std::string, base*> commands;
		std::set<set*> set_handles;
	};

	typedef stateobject::type<set, set_internal> set_internal_t;
	typedef stateobject::type<group, group_internal> group_internal_t;
}

set::listener::~listener()
{
}

void factory_base::_factory_base(set& _set, const std::string& cmd) throw(std::bad_alloc)
{
	threads::arlock h(get_cmd_lock());
	in_set = &_set;
	in_set->do_register(commandname = cmd, *this);
}

factory_base::~factory_base() throw()
{
	threads::arlock h(get_cmd_lock());
	if(in_set)
		in_set->do_unregister(commandname, *this);
}

void factory_base::set_died() throw()
{
	//The lock is held by assumption.
	in_set = NULL;
}

base::base(group& group, const std::string& cmd, bool dynamic) throw(std::bad_alloc)
{
	in_group = &group;
	is_dynamic = dynamic;
	threads::arlock h(get_cmd_lock());
	in_group->do_register(commandname = cmd, *this);
}

base::~base() throw()
{
	threads::arlock h(get_cmd_lock());
	if(in_group)
		in_group->do_unregister(commandname, *this);
}

void base::group_died() throw()
{
	threads::arlock h(get_cmd_lock());
	in_group = NULL;
	//If dynamic, we aren't needed anymore.
	if(is_dynamic) delete this;
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
}

set::~set() throw()
{
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	threads::arlock h(get_cmd_lock());
	//Call all DCBs on all factories.
	for(auto i : state->commands)
		for(auto j : state->callbacks)
			j->destroy(*this, i.first);
	//Call all TCBs.
	for(auto j : state->callbacks)
		j->kill(*this);
	//Notify all factories that base set died.
	for(auto i : state->commands)
		i.second->set_died();
	//We assume factories look after themselves, so we don't destroy those.
	set_internal_t::clear(this);
}

void set::do_register(const std::string& name, factory_base& cmd) throw(std::bad_alloc)
{
	threads::arlock h(get_cmd_lock());
	auto& state = set_internal_t::get(this);
	if(state.commands.count(name)) {
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
		return;
	}
	state.commands[name] = &cmd;
	//Call all CCBs on this.
	for(auto i : state.callbacks)
		i->create(*this, name, cmd);
}

void set::do_unregister(const std::string& name, factory_base& cmd) throw(std::bad_alloc)
{
	threads::arlock h(get_cmd_lock());
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	if(!state->commands.count(name) || state->commands[name] != &cmd) return; //Not this.
	state->commands.erase(name);
	//Call all DCBs on this.
	for(auto i : state->callbacks)
		i->destroy(*this, name);
}

void set::add_callback(set::listener& listener)
	throw(std::bad_alloc)
{
	threads::arlock h(get_cmd_lock());
	auto& state = set_internal_t::get(this);
	state.callbacks.insert(&listener);
	//To avoid races, call CCBs on all factories for this.
	for(auto j : state.commands)
		listener.create(*this, j.first, *j.second);
}

void set::drop_callback(set::listener& listener) throw()
{
	threads::arlock h(get_cmd_lock());
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	if(state->callbacks.count(&listener)) {
		//To avoid races, call DCBs on all factories for this.
		for(auto j : state->commands)
			listener.destroy(*this, j.first);
		state->callbacks.erase(&listener);
	}
}

group::group() throw(std::bad_alloc)
	: _listener(*this)
{
	oom_panic_routine = default_oom_panic;
	output = &std::cerr;
	//The builtin commands.
	builtin[0] = new run_script(*this, output);
}

group::~group() throw()
{
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	threads::arlock h(get_cmd_lock());

	//Notify all bases that base group died.
	//Builtin commands delete themselves on parent group dying.
	for(auto i : state->commands)
		i.second->group_died();

	//Drop all callbacks.
	for(auto i : state->set_handles)
		i->drop_callback(_listener);
	//We assume all bases that need destroying have already been destroyed.
	group_internal_t::clear(this);
}

void group::invoke(const std::string& cmd) throw()
{
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	try {
		std::string cmd2 = strip_CR(cmd);
		if(cmd2 == "?") {
			//The special ? command.
			threads::arlock lock(get_cmd_lock());
			for(auto i : state->commands)
				(*output) << i.first << ": " << i.second->get_short_help() << std::endl;
			return;
		}
		if(firstchar(cmd2) == '?') {
			//?command.
			threads::arlock lock(get_cmd_lock());
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
			if(!state->commands.count(rcmd))
				(*output) << "Unknown command '" << rcmd << "'" << std::endl;
			else
				(*output) << state->commands[rcmd]->get_long_help() << std::endl;
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
				threads::arlock lock(get_cmd_lock());
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
				threads::arlock lock(get_cmd_lock());
				if(!state->commands.count(rcmd)) {
					(*output) << "Unknown command '" << rcmd << "'" << std::endl;
					return;
				}
				cmdh = state->commands[rcmd];
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
			(*output) << "Error[" << cmd2 << "]: " << e.what() << std::endl;
			command_stack.erase(cmd2);
			return;
		}
	} catch(std::bad_alloc& e) {
		oom_panic_routine();
	}
}

std::set<std::string> group::get_aliases() throw(std::bad_alloc)
{
	threads::arlock lock(get_cmd_lock());
	std::set<std::string> r;
	for(auto i : aliases)
		r.insert(i.first);
	return r;
}

std::string group::get_alias_for(const std::string& aname) throw(std::bad_alloc)
{
	threads::arlock lock(get_cmd_lock());
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
	threads::arlock lock(get_cmd_lock());
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
	threads::arlock h(get_cmd_lock());
	auto& state = group_internal_t::get(this);
	if(state.commands.count(name))
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
	state.commands[name] = &cmd;
}

void group::do_unregister(const std::string& name, base& cmd) throw(std::bad_alloc)
{
	threads::arlock h(get_cmd_lock());
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	if(!state->commands.count(name) || state->commands[name] != &cmd) return;
	state->commands.erase(name);
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
	threads::arlock h(get_cmd_lock());
	auto& state = group_internal_t::get(this);
	if(state.set_handles.count(&s)) return;
	try {
		state.set_handles.insert(&s);
		s.add_callback(_listener);
	} catch(...) {
		state.set_handles.erase(&s);
	}
}

void group::drop_set(set& s) throw()
{
	threads::arlock h(get_cmd_lock());
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	//Drop the callback. This unregisters all.
	s.drop_callback(_listener);
	state->set_handles.erase(&s);
}

group::listener::listener(group& _grp)
	: grp(_grp)
{
}

group::listener::~listener()
{
}

void group::listener::create(set& s, const std::string& name, factory_base& cmd)
{
	threads::arlock h(get_cmd_lock());
	cmd.make(grp);
}

void group::listener::destroy(set& s, const std::string& name)
{
	threads::arlock h(get_cmd_lock());
	auto state = group_internal_t::get_soft(&grp);
	if(!state) return;
	state->commands.erase(name);
}

void group::listener::kill(set& s)
{
	threads::arlock h(get_cmd_lock());
	auto state = group_internal_t::get_soft(&grp);
	if(!state) return;
	state->set_handles.erase(&s);
}

template<>
void invoke_fn(std::function<void(const std::string& args)> fn, const std::string& args)
{
	fn(args);
}

template<>
void invoke_fn(std::function<void()> fn, const std::string& args)
{
	if(args != "")
		throw std::runtime_error("This command does not take arguments");
	fn();
}

template<>
void invoke_fn(std::function<void(struct arg_filename a)> fn, const std::string& args)
{
	if(args == "")
		throw std::runtime_error("Filename required");
	arg_filename b;
	b.v = args;
	fn(b);
}
}
