#include "command.hpp"
#include "globalwrap.hpp"
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
				o = &open_file_relative(filename, "");
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

	typedef register_queue<group, base> regqueue_t;
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
}

void group::invoke(const std::string& cmd) throw()
{
	try {
		std::string cmd2 = strip_CR(cmd);
		if(cmd2 == "?") {
			//The special ? command.
			umutex_class lock(int_mutex);
			for(auto i : commands)
				(*output) << i.first << ": " << i.second->get_short_help() << std::endl;
			return;
		}
		if(firstchar(cmd2) == '?') {
			//?command.
			umutex_class lock(int_mutex);
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
				umutex_class lock(int_mutex);
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
				umutex_class lock(int_mutex);
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
	umutex_class lock(int_mutex);
	std::set<std::string> r;
	for(auto i : aliases)
		r.insert(i.first);
	return r;
}

std::string group::get_alias_for(const std::string& aname) throw(std::bad_alloc)
{
	umutex_class lock(int_mutex);
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
	umutex_class lock(int_mutex);
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
	umutex_class lock(int_mutex);
	if(commands.count(name))
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
	commands[name] = &cmd;
}

void group::do_unregister(const std::string& name) throw(std::bad_alloc)
{
	umutex_class lock(int_mutex);
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
