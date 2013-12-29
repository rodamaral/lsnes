#ifndef _loadlib__hpp__included__
#define _loadlib__hpp__included__

#include "library/loadlib.hpp"

void handle_post_loadlibrary();
void autoload_libraries(void(*on_error)(const std::string& libname, const std::string& err) = NULL);
void with_loaded_library(const loadlib::module& l);

#endif
