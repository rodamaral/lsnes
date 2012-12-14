#include "keyboard.hpp"
#include <iostream>

void keyboard::do_register_modifier(const std::string& name, keyboard_modifier& mod) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	modifiers[name] = &mod;
}

void keyboard::do_unregister_modifier(const std::string& name) throw()
{
	umutex_class u(mutex);
	modifiers.erase(name);
}

keyboard_modifier& keyboard::lookup_modifier(const std::string& name) throw(std::runtime_error)
{
	keyboard_modifier* m = try_lookup_modifier(name);
	if(!m)
		throw std::runtime_error("No such modifier");
	return *m;
}

keyboard_modifier* keyboard::try_lookup_modifier(const std::string& name) throw()
{
	umutex_class u(mutex);
	if(!modifiers.count(name))
		return NULL;
	return modifiers[name];
}

std::list<keyboard_modifier*> keyboard::all_modifiers() throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::list<keyboard_modifier*> r;
	for(auto i : modifiers)
		r.push_back(i.second);
	return r;
}

keyboard::keyboard() throw(std::bad_alloc)
	: modifier_proxy(*this)
{
	register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_ready(modifier_proxy, true);
}

keyboard::~keyboard() throw()
{
	register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_ready(modifier_proxy, false);
}

void keyboard_modifier_set::add(keyboard_modifier& mod, bool really) throw(std::bad_alloc)
{
	if(really)
		set.insert(&mod);
}

void keyboard_modifier_set::remove(keyboard_modifier& mod, bool really) throw(std::bad_alloc)
{
	if(really)
		set.erase(&mod);
}

keyboard_modifier_set keyboard_modifier_set::construct(keyboard& kbd, const std::string& _modifiers)
	throw(std::bad_alloc, std::runtime_error)
{
	keyboard_modifier_set set;
	std::string modifiers = _modifiers;
	while(modifiers != "") {
		std::string mod = modifiers;
		std::string rest;
		size_t split = modifiers.find_first_of(",");
		if(split < modifiers.length()) {
			mod = modifiers.substr(0, split);
			rest = modifiers.substr(split + 1);
		}
		set.add(kbd.lookup_modifier(mod));
		modifiers = rest;
	}
	return set;
}

bool keyboard_modifier_set::valid(keyboard_modifier_set& mask) throw(std::bad_alloc)
{
	//No element can be together with its linkage group.
	for(auto i : set) {
		keyboard_modifier* j = i->get_link();
		if(j && set.count(j))
			return false;
	}
	for(auto i : mask.set) {
		keyboard_modifier* j = i->get_link();
		if(j && mask.set.count(j))
			return false;
	}
	//For every element of set, it or its linkage group must be in mask.
	for(auto i : set) {
		keyboard_modifier* j = i->get_link();
		if(!mask.set.count(i) && !mask.set.count(j ? j : i))
			return false;
	}
	return true;
}

bool keyboard_modifier_set::operator==(const keyboard_modifier_set& m) const throw()
{
	for(auto i : set)
		if(!m.set.count(i))
			return false;
	for(auto i : m.set)
		if(!set.count(i))
			return false;
	return true;
}

std::ostream& operator<<(std::ostream& os, const keyboard_modifier_set& m)
{
	os << "<modset:";
	for(auto i : m.set)
		os << i->get_name() << " ";
	os << ">";
	return os;
}

bool keyboard_modifier_set::triggers(keyboard_modifier_set& trigger, keyboard_modifier_set& mask)
	throw(std::bad_alloc)
{
	for(auto i : mask.set) {
		bool trigger_exact = trigger.set.count(i);
		bool trigger_ingroup = false;
		for(auto j : trigger.set)
			if(j->get_link() == i)
				trigger_ingroup = true;
		bool trigger_none = !trigger_exact && !trigger_ingroup;

		//If trigger_exact is set, then this key or a key in this group must be pressed.
		if(trigger_exact) {
			bool any = false;
			for(auto j : set)
				any = any || (j == i || j->get_link() == i);
			if(!any)
				return false;
		}
		//If trigger_ingroup is set, then exactly that set of keys in group must be pressed.
		if(trigger_ingroup) {
			for(auto j : i->get_keyboard().all_modifiers()) {
				if(j->get_link() != i)
					continue;	//Not interested.
				if((set.count(j) > 0) != (trigger.set.count(j) > 0))
					return false;
			}
		}
		//If trigger_none is set, then this key nor keys in this key group can't be pressed.
		if(trigger_none) {
			bool any = false;
			for(auto j : set)
				any = any || (j == i || j->get_link() == i);
			if(any)
				return false;
		}
	}
	return true;
}
