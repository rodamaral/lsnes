#include "settingvar.hpp"
#include "register-queue.hpp"

namespace
{
	typedef register_queue<setting_var_group, setting_var_base> regqueue_t;
}

setting_var_listener::~setting_var_listener() throw()
{
}

setting_var_group::setting_var_group() throw(std::bad_alloc)
{
	regqueue_t::do_ready(*this, true);
}

setting_var_group::~setting_var_group() throw()
{
	regqueue_t::do_ready(*this, false);
}

std::set<std::string> setting_var_group::get_settings_set() throw(std::bad_alloc)
{
	umutex_class(lock);
	std::set<std::string> x;
	for(auto i : settings)
		x.insert(i.first);
	return x;
}

setting_var_base& setting_var_group::operator[](const std::string& name)
{
	umutex_class(lock);
	if(!settings.count(name))
		throw std::runtime_error("No such setting");
	return *settings[name];
}

void setting_var_group::fire_listener(setting_var_base& var) throw()
{
	std::set<setting_var_listener*> l;
	{
		umutex_class h(lock);
		for(auto i : listeners)
			l.insert(i);
	}
	for(auto i : l)
		try {
			i->on_setting_change(*this, var);
		} catch(...) {
		}
}

void setting_var_group::add_listener(struct setting_var_listener& listener) throw(std::bad_alloc)
{
	umutex_class(lock);
	listeners.insert(&listener);
}

void setting_var_group::remove_listener(struct setting_var_listener& listener) throw(std::bad_alloc)
{
	umutex_class(lock);
	listeners.erase(&listener);
}

void setting_var_group::do_register(const std::string& name, setting_var_base& _setting) throw(std::bad_alloc)
{
	umutex_class h(lock);
	settings[name] = &_setting;
}

void setting_var_group::do_unregister(const std::string& name) throw(std::bad_alloc)
{
	umutex_class h(lock);
	settings.erase(name);
}

setting_var_base::setting_var_base(setting_var_group& _group, const std::string& _iname, const std::string& _hname)
	throw(std::bad_alloc)
	: group(_group), iname(_iname), hname(_hname)
{
	regqueue_t::do_register(group, iname, *this);
}

setting_var_base::~setting_var_base() throw()
{
	regqueue_t::do_unregister(group, iname);
}

setting_var_cache::setting_var_cache(setting_var_group& _grp)
	: grp(_grp)
{
}

std::map<std::string, std::string> setting_var_cache::get_all()
{
	std::map<std::string, std::string> x;
	auto s = grp.get_settings_set();
	for(auto i : s)
		x[i] = grp[i].str();
	for(auto i : badcache)
		x[i.first] = i.second;
	return x;
}

std::set<std::string> setting_var_cache::get_keys()
{
	return grp.get_settings_set();
}

void setting_var_cache::set(const std::string& name, const std::string& value, bool allow_invalid)
	throw(std::bad_alloc, std::runtime_error)
{
	try {
		grp[name].str(value);
		umutex_class h(lock);
		badcache.erase(name);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		if(allow_invalid) {
			umutex_class h(lock);
			badcache[name] = value;
		} else
			throw;
	}
}

std::string setting_var_cache::get(const std::string& name) throw(std::bad_alloc, std::runtime_error)
{
	return grp[name].str();
}
