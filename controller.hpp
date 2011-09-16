#ifndef _controller__hpp__included__
#define _controller__hpp__included__

#include "controllerdata.hpp"

int controller_index_by_logical(unsigned lid);
int controller_index_by_analog(unsigned aid);
bool controller_ismouse_by_analog(unsigned aid);
devicetype_t controller_type_by_logical(unsigned lid);
void controller_set_port_type(unsigned port, porttype_t ptype, bool set_core = true);

#endif