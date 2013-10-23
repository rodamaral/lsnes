#include "controller-parse.hpp"
#include "string.hpp"
#include "assembler.hpp"
#include <algorithm>

namespace
{
	std::string quote(const std::string& s)
	{
		std::ostringstream y;
		y << "\"";
		for(auto i : s)
			if(i == '\"')
				y << "\"";
			else if(i == '\\')
				y << "\\\\";
			else
				y << i;
		y << "\"";
		return y.str();
	}

	std::string read_str(const JSON::node& root, const JSON::pointer& ptr)
	{
		if(root.type_of(ptr) != JSON::string)
			(stringfmt() << "Expected string for '" << ptr << "'").throwex();
		return root[ptr].as_string8();
	}

	std::u32string read_str32(const JSON::node& root, const JSON::pointer& ptr)
	{
		if(root.type_of(ptr) != JSON::string)
			(stringfmt() << "Expected string for '" << ptr << "'").throwex();
		return root[ptr].as_string();
	}

	int64_t read_int(const JSON::node& root, const JSON::pointer& ptr)
	{
		if(root.type_of(ptr) != JSON::number)
			(stringfmt() << "Expected number for '" << ptr << "'").throwex();
		return root[ptr].as_int();
	}

	bool read_bool(const JSON::node& root, const JSON::pointer& ptr)
	{
		if(root.type_of(ptr) != JSON::boolean)
			(stringfmt() << "Expected boolean for '" << ptr << "'").throwex();
		return root[ptr].as_bool();
	}


	port_controller_button pcb_null(const JSON::node& root, const JSON::pointer& ptr)
	{
		auto pshadow = ptr.field("shadow");
		struct port_controller_button ret;
		ret.type = port_controller_button::TYPE_NULL;
		ret.name = "";
		ret.symbol = U'\0';
		ret.rmin = 0;
		ret.rmax = 0;
		ret.centers = false;
		ret.macro = "";
		ret.msymbol = '\0';
		ret.shadow = (root.type_of(pshadow) != JSON::none) ? read_bool(root, pshadow) : false;
		return ret;
	}

	port_controller_button pcb_button(const JSON::node& root, const JSON::pointer& ptr)
	{
		auto pshadow = ptr.field("shadow");
		auto pname = ptr.field("name");
		auto psymbol = ptr.field("symbol");
		auto pmacro = ptr.field("macro");
		auto pmovie = ptr.field("movie");
		struct port_controller_button ret;
		ret.type = port_controller_button::TYPE_BUTTON;
		ret.name = read_str(root, pname);
		std::u32string symbol = (root.type_of(psymbol) != JSON::none) ? read_str32(root, psymbol) :
			to_u32string(ret.name);
		if(symbol.length() != 1)
			(stringfmt() << "Symbol at '" << ptr << "' must be 1 codepoint").throwex();
		ret.symbol = symbol[0];
		ret.rmin = 0;
		ret.rmax = 0;
		ret.centers = false;
		ret.macro = (root.type_of(pmacro) != JSON::none) ? read_str(root, pmacro) : to_u8string(symbol);
		std::string movie = (root.type_of(pmovie) != JSON::none) ? read_str(root, pmovie) :
			to_u8string(symbol);
		if(movie.length() != 1)
			(stringfmt() << "Movie at '" << ptr << "' must be 1 character").throwex();
		ret.msymbol = movie[0];
		ret.shadow = (root.type_of(pshadow) != JSON::none) ? read_bool(root, pshadow) : false;
		return ret;
	}

	port_controller_button pcb_axis(const JSON::node& root, const JSON::pointer& ptr, const std::string& type)
	{
		auto pshadow = ptr.field("shadow");
		auto pname = ptr.field("name");
		auto pcenters = ptr.field("centers");
		struct port_controller_button ret;
		if(type == "axis") ret.type = port_controller_button::TYPE_AXIS;
		if(type == "raxis") ret.type = port_controller_button::TYPE_RAXIS;
		if(type == "taxis") ret.type = port_controller_button::TYPE_TAXIS;
		if(type == "lightgun") ret.type = port_controller_button::TYPE_LIGHTGUN;
		ret.name = read_str(root, pname);
		ret.symbol = U'\0';
		ret.shadow = (root.type_of(pshadow) != JSON::none) ? read_bool(root, pshadow) : false;
		ret.rmin = ret.shadow ? -32768 : read_int(root, ptr.field("min"));
		ret.rmax = ret.shadow ? 32767 : read_int(root, ptr.field("max"));
		ret.centers = (root.type_of(pcenters) != JSON::none) ? read_bool(root, pcenters) : false;
		ret.macro = "";
		ret.msymbol = '\0';
		return ret;
	}

	struct port_controller_button pcs_parse_button(const JSON::node& root, JSON::pointer ptr)
	{
		if(root.type_of_indirect(ptr) != JSON::object)
			(stringfmt() << "Expected (indirect) object for '" << ptr << "'").throwex();
		ptr = root.resolve_indirect(ptr);
		const std::string& type = read_str(root, ptr.field("type"));
		if(type == "null")
			return pcb_null(root, ptr);
		else if(type == "button") 
			return pcb_button(root, ptr);
		else if(type == "axis" || type == "raxis" || type == "taxis" || type == "lightgun")
			return pcb_axis(root, ptr, type);
		else
			(stringfmt() << "Unknown type '" << type << "' for '" << ptr << "'").throwex();
	}

	struct port_controller pcs_parse_controller(const JSON::node& root, JSON::pointer ptr)
	{
		if(root.type_of_indirect(ptr) != JSON::object)
			(stringfmt() << "Expected (indirect) object for '" << ptr << "'").throwex();
		ptr = root.resolve_indirect(ptr);
		port_controller ret;
		auto pbuttons = ptr.field("buttons");
		std::string cclass = read_str(root, ptr.field("class"));
		std::string type = read_str(root, ptr.field("type"));
		if(root.type_of_indirect(pbuttons) != JSON::array)
			(stringfmt() << "Expected (indirect) array for '" << pbuttons << "'").throwex();
		JSON::pointer _buttons = root.resolve_indirect(pbuttons);
		const JSON::node& n_buttons = root.follow(_buttons);
		std::vector<port_controller_button> buttons;
		for(auto i = n_buttons.begin(); i != n_buttons.end(); i++)
			buttons.push_back(pcs_parse_button(root, _buttons.index(i.index())));
		ret.cclass = cclass;
		ret.type = type;
		ret.buttons = buttons;
		return ret;
	}

	struct port_controller_set* pcs_parse_set(const JSON::node& root, JSON::pointer ptr)
	{
		if(root.type_of_indirect(ptr) != JSON::object)
			(stringfmt() << "Expected (indirect) object for '" << ptr << "'").throwex();
		ptr = root.resolve_indirect(ptr);
		port_controller_set* ret = NULL;
		auto pcontrollers = ptr.field("controllers");
		auto plegal = ptr.field("legal");
		auto phname = ptr.field("hname");
		auto psymbol = ptr.field("symbol");
		if(root.type_of_indirect(pcontrollers) != JSON::array)
			(stringfmt() << "Expected (indirect) array for '" << pcontrollers << "'").throwex();
		JSON::pointer _controllers = root.resolve_indirect(pcontrollers);
		const JSON::node& n_controllers = root.follow(_controllers);
		std::vector<port_controller> controllers;
		for(auto i = n_controllers.begin(); i != n_controllers.end(); i++)
			controllers.push_back(pcs_parse_controller(root, _controllers.index(i.index())));
		if(root.type_of_indirect(plegal) != JSON::array)
			(stringfmt() << "Expected (indirect) array for '" << plegal << "'").throwex();
		JSON::pointer _legal = root.resolve_indirect(plegal);
		const JSON::node& n_legal = root.follow_indirect(_legal);
		std::set<unsigned> legal;
		for(auto i = n_legal.begin(); i != n_legal.end(); i++)
			legal.insert(read_int(root, _legal.index(i.index())));
		std::string iname = read_str(root, ptr.field("name"));
		std::string hname = (root.type_of(phname) != JSON::none) ? read_str(root, phname) : iname;
		std::string symbol = (root.type_of(psymbol) != JSON::none) ? read_str(root, psymbol) : iname;
		ret = new port_controller_set;
		ret->iname = iname;
		ret->hname = hname;
		ret->symbol = symbol;
		ret->controllers = controllers;
		ret->legal_for = legal;
		return ret;
	}

	void write_button(std::ostream& s, const port_controller_button& b)
	{
		s << "{port_controller_button::TYPE_";
		switch(b.type) {
		case port_controller_button::TYPE_NULL: s << "NULL"; break;
		case port_controller_button::TYPE_BUTTON: s << "BUTTON"; break;
		case port_controller_button::TYPE_AXIS: s << "AXIS"; break;
		case port_controller_button::TYPE_RAXIS: s << "RAXIS"; break;
		case port_controller_button::TYPE_TAXIS: s << "TAXIS"; break;
		case port_controller_button::TYPE_LIGHTGUN: s << "LIGHTGUN"; break;
		}
		s << "," << (int)b.symbol << ", " << quote(b.name) << ", " << b.shadow << ", " << b.rmin << ", "
			<< b.rmax << ", " << b.centers << ", " << quote(b.macro) << ", " << (int)b.msymbol << "}";
	}

	void write_controller(std::ostream& s, const port_controller& c, unsigned idx)
	{
		s << "port_controller port_tmp_" << idx << " = {" << quote(c.cclass) << ", " << quote(c.type)
			<< ", {\n";
		for(auto i : c.buttons) {
			s << "\t";
			write_button(s, i);
			s << ",\n";
		}
		s << "}};\n";
	}

	void write_portdata(std::ostream& s, const port_controller_set& cs, unsigned& idx)
	{
		s << "namespace portdefs {\n";
		for(unsigned i = 0; i < cs.controllers.size(); i++)
			write_controller(s, cs.controllers[i], idx + i);
		s << "port_controller_set port_tmp_" << (idx + cs.controllers.size()) << " = {\n";
		s << "\t" << quote(cs.iname) << ", " << quote(cs.hname) << ", " << quote(cs.symbol) << ",{";
		for(unsigned i = 0; i < cs.controllers.size(); i++)
			s << "port_tmp_" << (idx + i) << ", ";
		s << "},{";
		for(auto i : cs.legal_for)
			s << i << ", ";
		s << "}\n";
		s << "};\n";
		s << "}\n";
		idx += (cs.controllers.size() + 1);
	}

	size_t get_ssize(const port_controller_set& cs)
	{
		size_t x = 0;
		for(auto i : cs.controllers)
			for(auto j : i.buttons) {
				switch(j.type) {
				case port_controller_button::TYPE_BUTTON: x++; break;
				case port_controller_button::TYPE_AXIS: x+=16; break;
				case port_controller_button::TYPE_LIGHTGUN: x+=16; break;
				case port_controller_button::TYPE_NULL: break;
				case port_controller_button::TYPE_RAXIS: x+=16; break;
				case port_controller_button::TYPE_TAXIS: x+=16; break;
			};
		}
		return (x + 7) / 8;
	}

	size_t get_aoffset(const port_controller_set& cs)
	{
		size_t x = 0;
		for(auto i : cs.controllers)
			for(auto j : i.buttons) {
				switch(j.type) {
				case port_controller_button::TYPE_BUTTON: x++; break;
				case port_controller_button::TYPE_AXIS: break;
				case port_controller_button::TYPE_LIGHTGUN: break;
				case port_controller_button::TYPE_NULL: break;
				case port_controller_button::TYPE_RAXIS: break;
				case port_controller_button::TYPE_TAXIS: break;
			};
		}
		return (x + 7) / 8;
	}

	std::vector<port_type_generic::idxinfo> get_idx_instructions(const port_controller_set& cs)
	{
		std::vector<port_type_generic::idxinfo> ret;
		size_t aoffset = get_aoffset(cs);
		size_t buttonidx2 = 0;
		size_t axisidx2 = 0;
		for(unsigned i = 0; i < cs.controllers.size(); i++) {
			for(unsigned j = 0; j < cs.controllers[i].buttons.size(); j++) {
				port_type_generic::idxinfo ii;
				ii.controller = i;
				ii.index = j;
				switch(cs.controllers[i].buttons[j].type) {
				case port_controller_button::TYPE_NULL:
					ii.type = 0;
					break;
				case port_controller_button::TYPE_BUTTON:
					ii.type = 1;
					ii.offset = buttonidx2 / 8;
					ii.mask = 1 << (buttonidx2 & 7);
					ii.imask = ~ii.mask;
					buttonidx2++;
					break;
				case port_controller_button::TYPE_AXIS:
				case port_controller_button::TYPE_RAXIS:
				case port_controller_button::TYPE_TAXIS:
				case port_controller_button::TYPE_LIGHTGUN:
					ii.type = 2;
					ii.offset = aoffset + 2 * axisidx2;
					axisidx2++;
					break;
				}
				ret.push_back(ii);
			}
		}
		return ret;
	}

	std::vector<port_type_generic::ser_instruction> get_ser_instructions(const port_controller_set& cs)
	{
		std::vector<port_type_generic::ser_instruction> ret;
		size_t aoffset = get_aoffset(cs);
		size_t buttonidx = 0;
		size_t axisidx = 0;
		port_type_generic::ser_instruction ins;
		for(unsigned i = 0; i < cs.controllers.size(); i++) {
			if(i == 0 && !cs.legal_for.count(0)) {
				ins.type = 2;
				ret.push_back(ins);
			}
			if(i > 0) {
				ins.type = 3;
				ret.push_back(ins);
			}
			for(unsigned j = 0; j < cs.controllers[i].buttons.size(); j++) {
				
				switch(cs.controllers[i].buttons[j].type) {
				case port_controller_button::TYPE_BUTTON:
					ins.type = 0;
					ins.offset = buttonidx / 8;
					ins.mask = 1 << (buttonidx & 7);
					ins.character = cs.controllers[i].buttons[j].msymbol;
					ret.push_back(ins);
					buttonidx++;
					break;
				default: break;
				}
			}
			for(unsigned j = 0; j < cs.controllers[i].buttons.size(); j++) {
				switch(cs.controllers[i].buttons[j].type) {
				case port_controller_button::TYPE_AXIS:
				case port_controller_button::TYPE_RAXIS:
				case port_controller_button::TYPE_TAXIS:
				case port_controller_button::TYPE_LIGHTGUN:
					ins.type = 1;
					ins.offset = aoffset + 2 * axisidx;
					ret.push_back(ins);
					axisidx++;
					break;
				default: break;
				}
			}
		}
		ins.type = cs.controllers.size() ? 4 : 5;
		ret.push_back(ins);
		return ret;
	}
}

struct port_controller_set* pcs_from_json(const JSON::node& root, const std::string& ptr)
{
	return pcs_parse_set(root, ptr);
}

std::string pcs_write_class(const struct port_controller_set& pset, unsigned& tmp_idx)
{
	size_t buttonidx = 0;
	size_t axisidx = 0;
	std::ostringstream s;
	write_portdata(s, pset, tmp_idx);
	unsigned pidx = tmp_idx - 1;
	size_t aoffset = get_aoffset(pset);
	auto repr = get_ser_instructions(pset);
	auto idxr = get_idx_instructions(pset);
	s << "struct _" << pset.symbol << " : public port_type\n";
	s << "{\n";
	s << "\t_" << pset.symbol << "() : port_type(" << quote(pset.iname) << ", " << quote(pset.hname)
		<< ", " << get_ssize(pset) << ")\n";
	s << "\t{\n";
	s << "\t\twrite = [](const port_type* _this, unsigned char* buffer, unsigned idx, unsigned ctrl, short x) -> "
		<< "void {\n";
	s << "\t\t\tswitch(idx) {\n";
	int last_controller = -1;
	for(auto i : idxr) {
		if(i.controller != last_controller) {
			if(last_controller >= 0) {
				s << "\t\t\t\t}\n";
				s << "\t\t\t\tbreak;\n";
			}
			s << "\t\t\tcase " << i.controller << ":\n";
			s << "\t\t\t\tswitch(ctrl) {\n";
			last_controller = i.controller;
		}
		s << "\t\t\t\tcase " << i.index << ":\n";
		switch(i.type) {
		case 0:
			s << "\t\t\t\t\tbreak;\n";
			break;
		case 1:
			s << "\t\t\t\t\tif(x) buffer[" << i.offset << "]|=" << (int)i.mask
				<<"; else buffer[" << i.offset << "]&=" << (int)i.imask
				<< ";\n";
			s << "\t\t\t\t\tbreak;\n";
			break;
		case 2:
			s << "\t\t\t\t\tbuffer[" << i.offset << "]=(unsigned short)x;\n";
			s << "\t\t\t\t\tbuffer[" << i.offset + 1 << "]=((unsigned short)x >> 8);\n";
			s << "\t\t\t\t\tbreak;\n";
			break;
		}
	}
	if(last_controller >= 0) {
		s << "\t\t\t\t}\n";
		s << "\t\t\t\tbreak;\n";
	}
	s << "\t\t\t}\n";
	s << "\t\t};\n";
	s << "\t\tread = [](const port_type* _this, const unsigned char* buffer, unsigned idx, unsigned ctrl) -> "
		<< "short {\n";
	buttonidx = 0;
	axisidx = 0;
	s << "\t\t\tswitch(idx) {\n";
	last_controller = -1;
	for(auto i : idxr) {
		if(i.controller != last_controller) {
			if(last_controller >= 0) {
				s << "\t\t\t\t}\n";
				s << "\t\t\t\treturn 0;\n";
			}
			s << "\t\t\tcase " << i.controller << ":\n";
			s << "\t\t\t\tswitch(ctrl) {\n";
			last_controller = i.controller;
		}
		s << "\t\t\t\tcase " << i.index << ":\n";
		switch(i.type) {
		case 0:
			s << "\t\t\t\t\treturn 0;\n";
			break;
		case 1:
			s << "\t\t\t\t\treturn (buffer[" << i.offset << "]&" << (int)i.mask << ") ? 1 : 0;\n";
			break;
		case 2:
			s << "\t\t\t\t\treturn (short)((unsigned short)buffer[" << i.offset
				<< "] + ((unsigned short)buffer[" << i.offset + 1 << "] << 8));\n";
		}
	}
	if(last_controller >= 0) {
		s << "\t\t\t\t}\n";
		s << "\t\t\treturn 0;\n";
	}
	s << "\t\t\treturn 0;\n";
	s << "\t\t\t}\n";
	s << "\t\t};\n";
	s << "\t\tserialize = [](const port_type* _this, const unsigned char* buffer, char* textbuf) -> size_t {\n";
	s << "\t\t\tsize_t ptr = 0;\n";
	s << "\t\t\tshort tmp;\n";
	for(auto i : repr) {
		switch(i.type) {
		case 0:
			s << "\t\t\ttextbuf[ptr++] = (buffer[" << i.offset << "] & " << (int)i.mask << ") ? (char)"
				<< (int)i.character << " : '.';\n";
			break;
		case 1:
			s << "\t\t\ttmp = (short)((unsigned short)buffer[" << i.offset
				<< "] + ((unsigned short)buffer[" << (i.offset + 1) << "] << 8));\n";
			s << "\t\t\tptr += sprintf(textbuf + ptr, \" %i\", tmp);\n";
		case 2:
			s << "\t\t\ttextbuf[ptr++] = '|';\n";
			break;
		case 3:
			s << "\t\t\ttextbuf[ptr++] = '|';\n";
			break;
		case 4:
			s << "\t\t\treturn ptr;\n";
			break;
		case 5:
			s << "\t\t\treturn 0;\n";
			break;
		}
	}
	s << "\t\t};\n";
	s << "\t\tdeserialize = [](const port_type* _this, unsigned char* buffer, const char* textbuf) -> size_t {\n";
	s << "\t\t\tmemset(buffer, 0, " << get_ssize(pset) << ");\n";
	s << "\t\t\tsize_t ptr = 0;\n";
	s << "\t\t\tshort tmp;\n";
	for(auto i : repr) {
		switch(i.type) {
		case 0:
			s << "\t\t\tif(read_button_value(textbuf, ptr)) buffer[" << i.offset << "]|=" << (int)i.mask
				<< ";\n";
			break;
		case 1:
			s << "\t\t\ttmp = read_axis_value(textbuf, ptr);\n";
			s << "\t\t\tbuffer[" << i.offset << "] = " << "(unsigned short)tmp;\n";
			s << "\t\t\tbuffer[" << (i.offset + 1) << "] = " << "((unsigned short)tmp >> 8);\n";
			break;
		case 2:
			break;
		case 3:
			s << "\t\t\tskip_rest_of_field(textbuf, ptr, true);\n";
			break;
		case 4:
			s << "\t\t\tskip_rest_of_field(textbuf, ptr, false);\n";
			s << "\t\t\treturn ptr;\n";
			break;
		case 5:
			s << "\t\t\treturn DESERIALIZE_SPECIAL_BLANK;\n";
			break;
		}
	}
	s << "\t\t};\n";
	s << "\t\tcontroller_info = &portdefs::port_tmp_" << pidx << ";\n";
	s << "\t}\n";
	s << "} " << pset.symbol << ";\n";
	return s.str();
}

std::string pcs_write_trailer(const std::vector<port_controller_set*>& p)
{
	std::ostringstream s;
	s << "std::vector<port_type*> _port_types{";
	for(auto i : p)
		s << "&" << i->symbol << ", ";
	s << "};\n";
	return s.str();
}

std::string pcs_write_classes(const std::vector<port_controller_set*>& p, unsigned& tmp_idx)
{
	std::string s;
	for(auto i : p)
		s = s + pcs_write_class(*i, tmp_idx);
	s = s + pcs_write_trailer(p);
	return s;
}

std::vector<port_controller_set*> pcs_from_json_array(const JSON::node& root, const std::string& _ptr)
{
	JSON::pointer ptr(_ptr);
	if(root.type_of_indirect(ptr) != JSON::array)
		(stringfmt() << "Expected (indirect) array for '" << ptr << "'").throwex();
	ptr = root.resolve_indirect(ptr);
	const JSON::node& n = root[ptr];
	std::vector<port_controller_set*> ret;
	try {
		for(auto i = n.begin(); i != n.end(); i++)
			ret.push_back(pcs_parse_set(root, ptr.index(i.index())));
	} catch(...) {
		for(auto i : ret)
			delete i;
		throw;
	}
	return ret;
}

void port_type_generic::_write(const port_type* _this, unsigned char* buffer, unsigned idx, unsigned ctrl, short x)
{
	const port_type_generic* th = dynamic_cast<const port_type_generic*>(_this);
	if(idx >= th->controller_info->controllers.size())
		return;
	if(ctrl >= th->controller_info->controllers[idx].buttons.size())
		return;
	auto& ii = th->indexinfo[th->indexbase[idx] + ctrl];
	switch(ii.type) {
	case 1:
		if(x) buffer[ii.offset]|=ii.mask; else buffer[ii.offset]&=ii.imask;
		break;
	case 2:
		buffer[ii.offset]=(unsigned short)x;
		buffer[ii.offset+1]=((unsigned short)x >> 8);
		break;
	}
}

short port_type_generic::_read(const port_type* _this, const unsigned char* buffer, unsigned idx, unsigned ctrl)
{
	const port_type_generic* th = dynamic_cast<const port_type_generic*>(_this);
	if(idx >= th->controller_info->controllers.size())
		return 0;
	if(ctrl >= th->controller_info->controllers[idx].buttons.size())
		return 0;
	auto& ii = th->indexinfo[th->indexbase[idx] + ctrl];
	switch(ii.type) {
	case 0:
		return 0;
	case 1:
		return (buffer[ii.offset]&ii.mask) ? 1 : 0;
	case 2:
		return (short)((unsigned short)buffer[ii.offset] + ((unsigned short)buffer[ii.offset+1] << 8));
	}
}

size_t port_type_generic::_serialize(const port_type* _this, const unsigned char* buffer, char* textbuf)
{
	size_t ptr = 0;
	short tmp;
	ser_instruction* si;
	const port_type_generic* th = dynamic_cast<const port_type_generic*>(_this);
	if(__builtin_expect(!buffer, 0)) {
		for(auto& i : th->serialize_instructions) {
			switch(i.type) {
			case 0: i.ejumpvector = &&ser_button; break;
			case 1: i.ejumpvector = &&ser_axis; break;
			case 2: i.ejumpvector = &&ser_pipe1; break;
			case 3: i.ejumpvector = &&ser_pipe2; break;
			case 4: i.ejumpvector = &&ser_pipe3; break;
			case 5: i.ejumpvector = &&ser_nothing; break;
			}
		}
		return 0;
	}
	size_t i = 0;
loop:
	si = &th->serialize_instructions[i++];
	goto *si->ejumpvector;
ser_button:
	textbuf[ptr++] = (buffer[si->offset] & si->mask) ? si->character : '.';
	goto loop;
ser_axis:
	tmp = (short)((unsigned short)buffer[si->offset] + ((unsigned short)buffer[si->offset+1] << 8));
	ptr += sprintf(textbuf + ptr, " %i", tmp);
	goto loop;
ser_pipe1:
	textbuf[ptr++] = '|';
	goto loop;
ser_pipe2:
	textbuf[ptr++] = '|';
	goto loop;
ser_pipe3:
	return ptr;
ser_nothing:
	return 0;
}

size_t port_type_generic::_deserialize(const port_type* _this, unsigned char* buffer, const char* textbuf)
{
	size_t ptr = 0;
	short tmp;
	ser_instruction* si;
	const port_type_generic* th = dynamic_cast<const port_type_generic*>(_this);
	if(__builtin_expect(!buffer, 0)) {
		for(auto& i : th->serialize_instructions) {
			switch(i.type) {
			case 0: i.djumpvector = &&ser_button; break;
			case 1: i.djumpvector = &&ser_axis; break;
			case 2: i.djumpvector = &&loop; break;
			case 3: i.djumpvector = &&ser_pipe2; break;
			case 4: i.djumpvector = &&ser_pipe3; break;
			case 5: i.djumpvector = &&ser_nothing; break;
			}
		}
		return 0;
	}
	memset(buffer, 0, th->storage_size);
	size_t i = 0;
loop:
	si = &th->serialize_instructions[i++];
	goto *si->djumpvector;
ser_button:
	if(read_button_value(textbuf, ptr)) buffer[si->offset]|=si->mask;
	goto loop;
ser_axis:
	tmp = read_axis_value(textbuf, ptr);
	buffer[si->offset] = (unsigned short)tmp;
	buffer[si->offset+1] = ((unsigned short)tmp >> 8);
	goto loop;
ser_pipe2:
	skip_rest_of_field(textbuf, ptr, true);
	goto loop;
ser_pipe3:
	skip_rest_of_field(textbuf, ptr, false);
	return ptr;
ser_nothing:
	return DESERIALIZE_SPECIAL_BLANK;
}

port_type_generic::port_type_generic(const JSON::node& root, const std::string& ptr) throw(std::exception)
	: port_type(port_iname(root, ptr), port_hname(root, ptr), port_size(root, ptr))
{
	controller_info = pcs_parse_set(root, ptr);
	write = _write;
	read = _read;
	serialize = _serialize;
	deserialize = _deserialize;
	size_t ibase = 0;
	size_t ii = 0;
	indexbase.resize(controller_info->controllers.size());
	for(auto i : controller_info->controllers) {
		indexbase[ii++] = ibase;
		ibase += i.buttons.size();
	}
	size_t aoffset = get_aoffset(*controller_info);
	serialize_instructions = get_ser_instructions(*controller_info);
	indexinfo = get_idx_instructions(*controller_info);
	_serialize(this, NULL, NULL);
	_deserialize(this, NULL, NULL);
	dyncode_block = NULL;
	make_dynamic_blocks();
}

port_type_generic::~port_type_generic() throw()
{
	delete reinterpret_cast<assembler::dynamic_code*>(dyncode_block);
}

std::string port_type_generic::port_iname(const JSON::node& root, const std::string& ptr)
{
	auto info = pcs_parse_set(root, ptr);
	std::string tmp = info->iname;
	delete info;
	return tmp;
}

std::string port_type_generic::port_hname(const JSON::node& root, const std::string& ptr)
{
	auto info = pcs_parse_set(root, ptr);
	std::string tmp = info->hname;
	delete info;
	return tmp;
}

size_t port_type_generic::port_size(const JSON::node& root, const std::string& ptr)
{
	auto info = pcs_parse_set(root, ptr);
	size_t tmp = get_ssize(*info);
	delete info;
	return tmp;
}

void port_type_generic::make_dynamic_blocks()
{
	try {
		std::list<assembler::label> labels;
		assembler::assembler a;
		make_routines(a, labels);

		assembler::dynamic_code* c;
		dyncode_block = c = new assembler::dynamic_code(a.size());
		auto m = a.flush(c->pointer());
		c->commit();
		if(m.count("read"))
			read = (short(*)(const port_type*, const unsigned char*, unsigned, unsigned))m["read"];
		if(m.count("write"))
			write = (void(*)(const port_type*, unsigned char*, unsigned, unsigned, short))m["write"];
		if(m.count("serialize"))
			serialize = (size_t(*)(const port_type*, const unsigned char*, char*))m["serialize"];
		if(m.count("deserialize"))
			deserialize = (size_t(*)(const port_type*, unsigned char*, const char*))m["deserialize"];
	} catch(std::exception& e) {
		std::cerr << "Error assembling block: " << e.what() << std::endl;
		delete reinterpret_cast<assembler::dynamic_code*>(dyncode_block);
		dyncode_block = NULL;
	} catch(...) {
		std::cerr << "Error assembling block!" << std::endl;
		delete reinterpret_cast<assembler::dynamic_code*>(dyncode_block);
		dyncode_block = NULL;
	}
}

#ifdef ARCH_IS_I386
assembler::label& alloc_label(std::list<assembler::label>& v)
{
	v.push_back(assembler::label());
	return v.back();
}

static bool i386_amd64()
{
	size_t r;
	asm volatile(
		//On i386, this is DEC EAX, MOV EAX, 0, NOP, NOP, NOP, NOP
		//On amd64, this is MOV RAX, 0x9090909000000000
		".byte 0x48\n"
		".byte 0xB8, 0, 0, 0, 0\n"
		".byte 0x90, 0x90, 0x90, 0x90\n"
		: "=a"(r));
	return (r != 0);
}

size_t axis_print(char* buf, short _v)
{
	int v = _v;
	size_t r = 0;
	buf[r++] = ' ';
	if(v < 0) { buf[r++] = '-'; v = -v; }
	if(v >= 10000) buf[r++] = '0' + (v / 10000 % 10);
	if(v >= 1000) buf[r++] = '0' + (v / 1000 % 10);
	if(v >= 100) buf[r++] = '0' + (v / 100 % 10);
	if(v >= 10) buf[r++] = '0' + (v / 10 % 10);
	buf[r++] = '0' + (v % 10);
	return r;
}

enum regnum { REG_AX, REG_CX, REG_DX, REG_BX, REG_SP, REG_BP, REG_SI, REG_DI };
enum modval { MOD_NO_OFF, MOD_OFF_1, MOD_OFF_4, MOD_REG };
enum specials { RM_SIB = 4, INDEX_NONE = 4, SCALE_1 = 0, SCALE_2 = 1, SCALE_4 = 2, SCALE_8 = 3};

void mov_reg_reg(assembler::assembler& a, uint8_t dest, uint8_t src, bool amd64)
{
	if(amd64) a(0x48);
	a(0x8B, assembler::i386_modrm(dest, MOD_REG, src));
}

void xor_ax_ax(assembler::assembler& a, bool amd64)
{
	if(amd64) a(0x48);
	a(0x31, assembler::i386_modrm(REG_AX, MOD_REG, REG_AX));
}

void inc_ax(assembler::assembler& a, bool amd64)
{
	if(amd64) a(0x48);
	a(0xFF, assembler::i386_modrm(0, MOD_REG, REG_AX));
}

void testb_si_offset_imm(assembler::assembler& a, size_t offset, uint8_t mask)
{
	if(offset > 127)
		a(0xF6, assembler::i386_modrm(0, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset), mask);
	else
		a(0xF6, assembler::i386_modrm(0, MOD_OFF_1, REG_SI), (uint8_t)offset, mask);
}

void movb_si_offset(assembler::assembler& a, size_t offset, uint8_t reg)
{
	if(offset > 127)
		a(0x8A, assembler::i386_modrm(reg, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset));
	else
		a(0x8A, assembler::i386_modrm(reg, MOD_OFF_1, REG_SI), (uint8_t)offset);
}

void movzbl_si_offset(assembler::assembler& a, size_t offset, uint8_t reg)
{
	if(offset > 127)
		a(0x0F, 0xB6, assembler::i386_modrm(reg, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset));
	else
		a(0x0F, 0xB6, assembler::i386_modrm(reg, MOD_OFF_1, REG_SI), (uint8_t)offset);
}

void load_cx_imm(assembler::assembler& a, int64_t val, bool amd64)
{
	if(amd64) a(0x48);
	a(0xC7, assembler::i386_modrm(0, MOD_REG, REG_CX), assembler::vle<int32_t>(val));
}

void orb_si_offset_imm(assembler::assembler& a, size_t offset, uint8_t mask)
{
	if(offset > 127)
		a(0x80, assembler::i386_modrm(1, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset), mask);
	else
		a(0x80, assembler::i386_modrm(1, MOD_OFF_1, REG_SI), (uint8_t)offset, mask);
}

void jnz_near(assembler::assembler& a, assembler::label& l)
{
	a(0x75, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
}

void jz_near(assembler::assembler& a, assembler::label& l)
{
	a(0x74, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
}

void jmp_near(assembler::assembler& a, assembler::label& l)
{
	a(0xEB, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
}

void mov_dxPax_imm(assembler::assembler& a, uint8_t val)
{
	a(0xC6, assembler::i386_modrm(0, MOD_NO_OFF, RM_SIB), assembler::i386_sib(REG_DX, REG_AX, SCALE_1), val);
}

void movb_dxPax_reg(assembler::assembler& a, uint8_t reg)
{
	a(0x88, assembler::i386_modrm(reg, MOD_NO_OFF, RM_SIB), assembler::i386_sib(REG_DX, REG_AX, SCALE_1));
}

void cmp_dxPax_imm(assembler::assembler& a, uint8_t val)
{
	a(0x80, assembler::i386_modrm(7, MOD_NO_OFF, RM_SIB), assembler::i386_sib(REG_DX, REG_AX, SCALE_1), val);
}

void andb_reg_imm(assembler::assembler& a, uint8_t reg, uint8_t val)
{
	a(0x80, assembler::i386_modrm(4, MOD_REG, reg), val);
}

void andd_reg_imm(assembler::assembler& a, uint8_t reg, uint8_t val)
{
	a(0x83, assembler::i386_modrm(4, MOD_REG, reg), val);
}

void cmpb_reg_imm(assembler::assembler& a, uint8_t reg, uint8_t val)
{
	a(0x80, assembler::i386_modrm(7, MOD_REG, reg), val);
}

void cmpd_reg_imm(assembler::assembler& a, uint8_t reg, uint8_t val)
{
	a(0x83, assembler::i386_modrm(7, MOD_REG, reg), val);
}

void addb_reg_imm(assembler::assembler& a, uint8_t reg, uint8_t val)
{
	a(0x80, assembler::i386_modrm(0, MOD_REG, reg), val);
}

void addd_reg_imm(assembler::assembler& a, uint8_t reg, uint8_t val)
{
	a(0x83, assembler::i386_modrm(0, MOD_REG, reg), val);
}

void sbbb_reg_reg(assembler::assembler& a, uint8_t dst, uint8_t src)
{
	a(0x18, assembler::i386_modrm(src, MOD_REG, dst));
}

void sbbd_reg_reg(assembler::assembler& a, uint8_t dst, uint8_t src)
{
	a(0x19, assembler::i386_modrm(src, MOD_REG, dst));
}

void mov_si_offset_si_word(assembler::assembler& a, size_t offset, bool amd64)
{
	if(offset > 127)
		a(0x66, 0x8B, assembler::i386_modrm(REG_SI, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset));
	else
		a(0x66, 0x8B, assembler::i386_modrm(REG_SI, MOD_OFF_1, REG_SI), offset);
	if(amd64) a(0x48);
	a(0x81, assembler::i386_modrm(4, MOD_REG, REG_SI), assembler::vle<int32_t>(0xFFFF));
}

void lea_load_out_addr_di(assembler::assembler& a, bool amd64)
{
	if(amd64) a(0x48);
	a(0x8D, assembler::i386_modrm(REG_DI, MOD_NO_OFF, RM_SIB), assembler::i386_sib(REG_DX, REG_AX, SCALE_1));
}

void add_ax_cx(assembler::assembler& a, bool amd64)
{
	if(amd64) a(0x48);
	a(0x01, assembler::i386_modrm(REG_CX, MOD_REG, REG_AX));
}

void call_label(assembler::assembler& a, assembler::label& l, uint8_t amd64_tmpreg, bool amd64)
{
	if(amd64) {
		a(0x48 | ((amd64_tmpreg&8) ? 1 : 0), 0xB8 | (amd64_tmpreg&7), 
			assembler::relocation_tag(assembler::i386_reloc_abs64, l), assembler::pad_tag(8),
			0x40 | ((amd64_tmpreg&8) ? 1 : 0), 0xFF, assembler::i386_modrm(2, MOD_REG, (amd64_tmpreg&7)));
	} else
		a(0xE8, assembler::relocation_tag(assembler::i386_reloc_rel32, l), assembler::pad_tag(4));
}

void push_gp(assembler::assembler& a, uint8_t reg)
{
	a(0x50 + (reg&7));
}

void pop_gp(assembler::assembler& a, uint8_t reg)
{
	a(0x58 + (reg&7));
}

void add_esp_imm(assembler::assembler& a, int8_t val)
{
	a(0x83, assembler::i386_modrm(0, MOD_REG, REG_SP), val);
}

void load_gp_esp_n(assembler::assembler& a, uint8_t reg, int8_t offset)
{
	a(0x8B, assembler::i386_modrm(reg, MOD_OFF_1, RM_SIB), assembler::i386_sib(REG_SP, INDEX_NONE, SCALE_1),
		offset);
}

void mov_si_offset_imm(assembler::assembler& a, int32_t offset, uint8_t val)
{
	if(offset > 127)
		a(0xC6, assembler::i386_modrm(0, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset), val);
	else
		a(0xC6, assembler::i386_modrm(0, MOD_OFF_1, REG_SI), offset, val);
}

void mov_ax_imm(assembler::assembler& a, size_t val, bool amd64)
{
	if(amd64) a(0x48, 0xB8, assembler::vle<uint64_t>(val));
	else a(0xB8, assembler::vle<uint32_t>(val));
}

void mov_si_offset_word(assembler::assembler& a, int32_t offset, uint8_t reg)
{
	if(offset > 127)
		a(0x66, 0x89, assembler::i386_modrm(reg, MOD_OFF_4, REG_SI), assembler::vle<int32_t>(offset));
	else
		a(0x66, 0x89, assembler::i386_modrm(reg, MOD_OFF_1, REG_SI), offset);
}

void ret(assembler::assembler& a)
{
	a(0xC3);
}

void port_type_generic::make_routines(assembler::assembler& a, std::list<assembler::label>& labels)
{
	bool amd64 = i386_amd64();
	assembler::label& axisprint = (alloc_label(labels) = assembler::label((void*)axis_print));
	assembler::label& axisread = (alloc_label(labels) = assembler::label((void*)read_axis_value));
	a._label(alloc_label(labels), "serialize");
	if(!amd64) {
		//Emit extra prologue for serialize.
		push_gp(a, REG_SI);
		push_gp(a, REG_DX);
		load_gp_esp_n(a, REG_SI, 16);
		load_gp_esp_n(a, REG_DX, 20);
	}
	//SI: State block.
	//DX: Output buffer.
	//AX: Counter.
	xor_ax_ax(a, amd64);
	for(auto& i : serialize_instructions) {
		switch(i.type) {
		case 0: { //Button.
/*
			This is really bad:
			movb_si_offset(a, i.offset, REG_CX);
			andb_reg_imm(a, REG_CX, i.mask);
			cmpb_reg_imm(a, REG_CX, i.mask);
			sbbb_reg_reg(a, REG_CX, REG_CX);
			andb_reg_imm(a, REG_CX, 0x2E - i.character);
			addb_reg_imm(a, REG_CX, i.character);
			movb_dxPax_reg(a, REG_CX);
			//Better but still bad.
			movzbl_si_offset(a, i.offset, REG_CX);
			andd_reg_imm(a, REG_CX, i.mask);
			cmpd_reg_imm(a, REG_CX, i.mask);
			sbbd_reg_reg(a, REG_CX, REG_CX);
			andd_reg_imm(a, REG_CX, 0x2E - i.character);
			addd_reg_imm(a, REG_CX, i.character);
			movb_dxPax_reg(a, REG_CX);
*/
			assembler::label& button_set = alloc_label(labels);
			assembler::label& end_char = alloc_label(labels);
			testb_si_offset_imm(a, i.offset, i.mask);
			jnz_near(a, button_set);
			mov_dxPax_imm(a, '.');
			jmp_near(a, end_char);
			a._label(button_set);
			mov_dxPax_imm(a, i.character);
			a._label(end_char);

			inc_ax(a, amd64);
			break;
		}
		case 1: //Axis
			if(!amd64) push_gp(a, REG_DI);
			push_gp(a, REG_SI);
			push_gp(a, REG_DX);
			push_gp(a, REG_AX);
			mov_si_offset_si_word(a, i.offset, amd64);
			lea_load_out_addr_di(a, amd64);
			if(!amd64) push_gp(a, REG_SI);
			if(!amd64) push_gp(a, REG_DI);
			call_label(a, axisprint, 11, amd64);
			if(!amd64) add_esp_imm(a, 0x8);
			pop_gp(a, REG_CX);	//POP CX instead of AX so we can add the return value.
			pop_gp(a, REG_DX);
			pop_gp(a, REG_SI);
			if(!amd64) pop_gp(a, REG_DI);
			add_ax_cx(a, amd64);
			break;
		case 2: //Pipe character
		case 3:
			mov_dxPax_imm(a, '|');
			inc_ax(a, amd64);
			break;
		case 4: //End or nothing
		case 5:
			if(!amd64) pop_gp(a, REG_DX);
			if(!amd64) pop_gp(a, REG_SI);
			ret(a);
			break;
		}
	}
	a._label(alloc_label(labels), "deserialize");
	if(!amd64) {
		//Emit extra prologue for deserialize.
		push_gp(a, REG_SI);
		push_gp(a, REG_DX);
		load_gp_esp_n(a, REG_SI, 16);
		load_gp_esp_n(a, REG_DX, 20);
	}
	//SI: State block.
	//DX: Output buffer.
	//AX: Counter.
	xor_ax_ax(a, amd64);
	for(size_t i = 0; i < storage_size; i++)
		mov_si_offset_imm(a, i, 0);
	for(auto& i : serialize_instructions) {
		switch(i.type) {
		case 0: { //Button.
			assembler::label& no_progress = alloc_label(labels);
			assembler::label& clear_button = alloc_label(labels);
			cmp_dxPax_imm(a, '|');
			jz_near(a, no_progress);
			cmp_dxPax_imm(a, '\r');
			jz_near(a, no_progress);
			cmp_dxPax_imm(a, '\n');
			jz_near(a, no_progress);
			cmp_dxPax_imm(a, '\0');
			jz_near(a, no_progress);
			cmp_dxPax_imm(a, ' ');
			jz_near(a, clear_button);
			cmp_dxPax_imm(a, '.');
			jz_near(a, clear_button);
			orb_si_offset_imm(a, i.offset, i.mask);
			a._label(clear_button);
			inc_ax(a, amd64);
			a._label(no_progress);
			break;
		}
		case 1: //Axis
			if(!amd64) push_gp(a, REG_DI);
			push_gp(a, REG_SI);
			push_gp(a, REG_DX);
			push_gp(a, REG_AX);
			mov_reg_reg(a, REG_DI, REG_DX, amd64);
			mov_reg_reg(a, REG_SI, REG_SP, amd64);
			if(!amd64) push_gp(a, REG_SI);
			if(!amd64) push_gp(a, REG_DI);
			call_label(a, axisread, 11, amd64);
			if(!amd64) add_esp_imm(a, 0x8);
			mov_reg_reg(a, REG_CX, REG_AX, amd64);
			pop_gp(a, REG_AX);
			pop_gp(a, REG_DX);
			pop_gp(a, REG_SI);
			if(!amd64) pop_gp(a, REG_DI);
			mov_si_offset_word(a, i.offset, REG_CX);
			break;
		case 2: //Pipe character 1
			break;  //Do nothing.
		case 3: //Pipe character 2 
		case 4: { //Pipe character 3
			assembler::label& not_last_pipe = alloc_label(labels);
			assembler::label& goto_out = alloc_label(labels);
			assembler::label& loop = alloc_label(labels);
			a._label(loop);
			cmp_dxPax_imm(a, '|');
			jz_near(a, goto_out);
			cmp_dxPax_imm(a, '\r');
			jz_near(a, goto_out);
			cmp_dxPax_imm(a, '\n');
			jz_near(a, goto_out);
			cmp_dxPax_imm(a, '\0');
			jz_near(a, goto_out);
			inc_ax(a, amd64);
			jmp_near(a, loop);
			a._label(goto_out);
			if(i.type == 3) {
				cmp_dxPax_imm(a, '|');
				jnz_near(a, not_last_pipe);
				inc_ax(a, amd64);
				a._label(not_last_pipe);
			} else {
				if(!amd64) pop_gp(a, REG_DX);
				if(!amd64) pop_gp(a, REG_SI);
				ret(a);
			}
			break;
		}
		case 5:
			mov_ax_imm(a, DESERIALIZE_SPECIAL_BLANK, amd64);
			if(!amd64) pop_gp(a, REG_DX);
			if(!amd64) pop_gp(a, REG_SI);
			ret(a);
			break;
		}
	}
	
}
#else
void port_type_generic::make_routines(assembler::assembler& a, std::list<assembler::label>& labels)
{
	throw std::runtime_error("ASM on this arch not supported");
}

#endif
