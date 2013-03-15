#include "keymapper.hpp"
#include "register-queue.hpp"
#include "string.hpp"

std::string keyboard_mapper::fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc)
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

key_specifier::key_specifier() throw(std::bad_alloc)
{
}

key_specifier::key_specifier(const std::string& keyspec) throw(std::bad_alloc, std::runtime_error)
{
	regex_results r = regex("([^/]*)/([^|]*)\\|(.*)", keyspec, "Invalid keyspec");
	mod = r[1];
	mask = r[2];
	key = r[3];
}

key_specifier::operator std::string() throw(std::bad_alloc)
{
	return mod + "/" + mask + "|" + key;
}

key_specifier::operator bool() throw()
{
	return (key != "");
}

bool key_specifier::operator!() throw()
{
	return (key == "");
}

void key_specifier::clear() throw()
{
	mod = "";
	mask = "";
	key = "";
}

bool key_specifier::operator==(const key_specifier& keyspec)
{
	return (mod == keyspec.mod && mask == keyspec.mask && key == keyspec.key);
}

bool key_specifier::operator!=(const key_specifier& keyspec)
{
	return (mod != keyspec.mod || mask != keyspec.mask || key != keyspec.key);
}

std::set<inverse_bind*> keyboard_mapper::get_inverses() throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::set<inverse_bind*> r;
	for(auto i : ibinds)
		r.insert(i.second);
	return r;
}

inverse_bind* keyboard_mapper::get_inverse(const std::string& command) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	if(ibinds.count(command))
		return ibinds[command];
	else
		return NULL;
}

std::set<controller_key*> keyboard_mapper::get_controller_keys() throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::set<controller_key*> r;
	for(auto i : ckeys)
		r.insert(i.second);
	return r;
}

controller_key* keyboard_mapper::get_controllerkey(const std::string& command) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	if(ckeys.count(command))
		return ckeys[command];
	else
		return NULL;
}

void keyboard_mapper::do_register_inverse(const std::string& name, inverse_bind& ibind) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	ibinds[name] = &ibind;
	//Search for matches.
	for(auto i : bindings)
		if(i.second == ibind.cmd) {
			umutex_class u2(ibind.mutex);
			if(!ibind.primary_spec)
				ibind.primary_spec = i.first.as_keyspec();
			else if(!ibind.secondary_spec)
				ibind.secondary_spec = i.first.as_keyspec();
		}
}

void keyboard_mapper::do_unregister_inverse(const std::string& name) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	ibinds.erase(name);
}

void keyboard_mapper::do_register_ckey(const std::string& name, controller_key& ckey) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	ckeys[name] = &ckey;
}

void keyboard_mapper::do_unregister_ckey(const std::string& name) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	ckeys.erase(name);
}

keyboard& keyboard_mapper::get_keyboard() throw()
{
	return kbd;
}

keyboard_mapper::keyboard_mapper(keyboard& _kbd, command_group& _domain) throw(std::bad_alloc)
	: inverse_proxy(*this), controllerkey_proxy(*this), kbd(_kbd), domain(_domain)
{
	register_queue<_inverse_proxy, inverse_bind>::do_ready(inverse_proxy, true);
	register_queue<_controllerkey_proxy, controller_key>::do_ready(controllerkey_proxy, true);
}

keyboard_mapper::~keyboard_mapper() throw()
{
	register_queue<_inverse_proxy, inverse_bind>::do_ready(inverse_proxy, false);
	register_queue<_controllerkey_proxy, controller_key>::do_ready(controllerkey_proxy, false);
}

keyboard_mapper::triplet::triplet(keyboard_modifier_set _mod, keyboard_modifier_set _mask, keyboard_key& _key,
	unsigned _subkey)
{
	mod = _mod;
	mask = _mask;
	key = &_key;
	subkey = _subkey;
	index = false;
}

keyboard_mapper::triplet::triplet(keyboard_key& _key, unsigned _subkey)
{
	key = &_key;
	subkey = _subkey;
	index = true;
}

bool keyboard_mapper::triplet::operator<(const struct triplet& a) const
{
	if((uint64_t)key < (uint64_t)a.key)
		return true;
	if((uint64_t)key > (uint64_t)a.key)
		return false;
	if(subkey < a.subkey)
		return true;
	if(subkey > a.subkey)
		return false;
	if(index && !a.index)
		return true;
	if(!index && a.index)
		return false;
	if(mask < a.mask)
		return true;
	if(a.mask < mask)
		return false;
	if(mod < a.mod)
		return true;
	if(a.mod < mod)
		return false;
	return false;
}

bool keyboard_mapper::triplet::operator==(const struct triplet& a) const
{
	if(index != a.index)
		return false;
	if(key != a.key)
		return false;
	if(subkey != a.subkey)
		return false;
	if(!(mod == a.mod))
		return false;
	if(!(mask == a.mask))
		return false;
	return true;
}

key_specifier keyboard_mapper::triplet::as_keyspec() const throw(std::bad_alloc)
{
	key_specifier k;
	k.mod = mod;
	k.mask = mask;
	auto s = key->get_subkeys();
	if(s.size() > subkey)
		k.key = key->get_name() + s[subkey];
	else
		k.key = key->get_name();
	return k;
}

std::list<key_specifier> keyboard_mapper::get_bindings() throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::list<key_specifier> r;
	for(auto i : bindings)
		r.push_back(i.first.as_keyspec());
	return r;
}

command_group& keyboard_mapper::get_command_group() throw()
{
	return domain;
}

void keyboard_mapper::bind(std::string mod, std::string modmask, std::string keyname, std::string command)
	throw(std::bad_alloc, std::runtime_error)
{
	key_specifier spec;
	spec.mod = mod;
	spec.mask = modmask;
	spec.key = keyname;
	triplet t(kbd, spec);
	umutex_class u(mutex);
	if(bindings.count(t))
		throw std::runtime_error("Key is already bound");
	if(!listening.count(t.key)) {
		t.key->add_listener(*this, false);
		listening.insert(t.key);
	}
	std::string old_command;
	if(bindings.count(t))
		old_command = bindings[t];
	bindings[t] = command;
	change_command(spec, old_command, command);
}

void keyboard_mapper::unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
	std::runtime_error)
{
	key_specifier spec;
	spec.mod = mod;
	spec.mask = modmask;
	spec.key = keyname;
	triplet t(kbd, spec);
	umutex_class u(mutex);
	if(!bindings.count(t))
		throw std::runtime_error("Key is not bound");
	//No harm at leaving listeners listening.
	std::string old_command;
	if(bindings.count(t))
		old_command = bindings[t];
	bindings.erase(t);
	change_command(spec, old_command, "");
}

std::string keyboard_mapper::get(const key_specifier& keyspec) throw(std::bad_alloc)
{
	triplet t(kbd, keyspec);
	umutex_class u(mutex);
	if(!bindings.count(t))
		return "";
	return bindings[t];
}

void keyboard_mapper::change_command(const key_specifier& spec, const std::string& old, const std::string& newc)
{
	if(old != "" && ibinds.count(old)) {
		auto& i = ibinds[old];
		i->primary_spec.clear();
		i->secondary_spec.clear();
		for(auto j : bindings)
			if(j.second == i->cmd && j.first.as_keyspec() != spec) {
				umutex_class u2(i->mutex);
				if(!i->primary_spec)
					i->primary_spec = j.first.as_keyspec();
				else if(!i->secondary_spec)
					i->secondary_spec = j.first.as_keyspec();
			}
	}
	if(newc != "" && ibinds.count(newc)) {
		auto& i = ibinds[newc];
		umutex_class u2(i->mutex);
		if(!i->primary_spec)
			i->primary_spec = spec;
		else if(!i->secondary_spec)
			i->secondary_spec = spec;
	}
}

void keyboard_mapper::set(const key_specifier& keyspec, const std::string& cmd) throw(std::bad_alloc,
	std::runtime_error)
{
	triplet t(kbd, keyspec);
	umutex_class u(mutex);
	if(!listening.count(t.key)) {
		t.key->add_listener(*this, false);
		listening.insert(t.key);
	}
	std::string oldcmd;
	if(bindings.count(t))
		oldcmd = bindings[t];
	bindings[t] = cmd;
	change_command(keyspec, oldcmd, cmd);
}

void keyboard_mapper::on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event)
{
	auto mask = event.get_change_mask();
	unsigned i = 0;
	while(mask) {
		unsigned k = mask & 3;
		if(k & 2)
			on_key_event_subkey(mods, key, i, k == 3);
		mask >>= 2;
		i++;
	}
}

void keyboard_mapper::on_key_event_subkey(keyboard_modifier_set& mods, keyboard_key& key, unsigned skey,
	bool polarity)
{
	triplet llow(key, skey);
	triplet lhigh(key, skey + 1);
	auto low = bindings.lower_bound(llow);
	auto high = bindings.lower_bound(lhigh);
	for(auto i = low; i != high; i++) {
		if(!mods.triggers(i->first.mod, i->first.mask))
			continue;
		std::string cmd = fixup_command_polarity(i->second, polarity);
		if(cmd != "")
			domain.invoke(cmd);
	}
}

keyboard_mapper::triplet::triplet(keyboard& k, const key_specifier& spec)
{
	mod = keyboard_modifier_set::construct(k, spec.mod);
	mask = keyboard_modifier_set::construct(k, spec.mask);
	if(!mod.valid(mask))
		throw std::runtime_error("Bad modifiers");
	auto g = keymapper_lookup_subkey(k, spec.key, false);
	key = g.first;
	subkey = g.second;
	index = false;
}

std::list<controller_key*> keyboard_mapper::get_controllerkeys_kbdkey(keyboard_key* kbdkey)
	throw(std::bad_alloc)
{
	umutex_class u(mutex);
	std::list<controller_key*> r;
	for(auto i : ckeys) {
		auto k = i.second->get();
		if(k.first == kbdkey)
			r.push_back(i.second);
	}
	return r;
}

inverse_bind::inverse_bind(keyboard_mapper& _mapper, const std::string& _command, const std::string& _name)
	throw(std::bad_alloc)
	: mapper(_mapper), cmd(_command), oname(_name)
{
	register_queue<keyboard_mapper::_inverse_proxy, inverse_bind>::do_register(mapper.inverse_proxy, cmd, *this);
}

inverse_bind::~inverse_bind() throw()
{
	register_queue<keyboard_mapper::_inverse_proxy, inverse_bind>::do_unregister(mapper.inverse_proxy, cmd);
}

key_specifier inverse_bind::get(bool primary) throw(std::bad_alloc)
{
	umutex_class u(mutex);
	return primary ? primary_spec : secondary_spec;
}

void inverse_bind::clear(bool primary) throw(std::bad_alloc)
{
	key_specifier unbind;
	{
		umutex_class u(mutex);
		unbind = primary ? primary_spec : secondary_spec;
	}
	if(unbind)
		mapper.set(unbind, "");
}

void inverse_bind::set(const key_specifier& keyspec, bool primary) throw(std::bad_alloc)
{
	key_specifier unbind;
	{
		umutex_class u(mutex);
		unbind = primary ? primary_spec : secondary_spec;
	}
	if(unbind)
		mapper.set(unbind, "");
	mapper.set(keyspec, cmd);
}


std::string inverse_bind::getname() throw(std::bad_alloc)
{
	return oname;
}


controller_key::controller_key(keyboard_mapper& _mapper, const std::string& _command, const std::string& _name,
	bool _axis) throw(std::bad_alloc)
	: mapper(_mapper), cmd(_command), oname(_name)
{
	register_queue<keyboard_mapper::_controllerkey_proxy, controller_key>::do_register(mapper.controllerkey_proxy,
		cmd, *this);
	key = NULL;
	subkey = 0;
	axis = _axis;
}

controller_key::~controller_key() throw()
{
	register_queue<keyboard_mapper::_controllerkey_proxy, controller_key>::do_unregister(
		mapper.controllerkey_proxy, cmd);
}

std::pair<keyboard_key*, unsigned> controller_key::get() throw()
{
	umutex_class u(mutex);
	return std::make_pair(key, subkey);
}

std::string controller_key::get_string() throw(std::bad_alloc)
{
	auto k = get();
	if(!k.first)
		return "";
	auto s = k.first->get_subkeys();
	if(subkey >= s.size() || axis)
		return k.first->get_name();
	return k.first->get_name() + s[k.second];
}

void controller_key::set(keyboard_key* _key, unsigned _subkey) throw()
{
	umutex_class u(mutex);
	if(key != _key) {
		if(_key) _key->add_listener(*this, axis);
		if(key) key->remove_listener(*this);
	}
	key = _key;
	subkey = _subkey;
}

void controller_key::set(const std::string& _key) throw(std::bad_alloc, std::runtime_error)
{
	auto g = keymapper_lookup_subkey(mapper.get_keyboard(), _key, axis);
	set(g.first, g.second);
}

std::pair<keyboard_key*, unsigned> keymapper_lookup_subkey(keyboard& kbd, const std::string& name, bool axis)
	throw(std::bad_alloc, std::runtime_error)
{
	if(name == "")
		return std::make_pair((keyboard_key*)NULL, 0);
	//Try direct lookup first.
	keyboard_key* key = kbd.try_lookup_key(name);
	if(key)
		return std::make_pair(key, 0);
	//Axes only do direct lookup.
	if(axis)
		throw std::runtime_error("Invalid key");
	std::string prefix = name;
	char letter = prefix[prefix.length() - 1];
	prefix = prefix.substr(0, prefix.length() - 1);
	key = kbd.try_lookup_key(prefix);
	if(!key)
		throw std::runtime_error("Invalid key");
	auto s = key->get_subkeys();
	for(size_t i = 0; i < s.size(); i++)
		if(s[i].length() > 0 && letter == s[i][0])
			return std::make_pair(key, i);
	throw std::runtime_error("Invalid key");
}

void controller_key::on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event)
{
	if(axis) {
		//Axes work specially.
		mapper.get_command_group().invoke((stringfmt() << cmd << " " << event.get_state()).str());
		return;
	}
	auto mask = event.get_change_mask();
	unsigned kmask = (mask >> (2 * subkey)) & 3;
	std::string cmd2;
	if(kmask & 2)
		cmd2 = keyboard_mapper::fixup_command_polarity(cmd, kmask == 3);
	if(cmd2 != "")
		mapper.get_command_group().invoke(cmd2);
}
