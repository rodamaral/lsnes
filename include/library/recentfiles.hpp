#ifndef _library__recentfiles__hpp__included__
#define _library__recentfiles__hpp__included__

#include <string>
#include <cstdlib>
#include <list>
#include <vector>
#include "text.hpp"

namespace recentfiles
{
class path
{
public:
	path();
	path(const text& p);
	text serialize() const;
	static path deserialize(const text& s);
	bool check() const;
	text display() const;
	text get_path() const;
	bool operator==(const path& p) const;
private:
	text pth;
};

class multirom
{
public:
	multirom();
	text serialize() const;
	static multirom deserialize(const text& s);
	bool check() const;
	text display() const;
	bool operator==(const multirom& p) const;

	text packfile;
	text singlefile;
	text core;
	text system;
	text region;
	std::vector<text> files;
};

class namedobj
{
public:
	namedobj();
	text serialize() const;
	static namedobj deserialize(const text& s);
	bool check() const;
	text display() const;
	bool operator==(const namedobj& p) const;

	text _id;
	text _filename;
	text _display;
};

struct hook
{
	virtual ~hook();
	virtual void operator()() = 0;
};

template<class T>
class set
{
public:
	set(const text& cfgfile, size_t maxcount) __attribute__((noinline));
	void add(const T& file);
	void add_hook(hook& h);
	void remove_hook(hook& h);
	std::list<T> get();
private:
	text cfgfile;
	size_t maxcount;
	std::list<hook*> hooks;
};
}
#endif
