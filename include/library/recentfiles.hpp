#ifndef _library__recentfiles__hpp__included__
#define _library__recentfiles__hpp__included__

#include <string>
#include <cstdlib>
#include <list>

class recent_files
{
public:
	struct hook
	{
		virtual ~hook();
		virtual void operator()() = 0;
	};
	recent_files(const std::string& cfgfile, size_t maxcount);
	void add(const std::string& file);
	void add_hook(hook& h);
	void remove_hook(hook& h);
	std::list<std::string> get();
private:
	std::string cfgfile;
	size_t maxcount;
	std::list<hook*> hooks;
};

#endif
