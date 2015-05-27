#ifndef _library__memtracker__hpp__included__
#define _library__memtracker__hpp__included__

#include "threads.hpp"
#include <map>

class memtracker
{
public:
	memtracker();
	~memtracker();
	void operator()(const char* category, ssize_t change);
	void reset(const char* category, size_t value);
	std::map<std::string, size_t> report();
	static memtracker& singleton();
	class autorelease
	{
	public:
		autorelease(memtracker& _track, const char* cat, size_t amount)
			: tracker(_track), category(cat), committed(amount)
		{
			tracker(category, committed);
		}
		~autorelease()
		{
			tracker(category, -(ssize_t)committed);
		}
		void operator()(ssize_t delta)
		{
			if(delta < 0 && committed < (size_t)-delta) {
				tracker(category, -(ssize_t)committed);
				committed = 0;
			} else {
				tracker(category, delta);
				committed = committed + delta;
			}
		}
	private:
		memtracker& tracker;
		const char* category;
		size_t committed;
	};
private:
	bool invalid;
	threads::lock mut;
	std::map<std::string, size_t> data;
	memtracker(const memtracker&);
	memtracker& operator=(const memtracker&);
};


#endif
