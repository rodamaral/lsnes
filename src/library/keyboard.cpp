#include "keyboard.hpp"
#include "stateobject.hpp"
#include "threads.hpp"
#include <iostream>

namespace keyboard
{
namespace {
	threads::rlock* global_lock;
	threads::rlock& get_keyboard_lock()
	{
		if(!global_lock) global_lock = new threads::rlock;
		return *global_lock;
	}

	struct keyboard_internal
	{
		std::map<std::string, modifier*> modifiers;
		std::map<std::string, key*> keys;
	};
	typedef stateobject::type<keyboard, keyboard_internal> keyboard_internal_t;
}

void keyboard::do_register(const std::string& name, modifier& mod) throw(std::bad_alloc)
{
	threads::arlock u(get_keyboard_lock());
	auto& state = keyboard_internal_t::get(this);
	if(state.modifiers.count(name)) return;
	state.modifiers[name] = &mod;
}

void keyboard::do_unregister(const std::string& name, modifier& mod) throw()
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	if(!state || !state->modifiers.count(name) || state->modifiers[name] != &mod) return;
	state->modifiers.erase(name);
}

modifier& keyboard::lookup_modifier(const std::string& name) throw(std::runtime_error)
{
	modifier* m = try_lookup_modifier(name);
	if(!m)
		throw std::runtime_error("No such modifier");
	return *m;
}

modifier* keyboard::try_lookup_modifier(const std::string& name) throw()
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	if(!state || !state->modifiers.count(name))
		return NULL;
	return state->modifiers[name];
}

std::list<modifier*> keyboard::all_modifiers() throw(std::bad_alloc)
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	std::list<modifier*> r;
	if(state)
		for(auto i : state->modifiers)
			r.push_back(i.second);
	return r;
}

void keyboard::do_register(const std::string& name, key& key) throw(std::bad_alloc)
{
	threads::arlock u(get_keyboard_lock());
	auto& state = keyboard_internal_t::get(this);
	if(state.keys.count(name)) return;
	state.keys[name] = &key;
}

void keyboard::do_unregister(const std::string& name, key& key) throw()
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	if(!state || !state->keys.count(name) || state->keys[name] != &key) return;
	state->keys.erase(name);
}

key& keyboard::lookup_key(const std::string& name) throw(std::runtime_error)
{
	key* m = try_lookup_key(name);
	if(!m)
		throw std::runtime_error("No such key");
	return *m;
}

key* keyboard::try_lookup_key(const std::string& name) throw()
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	if(!state || !state->keys.count(name))
		return NULL;
	return state->keys[name];
}

std::list<key*> keyboard::all_keys() throw(std::bad_alloc)
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	std::list<key*> r;
	if(state)
		for(auto i : state->keys)
			r.push_back(i.second);
	return r;
}

void keyboard::set_exclusive(event_listener* listener) throw()
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	if(state)
		for(auto i : state->keys)
			i.second->set_exclusive(listener);
}

void keyboard::set_current_key(key* key) throw()
{
	threads::arlock u(get_keyboard_lock());
	current_key = key;
}

key* keyboard::get_current_key() throw()
{
	threads::arlock u(get_keyboard_lock());
	return current_key;
}

keyboard::keyboard() throw(std::bad_alloc)
{
}

keyboard::~keyboard() throw()
{
	threads::arlock u(get_keyboard_lock());
	auto state = keyboard_internal_t::get_soft(this);
	if(!state) return;
	keyboard_internal_t::clear(this);
}

void modifier_set::add(modifier& mod, bool really) throw(std::bad_alloc)
{
	if(really)
		set.insert(&mod);
}

void modifier_set::remove(modifier& mod, bool really) throw(std::bad_alloc)
{
	if(really)
		set.erase(&mod);
}

modifier_set modifier_set::construct(keyboard& kbd, const std::string& _modifiers)
	throw(std::bad_alloc, std::runtime_error)
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
		set.add(kbd.lookup_modifier(mod));
		modifiers = rest;
	}
	return set;
}

bool modifier_set::valid(modifier_set& mask) throw(std::bad_alloc)
{
	//No element can be together with its linkage group.
	for(auto i : set) {
		modifier* j = i->get_link();
		if(j && set.count(j))
			return false;
	}
	for(auto i : mask.set) {
		modifier* j = i->get_link();
		if(j && mask.set.count(j))
			return false;
	}
	//For every element of set, it or its linkage group must be in mask.
	for(auto i : set) {
		modifier* j = i->get_link();
		if(!mask.set.count(i) && !mask.set.count(j ? j : i))
			return false;
	}
	return true;
}

bool modifier_set::operator==(const modifier_set& m) const throw()
{
	for(auto i : set)
		if(!m.set.count(i))
			return false;
	for(auto i : m.set)
		if(!set.count(i))
			return false;
	return true;
}

bool modifier_set::operator<(const modifier_set& m) const throw()
{
	auto i1 = set.begin();
	auto i2 = m.set.begin();
	for(; i1 != set.end() && i2 != m.set.end(); i1++, i2++) {
		if((uint64_t)*i1 < (uint64_t)*i2)
			return true;
		if((uint64_t)*i1 > (uint64_t)*i2)
			return false;
	}
	return (i2 != m.set.end());
}

modifier_set::operator std::string() const throw(std::bad_alloc)
{
	std::string r;
	for(auto i : set)
		r = r + ((r != "") ? "," : "") + i->get_name();
	return r;
}

std::ostream& operator<<(std::ostream& os, const modifier_set& m)
{
	os << "<modset:";
	for(auto i : m.set)
		os << i->get_name() << " ";
	os << ">";
	return os;
}

bool modifier_set::triggers(const modifier_set& trigger, const modifier_set& mask)
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

int32_t mouse_calibration::get_calibrated_value(int32_t x) const throw()
{
	return x - offset;
}

event::~event() throw() {}
event_key::~event_key() throw() {}
event_axis::~event_axis() throw() {}
event_hat::~event_hat() throw() {}
event_mouse::~event_mouse() throw() {}
event_listener::~event_listener() throw() {}

key::~key() throw()
{
	kbd.do_unregister(name, *this);
}

event_key::event_key(uint32_t chngmask)
	: event(chngmask, keytype::KBD_KEYTYPE_KEY)
{
}

event_axis::event_axis(int32_t _state, uint32_t chngmask)
	: event(chngmask, keytype::KBD_KEYTYPE_AXIS)
{
	state = _state;
}

event_hat::event_hat(uint32_t chngmask)
	: event(chngmask, keytype::KBD_KEYTYPE_HAT)
{
}

event_mouse::event_mouse(int32_t _state, const mouse_calibration& _cal)
	: event(0, keytype::KBD_KEYTYPE_MOUSE)
{
	state = _state;
	cal = _cal;
}

int32_t event_key::get_state() const throw() { return (get_change_mask() & 1) != 0; }
int32_t event_axis::get_state() const throw() { return state; }
int32_t event_mouse::get_state() const throw() { return state; }

int32_t event_hat::get_state() const throw()
{
	int32_t r = 0;
	uint32_t m = get_change_mask();
	if(m & 1) r |= 1;
	if(m & 4) r |= 2;
	if(m & 16) r |= 4;
	if(m & 64) r |= 8;
	return m;
}

key::key(keyboard& keyb, const std::string& _name, const std::string& _clazz,
	keytype _type) throw(std::bad_alloc)
	:  kbd(keyb), clazz(_clazz), name(_name), type(_type)
{
	exclusive_listener = NULL;
	kbd.do_register(name, *this);
}

void key::add_listener(event_listener& listener, bool analog) throw(std::bad_alloc)
{
	if(analog) {
		analog_listeners.insert(&listener);
		digital_listeners.erase(&listener);
	} else {
		digital_listeners.insert(&listener);
		analog_listeners.erase(&listener);
	}
}
void key::remove_listener(event_listener& listener) throw()
{
	digital_listeners.erase(&listener);
	analog_listeners.erase(&listener);
}

void key::set_exclusive(event_listener* listener) throw()
{
	threads::arlock u(get_keyboard_lock());
	exclusive_listener = listener;
}

void key::call_listeners(modifier_set& mods, event& event)
{
	kbd.set_current_key(this);
	bool digital = (event.get_change_mask() & 0xAAAAAAAAUL) != 0;
	get_keyboard_lock().lock();
	if(exclusive_listener) {
		get_keyboard_lock().unlock();
		exclusive_listener->on_key_event(mods, *this, event);
		kbd.set_current_key(NULL);
		return;
	}
	event_listener* itr = NULL;
	while(digital) {
		auto itr2 = digital_listeners.upper_bound(itr);
		if(itr2 == digital_listeners.end())
			break;
		itr = *itr2;
		get_keyboard_lock().unlock();
		itr->on_key_event(mods, *this, event);
		get_keyboard_lock().lock();
	}
	itr = NULL;
	while(true) {
		auto itr2 = analog_listeners.upper_bound(itr);
		if(itr2 == analog_listeners.end())
			break;
		itr = *itr2;
		get_keyboard_lock().unlock();
		itr->on_key_event(mods, *this, event);
		get_keyboard_lock().lock();
	}
	get_keyboard_lock().unlock();
	kbd.set_current_key(NULL);
}

key_axis* key::cast_axis() throw()
{
	if(type != KBD_KEYTYPE_AXIS)
		return NULL;
	return dynamic_cast<key_axis*>(this);
}

key_mouse* key::cast_mouse() throw()
{
	if(type != KBD_KEYTYPE_MOUSE)
		return NULL;
	return dynamic_cast<key_mouse*>(this);
}

key_key::key_key(keyboard& keyb, const std::string& name, const std::string& clazz)
	throw(std::bad_alloc)
	: key(keyb, name, clazz, keytype::KBD_KEYTYPE_KEY)
{
	state = 0;
}

key_key::~key_key() throw() {}

void key_key::set_state(modifier_set mods, int32_t _state) throw()
{
	uint32_t change = _state ? 1 : 0;
	bool edge = false;
	get_keyboard_lock().lock();
	if(state != _state) {
		state = _state;
		change |= 2;
		edge = true;
	}
	get_keyboard_lock().unlock();
	if(edge) {
		event_key e(change);
		call_listeners(mods, e);
	}
}

int32_t key_key::get_state() const throw() { return state; }
int32_t key_key::get_state_digital() const throw() { return state; }

std::vector<std::string> key_key::get_subkeys() throw(std::bad_alloc)
{
	std::vector<std::string> r;
	r.push_back("");
	return r;
}

key_hat::key_hat(keyboard& keyb, const std::string& name, const std::string& clazz)
	throw(std::bad_alloc)
	: key(keyb, name, clazz, keytype::KBD_KEYTYPE_HAT)
{
	state = 0;
}
key_hat::~key_hat() throw() {}

void key_hat::set_state(modifier_set mods, int32_t _state) throw()
{
	state &= 15;
	uint32_t change = 0;
	bool edge = false;
	get_keyboard_lock().lock();
	if(state != _state) {
		int32_t schange = state ^ _state;
		state = _state;
		if(state & 1) change |= 1;
		if(schange & 1) change |= 2;
		if(state & 2) change |= 4;
		if(schange & 2) change |= 8;
		if(state & 4) change |= 16;
		if(schange & 4) change |= 32;
		if(state & 8) change |= 64;
		if(schange & 8) change |= 128;
		edge = true;
	}
	get_keyboard_lock().unlock();
	if(edge) {
		event_hat e(change);
		call_listeners(mods, e);
	}
}

int32_t key_hat::get_state() const throw() { return state; }
int32_t key_hat::get_state_digital() const throw() { return state; }

std::vector<std::string> key_hat::get_subkeys() throw(std::bad_alloc)
{
	std::vector<std::string> r;
	r.push_back("n");
	r.push_back("e");
	r.push_back("s");
	r.push_back("w");
	return r;
}

key_axis::key_axis(keyboard& keyb, const std::string& name, const std::string& clazz,
	int mode) throw(std::bad_alloc)
	: key(keyb, name, clazz, keytype::KBD_KEYTYPE_AXIS)
{
	rawstate = 0;
	digitalstate = 0;
	last_tolerance = 0.5;
	_mode = mode;
}
key_axis::~key_axis() throw() {}

int32_t key_axis::get_state() const throw()
{
	threads::arlock u(get_keyboard_lock());
	return rawstate;
}

int32_t key_axis::get_state_digital() const throw()
{
	threads::arlock u(get_keyboard_lock());
	if(rawstate <= -32768 * last_tolerance)
		return -1;
	if(rawstate >= 32767 * last_tolerance)
		return 1;
	return 0;
}

int key_axis::get_mode() const throw()
{
	threads::arlock u(get_keyboard_lock());
	return _mode;
}

std::vector<std::string> key_axis::get_subkeys() throw(std::bad_alloc)
{
	threads::arlock u(get_keyboard_lock());
	std::vector<std::string> r;
	if(_mode == 0)
		r.push_back("");
	else if(_mode > 0) {
		r.push_back("+");
		r.push_back("-");
	}
	return r;
}

void key_axis::set_state(modifier_set mods, int32_t _rawstate) throw()
{
	bool edge = false;
	int32_t state;
	uint32_t change = 0;
	int dold = 0, dnew = 0;
	get_keyboard_lock().lock();
	if(rawstate != _rawstate) {
		dold = digitalstate;
		rawstate = _rawstate;
		state = rawstate;
		if(state <= -32768 * last_tolerance)
			dnew = -1;
		if(state >= 32767 * last_tolerance)
			dnew = 1;
		digitalstate = dnew;
		if(dnew > 0)
			change |= 1;
		if((dold > 0 && dnew <= 0) || (dold <= 0 && dnew > 0))
			change |= 2;
		if(dnew < 0)
			change |= 4;
		if((dold < 0 && dnew >= 0) || (dold >= 0 && dnew < 0))
			change |= 8;
		edge = true;
	}
	get_keyboard_lock().unlock();
	if(edge) {
		event_axis e(state, change);
		call_listeners(mods, e);
	}
}

void key_axis::set_mode(int mode, double tolerance) throw()
{
	threads::arlock u(get_keyboard_lock());
	_mode = mode;
	last_tolerance = tolerance;
}

key_mouse::key_mouse(keyboard& keyb, const std::string& name, const std::string& clazz,
	mouse_calibration _cal) throw(std::bad_alloc)
	: key(keyb, name, clazz, keytype::KBD_KEYTYPE_MOUSE)
{
	rawstate = 0;
	cal = _cal;
}

key_mouse::~key_mouse() throw() {}
int32_t key_mouse::get_state_digital() const throw() { return 0; }

int32_t key_mouse::get_state() const throw()
{
	threads::arlock u(get_keyboard_lock());
	return cal.get_calibrated_value(rawstate);
}

std::vector<std::string> key_mouse::get_subkeys() throw(std::bad_alloc)
{
	return std::vector<std::string>();
}

mouse_calibration key_mouse::get_calibration() const throw()
{
	threads::arlock u(get_keyboard_lock());
	mouse_calibration tmp = cal;
	return tmp;
}

void key_mouse::set_state(modifier_set mods, int32_t _rawstate) throw()
{
	bool edge = false;
	int32_t state;
	mouse_calibration _cal;
	get_keyboard_lock().lock();
	if(rawstate != _rawstate) {
		rawstate = _rawstate;
		state = cal.get_calibrated_value(rawstate);
		_cal = cal;
		edge = true;
	}
	get_keyboard_lock().unlock();
	if(edge) {
		event_mouse e(state, _cal);
		call_listeners(mods, e);
	}
}

void key_mouse::set_calibration(mouse_calibration _cal) throw()
{
	get_keyboard_lock().lock();
	cal = _cal;
	int32_t state = cal.get_calibrated_value(rawstate);
	get_keyboard_lock().unlock();
	event_mouse e(state, _cal);
	modifier_set mods;
	call_listeners(mods, e);
}
}
