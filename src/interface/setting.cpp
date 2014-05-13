#include "interface/setting.hpp"
#include "library/string.hpp"

core_setting_value::core_setting_value(const core_setting_value_param& p) throw(std::bad_alloc)
	: iname(p.iname), hname(p.hname), index(p.index)
{
}

core_setting::core_setting(const core_setting_param& p)
	: iname(p.iname), hname(p.hname), regex(p.regex ? p.regex : ""), dflt(p.dflt)
{
	for(auto i : p.values)
		values.push_back(core_setting_value(i));
}

bool core_setting::is_boolean() const throw()
{
	if(values.size() != 2)
		return false;
	std::string a = values[0].iname;
	std::string b = values[1].iname;
	if(a > b)
		std::swap(a, b);
	return (a == "0" && b == "1");
}

bool core_setting::is_freetext() const throw()
{
	return (values.size() == 0);
}

bool core_setting::validate(const std::string& value) const
{
	if(values.size() != 0) {
		for(auto i : values)
			if(i.iname == value)
				return true;
		return false;
	} else
		return regex_match(regex, value);
}

core_setting_group::core_setting_group()
{
}

core_setting_group::core_setting_group(std::initializer_list<core_setting_param> _settings)
{
	for(auto i : _settings)
		settings.insert(std::make_pair(i.iname, core_setting(i)));
}

core_setting_group::core_setting_group(std::vector<core_setting_param> _settings)
{
	for(auto i : _settings)
		settings.insert(std::make_pair(i.iname, core_setting(i)));
}

void core_setting_group::fill_defaults(std::map<std::string, std::string>& values) throw(std::bad_alloc)
{
	for(auto i : settings)
		if(!values.count(i.first))
			values[i.first] = i.second.dflt;
}

std::set<std::string> core_setting_group::get_setting_set()
{
	std::set<std::string> r;
	for(auto i : settings)
		r.insert(i.first);
	return r;
}

std::vector<std::string> core_setting::hvalues() const throw(std::runtime_error)
{
	std::vector<std::string> x;
	if(values.size() == 0)
		throw std::runtime_error("hvalues() not valid for freetext settings");
	for(auto i : values)
		x.push_back(i.hname);
	return x;
}

std::string core_setting::hvalue_to_ivalue(const std::string& hvalue) const throw(std::runtime_error)
{
	for(auto i : values)
		if(i.hname == hvalue)
			return i.iname;
	throw std::runtime_error("Invalid hvalue for setting");
}

signed core_setting::ivalue_to_index(const std::string& ivalue) const throw(std::runtime_error)
{
	for(auto i : values)
		if(i.iname == ivalue)
			return i.index;
	throw std::runtime_error("Invalid ivalue for setting");
}
