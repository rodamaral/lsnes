#ifndef _memorymanip__hpp__included__
#define _memorymanip__hpp__included__

#include <string>
#include <list>
#include <vector>
#include <cstdint>
#include <stdexcept>

class memory_space;
class movie_logic;

class cart_mappings_refresher
{
public:
	cart_mappings_refresher(memory_space& _mspace, movie_logic& _mlogic);
	void operator()() throw(std::bad_alloc);
private:
	memory_space& mspace;
	movie_logic& mlogic;
};

#endif
