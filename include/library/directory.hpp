#ifndef _library__directory__hpp__included__
#define _library__directory__hpp__included__

#include <set>
#include <string>
#include <cstdlib>

namespace directory
{
std::set<std::string> enumerate(const std::string& dir, const std::string& match);
std::string absolute_path(const std::string& relative);
uintmax_t size(const std::string& path);
time_t mtime(const std::string& path);
bool exists(const std::string& filename);
bool is_regular(const std::string& filename);
bool is_directory(const std::string& filename);
bool ensure_exists(const std::string& path);
int rename_overwrite(const char* oldname, const char* newname);
}

#endif
