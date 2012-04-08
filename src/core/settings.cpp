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
}

setting::setting(const std::string& name) throw(std::bad_alloc)
{
	settings()[settingname = name] = this;
}

setting::~setting() throw()
{
	settings().erase(settingname);
}

void setting::set(const std::string& _setting, const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings().count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	try {
		settings()[_setting]->set(value);
		information_dispatch::do_setting_change(_setting, value);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Can't set setting '" + _setting + "': " + e.what());
	}
}

void setting::blank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings().count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	try {
		settings()[_setting]->blank();
		information_dispatch::do_setting_clear(_setting);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Can't blank setting '" + _setting + "': " + e.what());
	}
}

std::string setting::get(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings().count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	return settings()[_setting]->get();
}

bool setting::is_set(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings().count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	return settings()[_setting]->is_set();
}

std::set<std::string> setting::get_settings_set() throw(std::bad_alloc)
{
	std::set<std::string> r;
	for(auto i : settings())
		r.insert(i.first);
	return r;
}


numeric_setting::numeric_setting(const std::string& sname, int32_t minv, int32_t maxv, int32_t dflt)
	throw(std::bad_alloc)
	: setting(sname)
{
	minimum = minv;
	maximum = maxv;
	value = dflt;
}

void numeric_setting::blank() throw(std::bad_alloc, std::runtime_error)
{
	throw std::runtime_error("This setting can't be blanked");
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
	return value;
}

boolean_setting::boolean_setting(const std::string& sname, bool dflt) throw(std::bad_alloc)
	: setting(sname)
{
	value = dflt;
}
void boolean_setting::blank() throw(std::bad_alloc, std::runtime_error)
{
	throw std::runtime_error("This setting can't be unset");
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
	return value;
}

path_setting::path_setting(const std::string& sname) throw(std::bad_alloc)
	: setting(sname)
{
	path = ".";
	_default = true;
}

void path_setting::blank() throw(std::bad_alloc, std::runtime_error)
{
	path = ".";
	_default = true;
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
	return path;
}