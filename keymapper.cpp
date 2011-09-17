#include "keymapper.hpp"
#include <stdexcept>
#include "lua.hpp"
#include "window.hpp"
#include <list>
#include <map>
#include <set>
#include "misc.hpp"
#include "memorymanip.hpp"
#include "command.hpp"

namespace
{
	function_ptr_command<tokensplitter&> bind_key("bind-key", "Bind a (pseudo-)key",
		"Syntax: bind-key [<mod>/<modmask>] <key> <command>\nBind command to specified key (with specified "
		" modifiers)\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			std::string mod, modmask, keyname, command;
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
			window::bind(mod, modmask, keyname, command);
		});

	function_ptr_command<tokensplitter&> unbind_key("unbind-key", "Unbind a (pseudo-)key",
		"Syntax: unbind-key [<mod>/<modmask>] <key>\nUnbind specified key (with specified modifiers)\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			std::string mod, modmask, keyname, command;
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
			window::unbind(mod, modmask, keyname);
		});

	function_ptr_command<> show_bindings("show-bindings", "Show active bindings",
		"Syntax: show-bindings\nShow bindings that are currently active.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::dumpbindings();
		});
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
