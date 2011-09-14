#include "settings.hpp"
#include "misc.hpp"
#include <map>
#include <sstream>
#include "misc.hpp"
#include "command.hpp"
#include <iostream>

namespace
{
	std::map<std::string, setting*>* settings;

	class set_command : public command
	{
	public:
		set_command() throw(std::bad_alloc) : command("set-setting") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			std::string syntax = "Syntax: set-setting <setting> [<value>]";
			tokensplitter t(args);
			std::string settingname = t;
			std::string settingvalue = t.tail();
			if(settingname == "")
				throw std::runtime_error("Setting name required.");
			setting_set(settingname, settingvalue);
			out(win) << "Setting '" << settingname << "' set to '" << settingvalue << "'" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "set a setting"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: set-setting <setting> [<value>]\n"
				"Set setting to a new value. Omit <value> to set to ''\n";
		}
	} set_setting;
	
	class unset_command : public command
	{
	public:
		unset_command() throw(std::bad_alloc) : command("unset-setting") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			std::string syntax = "Syntax: unset-setting <setting>";
			tokensplitter t(args);
			std::string settingname = t;
			if(settingname == "" || t)
				throw std::runtime_error("Expected setting name and nothing else");
			setting_blank(settingname);
			out(win) << "Setting '" << settingname << "' unset" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "unset a setting"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: unset-setting <setting>\n"
				"Try to unset a setting. Note that not all settings can be unset\n";
		}
	} unset_setting;

	class get_command : public command
	{
	public:
		get_command() throw(std::bad_alloc) : command("get-setting") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			std::string settingname = t;
			if(settingname == "" || t.tail() != "")
				throw std::runtime_error("Expected setting name and nothing else");
			if(!setting_isblank(settingname))
				out(win) << "Setting '" << settingname << "' has value '" << setting_get(settingname)
					<< "'" << std::endl;
			else
				out(win) << "Setting '" << settingname << "' unset" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "get value of a setting"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: get-setting <setting>\n"
				"Show value of setting\n";
		}
	} get_setting;
	
	class shsettings_command : public command
	{
	public:
		shsettings_command() throw(std::bad_alloc) : command("show-settings") {}
		void invoke(const std::string& args, window* win) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take arguments");
			setting_print_all(win);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Show value of all settings"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: show-settings\n"
				"Show value of all settings\n";
		}
	} sh_setting;
}

setting::setting(const std::string& name) throw(std::bad_alloc)
{
	if(!settings)
		settings = new std::map<std::string, setting*>();
	(*settings)[settingname = name] = this;
}

setting::~setting() throw()
{
	if(!settings)
		return;
	settings->erase(settingname);
}

void setting_set(const std::string& _setting, const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings || !settings->count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	try {
		(*settings)[_setting]->set(value);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Can't set setting '" + _setting + "': " + e.what());
	}
}

void setting_blank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings || !settings->count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	try {
		(*settings)[_setting]->blank();
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Can't blank setting '" + _setting + "': " + e.what());
	}
}

std::string setting_get(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings || !settings->count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	return (*settings)[_setting]->get();
}

bool setting_isblank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error)
{
	if(!settings || !settings->count(_setting))
		throw std::runtime_error("No such setting '" + _setting + "'");
	return !((*settings)[_setting]->is_set());
}

void setting_print_all(window* win) throw(std::bad_alloc)
{
	if(!settings)
		return;
	for(auto i = settings->begin(); i != settings->end(); i++) {
		if(!i->second->is_set())
			out(win) << i->first << ": (unset)" << std::endl;
		else
			out(win) << i->first << ": " << i->second->get() << std::endl;
	}
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
	if(v == "true" || v == "yes" || v == "on" || v == "1" || v == "enable" || v == "enabled")
		value = true;
	else if(v == "false" || v == "no" || v == "off" || v == "0" || v == "disable" || v == "disabled")
		value = false;
	else
		throw std::runtime_error("Invalid value for boolean setting");
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
