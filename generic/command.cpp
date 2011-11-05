#include "command.hpp"
#include "misc.hpp"
#include "zip.hpp"
#include "globalwrap.hpp"
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
				if(o)
					delete o;
				throw;
			}
		});

	function_ptr_command<> show_aliases("show-aliases", "show aliases",
		"Syntax: show-aliases\nShow expansions of all aliases\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : aliases)
				for(auto j = i.second.begin(); j != i.second.end(); j++)
					messages << "alias " << i.first << " " << *j << std::endl;
		});

	function_ptr_command<tokensplitter&> unalias_command("unalias-command", "unalias a command",
		"Syntax: unalias-command <aliasname>\nClear expansion of alias <aliasname>\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			std::string aliasname = t;
			if(t)
				throw std::runtime_error("This command only takes one argument");
			if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
				throw std::runtime_error("Illegal alias name");
			aliases[aliasname].clear();
			messages << "Command '" << aliasname << "' unaliased" << std::endl;
		});

	function_ptr_command<tokensplitter&> alias_command("alias-command", "alias a command",
		"Syntax: alias-command <aliasname> <command>\nAppend <command> to expansion of alias <aliasname>\n"
		"Valid alias names can't be empty nor start with '*' or '?'\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			std::string aliasname = t;
			std::string command = t.tail();
			if(command == "")
				throw std::runtime_error("Alias name and command needed");
			if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
				throw std::runtime_error("Illegal alias name");
			aliases[aliasname].push_back(command);
			messages << "Command '" << aliasname << "' aliased to '" << command << "'" << std::endl;
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
		std::string cmd2 = cmd;
		if(cmd2[cmd2.length() - 1] == '\r')
			cmd2 = cmd2.substr(0, cmd2.length() - 1);
		if(cmd2 == "?") {
			//The special ? command.
			for(auto i : commands())
				messages << i.first << ": " << i.second->get_short_help() << std::endl;
			return;
		}
		if(cmd2.length() > 1 && cmd2[0] == '?') {
			//?command.
			size_t split = cmd2.find_first_of(" \t");
			std::string rcmd;
			if(split >= cmd2.length())
				rcmd = cmd2.substr(1);
			else
				rcmd = cmd2.substr(1, split - 1);
			if(rcmd.length() > 0 && rcmd[0] != '*') {
				//This may be an alias.
				std::string aname = cmd2.substr(1);
				if(aliases.count(aname)) {
					//Yup.
					messages << aname << " is an alias for: " << std::endl;
					size_t j = 0;
					for(auto i : aliases[aname])
						messages << "#" + (++j) << ": " << i << std::endl;
					return;
				}
			}
			if(rcmd.length() > 0 && rcmd[0] == '*')
				rcmd = rcmd.substr(1);
			if(!commands().count(rcmd)) {
				if(rcmd != "")
					messages << "Unknown command '" << rcmd << "'" << std::endl;
				return;
			}
			messages << commands()[rcmd]->get_long_help() << std::endl;
			return;
		}
		bool may_be_alias_expanded = true;
		if(cmd2.length() > 0 && cmd2[0] == '*') {
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
			std::string rcmd;
			if(split >= cmd2.length())
				rcmd = cmd2;
			else
				rcmd = cmd2.substr(0, split);
			split = cmd2.find_first_not_of(" \t", split);
			std::string args;
			if(split < cmd2.length())
				args = cmd2.substr(split);
			command* cmdh = NULL;
			if(commands().count(rcmd))
				cmdh = commands()[rcmd];
			if(!cmdh) {
				messages << "Unknown command '" << rcmd << "'" << std::endl;
				return;
			}
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
		size_t nextsplit = avalue.find_first_of("\n");
		if(nextsplit >= avalue.length())
			nextsplit = avalue.length();
		std::string x = avalue.substr(avitr, nextsplit - avitr);
		if(x.length() > 0 && x[x.length() - 1] == '\r')
			x = x.substr(0, x.length() - 1);
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


tokensplitter::tokensplitter(const std::string& _line) throw(std::bad_alloc)
{
	line = _line;
	position = 0;
}

tokensplitter::operator bool() throw()
{
	return (position < line.length());
}

tokensplitter::operator std::string() throw(std::bad_alloc)
{
	size_t nextp, oldp = position;
	nextp = line.find_first_of(" \t", position);
	if(nextp > line.length()) {
		position = line.length();
		return line.substr(oldp);
	} else {
		position = nextp;
		while(position < line.length() && (line[position] == ' ' || line[position] == '\t'))
			position++;
		return line.substr(oldp, nextp - oldp);
	}
}

std::string tokensplitter::tail() throw(std::bad_alloc)
{
	return line.substr(position);
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

template<>
void invoke_command_fn(void (*fn)(tokensplitter& a), const std::string& args)
{
	tokensplitter t(args);
	fn(t);
}
