#include "globalwrap.hpp"
#include "register-queue.hpp"
#include "settings.hpp"
#include "string.hpp"

#include <set>
#include <map>
#include <sstream>
#include <iostream>

namespace
{
	struct pending_registration
	{
		setting_group* group;
		std::string name;
		setting* toreg;
	};

	typedef register_queue<setting_group, setting> regqueue_t;
}

setting_listener::~setting_listener() throw()
{
}

setting_group::setting_group() throw(std::bad_alloc)
{
	regqueue_t::do_ready(*this, true);
}

setting_group::~setting_group() throw()
{
	regqueue_t::do_ready(*this, false);
}

setting* setting_group::get_by_name(const std::string& name)
{
	umutex_class h(lock);
	if(!settings.count(name))
		throw std::runtime_error("No such setting '" + name + "'");
	return settings[name];
}

void setting_group::set(const std::string& _setting, const std::string& value) throw(std::bad_alloc,
	std::runtime_error)
{
	setting* tochange;
	try {
		tochange = get_by_name(_setting);
	} catch(...) {
		if(storage_mode)
			invalid_values[_setting] = value;
		throw;
	}
	try {
		{
			setting::lock_holder lck(tochange);
			tochange->set(value);
			invalid_values.erase(_setting);
		}
		std::set<setting_listener*> l;
		{
			umutex_class h(lock);
			for(auto i : listeners)
				l.insert(i);
		}
		for(auto i : l)
			i->changed(*this, _setting, value);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		if(storage_mode)
			invalid_values[_setting] = value;
		throw std::runtime_error("Can't set setting '" + _setting + "': " + e.what());
	}
}

bool setting_group::blankable(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	try {
		{
			setting::lock_holder lck(tochange);
			return tochange->blank(false);
		}
	} catch(...) {
		return false;
	}
}

void setting_group::blank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	try {
		{
			setting::lock_holder lck(tochange);
			if(!tochange->blank(true))
				throw std::runtime_error("This setting can't be cleared");
			invalid_values.erase(_setting);
		}
		std::set<setting_listener*> l;
		{
			umutex_class h(lock);
			for(auto i : listeners)
				l.insert(i);
		}
		for(auto i : l)
			i->blanked(*this, _setting);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Can't clear setting '" + _setting + "': " + e.what());
	}
}

std::string setting_group::get(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	{
		setting::lock_holder lck(tochange);
		return tochange->get();
	}
}

bool setting_group::is_set(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	{
		setting::lock_holder lck(tochange);
		return tochange->is_set();
	}
}

std::set<std::string> setting_group::get_settings_set() throw(std::bad_alloc)
{
	umutex_class(lock);
	std::set<std::string> r;
	for(auto i : settings)
		r.insert(i.first);
	return r;
}

void setting_group::set_storage_mode(bool enable) throw()
{
	storage_mode = enable;
}

std::map<std::string, std::string> setting_group::get_invalid_values() throw(std::bad_alloc)
{
	umutex_class(lock);
	return invalid_values;
}

void setting_group::add_listener(struct setting_listener& listener) throw(std::bad_alloc)
{
	umutex_class(lock);
	listeners.insert(&listener);
}

void setting_group::remove_listener(struct setting_listener& listener) throw(std::bad_alloc)
{
	umutex_class(lock);
	listeners.erase(&listener);
}

void setting_group::do_register(const std::string& name, setting& _setting) throw(std::bad_alloc)
{
	umutex_class(lock);
	settings[name] = &_setting;
}

void setting_group::do_unregister(const std::string& name) throw(std::bad_alloc)
{
	umutex_class(lock);
	settings.erase(name);
}

setting::setting(setting_group& group, const std::string& name) throw(std::bad_alloc)
	: in_group(group)
{
	regqueue_t::do_register(group, settingname = name, *this);
}

setting::~setting() throw()
{
	regqueue_t::do_unregister(in_group, settingname);
}

bool setting::blank(bool really) throw(std::bad_alloc, std::runtime_error)
{
	return false;
}

numeric_setting::numeric_setting(setting_group& group, const std::string& sname, int32_t minv, int32_t maxv,
	int32_t dflt) throw(std::bad_alloc)
	: setting(group, sname)
{
	minimum = minv;
	maximum = maxv;
	value = dflt;
}

bool numeric_setting::is_set() throw()
{
	return true;
}

void numeric_setting::set(const std::string& _value) throw(std::bad_alloc, std::runtime_error)
{
	int32_t v = parse_value<int32_t>(_value);
	if(v < minimum || v > maximum) {
		std::ostringstream x;
		x << "Value out of range (" << minimum << " - " << maximum << ")";
		throw std::runtime_error(x.str());
	}
	value = v;
}

std::string numeric_setting::get() throw(std::bad_alloc)
{
	std::ostringstream x;
	x << value;
	return x.str();
}

numeric_setting::operator int32_t() throw()
{
	lock_holder lck(this);
	return value;
}

boolean_setting::boolean_setting(setting_group& group, const std::string& sname, bool dflt) throw(std::bad_alloc)
	: setting(group, sname)
{
	value = dflt;
}

bool boolean_setting::is_set() throw()
{
	return true;
}

void boolean_setting::set(const std::string& v) throw(std::bad_alloc, std::runtime_error)
{
	switch(string_to_bool(v)) {
	case 1:
		value = true;
		break;
	case 0:
		value = false;
		break;
	default:
		throw std::runtime_error("Invalid value for boolean setting");
	};
}

std::string boolean_setting::get() throw(std::bad_alloc)
{
	if(value)
		return "true";
	else
		return "false";
}

boolean_setting::operator bool() throw()
{
	lock_holder lck(this);
	return value;
}

path_setting::path_setting(setting_group& group, const std::string& sname) throw(std::bad_alloc)
	: setting(group, sname)
{
	path = ".";
	_default = true;
}

bool path_setting::blank(bool really) throw(std::bad_alloc, std::runtime_error)
{
	if(!really)
		return true;
	path = ".";
	_default = true;
	return true;
}

bool path_setting::is_set() throw()
{
	return !_default;
}

void path_setting::set(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	if(value == "") {
		path = ".";
		_default = true;
	} else {
		path = value;
		_default = false;
	}
}

std::string path_setting::get() throw(std::bad_alloc)
{
	return path;
}

path_setting::operator std::string()
{
	lock_holder lck(this);
	return path;
}
