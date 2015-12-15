#ifndef _loadlib__hpp__included__
#define _loadlib__hpp__included__

#include "library/loadlib.hpp"

void handle_post_loadlibrary();
void autoload_libraries(void(*on_error)(const std::string& libname, const std::string& err, bool system) = NULL);
void with_loaded_library(const loadlib::module& l);
bool with_unloaded_library(loadlib::module& l);
std::string loadlib_debug_get_user_library_dir();
std::string loadlib_debug_get_system_library_dir();


#endif
