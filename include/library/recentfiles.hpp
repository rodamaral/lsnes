#ifndef _library__recentfiles__hpp__included__
#define _library__recentfiles__hpp__included__

#include <string>
#include <cstdlib>
#include <list>
#include <vector>

namespace recentfiles
{
class path
{
public:
	path();
	path(const std::string& p);
	std::string serialize() const;
	static path deserialize(const std::string& s);
	bool check() const;
	std::string display() const;
	std::string get_path() const;
	bool operator==(const path& p) const;
private:
	std::string pth;
};

class multirom
{
public:
	multirom();
	std::string serialize() const;
	static multirom deserialize(const std::string& s);
	bool check() const;
	std::string display() const;
	bool operator==(const multirom& p) const;

	std::string packfile;
	std::string singlefile;
	std::string core;
	std::string system;
	std::string region;
	std::vector<std::string> files;
};

class namedobj
{
public:
	namedobj();
	std::string serialize() const;
	static namedobj deserialize(const std::string& s);
	bool check() const;
	std::string display() const;
	bool operator==(const namedobj& p) const;

	std::string _id;
	std::string _filename;
	std::string _display;
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
	set(const std::string& cfgfile, size_t maxcount) __attribute__((noinline));
	void add(const T& file);
	void add_hook(hook& h);
	void remove_hook(hook& h);
	std::list<T> get();
private:
	std::string cfgfile;
	size_t maxcount;
	std::list<hook*> hooks;
};
}
#endif
