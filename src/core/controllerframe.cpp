#include "core/emucore.hpp"

#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"

#include <cstdio>
#include <iostream>

namespace
{
	port_type_set dummytypes;
}

controller_state::controller_state() throw()
{
	for(size_t i = 0; i < MAX_ANALOG; i++) {
		analog_indices[i] = std::make_pair(-1, -1);
		analog_mouse[i] = false;
	}
	types = &dummytypes;
}

std::pair<int,int> controller_state::lcid_to_pcid(unsigned lcid) throw()
{
	if(lcid >= types->number_of_controllers())
		return std::make_pair(-1, -1);
	auto k = types->lcid_to_pcid(lcid);
	return std::make_pair(k.first, k.second);
}

std::pair<int,int> controller_state::acid_to_pcid(unsigned acid) throw()
{
	if(acid > MAX_ANALOG)
		return std::make_pair(-1, -1);
	return analog_indices[acid];
}

bool controller_state::acid_is_mouse(unsigned acid) throw()
{
	if(acid > MAX_ANALOG)
		return -1;
	return analog_mouse[acid];
	
}

controller_frame controller_state::get(uint64_t framenum) throw()
{
	if(_autofire.size())
		return _input ^ _framehold ^ _autohold ^ _autofire[framenum % _autofire.size()];
	else
		return _input ^ _framehold ^ _autohold;
}

void controller_state::analog(unsigned port, unsigned controller, int x, int y) throw()
{
	_input.axis3(port, controller, 0, x);
	_input.axis3(port, controller, 1, y);
}

void controller_state::reset(int32_t delay) throw()
{
	if(delay >= 0) {
		_input.axis3(0, 0, 1, 1);
		_input.axis3(0, 0, 2, delay / 10000);
		_input.axis3(0, 0, 3, delay % 10000);
	} else {
		_input.axis3(0, 0, 1, 0);
		_input.axis3(0, 0, 2, 0);
		_input.axis3(0, 0, 3, 0);
	}
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

int controller_state::button_id(unsigned port, unsigned controller, unsigned lbid) throw()
{
	if(port >= types->ports())
		return -1;
	return types->port_type(port).button_id(controller, lbid);
}

void controller_state::set_ports(const port_type_set& ptype, bool set_core) throw(std::runtime_error)
{
	const port_type_set* oldtype = types;
	types = &ptype;
	if(set_core) {
		for(unsigned i = 1; i < types->ports(); i++)
			types->port_type(i).set_core_controller(i);
	}
	if(oldtype != types) {
		_input.set_types(ptype);
		_autohold.set_types(ptype);
		_committed.set_types(ptype);
		_framehold.set_types(ptype);
		//The old autofire pattern no longer applies.
		_autofire.clear();
	}
	int i = 0;
	for(unsigned j = 0; j < MAX_ANALOG; j++)
		analog_indices[j] = std::make_pair(-1, -1);
	for(unsigned j = 0; j < types->ports(); j++) {
		for(unsigned k = 0; k < types->port_type(j).controllers; k++) {
			if(types->port_type(j).is_mouse(k)) {
				analog_mouse[i] = true;
				analog_indices[i++] = std::make_pair(j, k);
			} else if(types->port_type(j).is_analog(k)) {
				analog_mouse[i] = false;
				analog_indices[i++] = std::make_pair(j, k);
			}
			if(i == MAX_ANALOG)
				break;
		}
	}
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

bool controller_state::is_analog(unsigned port, unsigned controller) throw()
{
	return _input.is_analog(port, controller);
}

bool controller_state::is_mouse(unsigned port, unsigned controller) throw()
{
	return _input.is_mouse(port, controller);
}
