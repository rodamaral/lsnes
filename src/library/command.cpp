#include "command.hpp"
#include "globalwrap.hpp"
#include "integer-pool.hpp"
#include "minmax.hpp"
#include "register-queue.hpp"
#include "stateobject.hpp"
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

	struct set_callbacks
	{
		std::function<void(set& s, const std::string& name, factory_base& cmd)> ccb;
		std::function<void(set& s, const std::string& name)> dcb;
		std::function<void(set& s)> tcb;
	};

	threads::lock* global_lock;

	struct set_internal
	{
		threads::lock lock;
		std::map<uint64_t, set_callbacks> callbacks;
		integer_pool pool;
		std::map<std::string, factory_base*> commands;
	};

	struct group_internal
	{
		threads::lock lock;
		std::map<std::string, base*> commands;
		std::map<set*, uint64_t> set_handles;
	};

	typedef stateobject::type<set, set_internal> set_internal_t;
	typedef stateobject::type<group, group_internal> group_internal_t;
}

threads::lock& get_cmd_lock()
{
	if(!global_lock) global_lock = new threads::lock;
	return *global_lock;
}

void factory_base::_factory_base(set& _set, const std::string& cmd) throw(std::bad_alloc)
{
	threads::alock h(get_cmd_lock());
	in_set = &_set;
	in_set->do_register_unlocked(commandname = cmd, *this);
}

factory_base::~factory_base() throw()
{
	threads::alock h(get_cmd_lock());
	if(in_set)
		in_set->do_unregister_unlocked(commandname, *this);
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
	if(!is_dynamic) {
		threads::alock h(get_cmd_lock());
		in_group->do_register_unlocked(commandname = cmd, *this);
	} else {
		in_group->do_register_unlocked(commandname = cmd, *this);
	}
}

base::~base() throw()
{
	if(!is_dynamic) {
		threads::alock h(get_cmd_lock());
		if(in_group)
			in_group->do_unregister_unlocked(commandname, *this);
	} else {
		if(in_group)
			in_group->do_unregister_unlocked(commandname, *this);
	}
}

void base::group_died() throw()
{
	//The lock is held by assumption.
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
	threads::alock h(get_cmd_lock());
	//Call all DCBs on all factories.
	for(auto i : state->commands)
		for(auto j : state->callbacks)
			j.second.dcb(*this, i.first);
	//Call all TCBs.
	for(auto j : state->callbacks)
		j.second.tcb(*this);
	//Notify all factories that base set died.
	for(auto i : state->commands)
		i.second->set_died();
	//We assume factories look after themselves, so we don't destroy those.
	set_internal_t::clear(this);
}

void set::do_register_unlocked(const std::string& name, factory_base& cmd) throw(std::bad_alloc)
{
	auto& state = set_internal_t::get(this);
	if(state.commands.count(name)) {
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
		return;
	}
	state.commands[name] = &cmd;
	//Call all CCBs on this.
	for(auto i : state.callbacks)
		i.second.ccb(*this, name, cmd);
}

void set::do_unregister_unlocked(const std::string& name, factory_base& cmd) throw(std::bad_alloc)
{
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	if(!state->commands.count(name) || state->commands[name] != &cmd) return; //Not this.
	state->commands.erase(name);
	//Call all DCBs on this.
	for(auto i : state->callbacks)
		i.second.dcb(*this, name);
}

uint64_t set::add_callback_unlocked(std::function<void(set& s, const std::string& name, factory_base& cmd)> ccb,
	std::function<void(set& s, const std::string& name)> dcb, std::function<void(set& s)> tcb)
	throw(std::bad_alloc)
{
	auto& state = set_internal_t::get(this);
	set_callbacks cb;
	cb.ccb = ccb;
	cb.dcb = dcb;
	cb.tcb = tcb;
	uint64_t i = state.pool();
	try {
		state.callbacks[i] = cb;
	} catch(...) {
		state.pool(i);
		throw;
	}
	//To avoid races, call CCBs on all factories for this.
	for(auto j : state.commands)
		ccb(*this, j.first, *j.second);
	return i;
}

void set::drop_callback_unlocked(uint64_t handle) throw()
{
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	if(state->callbacks.count(handle)) {
		//To avoid races, call DCBs on all factories for this.
		for(auto j : state->commands)
			state->callbacks[handle].dcb(*this, j.first);
		state->callbacks.erase(handle);
		state->pool(handle);
	}
}

std::map<std::string, factory_base*> set::get_commands_unlocked()
{
	auto state = set_internal_t::get_soft(this);
	if(!state) return std::map<std::string, factory_base*>();
	return state->commands;
}

group::group() throw(std::bad_alloc)
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
	threads::alock h(get_cmd_lock());

	//Notify all bases that base group died.
	//Builtin commands delete themselves on parent group dying.
	for(auto i : state->commands)
		i.second->group_died();

	//Drop all callbacks.
	for(auto i : state->set_handles)
		i.first->drop_callback_unlocked(i.second);
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
			threads::alock lock(get_cmd_lock());
			for(auto i : state->commands)
				(*output) << i.first << ": " << i.second->get_short_help() << std::endl;
			return;
		}
		if(firstchar(cmd2) == '?') {
			//?command.
			threads::alock lock(get_cmd_lock());
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
				threads::alock lock(get_cmd_lock());
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
				threads::alock lock(get_cmd_lock());
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
	threads::alock lock(get_cmd_lock());
	std::set<std::string> r;
	for(auto i : aliases)
		r.insert(i.first);
	return r;
}

std::string group::get_alias_for(const std::string& aname) throw(std::bad_alloc)
{
	threads::alock lock(get_cmd_lock());
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
	threads::alock lock(get_cmd_lock());
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

void group::do_register_unlocked(const std::string& name, base& cmd) throw(std::bad_alloc)
{
	auto& state = group_internal_t::get(this);
	if(state.commands.count(name))
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
	state.commands[name] = &cmd;
}

void group::do_unregister_unlocked(const std::string& name, base& cmd) throw(std::bad_alloc)
{
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

void group::add_set_unlocked(set& s) throw(std::bad_alloc)
{
	auto& state = group_internal_t::get(this);
	if(state.set_handles.count(&s))
		return;
	state.set_handles[&s] = 0xFFFFFFFFFFFFFFFF;
	try {
		state.set_handles[&s] = s.add_callback_unlocked(
			[this](set& s, const std::string& name, factory_base& cmd) {
				cmd.make(*this);
			}, [this](set& s, const std::string& name) { 
				auto state = group_internal_t::get_soft(this);
				if(!state) return;
				state->commands.erase(name);
			}, [this](set& s) {
				auto state = group_internal_t::get_soft(this);
				if(!state) return;
				state->set_handles.erase(&s);
			}
		);
	} catch(...) {
		state.set_handles.erase(&s);
	}
}

void group::drop_set_unlocked(set& s) throw()
{
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	if(!state->set_handles.count(&s))
		return;
	//Drop the callback. This unregisters all.
	s.drop_callback_unlocked(state->set_handles[&s]);
	state->set_handles.erase(&s);
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
