#include "keymapper.hpp"
#include <stdexcept>
#include "lua.hpp"
#include <list>
#include <map>
#include <set>
#include "misc.hpp"
#include "zip.hpp"
#include "videodumper2.hpp"
#include "settings.hpp"
#include "memorymanip.hpp"
#include "fieldsplit.hpp"

namespace
{
	std::map<std::string, std::list<std::string>> aliases;

	void handle_alias(std::string cmd, window* win)
	{
		std::string syntax = "Syntax: alias-command <alias> <command>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string aliasname = t;
		std::string command = t.tail();
		if(command == "")
			throw std::runtime_error(syntax);
		aliases[aliasname].push_back(command);
		out(win) << "Command '" << aliasname << "' aliased to '" << command << "'" << std::endl;
	}

	void handle_unalias(std::string cmd, window* win) throw(std::bad_alloc, std::runtime_error)
	{
		std::string syntax = "Syntax: unalias-command <alias>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string aliasname = t;
		if(aliasname == "" || t.tail() != "")
			throw std::runtime_error(syntax);
		aliases.erase(aliasname);
		out(win) << "Removed alias '" << aliasname << "'" << std::endl;
	}

	void handle_aliases(window* win) throw(std::bad_alloc, std::runtime_error)
	{
		for(auto i = aliases.begin(); i != aliases.end(); i++)
			for(auto j = i->second.begin(); j != i->second.end(); j++)
				out(win) << "alias " << i->first << " " << *j << std::endl;
	}

	void handle_run(std::string cmd, window* win, aliasexpand_commandhandler& cmdh,
		std::set<std::string>& recursive_commands) throw(std::bad_alloc, std::runtime_error)
	{
		std::string syntax = "Syntax: run-script <file>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string file = t.tail();
		if(file == "")
			throw std::runtime_error(syntax);
		std::istream* o = NULL;
		try {
			o = &open_file_relative(file, "");
			out(win) << "Running '" << file << "'" << std::endl;
			std::string line;
			while(std::getline(*o, line))
				cmdh.docommand(line, win, recursive_commands);
			delete o;
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::exception& e) {
			out(win) << "Error running script: " << e.what() << std::endl;
			if(o)
				delete o;
		}
	}

	void handle_set(std::string cmd, window* win) throw(std::bad_alloc, std::runtime_error)
	{
		std::string syntax = "Syntax: set-setting <setting> [<value>]";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string settingname = t;
		std::string settingvalue = t.tail();
		if(settingname == "")
			throw std::runtime_error(syntax);
		setting_set(settingname, settingvalue);
		out(win) << "Setting '" << settingname << "' set to '" << settingvalue << "'" << std::endl;
	}

	void handle_unset(std::string cmd, window* win) throw(std::bad_alloc, std::runtime_error)
	{
		std::string syntax = "Syntax: unset-setting <setting>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string settingname = t;
		if(settingname == "" || t.tail() != "")
			throw std::runtime_error(syntax);
		setting_blank(settingname);
		out(win) << "Setting '" << settingname << "' blanked" << std::endl;
	}

	void handle_get(std::string cmd, window* win) throw(std::bad_alloc, std::runtime_error)
	{
		std::string syntax = "Syntax: get-setting <setting>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string settingname = t;
		if(settingname == "" || t.tail() != "")
			throw std::runtime_error(syntax);
		if(!setting_isblank(settingname))
			out(win) << "Setting '" << settingname << "' has value '" << setting_get(settingname)
				<< "'" << std::endl;
		else
			out(win) << "Setting '" << settingname << "' unset" << std::endl;
	}

	struct binding_request
	{
		binding_request(std::string& cmd) throw(std::bad_alloc, std::runtime_error);
		bool polarity;
		std::string mod;
		std::string modmask;
		std::string keyname;
		std::string command;
	};

	struct set_raii
	{
		set_raii(std::set<std::string>& _set, const std::string& _foo) throw(std::bad_alloc)
			: set(_set), foo(_foo)
		{
			set.insert(foo);
		}

		~set_raii() throw()
		{
			set.erase(foo);
		}

		std::set<std::string>& set;
		std::string foo;
	};
}

aliasexpand_commandhandler::aliasexpand_commandhandler() throw()
{
}

aliasexpand_commandhandler::~aliasexpand_commandhandler() throw()
{
}

void aliasexpand_commandhandler::docommand(std::string& cmd, window* win) throw(std::bad_alloc)
{
	std::set<std::string> recursive_commands;
	docommand(cmd, win, recursive_commands);
}

void aliasexpand_commandhandler::docommand(std::string& cmd, window* win, std::set<std::string>& recursive_commands)
	throw(std::bad_alloc)
{
	std::string tmp = cmd;
	bool noalias = false;
	if(cmd.length() > 0 && cmd[0] == '*') {
		tmp = cmd.substr(1);
		noalias = true;
	}
	if(recursive_commands.count(tmp)) {
		out(win) << "Not executing '" << tmp << "' recursively" << std::endl;
		return;
	}
	set_raii rce(recursive_commands, tmp);
	try {
		if(is_cmd_prefix(tmp, "set-setting")) {
			handle_set(tmp, win);
			return;
		}
		if(is_cmd_prefix(tmp, "get-setting")) {
			handle_get(tmp, win);
			return;
		}
		if(is_cmd_prefix(tmp, "print-settings")) {
			setting_print_all(win);
			return;
		}
		if(is_cmd_prefix(tmp, "unset-setting")) {
			handle_unset(tmp, win);
			return;
		}
		if(is_cmd_prefix(tmp, "print-keybindings")) {
			if(win)
				win->dumpbindings();
			else
				throw std::runtime_error("Can't list bindings without graphics context");
			return;
		}
		if(is_cmd_prefix(tmp, "print-aliases")) {
			handle_aliases(win);
			return;
		}
		if(is_cmd_prefix(tmp, "alias-command")) {
			handle_alias(tmp, win);
			return;
		}
		if(is_cmd_prefix(tmp, "unalias-command")) {
			handle_unalias(tmp, win);
			return;
		}
		if(is_cmd_prefix(tmp, "run-script")) {
			handle_run(tmp, win, *this, recursive_commands);
			return;
		}
		if(is_cmd_prefix(tmp, "bind-key")) {
			binding_request req(tmp);
			if(win)
				win->bind(req.mod, req.modmask, req.keyname, req.command);
			else
				throw std::runtime_error("Can't bind keys without graphics context");
			return;
		}
		if(is_cmd_prefix(tmp, "unbind-key")) {
			binding_request req(tmp);
			if(win)
				win->unbind(req.mod, req.modmask, req.keyname);
			else
				throw std::runtime_error("Can't unbind keys without graphics context");
			return;
		}
		if(win && win->exec_command(tmp))
			return;
		if(vid_dumper_command(tmp, win))
			return;
		if(memorymanip_command(tmp, win))
			return;
		if(lua_command(tmp, win))
			return;
	} catch(std::bad_alloc& e) {
		OOM_panic(win);
	} catch(std::exception& e) {
		out(win) << "Error: " << e.what() << std::endl;
		return;
	}
	if(!noalias && aliases.count(tmp)) {
		auto& x = aliases[tmp];
		for(auto i = x.begin(); i != x.end(); i++) {
			std::string y = *i;
			docommand(y, win, recursive_commands);
		}
	} else {
		docommand2(tmp, win);
	}
}

std::string fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc)
{
	if(cmd == "")
		return "";
	if(cmd[0] != '+' && polarity)
		return "";
	if(cmd[0] == '+' && !polarity)
		cmd[0] = '-';
	return cmd;
}

binding_request::binding_request(std::string& cmd) throw(std::bad_alloc, std::runtime_error)
{
	if(is_cmd_prefix(cmd, "bind-key")) {
		std::string syntax = "Syntax: bind-key [<mod>/<modmask>] <key> <command>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string mod_or_key = t;
		if(mod_or_key.find_first_of("/") < mod_or_key.length()) {
			//Mod field.
			size_t split = mod_or_key.find_first_of("/");
			mod = mod_or_key.substr(0, split);
			modmask = mod_or_key.substr(split + 1);
			mod_or_key = static_cast<std::string>(t);
		}
		if(mod_or_key == "")
			throw std::runtime_error(syntax);
		keyname = mod_or_key;
		command = t.tail();
		if(command == "")
			throw std::runtime_error(syntax);
		polarity = true;
	} else if(is_cmd_prefix(cmd, "unbind-key")) {
		std::string syntax = "Syntax: unbind-key [<mod>/<modmask>] <key>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string mod_or_key = t;
		if(mod_or_key.find_first_of("/") < mod_or_key.length()) {
			//Mod field.
			size_t split = mod_or_key.find_first_of("/");
			mod = mod_or_key.substr(0, split);
			modmask = mod_or_key.substr(split + 1);
			mod_or_key = static_cast<std::string>(t);
		}
		if(mod_or_key == "")
			throw std::runtime_error(syntax);
		keyname = mod_or_key;
		if(t.tail() != "")
			throw std::runtime_error(syntax);
		polarity = false;
	} else
		throw std::runtime_error("Not a valid binding request");
}
