#ifndef _library__filelist__hpp__included__
#define _library__filelist__hpp__included__

#include <map>
#include <set>
#include <string>
#include "text.hpp"

/**
 * List of files.
 */
class filelist
{
public:
/**
 * Create a new list, backed by specific file.
 */
	filelist(const text& backingfile, const text& directory);
/**
 * Dtor.
 */
	~filelist();
/**
 * Enumerate the files on the list. Files that don't have matching timestamp are auto-removed.
 */
	std::set<text> enumerate();
/**
 * Add a file to the list. Current timestamp is used to mark version.
 */
	void add(const text& filename);
/**
 * Remove a file from the list.
 */
	void remove(const text& filename);
/**
 * Rename a file from the list.
 */
	void rename(const text& oldname, const text& newname);
private:
	filelist(const filelist&);
	filelist& operator=(const filelist&);
	std::map<text, int64_t> readfile();
	void check_stale(std::map<text, int64_t>& data);
	void writeback(const std::map<text, int64_t>& data);
	text backingfile;
	text directory;
};

#endif
