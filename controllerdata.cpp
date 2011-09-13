#include "lsnes.hpp"
#include "controllerdata.hpp"
#include <sstream>
#include <iostream>
#include <cstring>

void cdecode::system(fieldsplitter& line, short* controls, unsigned version) throw(std::bad_alloc, std::runtime_error)
{
	static controlfield_system p(version);
	std::string tmp = line;
	p.set_field(tmp);
	for(unsigned i = 0; i < MAX_SYSTEM_CONTROLS; i++)
		controls[i] = p[i];
}

namespace
{
	inline unsigned ccindex(unsigned port, unsigned controller, unsigned control) throw(std::logic_error)
	{
		if(port >= MAX_PORTS || controller >= MAX_CONTROLLERS_PER_PORT || control >= CONTROLLER_CONTROLS) {
			std::ostringstream x;
			x << "ccindex: Invalid (port, controller, control) tuple (" << port << "," << controller
				<< "," << control << ")";
			throw std::logic_error(x.str());
		}
		return MAX_SYSTEM_CONTROLS + port * CONTROLLER_CONTROLS * MAX_CONTROLLERS_PER_PORT +
				CONTROLLER_CONTROLS * controller + control;
	}

	template<unsigned components, class cfield>
	void decode(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
	{
		static cfield p;
		for(unsigned j = 0; j < components; j++) {
			std::string tmp = line;
			p.set_field(tmp);
			for(unsigned i = 0; i < p.indices(); i++)
				controls[MAX_SYSTEM_CONTROLS + port * CONTROLLER_CONTROLS * MAX_CONTROLLERS_PER_PORT +
					CONTROLLER_CONTROLS * j + i] = p[i];
		}
	}
}

unsigned ccindex2(unsigned port, unsigned controller, unsigned control) throw(std::logic_error)
{
	return ccindex(port, controller, control);
}


void cdecode::none(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<0, controlfield_gamepad>(port, line, controls);
}

void cdecode::gamepad(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<1, controlfield_gamepad>(port, line, controls);
}
void cdecode::multitap(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<4, controlfield_gamepad>(port, line, controls);
}

void cdecode::mouse(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<1, controlfield_mousejustifier>(port, line, controls);
}

void cdecode::superscope(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<1, controlfield_superscope>(port, line, controls);
}

void cdecode::justifier(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<1, controlfield_mousejustifier>(port, line, controls);
}

void cdecode::justifiers(unsigned port, fieldsplitter& line, short* controls) throw(std::bad_alloc, std::runtime_error)
{
	decode<2, controlfield_mousejustifier>(port, line, controls);
}

std::string cencode::system(const short* controls) throw(std::bad_alloc)
{
	std::string x = "..";
	if(controls[0])
		x[0] = 'F';
	if(controls[1])
		x[1] = 'R';
	if(controls[2] || controls[3]) {
		std::ostringstream out;
		out << x << " " << controls[2] << " " << controls[3];
		x = out.str();
	}
	return x;
}

std::string cencode::none(unsigned port, const short* controls) throw(std::bad_alloc)
{
	return "";
}

std::string cencode::gamepad(unsigned port, const short* controls) throw(std::bad_alloc)
{
	const char chars[] = "BYsSudlrAXLR";
	std::string x = "|............";
	for(unsigned i = 0; i < 12; i++)
		if(controls[ccindex(port, 0, i)])
			x[i + 1] = chars[i];
	return x;
}

std::string cencode::multitap(unsigned port, const short* controls) throw(std::bad_alloc)
{
	const char chars[] = "BYsSudlrAXLR";
	std::string x = "|............|............|............|............";
	for(unsigned j = 0; j < 4; j++)
		for(unsigned i = 0; i < 12; i++)
			if(controls[ccindex(port, j, i)])
				x[13 * j + i + 1] = chars[i];
	return x;
}

std::string cencode::mouse(unsigned port, const short* controls) throw(std::bad_alloc)
{
	std::ostringstream s;
	s << "|";
	s << (controls[ccindex(port, 0, 2)] ? 'L' : '.');
	s << (controls[ccindex(port, 0, 3)] ? 'R' : '.');
	s << " " << controls[ccindex(port, 0, 0)]  << " " << controls[ccindex(port, 0, 1)];
	return s.str();
}

std::string cencode::superscope(unsigned port, const short* controls) throw(std::bad_alloc)
{
	std::ostringstream s;
	s << "|";
	s << (controls[ccindex(port, 0, 2)] ? 'T' : '.');
	s << (controls[ccindex(port, 0, 3)] ? 'C' : '.');
	s << (controls[ccindex(port, 0, 4)] ? 'U' : '.');
	s << (controls[ccindex(port, 0, 5)] ? 'P' : '.');
	s << " " << controls[ccindex(port, 0, 0)]  << " " << controls[ccindex(port, 0, 1)];
	return s.str();
}

std::string cencode::justifier(unsigned port, const short* controls) throw(std::bad_alloc)
{
	std::ostringstream s;
	s << "|";
	s << (controls[ccindex(port, 0, 2)] ? 'T' : '.');
	s << (controls[ccindex(port, 0, 3)] ? 'S' : '.');
	s << " " << controls[ccindex(port, 0, 0)]  << " " << controls[ccindex(port, 0, 1)];
	return s.str();
}

std::string cencode::justifiers(unsigned port, const short* controls) throw(std::bad_alloc)
{
	std::ostringstream s;
	s << "|";
	s << (controls[ccindex(port, 0, 2)] ? 'T' : '.');
	s << (controls[ccindex(port, 0, 3)] ? 'S' : '.');
	s << " " << controls[ccindex(port, 0, 0)]  << " " << controls[ccindex(port, 0, 1)];
	s << "|";
	s << (controls[ccindex(port, 1, 2)] ? 'T' : '.');
	s << (controls[ccindex(port, 1, 3)] ? 'S' : '.');
	s << " " << controls[ccindex(port, 1, 0)]  << " " << controls[ccindex(port, 1, 1)];
	return s.str();
}

controls_t controls_t::operator^(controls_t other) throw()
{
	controls_t x;
	for(size_t i = 0; i < TOTAL_CONTROLS; i++)
		x.controls[i] = controls[i] ^ ((i < MAX_SYSTEM_CONTROLS) ? 0 : other.controls[i]);
	return x;
}

controls_t::controls_t(bool sync) throw()
{
	memset(controls, 0, sizeof(controls));
	if(sync)
		controls[CONTROL_FRAME_SYNC] = 1;
}

const short& controls_t::operator()(unsigned port, unsigned controller, unsigned control) const throw(std::logic_error)
{
	return controls[ccindex(port, controller, control)];
}

const short& controls_t::operator()(unsigned control) const throw(std::logic_error)
{
	if(control >= TOTAL_CONTROLS)
		throw std::logic_error("controls_t::operator(): Invalid control index");
	return controls[control];
}

short& controls_t::operator()(unsigned port, unsigned controller, unsigned control) throw(std::logic_error)
{
	return controls[ccindex(port, controller, control)];
}

short& controls_t::operator()(unsigned control) throw(std::logic_error)
{
	if(control >= TOTAL_CONTROLS)
		throw std::logic_error("controls_t::operator(): Invalid control index");
	return controls[control];
}

controls_t::controls_t(const std::string& line, const std::vector<cdecode::fn_t>& decoders, unsigned version)
	throw(std::bad_alloc, std::runtime_error)
{
	memset(controls, 0, sizeof(controls));
	fieldsplitter _line(line);
	cdecode::system(_line, controls, version);
	for(unsigned i = 0; i < decoders.size(); i++)
		decoders[i](i, _line, controls);
}

std::string controls_t::tostring(const std::vector<cencode::fn_t>& encoders) const throw(std::bad_alloc)
{
	std::string x;
	x = cencode::system(controls);
	for(unsigned i = 0; i < encoders.size(); i++)
		x = x + encoders[i](i, controls);
	return x;
}

bool controls_t::operator==(const controls_t& c) const throw()
{
	for(size_t i = 0; i < TOTAL_CONTROLS; i++)
		if(controls[i] != c.controls[i])
			return false;
	return true;
}


controlfield::controlfield(std::vector<control_subfield> _subfields) throw(std::bad_alloc)
{
	subfields = _subfields;
	size_t needed = 0;
	for(size_t i = 0; i < subfields.size(); i++)
		if(needed <= subfields[i].index)
			needed = subfields[i].index + 1;
	values.resize(needed);
	for(size_t i = 0; i < needed; i++)
		values[i] = 0;
}

short controlfield::operator[](unsigned index) throw(std::logic_error)
{
	if(index >= values.size())
		throw std::logic_error("controlfield::operator[]: Bad subfield index");
	return values[index];
}

unsigned controlfield::indices() throw()
{
	return values.size();
}

void controlfield::set_field(const std::string& str) throw(std::bad_alloc)
{
	size_t pos = 0;
	for(unsigned i = 0; i < subfields.size(); i++)
		//Buttons always come first.
		if(subfields[i].type == control_subfield::BUTTON) {
			values[subfields[i].index] = (pos < str.length() && str[pos] != '.' && str[pos] != ' ' &&
				str[pos] != '\t') ? 1 : 0;
			pos++;
		}
	for(unsigned i = 0; i < subfields.size(); i++)
		//Then the axis.
		if(subfields[i].type == control_subfield::AXIS) {
			short value = 0;
			//Skip whitespace before subfield.
			while(pos < str.length() && (str[pos] == ' ' || str[pos] == '\t'))
				pos++;
			if(pos < str.length())
				value = (short)atoi(str.c_str() + pos);
			values[subfields[i].index] = value;
		}
}

namespace
{
	std::vector<struct control_subfield> gamepad() throw(std::bad_alloc)
	{
		static std::vector<struct control_subfield> g;
		static bool init = false;
		if(!init) {
			for(unsigned i = 0; i < 12; i++)
				g.push_back(control_subfield(i, control_subfield::BUTTON));
			init = true;
		}
		return g;
	}

	std::vector<struct control_subfield> mousejustifier() throw(std::bad_alloc)
	{
		static std::vector<struct control_subfield> g;
		static bool init = false;
		if(!init) {
			g.push_back(control_subfield(0, control_subfield::AXIS));
			g.push_back(control_subfield(1, control_subfield::AXIS));
			g.push_back(control_subfield(2, control_subfield::BUTTON));
			g.push_back(control_subfield(3, control_subfield::BUTTON));
			init = true;
		}
		return g;
	}

	std::vector<struct control_subfield> superscope() throw(std::bad_alloc)
	{
		static std::vector<struct control_subfield> g;
		static bool init = false;
		if(!init) {
			g.push_back(control_subfield(0, control_subfield::AXIS));
			g.push_back(control_subfield(1, control_subfield::AXIS));
			g.push_back(control_subfield(2, control_subfield::BUTTON));
			g.push_back(control_subfield(3, control_subfield::BUTTON));
			g.push_back(control_subfield(4, control_subfield::BUTTON));
			g.push_back(control_subfield(5, control_subfield::BUTTON));
			init = true;
		}
		return g;
	}

	std::vector<struct control_subfield> csystem(unsigned version) throw(std::bad_alloc, std::runtime_error)
	{
		static std::vector<struct control_subfield> g0;
		static bool init0 = false;
		if(version == 0) {
			if(!init0) {
				g0.push_back(control_subfield(0, control_subfield::BUTTON));
				g0.push_back(control_subfield(1, control_subfield::BUTTON));
				g0.push_back(control_subfield(2, control_subfield::AXIS));
				g0.push_back(control_subfield(3, control_subfield::AXIS));
				init0 = true;
			}
			return g0;
		} else
			throw std::runtime_error("csystem: Unknown record version");
	}
}

controlfield_system::controlfield_system(unsigned version) throw(std::bad_alloc, std::runtime_error)
	: controlfield(csystem(version))
{
}

controlfield_gamepad::controlfield_gamepad() throw(std::bad_alloc)
	: controlfield(gamepad())
{
}

controlfield_mousejustifier::controlfield_mousejustifier() throw(std::bad_alloc)
	: controlfield(mousejustifier())
{
}

controlfield_superscope::controlfield_superscope() throw(std::bad_alloc)
	: controlfield(superscope())
{
}


control_subfield::control_subfield(unsigned _index, enum control_subfield::control_subfield_type _type) throw()
{
	index = _index;
	type = _type;
}

const port_type& port_type::lookup(const std::string& name, bool port2) throw(std::bad_alloc,
	std::runtime_error)
{
	for(unsigned i = 0; i <= PT_LAST_CTYPE; i++) {
		if(name != port_types[i].name)
			continue;
		if(!port2 && !port_types[i].valid_port1)
			throw std::runtime_error("Can't connect " + name + " to port #1");
		return port_types[i];
	}
	throw std::runtime_error("Unknown port type '" + name + "'");
}

port_type port_types[] = {
	{ "none", cdecode::none, cencode::none, PT_NONE, 0, DT_NONE, true },
	{ "gamepad", cdecode::gamepad, cencode::gamepad, PT_GAMEPAD, 1, DT_GAMEPAD, true },
	{ "multitap", cdecode::multitap, cencode::multitap, PT_MULTITAP, 4, DT_GAMEPAD, true },
	{ "mouse", cdecode::mouse, cencode::mouse, PT_MOUSE, 1, DT_MOUSE, true },
	{ "superscope", cdecode::superscope, cencode::superscope, PT_SUPERSCOPE, 1, DT_SUPERSCOPE, false },
	{ "justifier", cdecode::justifier, cencode::justifier, PT_JUSTIFIER, 1, DT_JUSTIFIER, false },
	{ "justifiers", cdecode::justifiers, cencode::justifiers, PT_JUSTIFIERS, 2, DT_JUSTIFIER, false }
};
