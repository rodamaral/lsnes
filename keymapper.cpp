#include "keymapper.hpp"
#include <stdexcept>
#include "lua.hpp"
#include <iostream>
#include <list>
#include <map>
#include <sstream>
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
			keymapper::bind(mod, modmask, keyname, command);
			if(mod != "" || modmask != "")
				messages << mod << "/" << modmask << " ";
			messages << keyname << " bound to '" << command << "'" << std::endl;
			
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
			keymapper::unbind(mod, modmask, keyname);
			if(mod != "" || modmask != "")
				messages << mod << "/" << modmask << " ";
			messages << keyname << " unbound" << std::endl;
		});

	function_ptr_command<> show_bindings("show-bindings", "Show active bindings",
		"Syntax: show-bindings\nShow bindings that are currently active.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			keymapper::dumpbindings();
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

namespace
{
	std::map<std::string, modifier*>* known_modifiers;
	std::map<std::string, std::string>* modifier_linkages;
	std::map<std::string, keygroup*>* keygroups;

	//Returns orig if not linked.
	const modifier* get_linked_modifier(const modifier* orig)
	{
		if(!modifier_linkages || !modifier_linkages->count(orig->name()))
			return orig;
		std::string l = (*modifier_linkages)[orig->name()];
		return (*known_modifiers)[l];
	}
}

modifier::modifier(const std::string& name) throw(std::bad_alloc)
{
	if(!known_modifiers)
		known_modifiers = new std::map<std::string, modifier*>();
	(*known_modifiers)[modname = name] = this;
}

modifier::modifier(const std::string& name, const std::string& linkgroup) throw(std::bad_alloc)
{
	if(!known_modifiers)
		known_modifiers = new std::map<std::string, modifier*>();
	if(!modifier_linkages)
		modifier_linkages = new std::map<std::string, std::string>();
	(*known_modifiers)[modname = name] = this;
	(*modifier_linkages)[name] = linkgroup;
}

modifier& modifier::lookup(const std::string& name) throw(std::bad_alloc, std::runtime_error)
{
	if(!known_modifiers || !known_modifiers->count(name)) {
		std::ostringstream x;
		x << "Invalid modifier '" << name << "'";
		throw std::runtime_error(x.str());
	}
	return *(*known_modifiers)[name];
}

std::string modifier::name() const throw(std::bad_alloc)
{
	return modname;
}


void modifier_set::add(const modifier& mod, bool really) throw(std::bad_alloc)
{
	if(really)
		set.insert(&mod);
}

void modifier_set::remove(const modifier& mod, bool really) throw(std::bad_alloc)
{
	if(really)
		set.erase(&mod);
}

modifier_set modifier_set::construct(const std::string& _modifiers) throw(std::bad_alloc, std::runtime_error)
{
	modifier_set set;
	std::string modifiers = _modifiers;
	while(modifiers != "") {
		std::string mod = modifiers;
		std::string rest;
		size_t split = modifiers.find_first_of(",");
		if(split < modifiers.length()) {
			mod = modifiers.substr(0, split);
			rest = modifiers.substr(split + 1);
		}
		set.add(modifier::lookup(mod));
		modifiers = rest;
	}
	return set;
}

bool modifier_set::valid(const modifier_set& set, const modifier_set& mask) throw(std::bad_alloc)
{
	//No element can be together with its linkage group.
	for(auto i = set.set.begin(); i != set.set.end(); ++i) {
		const modifier* j = get_linked_modifier(*i);
		if(*i != j && set.set.count(j))
			return false;
	}
	for(auto i = mask.set.begin(); i != mask.set.end(); ++i) {
		const modifier* j = get_linked_modifier(*i);
		if(*i != j && mask.set.count(j))
			return false;
	}
	//For every element of set, it or its linkage group must be in mask.
	for(auto i = set.set.begin(); i != set.set.end(); ++i) {
		const modifier* j = get_linked_modifier(*i);
		if(!mask.set.count(*i) && !mask.set.count(j))
			return false;
	}
	return true;
}

bool modifier_set::operator==(const modifier_set& m) const throw()
{
	for(auto i = set.begin(); i != set.end(); ++i)
		if(!m.set.count(*i))
			return false;
	for(auto i = m.set.begin(); i != m.set.end(); ++i)
		if(!set.count(*i))
			return false;
	return true;
}

bool modifier_set::triggers(const modifier_set& set, const modifier_set& trigger, const modifier_set& mask)
	throw(std::bad_alloc)
{
	modifier_set unmet = trigger;
	modifier_set blank;
	for(auto i = set.set.begin(); i != set.set.end(); i++) {
		auto linked = get_linked_modifier(*i);
		if(mask.set.count(linked) && trigger.set.count(linked))
			unmet.remove(*linked);
		if(mask.set.count(linked) && trigger.set.count(*i))
			unmet.remove(**i);
		if(mask.set.count(*i) && trigger.set.count(*i))
			unmet.remove(**i);
	}
	return (unmet == blank);
}

std::string keygroup::name() throw(std::bad_alloc)
{
	return keyname;
}


keygroup::keygroup(const std::string& name, enum type t) throw(std::bad_alloc)
{
	if(!keygroups)
		keygroups = new std::map<std::string, keygroup*>();
	(*keygroups)[keyname = name] = this;
	ktype = t;
	state = 0;
	cal_left = -32768;
	cal_center = 0;
	cal_right = 32767;
	cal_tolerance = 0.5;
}

void keygroup::change_type(enum type t)
{
	ktype = t;
	state = 0;
}

std::pair<keygroup*, unsigned> keygroup::lookup(const std::string& name) throw(std::bad_alloc,
	std::runtime_error)
{
	if(!keygroups)
		throw std::runtime_error("Invalid key");
	if(keygroups->count(name))
		return std::make_pair((*keygroups)[name], 0);
	std::string prefix = name;
	char letter = prefix[prefix.length() - 1];
	prefix = prefix.substr(0, prefix.length() - 1);
	if(!keygroups->count(prefix))
		throw std::runtime_error("Invalid key");
	keygroup* g = (*keygroups)[prefix];
	switch(letter) {
	case '+':
	case 'n':
		return std::make_pair(g, 0);
	case '-':
	case 'e':
		return std::make_pair(g, 1);
	case 's':
		return std::make_pair(g, 2);
	case 'w':
		return std::make_pair(g, 3);
	default:
		throw std::runtime_error("Invalid key");
	}
}

void keygroup::change_calibration(short left, short center, short right, double tolerance)
{
	cal_left = left;
	cal_center = center;
	cal_right = right;
	cal_tolerance = tolerance;
}

double keygroup::compensate(short value)
{
	if(ktype == KT_HAT || ktype == KT_KEY || ktype == KT_DISABLED)
		return value;	//These can't be calibrated.
	if(value <= cal_left)
		return -1.0;
	else if(value >= cal_right)
		return 1.0;
	else if(value == cal_center)
		return 0.0;
	else if(value < cal_center)
		return (static_cast<double>(value) - cal_center) / (static_cast<double>(cal_center) - cal_left);
	else
		return (static_cast<double>(value) - cal_center) / (static_cast<double>(cal_right) - cal_center);
}

double keygroup::compensate2(double value)
{
	switch(ktype) {
	case KT_DISABLED:
		return 0;	//Always neutral.
	case KT_KEY:
	case KT_HAT:
		return value;	//No mapping.
	case KT_PRESSURE_0M:
		return -value;
	case KT_PRESSURE_0P:
		return value;
	case KT_PRESSURE_M0:
		return 1 + value;
	case KT_PRESSURE_MP:
		return (1 + value) / 2;
	case KT_PRESSURE_P0:
		return 1 - value;
	case KT_PRESSURE_PM:
		return (1 - value) / 2;
	case KT_AXIS_PAIR:
		return value;
	case KT_AXIS_PAIR_INVERSE:
		return -value;
	};
}

void keygroup::set_position(short pos, const modifier_set& modifiers) throw()
{
	double x = compensate2(compensate(pos));
	unsigned tmp;
	bool left, right, up, down;
	bool oleft, oright, oup, odown;
	switch(ktype) {
	case KT_DISABLED:
		return;
	case KT_KEY:
	case KT_PRESSURE_0M:
	case KT_PRESSURE_0P:
	case KT_PRESSURE_M0:
	case KT_PRESSURE_MP:
	case KT_PRESSURE_P0:
	case KT_PRESSURE_PM:
		tmp = (x >= cal_tolerance);
		run_listeners(modifiers, 0, true, (!state && tmp), x);
		run_listeners(modifiers, 0, false, (state && !tmp), x);
		state = tmp;
		break;
	case KT_AXIS_PAIR:
	case KT_AXIS_PAIR_INVERSE:
		if(x <= -cal_tolerance)
			tmp = 2;
		else if(x >= cal_tolerance)
			tmp = 1;
		else
			tmp = 0;
		run_listeners(modifiers, 0, false, state == 1 && tmp != 1, x);
		run_listeners(modifiers, 1, false, state == 2 && tmp != 2, x);
		run_listeners(modifiers, 0, true, tmp == 1 && state != 1, x);
		run_listeners(modifiers, 1, true, tmp == 2 && state != 2, x);
		state = tmp;
		break;
	case KT_HAT:
		left = ((pos & 8) != 0);
		right = ((pos & 2) != 0);
		up = ((pos & 1) != 0);
		down = ((pos & 4) != 0);
		oleft = ((state & 8) != 0);
		oright = ((state & 2) != 0);
		oup = ((state & 1) != 0);
		odown = ((state & 4) != 0);
		run_listeners(modifiers, 3, false, oleft && !left, x);
		run_listeners(modifiers, 1, false, oright && !right, x);
		run_listeners(modifiers, 0, false, oup && !up, x);
		run_listeners(modifiers, 2, false, odown && !down, x);
		run_listeners(modifiers, 2, true, !odown && down, x);
		run_listeners(modifiers, 0, true, !oup && up, x);
		run_listeners(modifiers, 1, true, !oright && right, x);
		run_listeners(modifiers, 3, true, !oleft && left, x);
		state = pos;
		break;
	}
}

void keygroup::add_key_listener(key_listener& l) throw(std::bad_alloc)
{
	listeners.push_back(&l);
}

void keygroup::remove_key_listener(key_listener& l) throw(std::bad_alloc)
{
	for(auto i = listeners.begin(); i != listeners.end(); ++i)
		if(*i == &l) {
			listeners.erase(i);
			return;
		}
}

void keygroup::run_listeners(const modifier_set& modifiers, unsigned subkey, bool polarity, bool really, double x)
{
	if(!really)
		return;
	std::string name = keyname;
	if(ktype == KT_AXIS_PAIR && subkey == 0)
		name = name + "+";
	if(ktype == KT_AXIS_PAIR && subkey == 1)
		name = name + "-";
	if(ktype == KT_HAT && subkey == 0)
		name = name + "n";
	if(ktype == KT_HAT && subkey == 1)
		name = name + "e";
	if(ktype == KT_HAT && subkey == 2)
		name = name + "s";
	if(ktype == KT_HAT && subkey == 3)
		name = name + "w";
	if(exclusive) {
		exclusive->key_event(modifiers, *this, subkey, polarity, name);
		return;
	}
	for(auto i = listeners.begin(); i != listeners.end(); ++i)
		(*i)->key_event(modifiers, *this, subkey, polarity, name);
}

keygroup::key_listener* keygroup::exclusive;

void keygroup::set_exclusive_key_listener(key_listener* l) throw()
{
	exclusive = l;
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
	struct keybind_data : public keygroup::key_listener
	{
		modifier_set mod;
		modifier_set modmask;
		keygroup* group;
		unsigned subkey;
		std::string command;
		void key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned _subkey, bool polarity,
			const std::string& name)
		{
			if(!modifier_set::triggers(modifiers, mod, modmask))
				return;
			if(subkey != _subkey)
				return;
			std::string cmd = fixup_command_polarity(command, polarity);
			if(cmd == "")
				return;
			command::invokeC(cmd);
		}
	};
	
	std::map<triple, keybind_data> keybindings;
}

void keymapper::bind(std::string mod, std::string modmask, std::string keyname, std::string command)
	throw(std::bad_alloc, std::runtime_error)
{
	triple k(mod, modmask, keyname);
	modifier_set _mod = modifier_set::construct(mod);
	modifier_set _modmask = modifier_set::construct(modmask);
	if(!modifier_set::valid(_mod, _modmask))
		throw std::runtime_error("Invalid modifiers");
	auto g = keygroup::lookup(keyname);
	if(!keybindings.count(k)) {
		keybindings[k].mod = _mod;
		keybindings[k].modmask = _modmask;
		keybindings[k].group = g.first;
		keybindings[k].subkey = g.second;
		g.first->add_key_listener(keybindings[k]);
	}
	keybindings[k].command = command;
}
void keymapper::unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
		std::runtime_error)
{
	triple k(mod, modmask, keyname);
	if(!keybindings.count(k))
		throw std::runtime_error("Key is not bound");
	keybindings[k].group->remove_key_listener(keybindings[k]);
	keybindings.erase(k);
}

void keymapper::dumpbindings() throw(std::bad_alloc)
{
	for(auto i = keybindings.begin(); i != keybindings.end(); ++i) {
		messages << "bind-key ";
		if(i->first.a != "" || i->first.b != "")
			messages << i->first.a << "/" << i->first.b << " ";
		messages << i->first.c << std::endl;
	}
}
