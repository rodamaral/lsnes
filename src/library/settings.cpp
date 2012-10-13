#include "library/globalwrap.hpp"
#include "library/settings.hpp"
#include "library/string.hpp"

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

	globalwrap<mutex_class> reg_mutex;
	globalwrap<std::set<setting_group*>> ready_groups;
	globalwrap<std::list<pending_registration>> pending_registrations;

	void run_pending_registrations()
	{
		umutex_class m(reg_mutex());
		auto i = pending_registrations().begin();
		while(i != pending_registrations().end()) {
			auto entry = i++;
			if(ready_groups().count(entry->group)) {
				entry->group->register_setting(entry->name, *entry->toreg);
				pending_registrations().erase(entry);
			}
		}
	}

	void add_registration(setting_group& group, const std::string& name, setting& type)
	{
		{
			umutex_class m(reg_mutex());
			if(ready_groups().count(&group)) {
				group.register_setting(name, type);
				return;
			}
			pending_registration p;
			p.group = &group;
			p.name = name;
			p.toreg = &type;
			pending_registrations().push_back(p);
		}
		run_pending_registrations();
	}

	void delete_registration(setting_group& group, const std::string& name)
	{
		{
			umutex_class m(reg_mutex());
			if(ready_groups().count(&group))
				group.unregister_setting(name);
			else {
				auto i = pending_registrations().begin();
				while(i != pending_registrations().end()) {
					auto entry = i++;
					if(entry->group == &group && entry->name == name)
						pending_registrations().erase(entry);
				}
			}
		}
	}
}

setting_listener::~setting_listener() throw()
{
}

setting_group::setting_group() throw(std::bad_alloc)
{
	{
		umutex_class m(reg_mutex());
		ready_groups().insert(this);
	}
	run_pending_registrations();
}

setting_group::~setting_group() throw()
{
	{
		umutex_class m(reg_mutex());
		ready_groups().erase(this);
	}
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

void setting_group::register_setting(const std::string& name, setting& _setting) throw(std::bad_alloc)
{
	umutex_class(lock);
	settings[name] = &_setting;
}

void setting_group::unregister_setting(const std::string& name) throw(std::bad_alloc)
{
	umutex_class(lock);
	settings.erase(name);
}

setting::setting(setting_group& group, const std::string& name) throw(std::bad_alloc)
	: in_group(group)
{
	add_registration(group, settingname = name, *this);
}

setting::~setting() throw()
{
	delete_registration(in_group, settingname);
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
