#ifndef _library__memtracker__hpp__included__
#define _library__memtracker__hpp__included__

#include "threads.hpp"
#include <map>

class memtracker
{
public:
	void operator()(const char* category, ssize_t change);
	void reset(const char* category, size_t value);
	std::map<std::string, size_t> report();
private:
	threads::lock mut;
	std::map<std::string, size_t> data;
};

#endif