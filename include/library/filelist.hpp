#ifndef _library__filelist__hpp__included__
#define _library__filelist__hpp__included__

#include <map>
#include <set>
#include <string>

/**
 * List of files.
 */
class filelist
{
public:
/**
 * Create a new list, backed by specific file.
 */
	filelist(const std::string& backingfile, const std::string& directory);
/**
 * Dtor.
 */
	~filelist();
/**
 * Enumerate the files on the list. Files that don't have matching timestamp are auto-removed.
 */
	std::set<std::string> enumerate();
/**
 * Add a file to the list. Current timestamp is used to mark version.
 */
	void add(const std::string& filename);
/**
 * Remove a file from the list.
 */
	void remove(const std::string& filename);
/**
 * Rename a file from the list.
 */
	void rename(const std::string& oldname, const std::string& newname);
private:
	filelist(const filelist&);
	filelist& operator=(const filelist&);
	std::map<std::string, int64_t> readfile();
	void check_stale(std::map<std::string, int64_t>& data);
	void writeback(const std::map<std::string, int64_t>& data);
	std::string backingfile;
	std::string directory;
};

#endif
