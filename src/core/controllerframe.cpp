#include "core/controller.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "interface/romtype.hpp"

#include <cstdio>
#include <iostream>

void update_movie_state();

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
	apply_macro(tmp);
	return tmp;
}

void controller_state::analog(unsigned port, unsigned controller, unsigned control, short x) throw()
{
	_input.axis3(port, controller, control, x);
}

void controller_state::autohold2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw()
{
	_autohold.axis3(port, controller, pbid, newstate ? 1 : 0);
	notify_autohold_update(port, controller, pbid, newstate);
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
		_autofire[idx].first_frame = CORE().mlogic.get_movie().get_current_frame();
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
		if(pci.controllers.size() <= t.controller)
			continue;
		const port_controller& pc = pci.controllers[t.controller];
		if(pc.buttons.size() <= t.control)
			continue;
		const port_controller_button& pcb = pc.buttons[t.control];
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
		reread_active_buttons();
		notify_autohold_reconfigure();
	}
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

void controller_state::erase_macro(const std::string& macro)
{
	{
		threads::alock h(macro_lock);
		if(!all_macros.count(macro))
			return;
		auto m = &all_macros[macro];
		for(auto i = active_macros.begin(); i != active_macros.end(); i++) {
			if(i->second == m) {
				active_macros.erase(i);
				break;
			}
		}
		all_macros.erase(macro);
		project_info* p = CORE().project.get();
		if(p) {
			p->macros.erase(macro);
			p->flush();
		}
	}
	load_macros(*this);
}

std::set<std::string> controller_state::enumerate_macro()
{
	threads::alock h(macro_lock);
	std::set<std::string> r;
	for(auto i : all_macros)
		r.insert(i.first);
	return r;
}

controller_macro& controller_state::get_macro(const std::string& macro)
{
	threads::alock h(macro_lock);
	if(!all_macros.count(macro))
		throw std::runtime_error("No such macro");
	return all_macros[macro];
}

void controller_state::set_macro(const std::string& macro, const controller_macro& m)
{
	{
		threads::alock h(macro_lock);
		controller_macro* old = NULL;
		if(all_macros.count(macro))
			old = &all_macros[macro];
		all_macros[macro] = m;
		for(auto i = active_macros.begin(); i != active_macros.end(); i++) {
			if(i->second == old) {
				i->second = &all_macros[macro];
				break;
			}
		}
		project_info* p = CORE().project.get();
		if(p) {
			p->macros[macro] = all_macros[macro].serialize();
			p->flush();
		}
	}
	load_macros(*this);
}

void controller_state::apply_macro(controller_frame& f)
{
	threads::alock h(macro_lock);
	for(auto i : active_macros)
		i.second->write(f, i.first);
}

void controller_state::advance_macros()
{
	threads::alock h(macro_lock);
	for(auto& i : active_macros)
		i.first++;
}

std::map<std::string, uint64_t> controller_state::get_macro_frames()
{
	threads::alock h(macro_lock);
	std::map<std::string, uint64_t> r;
	for(auto i : active_macros) {
		for(auto& j : all_macros)
			if(i.second == &j.second) {
				r[j.first] = i.first;
			}
	}
	return r;
}

void controller_state::set_macro_frames(const std::map<std::string, uint64_t>& f)
{
	threads::alock h(macro_lock);
	std::list<std::pair<uint64_t, controller_macro*>> new_active_macros;
	for(auto i : f)
		if(all_macros.count(i.first))
			new_active_macros.push_back(std::make_pair(i.second, &all_macros[i.first]));
		else
			messages << "Warning: Can't find defintion for '" << i.first << "'" << std::endl;
	std::swap(active_macros, new_active_macros);
}

void controller_state::rename_macro(const std::string& old, const std::string& newn)
{
	{
		threads::alock h(macro_lock);
		if(!all_macros.count(old))
			throw std::runtime_error("Old macro doesn't exist");
		if(all_macros.count(newn))
			throw std::runtime_error("Target name already exists");
		if(old == newn)
			return;
		all_macros[newn] = all_macros[old];
		controller_macro* _old = &all_macros[old];
		all_macros.erase(old);
		for(auto i = active_macros.begin(); i != active_macros.end(); i++) {
			if(i->second == _old) {
				i->second = &all_macros[newn];
				break;
			}
		}
		project_info* p = CORE().project.get();
		if(p) {
			p->macros[newn] = p->macros[old];
			p->macros.erase(old);
		}
	}
	load_macros(*this);
}

void controller_state::do_macro(const std::string& a, int mode) {
	{
		threads::alock h(macro_lock);
		if(!all_macros.count(a)) {
			if(mode & 1) messages << "No such macro '" << a << "'" << std::endl;
			return;
		}
		controller_macro* m = &all_macros[a];
		for(auto i = active_macros.begin(); i != active_macros.end(); i++) {
			if(i->second == m) {
				if(mode & 2) active_macros.erase(i);
				goto end;
			}
		}
		if(mode & 4) active_macros.push_back(std::make_pair(0, m));
	}
end:
	update_movie_state();
	notify_status_update();
}

std::set<std::string> controller_state::active_macro_set()
{
	threads::alock h(macro_lock);
	std::set<std::string> r;
	for(auto i : active_macros) {
		for(auto& j : all_macros)
			if(i.second == &j.second) {
				r.insert(j.first);
			}
	}
	return r;
}
