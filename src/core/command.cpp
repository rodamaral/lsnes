#include "core/command.hpp"
#include "core/globalwrap.hpp"
#include "core/misc.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <set>
#include <map>

namespace
{
	globalwrap<std::map<std::string, command*>> commands;
	std::set<std::string> command_stack;
	std::map<std::string, std::list<std::string>> aliases;

	function_ptr_command<arg_filename> run_script("run-script", "run file as a script",
		"Syntax: run-script <file>\nRuns file <file> just as it would have been entered in the command line\n",
		[](arg_filename filename) throw(std::bad_alloc, std::runtime_error) {
			std::istream* o = NULL;
			try {
				o = &open_file_relative(filename, "");
				messages << "Running '" << std::string(filename) << "'" << std::endl;
				std::string line;
				while(std::getline(*o, line))
					command::invokeC(line);
				delete o;
			} catch(std::exception& e) {
				delete o;
				throw;
			}
		});

	function_ptr_command<> show_aliases("show-aliases", "show aliases",
		"Syntax: show-aliases\nShow expansions of all aliases\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : aliases)
				for(auto j : i.second)
					messages << "alias " << i.first << " " << j << std::endl;
		});

	function_ptr_command<const std::string&> unalias_command("unalias-command", "unalias a command",
		"Syntax: unalias-command <aliasname>\nClear expansion of alias <aliasname>\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]*", t, "This command only takes one argument");
			if(!command::valid_alias_name(r[1]))
				throw std::runtime_error("Illegal alias name");
			aliases[r[1]].clear();
			messages << "Command '" << r[1] << "' unaliased" << std::endl;
		});

	function_ptr_command<const std::string&> alias_command("alias-command", "alias a command",
		"Syntax: alias-command <aliasname> <command>\nAppend <command> to expansion of alias <aliasname>\n"
		"Valid alias names can't be empty nor start with '*' or '?'\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]+([^ \t].*)", t, "Alias name and command needed");
			if(!command::valid_alias_name(r[1]))
				throw std::runtime_error("Illegal alias name");
			aliases[r[1]].push_back(r[2]);
			messages << "Command '" << r[1] << "' aliased to '" << r[2] << "'" << std::endl;
		});
}

command::command(const std::string& cmd) throw(std::bad_alloc)
{
	if(commands().count(cmd))
		std::cerr << "WARNING: Command collision for " << cmd << "!" << std::endl;
	commands()[commandname = cmd] = this;
}

command::~command() throw()
{
	commands().erase(commandname);
}

void command::invokeC(const std::string& cmd) throw()
{
	try {
		std::string cmd2 = strip_CR(cmd);
		if(cmd2 == "?") {
			//The special ? command.
			for(auto i : commands())
				messages << i.first << ": " << i.second->get_short_help() << std::endl;
			return;
		}
		if(firstchar(cmd2) == '?') {
			//?command.
			std::string rcmd = cmd2.substr(1, min(cmd2.find_first_of(" \t"), cmd2.length()));
			if(firstchar(rcmd) != '*') {
				//This may be an alias.
				if(aliases.count(rcmd)) {
					//Yup.
					messages << rcmd << " is an alias for: " << std::endl;
					size_t j = 0;
					for(auto i : aliases[rcmd])
						messages << "#" << (++j) << ": " << i << std::endl;
					return;
				}
			} else
				rcmd = rcmd.substr(1);
			if(!commands().count(rcmd))
				messages << "Unknown command '" << rcmd << "'" << std::endl;
			else
				messages << commands()[rcmd]->get_long_help() << std::endl;
			return;
		}
		bool may_be_alias_expanded = true;
		if(firstchar(cmd2) == '*') {
			may_be_alias_expanded = false;
			cmd2 = cmd2.substr(1);
		}
		if(may_be_alias_expanded && aliases.count(cmd2)) {
			for(auto i : aliases[cmd2])
				invokeC(i);
			return;
		}
		try {
			size_t split = cmd2.find_first_of(" \t");
			std::string rcmd = cmd2.substr(0, min(split, cmd2.length()));
			std::string args = cmd2.substr(min(cmd2.find_first_not_of(" \t", split), cmd2.length()));
			command* cmdh = NULL;
			if(!commands().count(rcmd)) {
				messages << "Unknown command '" << rcmd << "'" << std::endl;
				return;
			}
			cmdh = commands()[rcmd];
			if(command_stack.count(cmd2))
				throw std::runtime_error("Recursive command invocation");
			command_stack.insert(cmd2);
			cmdh->invoke(args);
			command_stack.erase(cmd2);
			return;
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			messages << "Error: " << e.what() << std::endl;
			command_stack.erase(cmd2);
			return;
		}
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

std::string command::get_short_help() throw(std::bad_alloc)
{
	return "No description available";
}

std::string command::get_long_help() throw(std::bad_alloc)
{
	return "No help available on command " + commandname;
}

std::set<std::string> command::get_aliases() throw(std::bad_alloc)
{
	std::set<std::string> r;
	for(auto i : aliases)
		r.insert(i.first);
	return r;
}

std::string command::get_alias_for(const std::string& aname) throw(std::bad_alloc)
{
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

void command::set_alias_for(const std::string& aname, const std::string& avalue) throw(std::bad_alloc)
{
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

bool command::valid_alias_name(const std::string& aliasname) throw(std::bad_alloc)
{
	if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
		return false;
	if(aliasname.find_first_of(" \t") < aliasname.length())
		return false;
	return true;
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
