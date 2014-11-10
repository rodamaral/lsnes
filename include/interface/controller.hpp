#ifndef _interface__controller__hpp__included__
#define _interface__controller__hpp__included__

#include "library/portctrl-data.hpp"

struct controller_set
{
	struct portctrl::index_map portindex();
	std::vector<portctrl::type*> ports;
	std::vector<std::pair<unsigned, unsigned>> logical_map;
};

extern uint32_t magic_flags;

#endif
