#ifndef _memorymanip__hpp__included__
#define _memorymanip__hpp__included__

#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "library/memoryspace.hpp"

class cart_mappings_refresher
{
public:
	cart_mappings_refresher(memory_space& _mspace);
	void operator()() throw(std::bad_alloc);
private:
	memory_space& mspace;
};

#endif
