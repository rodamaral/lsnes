#ifndef _loadlib__hpp__included__
#define _loadlib__hpp__included__

#include "library/loadlib.hpp"

void handle_post_loadlibrary();
void autoload_libraries();
void with_loaded_library(loaded_library* l);

#endif
