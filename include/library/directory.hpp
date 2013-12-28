#ifndef _library__directory__hpp__included__
#define _library__directory__hpp__included__

#include <set>
#include <string>

std::set<std::string> enumerate_directory(const std::string& dir, const std::string& match);
std::string get_absolute_path(const std::string& relative);
uintmax_t file_get_size(const std::string& path);
time_t file_get_mtime(const std::string& path);
bool file_exists(const std::string& filename);
bool file_is_regular(const std::string& filename);
bool file_is_directory(const std::string& filename);
bool ensure_directory_exists(const std::string& path);

#endif
