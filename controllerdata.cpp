#include "lsnes.hpp"
#include "controllerdata.hpp"
#include <sstream>
#include <cctype>
#include <iostream>
#include <cstring>
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

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

	bool parse_button_ctrl(const std::string& str, size_t& pos) throw()
	{
		if(pos >= str.length())
			return false;
		switch(str[pos]) {
		case '.':
		case ' ':
		case '\t':
			pos++;
		case '|':
			return false;
		default:
			pos++;
			return true;
		}
	}

	short parse_number_ctrl(const std::string& str, size_t& pos) throw()
	{
		char ch;
		//Skip ws.
		while(pos < str.length()) {
			char ch = str[pos];
			if(ch != ' ' && ch != '\t')
				break;
			pos++;
		}
		//Read the sign if any.
		if(pos >= str.length() || (ch = str[pos]) == '|')
			return 0;
		bool negative = false;
		if(ch == '-') {
			negative = true;
			pos++;
		}
		if(ch == '+')
			pos++;

		//Read numeric value.
		int numval = 0;
		while(pos < str.length() && isdigit(static_cast<unsigned char>(ch = str[pos]))) {
			numval = numval * 10 + (ch - '0');
			pos++;
		}
		if(negative)
			numval = -numval;

		return static_cast<short>(numval);
	}

	void parse_end_of_field(const std::string& str, size_t& pos) throw()
	{
		while(pos < str.length() && str[pos] != '|')
			pos++;
	}
}

size_t cdecode::system(const std::string& line, size_t pos, short* controls, unsigned version) throw(std::bad_alloc,
	std::runtime_error)
{
	controls[0] = parse_button_ctrl(line, pos);	//Frame sync.
	controls[1] = parse_button_ctrl(line, pos);	//Reset.
	controls[2] = parse_number_ctrl(line, pos);	//Reset cycles hi.
	controls[3] = parse_number_ctrl(line, pos);	//Reset cycles lo.
	parse_end_of_field(line, pos);
	return pos;
}

size_t cdecode::none(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	return pos;
}

size_t cdecode::gamepad(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	for(unsigned i = 0; i < 12; i++)
		controls[ccindex(port, 0, i)] = parse_button_ctrl(line, pos);
	parse_end_of_field(line, pos);
	return pos;
}

size_t cdecode::multitap(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	for(unsigned j = 0; j < 4; j++) {
		for(unsigned i = 0; i < 12; i++)
			controls[ccindex(port, j, i)] = parse_button_ctrl(line, pos);
		parse_end_of_field(line, pos);
		pos++;
	}
	return pos;
}

size_t cdecode::mouse(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	controls[ccindex(port, 0, 2)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 3)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 0)] = parse_number_ctrl(line, pos);
	controls[ccindex(port, 0, 1)] = parse_number_ctrl(line, pos);
	parse_end_of_field(line, pos);
	return pos;
}

size_t cdecode::superscope(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	controls[ccindex(port, 0, 2)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 3)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 4)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 5)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 0)] = parse_number_ctrl(line, pos);
	controls[ccindex(port, 0, 1)] = parse_number_ctrl(line, pos);
	parse_end_of_field(line, pos);
	return pos;
}

size_t cdecode::justifier(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	controls[ccindex(port, 0, 2)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 3)] = parse_button_ctrl(line, pos);
	controls[ccindex(port, 0, 0)] = parse_number_ctrl(line, pos);
	controls[ccindex(port, 0, 1)] = parse_number_ctrl(line, pos);
	parse_end_of_field(line, pos);
	return pos;
}

size_t cdecode::justifiers(unsigned port, const std::string& line, size_t pos, short* controls) throw(std::bad_alloc,
	std::runtime_error)
{
	for(unsigned i = 0; i < 2; i++) {
		controls[ccindex(port, i, 2)] = parse_button_ctrl(line, pos);
		controls[ccindex(port, i, 3)] = parse_button_ctrl(line, pos);
		controls[ccindex(port, i, 0)] = parse_number_ctrl(line, pos);
		controls[ccindex(port, i, 1)] = parse_number_ctrl(line, pos);
		parse_end_of_field(line, pos);
		pos++;
	}
	return pos;
}

size_t cencode::system(char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	buffer[bufferpos++] = controls[0] ? 'F' : '.';
	buffer[bufferpos++] = controls[1] ? 'R' : '.';
	if(controls[2] || controls[3]) {
		bufferpos += sprintf(buffer + bufferpos, " %i %i", static_cast<int>(controls[2]),
			static_cast<int>(controls[3]));
	}
	return bufferpos;
}

size_t cencode::none(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	return ENCODE_SPECIAL_NO_OUTPUT;
}

size_t cencode::gamepad(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	static const char* characters = "BYsSudlrAXLR";
	for(unsigned i = 0; i < 12; i++)
		buffer[bufferpos++] = controls[ccindex(port, 0, i)] ? characters[i] : '.';
	return bufferpos;
}

size_t cencode::multitap(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	static const char* characters = "BYsSudlrAXLR";
	for(unsigned j = 0; j < 4; j++) {
		for(unsigned i = 0; i < 12; i++)
			buffer[bufferpos++] = controls[ccindex(port, j, i)] ? characters[i] : '.';
		buffer[bufferpos++] = '|';
	}
	bufferpos--;	//Eat the last '|', it shouldn't be there.
	return bufferpos;
}

size_t cencode::mouse(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	bufferpos += sprintf(buffer + bufferpos, "%c%c %i %i", controls[ccindex(port, 0, 2)] ? 'L' : '.',
		controls[ccindex(port, 0, 3)] ? 'R' : '.', static_cast<int>(controls[ccindex(port, 0, 0)]),
		static_cast<int>(controls[ccindex(port, 0, 1)]));
	return bufferpos;
}

size_t cencode::superscope(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	bufferpos += sprintf(buffer + bufferpos, "%c%c%c%c %i %i", controls[ccindex(port, 0, 2)] ? 'T' : '.',
		controls[ccindex(port, 0, 3)] ? 'C' : '.', controls[ccindex(port, 0, 4)] ? 'U' : '.',
		controls[ccindex(port, 0, 5)] ? 'P' : '.', static_cast<int>(controls[ccindex(port, 0, 0)]),
		static_cast<int>(controls[ccindex(port, 0, 1)]));
	return bufferpos;
}

size_t cencode::justifier(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	bufferpos += sprintf(buffer + bufferpos, "%c%c %i %i", controls[ccindex(port, 0, 2)] ? 'T' : '.',
		controls[ccindex(port, 0, 3)] ? 'S' : '.', static_cast<int>(controls[ccindex(port, 0, 0)]),
		static_cast<int>(controls[ccindex(port, 0, 1)]));
	return bufferpos;
}

size_t cencode::justifiers(unsigned port, char* buffer, size_t bufferpos, const short* controls) throw(std::bad_alloc)
{
	bufferpos += sprintf(buffer + bufferpos, "%c%c %i %i", controls[ccindex(port, 0, 2)] ? 'T' : '.',
		controls[ccindex(port, 0, 3)] ? 'S' : '.', static_cast<int>(controls[ccindex(port, 0, 0)]),
		static_cast<int>(controls[ccindex(port, 0, 1)]));
	buffer[bufferpos++] = '|';
	bufferpos += sprintf(buffer + bufferpos, "%c%c %i %i", controls[ccindex(port, 0, 2)] ? 'T' : '.',
		controls[ccindex(port, 0, 3)] ? 'S' : '.', static_cast<int>(controls[ccindex(port, 0, 0)]),
		static_cast<int>(controls[ccindex(port, 0, 1)]));
	return bufferpos;
}


unsigned ccindex2(unsigned port, unsigned controller, unsigned control) throw(std::logic_error)
{
	return ccindex(port, controller, control);
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
	size_t position = 0;
	position = cdecode::system(line, position, controls, version);
	for(unsigned i = 0; i < decoders.size(); i++) {
		if(position < line.length() && line[position] == '|')
			position++;
		position = decoders[i](i, line, position, controls);
	}
}

std::string controls_t::tostring(const std::vector<cencode::fn_t>& encoders) const throw(std::bad_alloc)
{
	char buffer[1024];
	size_t linelen = 0, tmp;
	tmp = cencode::system(buffer, linelen, controls);
	for(unsigned i = 0; i < encoders.size(); i++) {
		if(tmp != ENCODE_SPECIAL_NO_OUTPUT)
			buffer[(linelen = tmp)++] = '|';
		tmp = encoders[i](i, buffer, linelen, controls);
	}
	if(tmp != ENCODE_SPECIAL_NO_OUTPUT)
		linelen = tmp;
	return std::string(buffer, buffer + linelen);
}

bool controls_t::operator==(const controls_t& c) const throw()
{
	for(size_t i = 0; i < TOTAL_CONTROLS; i++)
		if(controls[i] != c.controls[i])
			return false;
	return true;
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
	{ "none", cdecode::none, cencode::none, PT_NONE, 0, DT_NONE, true, SNES_DEVICE_NONE },
	{ "gamepad", cdecode::gamepad, cencode::gamepad, PT_GAMEPAD, 1, DT_GAMEPAD, true, SNES_DEVICE_JOYPAD },
	{ "multitap", cdecode::multitap, cencode::multitap, PT_MULTITAP, 4, DT_GAMEPAD, true, SNES_DEVICE_MULTITAP },
	{ "mouse", cdecode::mouse, cencode::mouse, PT_MOUSE, 1, DT_MOUSE, true, SNES_DEVICE_MOUSE },
	{ "superscope", cdecode::superscope, cencode::superscope, PT_SUPERSCOPE, 1, DT_SUPERSCOPE, false,
		SNES_DEVICE_SUPER_SCOPE },
	{ "justifier", cdecode::justifier, cencode::justifier, PT_JUSTIFIER, 1, DT_JUSTIFIER, false,
		SNES_DEVICE_JUSTIFIER },
	{ "justifiers", cdecode::justifiers, cencode::justifiers, PT_JUSTIFIERS, 2, DT_JUSTIFIER, false,
		SNES_DEVICE_JUSTIFIERS }
};
