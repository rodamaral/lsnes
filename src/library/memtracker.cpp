#include "memtracker.hpp"

void memtracker::operator()(const char* category, ssize_t change)
{
	if(invalid) return;
	std::string cat = category;
	threads::alock h(mut);
	if(change > 0) {
		if(data.count(cat))
			data[cat] = data[cat] + change;
		else
			data[cat] = change;
	} else if(change < 0 && data.count(cat)) {
		if(data[cat] <= (size_t)-change)
			data[cat] = 0;
		else
			data[cat] = data[cat] + change;
	}
}

void memtracker::reset(const char* category, size_t value)
{
	if(invalid) return;
	std::string cat = category;
	threads::alock h(mut);
	data[cat] = value;
}

std::map<std::string, size_t> memtracker::report()
{
	std::map<std::string, size_t> ret;
	if(!invalid) {
		threads::alock h(mut);
		ret = data;
	}
	return ret;
}

memtracker::memtracker()
{
	invalid = false;
}
memtracker::~memtracker()
{
	invalid = true;
}

memtracker& memtracker::singleton()
{
	static memtracker x;
	return x;
}
