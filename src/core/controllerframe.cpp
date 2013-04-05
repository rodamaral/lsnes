#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
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
	tasinput_enaged = false;
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
	controller_frame tmp =  _input ^ _framehold ^ _autohold;
	for(auto i : _autofire)
		if(i.second.eval_at(framenum))
			tmp.axis2(i.first, tmp.axis2(i.first) ^ 1);
	if(tasinput_enaged)
		for(auto i : _tasinput) {
			if(i.second.mode == 0 && i.second.state)
				tmp.axis2(i.first, tmp.axis2(i.first) ^ 1);
			else if(i.second.mode == 1)
				tmp.axis2(i.first, i.second.state);
		}
	return tmp;
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

void controller_state::autofire2(unsigned port, unsigned controller, unsigned pbid, unsigned duty, unsigned cyclelen)
	throw()
{
	unsigned idx = _input.porttypes().triple_to_index(port, controller, pbid);
	if(duty) {
		_autofire[idx].first_frame = movb.get_movie().get_current_frame();
		_autofire[idx].duty = duty;
		_autofire[idx].cyclelen = cyclelen;
	} else
		_autofire.erase(idx);
}

std::pair<unsigned, unsigned> controller_state::autofire2(unsigned port, unsigned controller, unsigned pbid) throw()
{
	unsigned idx = _input.porttypes().triple_to_index(port, controller, pbid);
	if(!_autofire.count(idx))
		return std::make_pair(0, 1);
	else
		return std::make_pair(_autofire[idx].duty, _autofire[idx].cyclelen);
}

bool controller_state::autofire_info::eval_at(uint64_t frame)
{
	if(frame < first_frame) {
		uint64_t diff = first_frame - frame;
		frame += ((diff / cyclelen) + 1) * cyclelen;
	}
	uint64_t diff2 = frame - first_frame;
	return frame % cyclelen < duty;
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

void controller_state::tasinput(unsigned port, unsigned controller, unsigned pbid, int16_t state) throw()
{
	unsigned idx = _input.porttypes().triple_to_index(port, controller, pbid);
	if(!_tasinput.count(idx))
		_tasinput[idx].mode = 0;	//Just to be sure.
	_tasinput[idx].state = state;
}

int16_t controller_state::tasinput(unsigned port, unsigned controller, unsigned pbid) throw()
{
	unsigned idx = _input.porttypes().triple_to_index(port, controller, pbid);
	return  _tasinput.count(idx) ? _tasinput[idx].state : 0;
}

void controller_state::tasinput_enable(bool enabled)
{
	tasinput_enaged = enabled;
}

void reread_active_buttons();

void controller_state::reread_tasinput_mode(const port_type_set& ptype)
{
	unsigned indices = ptype.indices();
	_tasinput.clear();
	for(unsigned i = 0; i < indices; i++) {
		auto t = ptype.index_to_triple(i);
		if(!t.valid)
			continue;
		//See what the heck that is...
		const port_type& pt = ptype.port_type(t.port);
		const port_controller_set& pci = *(pt.controller_info);
		if(pci.controller_count <= t.controller || !pci.controllers[t.controller])
			continue;
		const port_controller& pc = *(pci.controllers[t.controller]);
		if(!pc.buttons[t.control])
			continue;
		const port_controller_button& pcb = *(pc.buttons[t.control]);
		if(pcb.shadow)
			continue;
		if(pcb.type == port_controller_button::TYPE_BUTTON)
			_tasinput[i].mode = 0;
		else
			_tasinput[i].mode = 1;
		_tasinput[i].state = 0;
	}
}

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
		reread_tasinput_mode(ptype);
		_autohold = _autohold.blank_frame();
	}
	reread_active_buttons();
	information_dispatch::do_autohold_reconfigure();
}

controller_frame controller_state::get_blank() throw()
{
	return _input.blank_frame();
}

controller_frame controller_state::get_committed() throw()
{
	return _committed;
}

void controller_state::commit(controller_frame controls) throw()
{
	_committed = controls;
}

bool controller_state::is_present(unsigned port, unsigned controller) throw()
{
	return _input.is_present(port, controller);
}
