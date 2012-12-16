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

void keyboard::do_register_key(const std::string& name, keyboard_key& key) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	keys[name] = &key;
}

void keyboard::do_unregister_key(const std::string& name) throw()
{
	umutex_class u(mutex);
	keys.erase(name);
}

keyboard_key& keyboard::lookup_key(const std::string& name) throw(std::runtime_error)
{
	keyboard_key* m = try_lookup_key(name);
	if(!m)
		throw std::runtime_error("No such key");
	return *m;
}

keyboard_key* keyboard::try_lookup_key(const std::string& name) throw()
{
	umutex_class u(mutex);
	if(!keys.count(name))
		return NULL;
	return keys[name];
}

std::list<keyboard_key*> keyboard::all_keys() throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::list<keyboard_key*> r;
	for(auto i : keys)
		r.push_back(i.second);
	return r;
}

void keyboard::set_exclusive(keyboard_event_listener* listener) throw()
{
	umutex_class u(mutex);
	for(auto i : keys)
		i.second->set_exclusive(listener);
}

keyboard::keyboard() throw(std::bad_alloc)
	: modifier_proxy(*this), key_proxy(*this)
{
	register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_ready(modifier_proxy, true);
	register_queue<keyboard::_key_proxy, keyboard_key>::do_ready(key_proxy, true);
}

keyboard::~keyboard() throw()
{
	register_queue<keyboard::_modifier_proxy, keyboard_modifier>::do_ready(modifier_proxy, false);
	register_queue<keyboard::_key_proxy, keyboard_key>::do_ready(key_proxy, false);
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

int32_t keyboard_axis_calibration::get_calibrated_value(int32_t x) const throw()
{
	if(x < left)
		return -32767;
	if(x > right)
		return 32767;
	if(x == center)
		return 0;
	double width;
	if(x < center)
		width = center - (double)left;
	else if(x > center)
		width = (double)right - center;
	return 32767.0 * (x - (double)center) / width;
}

int keyboard_axis_calibration::get_digital_state(int32_t x) const throw()
{
	if(mode < 0)
		return 0;
	if(mode == 0) {
		int m = (esign_a + 1) * 3 + esign_b + 1;
		bool dwidth;
		bool negative;
		int32_t shift;
		switch(m) {
		case 0: case 4: case 8:	break;		//These are not legal.
		case 1: dwidth = false; negative = false; shift=32767; break;	//- to 0.
		case 2: dwidth = true;  negative = false; shift=32767; break;	//- to +.
		case 3: dwidth = false; negative = true;  shift=0; break;	//0 to -.
		case 5: dwidth = false; negative = false; shift=0; break;	//0 to +.
		case 6: dwidth = true;  negative = true;  shift=32767; break;	//+ to -.
		case 7: dwidth = false; negative = true;  shift=32767; break;	//+ to 0.
		};
		if(negative)
			x = -x;
		x += shift;
		int32_t threshold = (int32_t)((dwidth ? 65535 : 32767) * nullwidth);
		return (x > threshold) ? 1 : 0;
	}
	if(mode == 1) {
		int32_t tolerance = (int32_t)(32767 * nullwidth);
		if(x < -tolerance)
			return esign_a;
		if(x > tolerance)
			return esign_b;
		return 0;
	}
}

int32_t keyboard_mouse_calibration::get_calibrated_value(int32_t x) const throw()
{
	return x - offset;
}

keyboard_event::~keyboard_event() throw() {}
keyboard_event_key::~keyboard_event_key() throw() {}
keyboard_event_axis::~keyboard_event_axis() throw() {}
keyboard_event_hat::~keyboard_event_hat() throw() {}
keyboard_event_mouse::~keyboard_event_mouse() throw() {}
keyboard_event_listener::~keyboard_event_listener() throw() {}

keyboard_key::~keyboard_key() throw()
{
	register_queue<keyboard::_key_proxy, keyboard_key>::do_unregister(kbd.key_proxy, name);
}

keyboard_event_key::keyboard_event_key(uint32_t chngmask)
	: keyboard_event(chngmask, keyboard_keytype::KBD_KEYTYPE_KEY)
{
}

keyboard_event_axis::keyboard_event_axis(int32_t _state, uint32_t chngmask, const keyboard_axis_calibration& _cal)
	: keyboard_event(chngmask, keyboard_keytype::KBD_KEYTYPE_AXIS)
{
	state = _state;
	cal = _cal;
}

keyboard_event_hat::keyboard_event_hat(uint32_t chngmask)
	: keyboard_event(chngmask, keyboard_keytype::KBD_KEYTYPE_HAT)
{
}

keyboard_event_mouse::keyboard_event_mouse(int32_t _state, const keyboard_mouse_calibration& _cal)
	: keyboard_event(0, keyboard_keytype::KBD_KEYTYPE_MOUSE)
{
	state = _state;
	cal = _cal;
}

int32_t keyboard_event_key::get_state() const throw() { return (get_change_mask() & 1) != 0; }
int32_t keyboard_event_axis::get_state() const throw() { return state; }
int32_t keyboard_event_mouse::get_state() const throw() { return state; }

int32_t keyboard_event_hat::get_state() const throw()
{
	int32_t r = 0;
	uint32_t m = get_change_mask();
	if(m & 1) r |= 1;
	if(m & 4) r |= 2;
	if(m & 16) r |= 4;
	if(m & 64) r |= 8;
	return m;
}

keyboard_key::keyboard_key(keyboard& keyb, const std::string& _name, const std::string& _clazz,
	keyboard_keytype _type) throw(std::bad_alloc)
	:  kbd(keyb), name(_name), clazz(_clazz), type(_type)
{
	exclusive_listener = NULL;
	register_queue<keyboard::_key_proxy, keyboard_key>::do_register(kbd.key_proxy, name, *this);
}

void keyboard_key::add_listener(keyboard_event_listener& listener, bool analog) throw(std::bad_alloc)
{
	if(analog) {
		analog_listeners.insert(&listener);
		digital_listeners.erase(&listener);
	} else {
		digital_listeners.insert(&listener);
		analog_listeners.erase(&listener);
	}
}
void keyboard_key::remove_listener(keyboard_event_listener& listener) throw()
{
	digital_listeners.erase(&listener);
	analog_listeners.erase(&listener);
}

void keyboard_key::set_exclusive(keyboard_event_listener* listener) throw()
{
	umutex_class u(mutex);
	exclusive_listener = listener;
}

void keyboard_key::call_listeners(keyboard_modifier_set& mods, keyboard_event& event)
{
	bool digital = (event.get_change_mask() & 0xAAAAAAAAUL) != 0;
	mutex.lock();
	if(exclusive_listener) {
		mutex.unlock();
		exclusive_listener->on_key_event(mods, *this, event);
		return;
	}
	keyboard_event_listener* itr = NULL;
	while(digital) {
		auto itr2 = digital_listeners.upper_bound(itr);
		if(itr2 == digital_listeners.end())
			break;
		itr = *itr2;
		mutex.unlock();
		itr->on_key_event(mods, *this, event);
		mutex.lock();
	}
	itr = NULL;
	while(true) {
		auto itr2 = analog_listeners.upper_bound(itr);
		if(itr2 == analog_listeners.end())
			break;
		itr = *itr2;
		mutex.unlock();
		itr->on_key_event(mods, *this, event);
		mutex.lock();
	}
	mutex.unlock();
}

keyboard_key_axis* keyboard_key::cast_axis() throw()
{
	if(type != KBD_KEYTYPE_AXIS)
		return NULL;
	return dynamic_cast<keyboard_key_axis*>(this);
}

keyboard_key_mouse* keyboard_key::cast_mouse() throw()
{
	if(type != KBD_KEYTYPE_MOUSE)
		return NULL;
	return dynamic_cast<keyboard_key_mouse*>(this);
}

keyboard_key_key::keyboard_key_key(keyboard& keyb, const std::string& name, const std::string& clazz)
	throw(std::bad_alloc)
	: keyboard_key(keyb, name, clazz, keyboard_keytype::KBD_KEYTYPE_KEY)
{
	state = 0;
}

keyboard_key_key::~keyboard_key_key() throw() {}

void keyboard_key_key::set_state(keyboard_modifier_set mods, int32_t _state) throw()
{
	uint32_t change = _state ? 1 : 0;
	bool edge = false;
	mutex.lock();
	if(state != _state) {
		state = _state;
		change |= 2;
		edge = true;
	}
	mutex.unlock();
	if(edge) {
		keyboard_event_key e(change);
		call_listeners(mods, e);
	}
}

int32_t keyboard_key_key::get_state() const throw() { return state; }
int32_t keyboard_key_key::get_state_digital() const throw() { return state; }

std::vector<std::string> keyboard_key_key::get_subkeys() throw(std::bad_alloc)
{
	std::vector<std::string> r;
	r.push_back("");
	return r;
}

keyboard_key_hat::keyboard_key_hat(keyboard& keyb, const std::string& name, const std::string& clazz)
	throw(std::bad_alloc)
	: keyboard_key(keyb, name, clazz, keyboard_keytype::KBD_KEYTYPE_HAT)
{
	state = 0;
}
keyboard_key_hat::~keyboard_key_hat() throw() {}

void keyboard_key_hat::set_state(keyboard_modifier_set mods, int32_t _state) throw()
{
	state &= 15;
	uint32_t change = 0;
	bool edge = false;
	mutex.lock();
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
	mutex.unlock();
	if(edge) {
		keyboard_event_hat e(change);
		call_listeners(mods, e);
	}
}

int32_t keyboard_key_hat::get_state() const throw() { return state; }
int32_t keyboard_key_hat::get_state_digital() const throw() { return state; }

std::vector<std::string> keyboard_key_hat::get_subkeys() throw(std::bad_alloc)
{
	std::vector<std::string> r;
	r.push_back("n");
	r.push_back("e");
	r.push_back("s");
	r.push_back("w");
	return r;
}

keyboard_key_axis::keyboard_key_axis(keyboard& keyb, const std::string& name, const std::string& clazz,
	keyboard_axis_calibration _cal) throw(std::bad_alloc)
	: keyboard_key(keyb, name, clazz, keyboard_keytype::KBD_KEYTYPE_AXIS)
{
	rawstate = 0;
	cal = _cal;
}
keyboard_key_axis::~keyboard_key_axis() throw() {}

int32_t keyboard_key_axis::get_state() const throw()
{
	umutex_class u(mutex);
	return cal.get_calibrated_value(rawstate);
}

int32_t keyboard_key_axis::get_state_digital() const throw()
{
	umutex_class u(mutex);
	int32_t tmp = cal.get_calibrated_value(rawstate);
	return cal.get_digital_state(tmp);
}

keyboard_axis_calibration keyboard_key_axis::get_calibration() const throw()
{
	umutex_class u(mutex);
	keyboard_axis_calibration tmp = cal;
	return tmp;
}

std::vector<std::string> keyboard_key_axis::get_subkeys() throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::vector<std::string> r;
	if(cal.mode == 0)
		r.push_back("");
	else if(cal.mode > 0) {
		r.push_back("+");
		r.push_back("-");
	}
	return r;
}

void keyboard_key_axis::set_state(keyboard_modifier_set mods, int32_t _rawstate) throw()
{
	bool edge = false;
	int32_t state, ostate;
	uint32_t change = 0;
	int dold = 0, dnew = 0;
	keyboard_axis_calibration _cal;
	mutex.lock();
	if(rawstate != _rawstate) {
		ostate = cal.get_calibrated_value(rawstate);
		dold = cal.get_digital_state(ostate);
		rawstate = _rawstate;
		state = cal.get_calibrated_value(rawstate);
		dnew = cal.get_digital_state(state);
		if(dnew > 0)
			change |= 1;
		if((dold > 0 && dnew <= 0) || (dold <= 0 && dnew > 0))
			change |= 2;
		if(dnew < 0)
			change |= 4;
		if((dold < 0 && dnew >= 0) || (dold >= 0 && dnew < 0))
			change |= 8;
		_cal = cal;
		edge = true;
	}
	mutex.unlock();
	if(edge) {
		keyboard_event_axis e(state, change, _cal);
		call_listeners(mods, e);
	}
}

void keyboard_key_axis::set_calibration(keyboard_axis_calibration _cal) throw()
{
	uint32_t change = 0;
	int dold = 0, dnew = 0;
	mutex.lock();
	int32_t ostate = cal.get_calibrated_value(rawstate);
	dold = cal.get_digital_state(ostate);
	cal = _cal;
	int32_t state = cal.get_calibrated_value(rawstate);
	dnew = cal.get_digital_state(state);
	if(dnew < 0)
		change |= 1;
	if((dold < 0 && dnew >= 0) || (dold >= 0 && dnew < 0))
		change |= 2;
	if(dnew > 0)
		change |= 4;
	if((dold > 0 && dnew <= 0) || (dold <= 0 && dnew > 0))
		change |= 8;
	mutex.unlock();
	keyboard_event_axis e(change, state, _cal);
	keyboard_modifier_set mods;
	call_listeners(mods, e);
}

keyboard_key_mouse::keyboard_key_mouse(keyboard& keyb, const std::string& name, const std::string& clazz,
	keyboard_mouse_calibration _cal) throw(std::bad_alloc)
	: keyboard_key(keyb, name, clazz, keyboard_keytype::KBD_KEYTYPE_MOUSE)
{
	rawstate = 0;
	cal = _cal;
}

keyboard_key_mouse::~keyboard_key_mouse() throw() {}
int32_t keyboard_key_mouse::get_state_digital() const throw() { return 0; }

int32_t keyboard_key_mouse::get_state() const throw()
{
	umutex_class u(mutex);
	return cal.get_calibrated_value(rawstate);
}

std::vector<std::string> keyboard_key_mouse::get_subkeys() throw(std::bad_alloc)
{
	return std::vector<std::string>();
}

keyboard_mouse_calibration keyboard_key_mouse::get_calibration() const throw()
{
	umutex_class u(mutex);
	keyboard_mouse_calibration tmp = cal;
	return tmp;
}

void keyboard_key_mouse::set_state(keyboard_modifier_set mods, int32_t _rawstate) throw()
{
	bool edge = false;
	int32_t state;
	keyboard_mouse_calibration _cal;
	mutex.lock();
	if(rawstate != _rawstate) {
		rawstate = _rawstate;
		state = cal.get_calibrated_value(rawstate);
		_cal = cal;
		edge = true;
	}
	mutex.unlock();
	if(edge) {
		keyboard_event_mouse e(state, _cal);
		call_listeners(mods, e);
	}
}

void keyboard_key_mouse::set_calibration(keyboard_mouse_calibration _cal) throw()
{
	mutex.lock();
	cal = _cal;
	int32_t state = cal.get_calibrated_value(rawstate);
	mutex.unlock();
	keyboard_event_mouse e(state, _cal);
	keyboard_modifier_set mods;
	call_listeners(mods, e);
}
