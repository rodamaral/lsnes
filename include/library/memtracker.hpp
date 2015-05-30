#ifndef _library__memtracker__hpp__included__
#define _library__memtracker__hpp__included__

#include "threads.hpp"
#include <cstring>
#include <map>

class text;

class memtracker
{
public:
	memtracker();
	~memtracker();
	void operator()(const char* category, ssize_t change);
	void reset(const char* category, size_t value);
	std::map<text, size_t> report();
	static memtracker& singleton();
	class autorelease
	{
	public:
		autorelease(size_t amount)
			: tracker(NULL), category(NULL), committed(amount)
		{
		}
		autorelease(memtracker& _track, const char* cat, size_t amount)
			: tracker(&_track), category(cat), committed(amount)
		{
			if(category && tracker)
				(*tracker)(category, committed);
		}
		~autorelease()
		{
			if(committed && category && tracker)
				(*tracker)(category, -(ssize_t)committed);
		}
		void operator()(ssize_t delta)
		{
			if(delta < 0 && committed < (size_t)-delta) {
				if(category && tracker)
					(*tracker)(category, -(ssize_t)committed);
				committed = 0;
			} else {
				if(category && tracker)
					(*tracker)(category, delta);
				committed = committed + delta;
			}
		}
		void untrack()
		{
			if(category && tracker)
				(*tracker)(category, -(ssize_t)committed);
			category = NULL;
			tracker = NULL;
		}
		void track(memtracker& _tracker, const char* cat)
		{
			if(category && tracker)
				(*tracker)(category, -(ssize_t)committed);
			category = cat;
			tracker = &_tracker;
			if(category && tracker)
				(*tracker)(category, committed);
		}
		memtracker& get_tracker() const throw() { return *tracker; }
		const char* get_category() const throw() { return category; }
	private:
		memtracker* tracker;
		const char* category;
		size_t committed;
	};
private:
	class cstr_container
	{
	public:
		cstr_container(const char* _str) { str = _str; }
		const char* as_str() const { return str; }
		bool operator<(const cstr_container& x) const { return strcmp(str, x.str) < 0; }
		bool operator<=(const cstr_container& x) const { return strcmp(str, x.str) <= 0; }
		bool operator==(const cstr_container& x) const { return strcmp(str, x.str) == 0; }
		bool operator!=(const cstr_container& x) const { return strcmp(str, x.str) != 0; }
		bool operator>=(const cstr_container& x) const { return strcmp(str, x.str) >= 0; }
		bool operator>(const cstr_container& x) const { return strcmp(str, x.str) > 0; }
	private:
		const char* str;
	};
	bool invalid;
	threads::lock mut;
	std::map<cstr_container, size_t> data;
	memtracker(const memtracker&);
	memtracker& operator=(const memtracker&);
};


#endif
