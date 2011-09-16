#include "command.hpp"
#include "misc.hpp"
#include "zip.hpp"
#include "window.hpp"
#include <set>
#include <map>

namespace
{
	std::map<std::string, command*>* commands;
	std::set<std::string> command_stack;
	std::map<std::string, std::list<std::string>> aliases;

	function_ptr_command run_script("run-script", "run file as a script",
		"Syntax: run-script <file>\nRuns file <file> just as it would have been entered in the command line\n",
		[](const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error) {
			if(args == "")
				throw std::runtime_error("Filename needed");
			std::istream* o = NULL;
			try {
				o = &open_file_relative(args, "");
				out(win) << "Running '" << args << "'" << std::endl;
				std::string line;
				while(std::getline(*o, line))
					command::invokeC(line, win);
				delete o;
			} catch(std::exception& e) {
				if(o)
					delete o;
				throw;
			}
		});

	function_ptr_command show_aliases("show-aliases", "show aliases",
		"Syntax: show-aliases\nShow expansions of all aliases\n",
		[](const std::string& args, std::ostream& os) throw(std::bad_alloc, std::runtime_error) {
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			for(auto i = aliases.begin(); i != aliases.end(); i++)
				for(auto j = i->second.begin(); j != i->second.end(); j++)
					os << "alias " << i->first << " " << *j << std::endl;
		});

	function_ptr_command unalias_command("unalias-command", "unalias a command",
		"Syntax: unalias-command <aliasname>\nClear expansion of alias <aliasname>\n",
		[](const std::string& args, std::ostream& os) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			std::string aliasname = t;
			if(t)
				throw std::runtime_error("This command only takes one argument");
			if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
				throw std::runtime_error("Illegal alias name");
			aliases[aliasname].clear();
			os << "Command '" << aliasname << "' unaliased" << std::endl;
		});

	function_ptr_command alias_command("alias-command", "alias a command",
		"Syntax: alias-command <aliasname> <command>\nAppend <command> to expansion of alias <aliasname>\n"
		"Valid alias names can't be empty nor start with '*' or '?'\n",
		[](const std::string& args, std::ostream& os) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			std::string aliasname = t;
			std::string command = t.tail();
			if(command == "")
				throw std::runtime_error("Alias name and command needed");
			if(aliasname.length() == 0 || aliasname[0] == '?' || aliasname[0] == '*')
				throw std::runtime_error("Illegal alias name");
			aliases[aliasname].push_back(command);
			os << "Command '" << aliasname << "' aliased to '" << command << "'" << std::endl;
		});
}

command::command(const std::string& cmd) throw(std::bad_alloc)
{
	if(!commands)
		commands = new std::map<std::string, command*>();
	if(commands->count(cmd))
		std::cerr << "WARNING: Command collision for " << cmd << "!" << std::endl;
	(*commands)[commandname = cmd] = this;
}

command::~command() throw()
{
	if(!commands)
		return;
	commands->erase(commandname);
}

void command::invokeC(const std::string& cmd, window* win) throw()
{
	try {
		if(command_stack.count(cmd)) {
			out(win) << "Can not invoke recursively: " << cmd << std::endl;
			return;
		}
		command_stack.insert(cmd);
		std::string cmd2 = cmd;
		if(cmd2 == "?") {
			//The special ? command.
			if(commands) {
				for(auto i = commands->begin(); i != commands->end(); ++i)
					out(win) << i->first << ": " << i->second->get_short_help() << std::endl;
			}
			command_stack.erase(cmd);
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
					out(win) << aname << " is an alias for: " << std::endl;
					size_t j = 0;
					for(auto i = aliases[aname].begin(); i != aliases[aname].end(); ++i, ++j)
						out(win) << "#" + (j + 1) << ": " << *i << std::endl;
					command_stack.erase(cmd);
					return;
				}
			}
			if(rcmd.length() > 0 && rcmd[0] == '*')
				rcmd = rcmd.substr(1);
			if(!commands || !commands->count(rcmd)) {
				if(rcmd != "")
					out(win) << "Unknown command '" << rcmd << "'" << std::endl;
				command_stack.erase(cmd);
				return;
			}
			out(win) << (*commands)[rcmd]->get_long_help() << std::endl;
			command_stack.erase(cmd);
			return;
		}
		bool may_be_alias_expanded = true;
		if(cmd2.length() > 0 && cmd2[0] == '*') {
			may_be_alias_expanded = false;
			cmd2 = cmd2.substr(1);
		}
		if(may_be_alias_expanded && aliases.count(cmd2)) {
			for(auto i = aliases[cmd2].begin(); i != aliases[cmd2].end(); ++i)
				invokeC(*i, win);
			command_stack.erase(cmd);
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
			if(commands && commands->count(rcmd))
				cmdh = (*commands)[rcmd];
			if(!cmdh) {
				out(win) << "Unknown command '" << rcmd << "'" << std::endl;
				command_stack.erase(cmd);
				return;
			}
			cmdh->invoke(args, win);
			command_stack.erase(cmd);
			return;
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::exception& e) {
			out(win) << "Error: " << e.what() << std::endl;
			command_stack.erase(cmd);
			return;
		}
	} catch(std::bad_alloc& e) {
		OOM_panic(win);
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

function_ptr_command::function_ptr_command(const std::string& name, const std::string& _description,
	const std::string& _help, void (*_fn)(const std::string& arguments, window* win)) throw(std::bad_alloc)
	: command(name)
{
	description = _description;
	help = _help;
	fn = _fn;
	fn2 = NULL;
}

function_ptr_command::function_ptr_command(const std::string& name, const std::string& _description,
	const std::string& _help, void (*_fn)(const std::string& arguments, std::ostream& win)) throw(std::bad_alloc)
	: command(name)
{
	description = _description;
	help = _help;
	fn = NULL;
	fn2 = _fn;
}

void function_ptr_command::invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
{
	if(fn)
		fn(args, win);
	else if(fn2)
		fn2(args, out(win));
}

std::string function_ptr_command::get_short_help() throw(std::bad_alloc)
{
	return description;
}

std::string function_ptr_command::get_long_help() throw(std::bad_alloc)
{
	return help;
}

function_ptr_command::~function_ptr_command() throw()
{
}