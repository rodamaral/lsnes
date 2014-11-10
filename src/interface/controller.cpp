#include "interface/controller.hpp"
#include <algorithm>

namespace
{
	portctrl::index_triple t(unsigned p, unsigned c, unsigned i)
	{
		portctrl::index_triple x;
		x.valid = true;
		x.port = p;
		x.controller = c;
		x.control = i;
		return x;
	}

	void push_port_indices(std::vector<portctrl::index_triple>& tab, unsigned p, portctrl::type& pt)
	{
		unsigned ctrls = pt.controller_info->controllers.size();
		for(unsigned i = 0; i < ctrls; i++)
			for(unsigned j = 0; j < pt.controller_info->controllers[i].buttons.size(); j++)
				tab.push_back(t(p, i, j));
	}
}

struct portctrl::index_map controller_set::portindex()
{
	portctrl::index_map m;
	m.logical_map = logical_map;
	m.pcid_map = logical_map;
	std::sort(m.pcid_map.begin(), m.pcid_map.end());
	for(unsigned i = 0; i < ports.size(); i++)
		push_port_indices(m.indices, i, *ports[i]);
	return m;
}
