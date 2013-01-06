#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "interface/romtype.hpp"

#include <cstdio>
#include <iostream>

namespace
{
	port_type_set dummytypes;
}

controller_state::controller_state() throw()
{
	types = &dummytypes;
}

std::pair<int,int> controller_state::lcid_to_pcid(unsigned lcid) throw()
{
	if(lcid >= types->number_of_controllers())
		return std::make_pair(-1, -1);
	auto k = types->lcid_to_pcid(lcid);
	return std::make_pair(k.first, k.second);
}

std::pair<int, int> controller_state::legacy_pcid_to_pair(unsigned pcid) throw()
{
	if(pcid >= types->number_of_legacy_pcids())
		return std::make_pair(-1, -1);
	auto k = types->legacy_pcid_to_pair(pcid);
	return std::make_pair(k.first, k.second);
}


controller_frame controller_state::get(uint64_t framenum) throw()
{
	if(_autofire.size())
		return _input ^ _framehold ^ _autohold ^ _autofire[framenum % _autofire.size()];
	else
		return _input ^ _framehold ^ _autohold;
}

void controller_state::analog(unsigned port, unsigned controller, unsigned control, short x) throw()
{
	_input.axis3(port, controller, control, x);
}

void controller_state::autohold2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw()
{
	_autohold.axis3(port, controller, pbid, newstate ? 1 : 0);
	information_dispatch::do_autohold_update(port, controller, pbid, newstate);
}

bool controller_state::autohold2(unsigned port, unsigned controller, unsigned pbid) throw()
{
	return (_autohold.axis3(port, controller, pbid) != 0);
}

void controller_state::reset_framehold() throw()
{
	_framehold = _framehold.blank_frame();
}

void controller_state::framehold2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw()
{
	_framehold.axis3(port, controller, pbid, newstate ? 1 : 0);
}

bool controller_state::framehold2(unsigned port, unsigned controller, unsigned pbid) throw()
{
	return (_framehold.axis3(port, controller, pbid) != 0);
}

void controller_state::button2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw()
{
	_input.axis3(port, controller, pbid, newstate ? 1 : 0);
}

bool controller_state::button2(unsigned port, unsigned controller, unsigned pbid) throw()
{
	return (_input.axis3(port, controller, pbid) != 0);
}

void controller_state::autofire(std::vector<controller_frame> pattern) throw(std::bad_alloc)
{
	_autofire = pattern;
}

void reread_active_buttons();

void controller_state::set_ports(const port_type_set& ptype) throw(std::runtime_error)
{
	const port_type_set* oldtype = types;
	types = &ptype;
	if(oldtype != types) {
		_input.set_types(ptype);
		_autohold.set_types(ptype);
		_committed.set_types(ptype);
		_framehold.set_types(ptype);
		//The old autofire pattern no longer applies.
		_autofire.clear();
		_autohold = _autohold.blank_frame();
	}
	reread_active_buttons();
	information_dispatch::do_autohold_reconfigure();
}

controller_frame controller_state::get_blank() throw()
{
	return _input.blank_frame();
}

controller_frame controller_state::commit(uint64_t framenum) throw()
{
	controller_frame f = get(framenum);
	_committed = f;
	return _committed;
}

controller_frame controller_state::get_committed() throw()
{
	return _committed;
}

controller_frame controller_state::commit(controller_frame controls) throw()
{
	_committed = controls;
	return _committed;
}

bool controller_state::is_present(unsigned port, unsigned controller) throw()
{
	return _input.is_present(port, controller);
}
