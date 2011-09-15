#include "keymapper.hpp"
#include <stdexcept>
#include "lua.hpp"
#include <list>
#include <map>
#include <set>
#include "misc.hpp"
#include "memorymanip.hpp"
#include "fieldsplit.hpp"
#include "command.hpp"

namespace
{
	class bind_key : public command
	{
	public:
		bind_key() throw(std::bad_alloc) : command("bind-key") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			std::string mod, modmask, keyname, command;
			tokensplitter t(args);
			std::string mod_or_key = t;
			if(mod_or_key.find_first_of("/") < mod_or_key.length()) {
				//Mod field.
				size_t split = mod_or_key.find_first_of("/");
				mod = mod_or_key.substr(0, split);
				modmask = mod_or_key.substr(split + 1);
				mod_or_key = static_cast<std::string>(t);
			}
			if(mod_or_key == "")
				throw std::runtime_error("Expected optional modifiers and key");
			keyname = mod_or_key;
			command = t.tail();
			if(command == "")
				throw std::runtime_error("Expected command");
			if(!win)
				throw std::runtime_error("Bindings require graphics context");
			else
				win->bind(mod, modmask, keyname, command);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Bind a (pseudo-)key"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: bind-key [<mod>/<modmask>] <key> <command>\n"
				"Bind command to specified key (with specified modifiers)\n";
		}
	} bindkey;

	class unbind_key : public command
	{
	public:
		unbind_key() throw(std::bad_alloc) : command("unbind-key") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			std::string mod, modmask, keyname, command;
			tokensplitter t(args);
			std::string mod_or_key = t;
			if(mod_or_key.find_first_of("/") < mod_or_key.length()) {
				//Mod field.
				size_t split = mod_or_key.find_first_of("/");
				mod = mod_or_key.substr(0, split);
				modmask = mod_or_key.substr(split + 1);
				mod_or_key = static_cast<std::string>(t);
			}
			if(mod_or_key == "")
				throw std::runtime_error("Expected optional modifiers and key");
			keyname = mod_or_key;
			command = t.tail();
			if(command != "")
				throw std::runtime_error("Unexpected argument");
			if(!win)
				throw std::runtime_error("Bindings require graphics context");
			else
				win->unbind(mod, modmask, keyname);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Unbind a (pseudo-)key"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: unbind-key [<mod>/<modmask>] <key>\n"
				"Unbind specified key (with specified modifiers)\n";
		}
	} unbindkey;

	class shbind_key : public command
	{
	public:
		shbind_key() throw(std::bad_alloc) : command("show-bindings") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			if(!win)
				throw std::runtime_error("Bindings require graphics context");
			else
				win->dumpbindings();
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Show active bindings"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: show-bindings\n"
				"Show bindings that are currently active.\n";
		}
	} shbindkey;
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
