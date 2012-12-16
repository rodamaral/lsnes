#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "library/globalwrap.hpp"
#include "core/keymapper.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"

#include <stdexcept>
#include <stdexcept>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <set>

keyboard lsnes_kbd;

std::string fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc)
{
	if(cmd == "" || cmd == "*")
		return "";
	if(cmd[0] != '*') {
		if(cmd[0] != '+' && polarity)
			return "";
		if(cmd[0] == '+' && !polarity)
			cmd[0] = '-';
	} else {
		if(cmd[1] != '+' && polarity)
			return "";
		if(cmd[1] == '+' && !polarity)
			cmd[1] = '-';
	}
	return cmd;
}

namespace
{
	//Return the recursive mutex.
	mutex& kmlock()
	{
		static mutex& m = mutex::aquire_rec();
		return m;
	}

	class kmlock_hold
	{
	public:
		kmlock_hold() { kmlock().lock(); }
		~kmlock_hold() { kmlock().unlock(); }
	private:
		kmlock_hold(const kmlock_hold& k);
		kmlock_hold& operator=(const kmlock_hold& k);
	};
}

namespace
{
	struct triple
	{
		triple(const std::string& _a, const std::string& _b, const std::string& _c)
		{
			a = _a;
			b = _b;
			c = _c;
		}
		std::string a;
		std::string b;
		std::string c;
		bool operator==(const triple& t) const
		{
			bool x = (a == t.a && b == t.b && c == t.c);
			return x;
		}
		bool operator<(const triple& t) const
		{
			bool x = (a < t.a || (a == t.a && b < t.b) || (a == t.a && b == t.b && c < t.c));
			return x;
		}
	};
	struct keybind_data : public keyboard_event_listener
	{
		keyboard_modifier_set mod;
		keyboard_modifier_set modmask;
		keyboard_key* group;
		unsigned subkey;
		std::string command;

		keybind_data() { group = NULL; }
		~keybind_data() throw() { if(group) group->remove_listener(*this); }
		
		void change_group(keyboard_key* ngroup)
		{
			if(group)
				group->remove_listener(*this);
			group = ngroup;
			if(group)
				group->add_listener(*this, false);
		}
		
		void on_key_event(keyboard_modifier_set& modifiers, keyboard_key& key, keyboard_event& event)
		{
			uint32_t x = event.get_change_mask();
			if(!((x >> (2 * subkey + 1)) & 1))
				return;
			bool polarity = ((x >> (2 * subkey)) & 1) != 0;
			if(!modifiers.triggers(mod, modmask))
				return;
			std::string cmd = fixup_command_polarity(command, polarity);
			if(cmd == "")
				return;
			lsnes_cmd.invoke(cmd);
		}
	};

	triple parse_to_triple(const std::string& keyspec)
	{
		triple k("", "", "");
		std::string _keyspec = keyspec;
		size_t split1 = _keyspec.find_first_of("/");
		size_t split2 = _keyspec.find_first_of("|");
		if(split1 >= keyspec.length() || split2 >= keyspec.length() || split1 > split2)
			throw std::runtime_error("Bad keyspec " + keyspec);
		k.a = _keyspec.substr(0, split1);
		k.b = _keyspec.substr(split1 + 1, split2 - split1 - 1);
		k.c = _keyspec.substr(split2 + 1);
		return k;
	}

	std::map<triple, keybind_data*>& keybindings()
	{
		kmlock_hold lck;
		static std::map<triple, keybind_data*> x;
		return x;
	}

	std::pair<keyboard_key*, unsigned> lookup_subkey(const std::string& name) throw(std::bad_alloc,
		std::runtime_error)
	{
		//Try direct lookup first.
		keyboard_key* key = lsnes_kbd.try_lookup_key(name);
		if(key)
			return std::make_pair(key, 0);
		std::string prefix = name;
		char letter = prefix[prefix.length() - 1];
		prefix = prefix.substr(0, prefix.length() - 1);
		key = lsnes_kbd.try_lookup_key(prefix);
		if(!key)
			throw std::runtime_error("Invalid key");
		switch(letter) {
		case '+':
		case 'n':
			return std::make_pair(key, 0);
		case '-':
		case 'e':
			return std::make_pair(key, 1);
		case 's':
			return std::make_pair(key, 2);
		case 'w':
			return std::make_pair(key, 3);
		default:
			throw std::runtime_error("Invalid key");
		}
	}

}

void keymapper::bind(std::string mod, std::string modmask, std::string keyname, std::string command)
	throw(std::bad_alloc, std::runtime_error)
{
	{
		kmlock_hold lck;
		triple k(mod, modmask, keyname);
		keyboard_modifier_set _mod = keyboard_modifier_set::construct(lsnes_kbd, mod);
		keyboard_modifier_set _modmask = keyboard_modifier_set::construct(lsnes_kbd, modmask);
		if(!_mod.valid(_modmask))
			throw std::runtime_error("Invalid modifiers");
		auto g = lookup_subkey(keyname);
		if(!keybindings().count(k)) {
			keybindings()[k] = new keybind_data;
			keybindings()[k]->mod = _mod;
			keybindings()[k]->modmask = _modmask;
			keybindings()[k]->subkey = g.second;
			keybindings()[k]->change_group(g.first);
		}
		keybindings()[k]->command = command;
	}
	inverse_key::notify_update(mod + "/" + modmask + "|" + keyname, command);
}
void keymapper::unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
		std::runtime_error)
{
	{
		kmlock_hold lck;
		triple k(mod, modmask, keyname);
		if(!keybindings().count(k))
			throw std::runtime_error("Key is not bound");
		delete keybindings()[k];
		keybindings().erase(k);
	}
	inverse_key::notify_update(mod + "/" + modmask + "|" + keyname, "");
}

void keymapper::dumpbindings() throw(std::bad_alloc)
{
	kmlock_hold lck;
	for(auto i : keybindings()) {
		messages << "bind-key ";
		if(i.first.a != "" || i.first.b != "")
			messages << i.first.a << "/" << i.first.b << " ";
		messages << i.first.c << " " << i.second->command << std::endl;
	}
}

std::set<std::string> keymapper::get_bindings() throw(std::bad_alloc)
{
	kmlock_hold lck;
	std::set<std::string> r;
	for(auto i : keybindings())
		r.insert(i.first.a + "/" + i.first.b + "|" + i.first.c);
	return r;
}

std::string keymapper::get_command_for(const std::string& keyspec) throw(std::bad_alloc)
{
	kmlock_hold lck;
	triple k("", "", "");
	try {
		k = parse_to_triple(keyspec);
	} catch(std::exception& e) {
		return "";
	}
	if(!keybindings().count(k))
		return "";
	return keybindings()[k]->command;
}

void keymapper::bind_for(const std::string& keyspec, const std::string& cmd) throw(std::bad_alloc, std::runtime_error)
{
	triple k("", "", "");
	k = parse_to_triple(keyspec);
	if(cmd != "")
		bind(k.a, k.b, k.c, cmd);
	else
		unbind(k.a, k.b, k.c);
}


inverse_key::inverse_key(const std::string& command, const std::string& name) throw(std::bad_alloc)
{
	kmlock_hold lck;
	cmd = command;
	oname = name;
	ikeys().insert(this);
	forkey()[cmd] = this;
	//Search the keybindings for matches.
	auto b = keymapper::get_bindings();
	for(auto c : b)
		if(keymapper::get_command_for(c) == cmd)
			addkey(c);
}

inverse_key::~inverse_key()
{
	kmlock_hold lck;
	ikeys().erase(this);
	forkey().erase(cmd);
}

std::set<inverse_key*> inverse_key::get_ikeys() throw(std::bad_alloc)
{
	kmlock_hold lck;
	return ikeys();
}

std::string inverse_key::getname() throw(std::bad_alloc)
{
	kmlock_hold lck;
	return oname;
}

inverse_key* inverse_key::get_for(const std::string& command) throw(std::bad_alloc)
{
	kmlock_hold lck;
	return forkey().count(command) ? forkey()[command] : NULL;
}

std::set<inverse_key*>& inverse_key::ikeys()
{
	kmlock_hold lck;
	static std::set<inverse_key*> x;
	return x;
}

std::map<std::string, inverse_key*>& inverse_key::forkey()
{
	kmlock_hold lck;
	static std::map<std::string, inverse_key*> x;
	return x;
}

std::string inverse_key::get(bool primary) throw(std::bad_alloc)
{
	kmlock_hold lck;
	return primary ? primary_spec : secondary_spec;
}

void inverse_key::clear(bool primary) throw(std::bad_alloc)
{
	kmlock_hold lck;
	if(primary) {
		if(primary_spec != "")
			keymapper::bind_for(primary_spec, "");
		primary_spec = secondary_spec;
		secondary_spec = "";
	} else {
		if(secondary_spec != "")
			keymapper::bind_for(secondary_spec, "");
		secondary_spec = "";
	}
	//Search the keybindings for matches.
	auto b = keymapper::get_bindings();
	for(auto c : b)
		if(keymapper::get_command_for(c) == cmd)
			addkey(c);
}

void inverse_key::set(std::string keyspec, bool primary) throw(std::bad_alloc)
{
	kmlock_hold lck;
	if(keyspec == "") {
		clear(primary);
		return;
	}
	if(!primary && (primary_spec == "" || primary_spec == keyspec))
		primary = true;
	if(primary) {
		if(primary_spec != "")
			keymapper::bind_for(primary_spec, "");
		primary_spec = keyspec;
		keymapper::bind_for(primary_spec, cmd);
	} else {
		if(secondary_spec != "")
			keymapper::bind_for(secondary_spec, "");
		secondary_spec = keyspec;
		keymapper::bind_for(secondary_spec, cmd);
	}
}

void inverse_key::addkey(const std::string& keyspec)
{
	kmlock_hold lck;
	if(primary_spec == "" || primary_spec == keyspec)
		primary_spec = keyspec;
	else if(secondary_spec == "")
		secondary_spec = keyspec;
}

void inverse_key::notify_update(const std::string& keyspec, const std::string& command)
{
	kmlock_hold lck;
	for(auto k : ikeys()) {
		bool u = false;
		if(k->primary_spec == keyspec || k->secondary_spec == keyspec) {
			if(command == "" || command != k->cmd) {
				//Unbound.
				k->primary_spec = "";
				k->secondary_spec = "";
				u = true;
			}
		} else if(command == k->cmd)
			k->addkey(keyspec);
		if(u) {
			auto b = keymapper::get_bindings();
			for(auto c : b)
				if(keymapper::get_command_for(c) == k->cmd)
					k->addkey(c);
		}
	}
}

std::string calibration_to_mode(keyboard_axis_calibration p)
{
	if(p.mode == -1) return "disabled";
	if(p.mode == 1 && p.esign_b == 1) return "axis";
	if(p.mode == 1 && p.esign_b == -1) return "axis-inverse";
	if(p.mode == 0 && p.esign_a == -1 && p.esign_b == 0) return "pressure-0";
	if(p.mode == 0 && p.esign_a == -1 && p.esign_b == 1) return "pressure-+";
	if(p.mode == 0 && p.esign_a == 0 && p.esign_b == -1) return "pressure0-";
	if(p.mode == 0 && p.esign_a == 0 && p.esign_b == 1) return "pressure0+";
	if(p.mode == 0 && p.esign_a == 1 && p.esign_b == -1) return "pressure+-";
	if(p.mode == 0 && p.esign_a == 1 && p.esign_b == 0) return "pressure+0";
	return "";
}
