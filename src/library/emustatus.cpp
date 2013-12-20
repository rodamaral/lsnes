#include "emustatus.hpp"

emulator_status::emulator_status() throw(std::bad_alloc)
{
}

emulator_status::~emulator_status() throw()
{
}

void emulator_status::set(const std::string& key, const std::string& value) throw(std::bad_alloc)
{
	umutex_class h(lock);
	content[key] = utf8::to32(value);
}

void emulator_status::set(const std::string& key, const std::u32string& value) throw(std::bad_alloc)
{
	umutex_class h(lock);
	content[key] = value;
}

bool emulator_status::haskey(const std::string& key) throw()
{
	umutex_class h(lock);
	return (content.count(key) != 0);
}

void emulator_status::erase(const std::string& key) throw()
{
	umutex_class h(lock);
	content.erase(key);
}

std::u32string emulator_status::get(const std::string& key) throw(std::bad_alloc)
{
	umutex_class h(lock);
	return content[key];
}

emulator_status::iterator emulator_status::first() throw(std::bad_alloc)
{
	iterator i;
	i.not_valid = true;
	return i;
}

bool emulator_status::next(iterator& itr) throw(std::bad_alloc)
{
	umutex_class h(lock);
	std::map<std::string, std::u32string>::iterator j;
	if(itr.not_valid)
		j = content.lower_bound("");
	else
		j = content.upper_bound(itr.key);
	if(j == content.end()) {
		itr.not_valid = true;
		itr.key = "";
		itr.value = U"";
		return false;
	} else {
		itr.not_valid = false;
		itr.key = j->first;
		itr.value = j->second;
		return true;
	}
}
