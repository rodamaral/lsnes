#include "interface/setting.hpp"
#include "library/register-queue.hpp"
#include "library/string.hpp"

core_setting_value::core_setting_value(struct core_setting& _setting, const std::string& _iname,
	const std::string& _hname, signed _index) throw(std::bad_alloc)
	: setting(_setting), iname(_iname), hname(_hname), index(_index)
{
	register_queue<core_setting, core_setting_value>::do_register(setting, iname, *this);
}

core_setting_value::~core_setting_value()
{
	register_queue<core_setting, core_setting_value>::do_unregister(setting, iname);
}

core_setting::core_setting(core_setting_group& _group, const std::string& _iname, const std::string& _hname,
	const std::string& _dflt) throw(std::bad_alloc)
	: group(_group), iname(_iname), hname(_hname), dflt(_dflt), regex(".*")
{
	register_queue<core_setting, core_setting_value>::do_ready(*this, true);
	register_queue<core_setting_group, core_setting>::do_register(group, iname, *this);
}

core_setting::core_setting(core_setting_group& _group, const std::string& _iname, const std::string& _hname,
	const std::string& _dflt, const std::string& _regex) throw(std::bad_alloc)
	: group(_group), iname(_iname), hname(_hname), dflt(_dflt), regex(_regex)
{
	register_queue<core_setting, core_setting_value>::do_ready(*this, true);
	register_queue<core_setting_group, core_setting>::do_register(group, iname, *this);
}

core_setting::~core_setting()
{
	register_queue<core_setting, core_setting_value>::do_ready(*this, false);
	register_queue<core_setting_group, core_setting>::do_unregister(group, iname);
}

void core_setting::do_register(const std::string& name, core_setting_value& value)
{
	values.push_back(&value);
}

void core_setting::do_unregister(const std::string& name)
{
	for(auto i = values.begin(); i != values.end(); i++)
		if((*i)->iname == name) {
			values.erase(i);
			return;
		}
}

bool core_setting::is_boolean() const throw()
{
	if(values.size() != 2)
		return false;
	std::string a = values[0]->iname;
	std::string b = values[1]->iname;
	if(a < b)
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
			if(i->iname == value)
				return true;
		return false;
	} else
		return regex_match(regex, value);
}

core_setting_group::core_setting_group() throw()
{
	register_queue<core_setting_group, core_setting>::do_ready(*this, true);
}
core_setting_group::~core_setting_group() throw()
{
	register_queue<core_setting_group, core_setting>::do_ready(*this, false);
}

void core_setting_group::do_register(const std::string& name, core_setting& setting)
{
	settings[name] = &setting;
}

void core_setting_group::do_unregister(const std::string& name)
{
	settings.erase(name);
}

void core_setting_group::fill_defaults(std::map<std::string, std::string>& values) throw(std::bad_alloc)
{
	for(auto i : settings)
		if(!values.count(i.first))
			values[i.first] = i.second->dflt;
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
		x.push_back(i->hname);
	return x;
}

std::string core_setting::hvalue_to_ivalue(const std::string& hvalue) const throw(std::runtime_error)
{
	for(auto i : values)
		if(i->hname == hvalue)
			return i->iname;
	throw std::runtime_error("Invalid hvalue for setting");
}

signed core_setting::ivalue_to_index(const std::string& ivalue) const throw(std::runtime_error)
{
	for(auto i : values)
		if(i->iname == ivalue)
			return i->index;
	throw std::runtime_error("Invalid ivalue for setting");
}
