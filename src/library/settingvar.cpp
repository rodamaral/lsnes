#include "settingvar.hpp"
#include "stateobject.hpp"

namespace settingvar
{
namespace
{
	threads::rlock* global_lock;

	struct set_internal
	{
		std::map<std::string, superbase*> supervars;
		std::set<set_listener*> callbacks;
	};

	struct group_internal
	{
		std::map<std::string, class base*> settings;
		std::set<struct listener*> listeners;
		std::set<struct set*> sets_listened;
	};

	typedef stateobject::type<set, set_internal> set_internal_t;
	typedef stateobject::type<group, group_internal> group_internal_t;
}

threads::rlock& get_setting_lock()
{
	if(!global_lock) global_lock = new threads::rlock;
	return *global_lock;
}

listener::~listener() throw()
{
}

set_listener::~set_listener() throw()
{
}

group::group() throw(std::bad_alloc)
	: _listener(*this)
{
}

group::~group() throw()
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	for(auto i : state->settings)
		i.second->group_died();
	for(auto i : state->sets_listened)
		i->drop_callback(_listener);
	group_internal_t::clear(this);
}

std::set<std::string> group::get_settings_set() throw(std::bad_alloc)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(this);
	std::set<std::string> x;
	if(state)
		for(auto i : state->settings)
			x.insert(i.first);
	return x;
}

base& group::operator[](const std::string& name)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(this);
	if(!state || !state->settings.count(name))
		throw std::runtime_error("No such setting");
	return *state->settings[name];
}

void group::fire_listener(base& var) throw()
{
	std::set<listener*> l;
	{
		threads::arlock h(get_setting_lock());
		auto state = group_internal_t::get_soft(this);
		if(!state) return;
		for(auto i : state->listeners)
			l.insert(i);
	}
	for(auto i : l)
		try {
			i->on_setting_change(*this, var);
		} catch(...) {
		}
}

void group::add_listener(struct listener& listener) throw(std::bad_alloc)
{
	threads::arlock h(get_setting_lock());
	auto& state = group_internal_t::get(this);
	state.listeners.insert(&listener);
}

void group::remove_listener(struct listener& listener) throw(std::bad_alloc)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(this);
	if(state)
		state->listeners.erase(&listener);
}

void group::do_register(const std::string& name, base& _setting) throw(std::bad_alloc)
{
	threads::arlock h(get_setting_lock());
	auto& state = group_internal_t::get(this);
	if(state.settings.count(name)) return;
	state.settings[name] = &_setting;
}

void group::do_unregister(const std::string& name, base& _setting) throw(std::bad_alloc)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(this);
	if(!state || !state->settings.count(name) || state->settings[name] != &_setting) return;
	state->settings.erase(name);
}

void group::add_set(set& s) throw(std::bad_alloc)
{
	threads::arlock u(get_setting_lock());
	auto& state = group_internal_t::get(this);
	if(state.sets_listened.count(&s)) return;
	try {
		state.sets_listened.insert(&s);
		s.add_callback(_listener);
	} catch(...) {
		state.sets_listened.erase(&s);
	}
}

void group::drop_set(set& s)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(this);
	if(!state) return;
	//Drop the callback. This unregisters all.
	s.drop_callback(_listener);
	state->sets_listened.erase(&s);
}

group::xlistener::xlistener(group& _grp)
	: grp(_grp)
{
}

group::xlistener::~xlistener()
{
}

void group::xlistener::create(set& s, const std::string& name, superbase& sb)
{
	threads::arlock h(get_setting_lock());
	sb.make(grp);
}

void group::xlistener::destroy(set& s, const std::string& name)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(&grp);
	if(state)
		state->settings.erase(name);
}

void group::xlistener::kill(set& s)
{
	threads::arlock h(get_setting_lock());
	auto state = group_internal_t::get_soft(&grp);
	if(state)
		state->sets_listened.erase(&s);
}

set::set()
{
}

set::~set()
{
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	threads::arlock u(get_setting_lock());
	//Call all DCBs on all factories.
	for(auto i : state->supervars)
		for(auto j : state->callbacks)
			j->destroy(*this, i.first);
	//Call all TCBs.
	for(auto j : state->callbacks)
		j->kill(*this);
	//Notify all factories that base set died.
	for(auto i : state->supervars)
		i.second->set_died();
	//We assume factories look after themselves, so we don't destroy those.
	set_internal_t::clear(this);
}

void set::do_register(const std::string& name, superbase& info)
{
	threads::arlock u(get_setting_lock());
	auto& state = set_internal_t::get(this);
	if(state.supervars.count(name)) {
		std::cerr << "WARNING: Command collision for " << name << "!" << std::endl;
		return;
	}
	state.supervars[name] = &info;
	//Call all CCBs on this.
	for(auto i : state.callbacks)
		i->create(*this, name, info);
}

void set::do_unregister(const std::string& name, superbase& info)
{
	threads::arlock u(get_setting_lock());
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	if(!state->supervars.count(name) || state->supervars[name] != &info) return; //Not this.
	state->supervars.erase(name);
	//Call all DCBs on this.
	for(auto i : state->callbacks)
		i->destroy(*this, name);
}

void set::add_callback(set_listener& listener) throw(std::bad_alloc)
{
	threads::arlock u(get_setting_lock());
	auto& state = set_internal_t::get(this);
	state.callbacks.insert(&listener);
	//To avoid races, call CCBs on all factories for this.
	for(auto j : state.supervars)
		listener.create(*this, j.first, *j.second);
}

void set::drop_callback(set_listener& listener)
{
	threads::arlock u(get_setting_lock());
	auto state = set_internal_t::get_soft(this);
	if(!state) return;
	if(state->callbacks.count(&listener)) {
		//To avoid races, call DCBs on all factories for this.
		for(auto j : state->supervars)
			listener.destroy(*this, j.first);
		state->callbacks.erase(&listener);
	}
}

base::base(group& _group, const std::string& _iname, const std::string& _hname, bool dynamic)
	throw(std::bad_alloc)
	: sgroup(&_group), iname(_iname), hname(_hname), is_dynamic(dynamic)
{
	sgroup->do_register(iname, *this);
}

base::~base() throw()
{
	threads::arlock u(get_setting_lock());
	if(sgroup)
		sgroup->do_unregister(iname, *this);
}

void base::group_died()
{
	threads::arlock u(get_setting_lock());
	sgroup = NULL;
	if(is_dynamic) delete this;
}

void superbase::_superbase(set& _s, const std::string& _iname) throw(std::bad_alloc)
{
	s = &_s;
	iname = _iname;
	s->do_register(iname, *this);
}

superbase::~superbase() throw()
{
	threads::arlock u(get_setting_lock());
	if(s)
		s->do_unregister(iname, *this);
}

void superbase::set_died()
{
	s = NULL;
}

cache::cache(group& _grp)
	: grp(_grp)
{
}

std::map<std::string, std::string> cache::get_all()
{
	std::map<std::string, std::string> x;
	auto s = grp.get_settings_set();
	for(auto i : s)
		x[i] = grp[i].str();
	for(auto i : badcache)
		x[i.first] = i.second;
	return x;
}

std::set<std::string> cache::get_keys()
{
	return grp.get_settings_set();
}

void cache::set(const std::string& name, const std::string& value, bool allow_invalid)
	throw(std::bad_alloc, std::runtime_error)
{
	try {
		grp[name].str(value);
		threads::arlock u(get_setting_lock());
		badcache.erase(name);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		if(allow_invalid) {
			threads::arlock u(get_setting_lock());
			badcache[name] = value;
		} else
			throw;
	}
}

std::string cache::get(const std::string& name) throw(std::bad_alloc, std::runtime_error)
{
	return grp[name].str();
}

const description& cache::get_description(const std::string& name) throw(std::bad_alloc,
	std::runtime_error)
{
	return grp[name].get_description();
}

std::string cache::get_hname(const std::string& name) throw(std::bad_alloc, std::runtime_error)
{
	return grp[name].get_hname();
}

const char* yes_no::enable = "yes";
const char* yes_no::disable = "no";
}
