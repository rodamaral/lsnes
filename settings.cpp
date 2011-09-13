#include "settings.hpp"
#include "misc.hpp"
#include <map>
#include <sstream>
#include "misc.hpp"
#include <iostream>

namespace
{
	std::map<std::string, setting*>* settings;
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
