#ifndef _loadlib__hpp__included__
#define _loadlib__hpp__included__

#include <string>

void load_library(const std::string& filename);
extern const bool load_library_supported;
extern const char* library_is_called;

#endif