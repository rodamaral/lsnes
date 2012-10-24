#ifndef _memorymanip__hpp__included__
#define _memorymanip__hpp__included__

#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "library/memoryspace.hpp"

extern memory_space lsnes_memory;

void refresh_cart_mappings() throw(std::bad_alloc);

#endif
