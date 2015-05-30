#ifndef _library__directory__hpp__included__
#define _library__directory__hpp__included__

#include <set>
#include <string>
#include <cstdlib>
#include "text.hpp"

namespace directory
{
std::set<text> enumerate(const text& dir, const text& match);
text absolute_path(const text& relative);
uintmax_t size(const text& path);
time_t mtime(const text& path);
bool exists(const text& filename);
bool is_regular(const text& filename);
bool is_directory(const text& filename);
bool ensure_exists(const text& path);
int rename_overwrite(const char* oldname, const char* newname);
}

#endif
