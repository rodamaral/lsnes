#include "settingvar.hpp"
#include "register-queue.hpp"

namespace settingvar
{
namespace
{
	typedef register_queue<group, base> regqueue_t;
}

listener::~listener() throw()
{
}

group::group() throw(std::bad_alloc)
{
	regqueue_t::do_ready(*this, true);
}

group::~group() throw()
{
	regqueue_t::do_ready(*this, false);
}

std::set<std::string> group::get_settings_set() throw(std::bad_alloc)
{
	threads::alock(lock);
	std::set<std::string> x;
	for(auto i : settings)
		x.insert(i.first);
	return x;
}

base& group::operator[](const std::string& name)
{
	threads::alock(lock);
	if(!settings.count(name))
		throw std::runtime_error("No such setting");
	return *settings[name];
}

void group::fire_listener(base& var) throw()
{
	std::set<listener*> l;
	{
		threads::alock h(lock);
		for(auto i : listeners)
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
	threads::alock(lock);
	listeners.insert(&listener);
}

void group::remove_listener(struct listener& listener) throw(std::bad_alloc)
{
	threads::alock(lock);
	listeners.erase(&listener);
}

void group::do_register(const std::string& name, base& _setting) throw(std::bad_alloc)
{
	threads::alock h(lock);
	settings[name] = &_setting;
}

void group::do_unregister(const std::string& name, base* dummy) throw(std::bad_alloc)
{
	threads::alock h(lock);
	settings.erase(name);
}

base::base(group& _group, const std::string& _iname, const std::string& _hname)
	throw(std::bad_alloc)
	: sgroup(_group), iname(_iname), hname(_hname)
{
	regqueue_t::do_register(sgroup, iname, *this);
}

base::~base() throw()
{
	regqueue_t::do_unregister(sgroup, iname);
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
		threads::alock h(lock);
		badcache.erase(name);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		if(allow_invalid) {
			threads::alock h(lock);
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


const char* yes_no::enable = "yes";
const char* yes_no::disable = "no";
}
