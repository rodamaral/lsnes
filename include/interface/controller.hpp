#ifndef _interface__controller__hpp__included__
#define _interface__controller__hpp__included__

#include "library/controller-data.hpp"

struct controller_set
{
	struct port_index_map portindex;
	std::vector<port_type*> ports;
};

#endif
