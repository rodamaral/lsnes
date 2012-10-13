#include "library/commands.hpp"
#include "library/globalwrap.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include <iostream>
#include <cstdlib>

namespace
{
	struct run_script : public command
	{
		run_script(command_group& group, std::ostream*& _output)
			: command(group, "run-script"), in_group(group), output(_output)
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

		command_group& in_group;
		std::ostream*& output;
	};

	struct show_aliases : public command
	{
		show_aliases(command_group& group, std::ostream*& _output)
			: command(group, "show-aliases"), in_group(group), output(_output)
		{
		}

		~show_aliases() throw()
		{
		}

		void invoke(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
		{
			if(filename != "") {
				(*output) << "Syntax: show-aliases" << std::endl;
				return;
			}
			auto aliases = in_group.get_aliases();
			for(auto i : aliases) {
				std::string acmd = in_group.get_alias_for(i);
				while(acmd != "") {
					std::string j;
					extract_token(acmd, j, "\n");
					if(j != "")
						(*output) << "alias " << i << " " << j << std::endl;
				}
			}
		}

		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Show aliases";
		}

		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: show-aliases\nShow expansions of all aliases\n";
		}

		command_group& in_group;
		std::ostream*& output;
	};

	struct unalias_command : public command
	{
		unalias_command(command_group& group, std::ostream*& _output)
			: command(group, "unalias-command"), in_group(group), output(_output)
		{
		}

		~unalias_command() throw()
		{
		}

		void invoke(const std::string& t) throw(std::bad_alloc, std::runtime_error)
		{
			auto r = regex("([^ \t]+)[ \t]*", t, "This command only takes one argument");
			if(!in_group.valid_alias_name(r[1]))
				throw std::runtime_error("Illegal alias name");
			in_group.set_alias_for(r[1], "");
			(*output) << "Command '" << r[1] << "' unaliased" << std::endl;
		}

		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Unalias a command";
		}

		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: unalias-command <aliasname>\nClear expansion of alias <aliasname>\n";
		}

		command_group& in_group;
		std::ostream*& output;
	};

	struct alias_command : public command
	{
		alias_command(command_group& group, std::ostream*& _output)
			: command(group, "alias-command"), in_group(group), output(_output)
		{
		}

		~alias_command() throw()
		{
		}

		void invoke(const std::string& t) throw(std::bad_alloc, std::runtime_error)
		{
			auto r = regex("([^ \t]+)[ \t]+([^ \t].*)", t, "Alias name and command needed");
			if(!in_group.valid_alias_name(r[1]))
				throw std::runtime_error("Illegal alias name");
			std::string tmp = in_group.get_alias_for(r[1]);
			tmp = tmp + r[2] + "\n";
			in_group.set_alias_for(r[1], tmp);
			(*output) << "Command '" << r[1] << "' aliased to '" << r[2] << "'" << std::endl;
		}

		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Alias a command";
		}

		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: alias-command <aliasname> <command>\nAppend <command> to expansion of alias "
				"<aliasname>\nValid alias names can't be empty nor start with '*' or '?'\n";
		}

		command_group& in_group;
		std::ostream*& output;
	};
}

namespace
{
	void default_oom_panic()
	{
		std::cerr << "PANIC: Fatal error, can't continue: Out of memory." << std::endl;
		exit(1);
	}

	struct pending_registration
	{
		command_group* group;
		std::string name;
		command* toreg;
	};

	globalwrap<mutex_class> reg_mutex;
	globalwrap<std::set<command_group*>> ready_groups;
	globalwrap<std::list<pending_registration>> pending_registrations;

	void run_pending_registrations()
	{
		umutex_class m(reg_mutex());
		auto i = pending_registrations().begin();
		while(i != pending_registrations().end()) {
			auto entry = i++;
			if(ready_groups().count(entry->group)) {
				entry->group->register_command(entry->name, *entry->toreg);
				pending_registrations().erase(entry);
			}
		}
	}

	void add_registration(command_group& group, const std::string& name, command& type)
	{
		{
			umutex_class m(reg_mutex());
			if(ready_groups().count(&group)) {
				group.register_command(name, type);
				return;
			}
			pending_registration p;
			p.group = &group;
			p.name = name;
			p.toreg = &type;
			pending_registrations().push_back(p);
		}
		run_pending_registrations();
	}

	void delete_registration(command_group& group, const std::string& name)
	{
		{
			umutex_class m(reg_mutex());
			if(ready_groups().count(&group))
				group.unregister_command(name);
			else {
				auto i = pending_registrations().begin();
				while(i != pending_registrations().end()) {
					auto entry = i++;
					if(entry->group == &group && entry->name == name)
						pending_registrations().erase(entry);
				}
			}
		}
	}

	
}

command::command(command_group& group, const std::string& cmd) throw(std::bad_alloc)
	: in_group(group)
{
	add_registration(in_group, commandname = cmd, *this);
}

command::~command() throw()
{
	delete_registration(in_group, commandname);
}


std::string command::get_short_help() throw(std::bad_alloc)
{
	return "No description available";
}

std::string command::get_long_help() throw(std::bad_alloc)
{
	return "No help available on command " + commandname;
}

command_group::command_group() throw(std::bad_alloc)
{
	oom_panic_routine = default_oom_panic;
	output = &std::cerr;
	{
		umutex_class m(reg_mutex());
		ready_groups().insert(this);
	}
	run_pending_registrations();
	//The builtin commands.
	new run_script(*this, output);
	new show_aliases(*this, output);
	new unalias_command(*this, output);
	new alias_command(*this, output);
}

command_group::~command_group() throw()
{
	{
		umutex_class m(reg_mutex());
		ready_groups().erase(this);
	}
}

void command_group::invoke(const std::string& cmd) throw()
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
			command* cmdh = NULL;
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

std::set<std::string> command_group::get_aliases() throw(std::bad_alloc)
{
	umutex_class lock(int_mutex);
	std::set<std::string> r;
	for(auto i : aliases)
		r.insert(i.first);
	return r;
}

std::string command_group::get_alias_for(const std::string& aname) throw(std::bad_alloc)
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

void command_group::set_alias_for(const std::string& aname, const std::string& avalue) throw(std::bad_alloc)
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

bool command_group::valid_alias_name(const std::string& aliasname) throw(std::bad_alloc)
{
	if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
		return false;
	if(aliasname.find_first_of(" \t") < aliasname.length())
		return false;
	return true;
}

void command_group::register_command(const std::string& name, command& cmd) throw(std::bad_alloc)
{
	umutex_class lock(int_mutex);
	if(commands.count(name))
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
	commands[name] = &cmd;
}

void command_group::unregister_command(const std::string& name) throw(std::bad_alloc)
{
	umutex_class lock(int_mutex);
	commands.erase(name);
}

void command_group::set_output(std::ostream& s)
{
	output = &s;
}

void command_group::set_oom_panic(void (*fn)())
{
	if(fn)
		oom_panic_routine = fn;
	else
		oom_panic_routine = default_oom_panic;
}

template<>
void invoke_command_fn(void (*fn)(const std::string& args), const std::string& args)
{
	fn(args);
}

template<>
void invoke_command_fn(void (*fn)(), const std::string& args)
{
	if(args != "")
		throw std::runtime_error("This command does not take arguments");
	fn();
}

template<>
void invoke_command_fn(void (*fn)(struct arg_filename a), const std::string& args)
{
	if(args == "")
		throw std::runtime_error("Filename required");
	arg_filename b;
	b.v = args;
	fn(b);
}
