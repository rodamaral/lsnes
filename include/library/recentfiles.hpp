#ifndef _library__recentfiles__hpp__included__
#define _library__recentfiles__hpp__included__

#include <string>
#include <cstdlib>
#include <list>
#include <vector>

class recentfile_path
{
public:
	recentfile_path();
	recentfile_path(const std::string& p);
	std::string serialize() const;
	static recentfile_path deserialize(const std::string& s);
	bool check() const;
	std::string display() const;
	std::string get_path() const;
	bool operator==(const recentfile_path& p) const;
private:
	std::string path;
};

class recentfile_multirom
{
public:
	recentfile_multirom();
	std::string serialize() const;
	static recentfile_multirom deserialize(const std::string& s);
	bool check() const;
	std::string display() const;
	bool operator==(const recentfile_multirom& p) const;

	std::string packfile;
	std::string singlefile;
	std::string core;
	std::string system;
	std::string region;
	std::vector<std::string> files;
};

struct recent_files_hook
{
	virtual ~recent_files_hook();
	virtual void operator()() = 0;
};

template<class T>
class recent_files
{
public:
	recent_files(const std::string& cfgfile, size_t maxcount) __attribute__((noinline));
	void add(const T& file);
	void add_hook(recent_files_hook& h);
	void remove_hook(recent_files_hook& h);
	std::list<T> get();
private:
	std::string cfgfile;
	size_t maxcount;
	std::list<recent_files_hook*> hooks;
};

#endif
