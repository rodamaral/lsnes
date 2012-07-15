#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/globalwrap.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"

#include <map>
#include <sstream>
#include <iostream>

namespace
{
	globalwrap<std::map<std::string, setting*>> settings;

	function_ptr_command<const std::string&> set_setting("set-setting", "set a setting",
		"Syntax: set-setting <setting> [<value>]\nSet setting to a new value. Omit <value> to set to ''\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)([ \t]+(|[^ \t].*))?", t, "Setting name required.");
			setting::set(r[1], r[3]);
			messages << "Setting '" << r[1] << "' set to '" << r[3] << "'" << std::endl;
		});

	function_ptr_command<const std::string&> unset_setting("unset-setting", "unset a setting",
		"Syntax: unset-setting <setting>\nTry to unset a setting. Note that not all settings can be unset\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]*", t, "Expected setting name and nothing else");
			setting::blank(r[1]);
			messages << "Setting '" << r[1] << "' unset" << std::endl;
		});

	function_ptr_command<const std::string&> get_command("get-setting", "get value of a setting",
		"Syntax: get-setting <setting>\nShow value of setting\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]*", t, "Expected setting name and nothing else");
			if(setting::is_set(r[1]))
				messages << "Setting '" << r[1] << "' has value '"
					<< setting::get(r[1]) << "'" << std::endl;
			else
				messages << "Setting '" << r[1] << "' is unset" << std::endl;
		});

	function_ptr_command<> show_settings("show-settings", "Show values of all settings",
		"Syntax: show-settings\nShow value of all settings\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : setting::get_settings_set()) {
				if(!setting::is_set(i))
					messages << i << ": (unset)" << std::endl;
				else
					messages << i << ": " << setting::get(i) << std::endl;
			}
		});

	//Return the mutex.
	mutex& stlock()
	{
		static mutex& m = mutex::aquire();
		return m;
	}

	class stlock_hold
	{
	public:
		stlock_hold() { stlock().lock(); }
		~stlock_hold() { stlock().unlock(); }
	private:
		stlock_hold(const stlock_hold& k);
		stlock_hold& operator=(const stlock_hold& k);
	};
}

std::map<std::string, std::string> setting::invalid_values;
bool setting::storage_mode = false;

setting::setting(const std::string& name) throw(std::bad_alloc)
{
	stlock_hold lck;
	mut = &mutex::aquire();
	settings()[settingname = name] = this;
}

setting::~setting() throw()
{
	stlock_hold lck;
	delete mut;
	settings().erase(settingname);
}

setting* setting::get_by_name(const std::string& name)
{
	stlock_hold lck;
	if(!settings().count(name))
		throw std::runtime_error("No such setting '" + name + "'");
	return settings()[name];
}

void setting::set(const std::string& _setting, const std::string& value) throw(std::bad_alloc, std::runtime_error)
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
			lock_holder lck(tochange);
			tochange->set(value);
			invalid_values.erase(_setting);
		}
		information_dispatch::do_setting_change(_setting, value);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		if(storage_mode)
			invalid_values[_setting] = value;
		throw std::runtime_error("Can't set setting '" + _setting + "': " + e.what());
	}
}

bool setting::blank(bool really) throw(std::bad_alloc, std::runtime_error)
{
	return false;
}

bool setting::blankable(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	try {
		{
			lock_holder lck(tochange);
			return tochange->blank(false);
		}
	} catch(...) {
		return false;
	}
}

void setting::blank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	try {
		{
			lock_holder lck(tochange);
			if(!tochange->blank(true))
				throw std::runtime_error("This setting can't be cleared");
			invalid_values.erase(_setting);
		}
		information_dispatch::do_setting_clear(_setting);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Can't clear setting '" + _setting + "': " + e.what());
	}
}

std::string setting::get(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	{
		lock_holder lck(tochange);
		return tochange->get();
	}
}

bool setting::is_set(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	setting* tochange = get_by_name(_setting);
	{
		lock_holder lck(tochange);
		return tochange->is_set();
	}
}

std::set<std::string> setting::get_settings_set() throw(std::bad_alloc)
{
	std::set<std::string> r;
	for(auto i : settings())
		r.insert(i.first);
	return r;
}

void setting::set_storage_mode(bool enable) throw()
{
	storage_mode = enable;
}

std::map<std::string, std::string> setting::get_invalid_values() throw(std::bad_alloc)
{
	return invalid_values;
}


numeric_setting::numeric_setting(const std::string& sname, int32_t minv, int32_t maxv, int32_t dflt)
	throw(std::bad_alloc)
	: setting(sname)
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

boolean_setting::boolean_setting(const std::string& sname, bool dflt) throw(std::bad_alloc)
	: setting(sname)
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

path_setting::path_setting(const std::string& sname) throw(std::bad_alloc)
	: setting(sname)
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