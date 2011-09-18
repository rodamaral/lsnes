#include "controller.hpp"
#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

namespace
{
	porttype_t porttypes[2];
	int analog_indices[3] = {-1, -1, -1};
	bool analog_is_mouse[3];

	void update_analog_indices() throw()
	{
		int i = 0;
		for(unsigned j = 0; j < sizeof(analog_indices) / sizeof(analog_indices[0]); j++)
			analog_indices[j] = -1;
		for(unsigned j = 0; j < 8; j++) {
			devicetype_t d = controller_type_by_logical(j);
			switch(d) {
			case DT_NONE:
			case DT_GAMEPAD:
				break;
			case DT_MOUSE:
				analog_is_mouse[i] = true;
				analog_indices[i++] = j;
				break;
			case DT_SUPERSCOPE:
			case DT_JUSTIFIER:
				analog_is_mouse[i] = false;
				analog_indices[i++] = j;
				break;
			}
		}
	}
}

int controller_index_by_logical(unsigned lid) throw()
{
	bool p1multitap = (porttypes[0] == PT_MULTITAP);
	unsigned p1devs = port_types[porttypes[0]].devices;
	unsigned p2devs = port_types[porttypes[1]].devices;
	if(lid >= p1devs + p2devs)
		return -1;
	if(!p1multitap)
		if(lid < p1devs)
			return lid;
		else
			return 4 + lid - p1devs;
	else
		if(lid == 0)
			return 0;
		else if(lid < 5)
			return lid + 3;
		else
			return lid - 4;
}

int controller_index_by_analog(unsigned aid) throw()
{
	if(aid > 2)
		return -1;
	return analog_indices[aid];
}

bool controller_ismouse_by_analog(unsigned aid) throw()
{
	if(aid > 2)
		return false;
	return analog_is_mouse[aid];
}

devicetype_t controller_type_by_logical(unsigned lid) throw()
{
	int x = controller_index_by_logical(lid);
	if(x < 0)
		return DT_NONE;
	enum porttype_t rawtype = porttypes[x >> 2];
	if((x & 3) < port_types[rawtype].devices)
		return port_types[rawtype].dtype;
	else
		return DT_NONE;
}

void controller_set_port_type(unsigned port, porttype_t ptype, bool set_core) throw()
{
	if(set_core && ptype != PT_INVALID)
		snes_set_controller_port_device(port != 0, port_types[ptype].bsnes_type);
	porttypes[port] = ptype;
	update_analog_indices();
}
