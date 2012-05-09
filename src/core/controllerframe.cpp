#include "core/bsnes.hpp"

#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"

#include <cstdio>
#include <iostream>

#define SYSTEM_BYTES 5

namespace
{
	std::set<porttype_info*>& porttypes()
	{
		static std::set<porttype_info*> p;
		return p;
	}

	const char* buttonnames[MAX_LOGICAL_BUTTONS] = {
		"left", "right", "up", "down", "A", "B", "X", "Y", "L", "R", "select", "start", "trigger",
		"cursor", "turbo", "pause"
	};

	template<unsigned type>
	void set_core_controller_bsnes(unsigned port) throw()
	{
		if(port > 1)
			return;
		snes_set_controller_port_device(port != 0, type);
	}

	void set_core_controller_illegal(unsigned port) throw()
	{
		std::cerr << "Attempt to set core port type to INVALID port type" << std::endl;
		exit(1);
	}

	struct porttype_invalid : public porttype_info
	{
		porttype_invalid() : porttype_info(PT_INVALID, "invalid-port-type", 0)
		{
			write = NULL;
			read = NULL;
			display = NULL;
			serialize = NULL;
			deserialize = NULL;
			devicetype = generic_port_devicetype<0, DT_NONE>;
			controllers = 0;
			internal_type = 0;
			legal = generic_port_legal<0xFFFFFFFFU>;
			set_core_controller = set_core_controller_illegal;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return -1;
		}
	};

	struct porttype_gamepad : public porttype_info
	{
		porttype_gamepad() : porttype_info(PT_GAMEPAD, "gamepad", generic_port_size<1, 0, 12>())
		{
			write = generic_port_write<1, 0, 12>;
			read = generic_port_read<1, 0, 12>;
			display = generic_port_display<1, 0, 12, 0>;
			serialize = generic_port_serialize<1, 0, 12, 0>;
			deserialize = generic_port_deserialize<1, 0, 12>;
			devicetype = generic_port_devicetype<1, DT_GAMEPAD>;
			legal = generic_port_legal<3>;
			controllers = 1;
			internal_type = SNES_DEVICE_JOYPAD;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_JOYPAD>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 0)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_LEFT:	return SNES_DEVICE_ID_JOYPAD_LEFT;
			case LOGICAL_BUTTON_RIGHT:	return SNES_DEVICE_ID_JOYPAD_RIGHT;
			case LOGICAL_BUTTON_UP:		return SNES_DEVICE_ID_JOYPAD_UP;
			case LOGICAL_BUTTON_DOWN:	return SNES_DEVICE_ID_JOYPAD_DOWN;
			case LOGICAL_BUTTON_A:		return SNES_DEVICE_ID_JOYPAD_A;
			case LOGICAL_BUTTON_B:		return SNES_DEVICE_ID_JOYPAD_B;
			case LOGICAL_BUTTON_X:		return SNES_DEVICE_ID_JOYPAD_X;
			case LOGICAL_BUTTON_Y:		return SNES_DEVICE_ID_JOYPAD_Y;
			case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_JOYPAD_L;
			case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_JOYPAD_R;
			case LOGICAL_BUTTON_SELECT:	return SNES_DEVICE_ID_JOYPAD_SELECT;
			case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JOYPAD_START;
			default:			return -1;
			}
		}
	} gamepad;

	struct porttype_justifier : public porttype_info
	{
		porttype_justifier() : porttype_info(PT_JUSTIFIER, "justifier", generic_port_size<1, 2, 2>())
		{
			write = generic_port_write<1, 2, 2>;
			read = generic_port_read<1, 2, 2>;
			display = generic_port_display<1, 2, 2, 12>;
			serialize = generic_port_serialize<1, 2, 2, 12>;
			deserialize = generic_port_deserialize<1, 2, 2>;
			devicetype = generic_port_devicetype<1, DT_LIGHTGUN>;
			legal = generic_port_legal<2>;
			controllers = 1;
			internal_type = SNES_DEVICE_JUSTIFIER;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_JUSTIFIER>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 0)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JUSTIFIER_START;
			case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_JUSTIFIER_TRIGGER;
			default:			return -1;
			}
		}
	} justifier;

	struct porttype_justifiers : public porttype_info
	{
		porttype_justifiers() : porttype_info(PT_JUSTIFIERS, "justifiers", generic_port_size<2, 2, 2>())
		{
			write = generic_port_write<2, 2, 2>;
			read = generic_port_read<2, 2, 2>;
			display = generic_port_display<2, 2, 2, 0>;
			serialize = generic_port_serialize<2, 2, 2, 12>;
			deserialize = generic_port_deserialize<2, 2, 2>;
			devicetype = generic_port_devicetype<2, DT_LIGHTGUN>;
			legal = generic_port_legal<2>;
			controllers = 2;
			internal_type = SNES_DEVICE_JUSTIFIERS;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_JUSTIFIERS>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 1)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JUSTIFIER_START;
			case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_JUSTIFIER_TRIGGER;
			default:			return -1;
			}
		}
	} justifiers;

	struct porttype_mouse : public porttype_info
	{
		porttype_mouse() : porttype_info(PT_MOUSE, "mouse", generic_port_size<1, 2, 2>())
		{
			write = generic_port_write<1, 2, 2>;
			read = generic_port_read<1, 2, 2>;
			display = generic_port_display<1, 2, 2, 0>;
			serialize = generic_port_serialize<1, 2, 2, 12>;
			deserialize = generic_port_deserialize<1, 2, 2>;
			devicetype = generic_port_devicetype<1, DT_MOUSE>;
			legal = generic_port_legal<3>;
			controllers = 1;
			internal_type = SNES_DEVICE_MOUSE;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_MOUSE>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 0)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_MOUSE_LEFT;
			case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_MOUSE_RIGHT;
			default:			return -1;
			}
		}
	} mouse;

	struct porttype_multitap : public porttype_info
	{
		porttype_multitap() : porttype_info(PT_MULTITAP, "multitap", generic_port_size<4, 0, 12>())
		{
			write = generic_port_write<4, 0, 12>;
			read = generic_port_read<4, 0, 12>;
			display = generic_port_display<4, 0, 12, 0>;
			serialize = generic_port_serialize<4, 0, 12, 0>;
			deserialize = generic_port_deserialize<4, 0, 12>;
			devicetype = generic_port_devicetype<4, DT_GAMEPAD>;
			legal = generic_port_legal<3>;
			controllers = 4;
			internal_type = SNES_DEVICE_MULTITAP;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_MULTITAP>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 3)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_LEFT:	return SNES_DEVICE_ID_JOYPAD_LEFT;
			case LOGICAL_BUTTON_RIGHT:	return SNES_DEVICE_ID_JOYPAD_RIGHT;
			case LOGICAL_BUTTON_UP:		return SNES_DEVICE_ID_JOYPAD_UP;
			case LOGICAL_BUTTON_DOWN:	return SNES_DEVICE_ID_JOYPAD_DOWN;
			case LOGICAL_BUTTON_A:		return SNES_DEVICE_ID_JOYPAD_A;
			case LOGICAL_BUTTON_B:		return SNES_DEVICE_ID_JOYPAD_B;
			case LOGICAL_BUTTON_X:		return SNES_DEVICE_ID_JOYPAD_X;
			case LOGICAL_BUTTON_Y:		return SNES_DEVICE_ID_JOYPAD_Y;
			case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_JOYPAD_L;
			case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_JOYPAD_R;
			case LOGICAL_BUTTON_SELECT:	return SNES_DEVICE_ID_JOYPAD_SELECT;
			case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JOYPAD_START;
			default:			return -1;
			}
		}
	} multitap;

	struct porttype_none : public porttype_info
	{
		porttype_none() : porttype_info(PT_NONE, "none", generic_port_size<0, 0, 0>())
		{
			write = generic_port_write<0, 0, 0>;
			read = generic_port_read<0, 0, 0>;
			display = generic_port_display<0, 0, 0, 0>;
			serialize = generic_port_serialize<0, 0, 0, 0>;
			deserialize = generic_port_deserialize<0, 0, 0>;
			devicetype = generic_port_devicetype<0, DT_GAMEPAD>;
			legal = generic_port_legal<3>;
			controllers = 0;
			internal_type = SNES_DEVICE_NONE;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_NONE>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return -1;
		}
	} none;

	struct porttype_superscope : public porttype_info
	{
		porttype_superscope() : porttype_info(PT_SUPERSCOPE, "superscope", generic_port_size<1, 2, 4>())
		{
			write = generic_port_write<1, 2, 4>;
			read = generic_port_read<1, 2, 4>;
			display = generic_port_display<1, 2, 4, 0>;
			serialize = generic_port_serialize<1, 2, 4, 14>;
			deserialize = generic_port_deserialize<1, 2, 4>;
			devicetype = generic_port_devicetype<1, DT_LIGHTGUN>;
			legal = generic_port_legal<2>;
			controllers = 1;
			internal_type = SNES_DEVICE_SUPER_SCOPE;
			set_core_controller = set_core_controller_bsnes<SNES_DEVICE_SUPER_SCOPE>;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 0)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER;
			case LOGICAL_BUTTON_CURSOR:	return SNES_DEVICE_ID_SUPER_SCOPE_CURSOR;
			case LOGICAL_BUTTON_TURBO:	return SNES_DEVICE_ID_SUPER_SCOPE_TURBO;
			case LOGICAL_BUTTON_PAUSE:	return SNES_DEVICE_ID_SUPER_SCOPE_PAUSE;
			default:			return -1;
			}
		}
	} superscope;

	porttype_invalid& get_invalid_port_type()
	{
		static porttype_invalid inv;
		return inv;
	}
}

/**
 * Get name of logical button.
 *
 * Parameter lbid: ID of logical button.
 * Returns: The name of button.
 * Throws std::bad_alloc: Not enough memory.
 */
std::string get_logical_button_name(unsigned lbid) throw(std::bad_alloc)
{
	if(lbid >= MAX_LOGICAL_BUTTONS)
		return "";
	return buttonnames[lbid];
}

const porttype_info& porttype_info::lookup(porttype_t p) throw(std::runtime_error)
{
	get_invalid_port_type();
	for(auto i : porttypes())
		if(p == i->value)
			return *i;
	throw std::runtime_error("Bad port type");
}

const porttype_info& porttype_info::lookup(const std::string& p) throw(std::runtime_error)
{
	get_invalid_port_type();
	for(auto i : porttypes())
		if(p == i->name && i->value != PT_INVALID)
			return *i;
	throw std::runtime_error("Bad port type");
}

porttype_info::~porttype_info() throw()
{
	porttypes().erase(this);
}

porttype_info::porttype_info(porttype_t ptype, const std::string& pname, size_t psize) throw(std::bad_alloc)
{
	value = ptype;
	name = pname;
	storage_size = psize;
	porttypes().insert(this);
}

bool porttype_info::is_analog(unsigned controller) const throw()
{
	devicetype_t d = devicetype(controller);
	return (d == DT_MOUSE || d == DT_LIGHTGUN);
}

bool porttype_info::is_mouse(unsigned controller) const throw()
{
	return (devicetype(controller) == DT_MOUSE);
}

pollcounter_vector::pollcounter_vector() throw()
{
	clear();
}

void pollcounter_vector::clear() throw()
{
	system_flag = false;
	//FIXME: Support more controllers/ports
	ctrs.resize(96);
	memset(&ctrs[0], 0, sizeof(uint32_t) * ctrs.size());
}

void pollcounter_vector::set_all_DRDY() throw()
{
	for(size_t i = 0; i < ctrs.size() ; i++)
		ctrs[i] |= 0x80000000UL;
}

namespace
{
	inline size_t INDEXOF(unsigned port, unsigned pcid, unsigned ctrl)
	{
		//FIXME: Support more controls.
		return port * 48 + pcid * 12 + ctrl;
	}

	inline unsigned IINDEXOF_PORT(unsigned c)
	{
		//FIXME: Support more controls.
		return c / 48;
	}

	inline unsigned IINDEXOF_PCID(unsigned c)
	{
		//FIXME: Support more controls.
		return c / 12 % 4;
	}

	inline unsigned IINDEXOF_CTRL(unsigned c)
	{
		//FIXME: Support more controls.
		return c % 12;
	}
}

void pollcounter_vector::clear_DRDY(unsigned port, unsigned pcid, unsigned ctrl) throw()
{
	ctrs[INDEXOF(port, pcid, ctrl)] &= 0x7FFFFFFFUL;
}

bool pollcounter_vector::get_DRDY(unsigned port, unsigned pcid, unsigned ctrl) throw()
{
	return ((ctrs[INDEXOF(port, pcid, ctrl)] & 0x80000000UL) != 0);
}

bool pollcounter_vector::has_polled() throw()
{
	uint32_t res = system_flag ? 1 : 0;
	for(size_t i = 0; i < ctrs.size(); i++)
		res |= ctrs[i];
	return ((res & 0x7FFFFFFFUL) != 0);
}

uint32_t pollcounter_vector::get_polls(unsigned port, unsigned pcid, unsigned ctrl) throw()
{
	return ctrs[INDEXOF(port, pcid, ctrl)] & 0x7FFFFFFFUL;
}

uint32_t pollcounter_vector::get_polls(unsigned ctrl) throw()
{
	return get_polls(IINDEXOF_PORT(ctrl), IINDEXOF_PCID(ctrl), IINDEXOF_CTRL(ctrl));
}

uint32_t pollcounter_vector::increment_polls(unsigned port, unsigned pcid, unsigned ctrl) throw()
{
	size_t i = INDEXOF(port, pcid, ctrl);
	uint32_t x = ctrs[i] & 0x7FFFFFFFUL;
	++ctrs[i];
	return x;
}

unsigned pollcounter_vector::maxbuttons() throw()
{
	return ctrs.size();
}

void pollcounter_vector::set_system() throw()
{
	system_flag = true;
}

bool pollcounter_vector::get_system() throw()
{
	return system_flag;
}

uint32_t pollcounter_vector::max_polls() throw()
{
	uint32_t max = system_flag ? 1 : 0;
	for(unsigned i = 0; i < ctrs.size(); i++) {
		uint32_t tmp = ctrs[i] & 0x7FFFFFFFUL;
		max = (max < tmp) ? tmp : max;
	}
	return max;
}

void pollcounter_vector::save_state(std::vector<uint32_t>& mem) throw(std::bad_alloc)
{
	mem.resize(4 + ctrs.size());
	//Compatiblity fun.
	mem[0] = 0x80000000UL;
	mem[1] = system_flag ? 1 : 0x80000000UL;
	mem[2] = system_flag ? 1 : 0x80000000UL;
	mem[3] = system_flag ? 1 : 0x80000000UL;
	for(size_t i = 0; i < ctrs.size(); i++)
		mem[4 + i] = ctrs[i];
}

void pollcounter_vector::load_state(const std::vector<uint32_t>& mem) throw()
{
	system_flag = (mem[1] | mem[2] | mem[3]) & 0x7FFFFFFFUL;
	for(size_t i = 0; i < ctrs.size(); i++)
		ctrs[i] = mem[i + 4];
}

bool pollcounter_vector::check(const std::vector<uint32_t>& mem) throw()
{
	return (mem.size() == ctrs.size() + 4);
}

controller_frame::controller_frame(porttype_t p1, porttype_t p2) throw(std::runtime_error)
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types[0] = p1;
	types[1] = p2;
	set_types(types);
}

controller_frame::controller_frame(unsigned char* mem, porttype_t p1, porttype_t p2) throw(std::runtime_error)
{
	if(!mem)
		throw std::runtime_error("NULL backing memory not allowed");
	memset(memory, 0, sizeof(memory));
	backing = mem;
	types[0] = p1;
	types[1] = p2;
	set_types(types);
}

controller_frame::controller_frame(const controller_frame& obj) throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	set_types(obj.types);
	memcpy(backing, obj.backing, totalsize);
}

controller_frame& controller_frame::operator=(const controller_frame& obj) throw(std::runtime_error)
{
	set_types(obj.types);
	memcpy(backing, obj.backing, totalsize);
}

void controller_frame::set_types(const porttype_t* tarr)
{
	for(unsigned i = 0; i < MAX_PORTS; i++) {
		if(memory != backing && types[i] != tarr[i])
			throw std::runtime_error("Controller_frame: Type mismatch");
		if(!porttype_info::lookup(tarr[i]).legal(i))
			throw std::runtime_error("Illegal port type for port index");
	}
	size_t offset = SYSTEM_BYTES;
	for(unsigned i = 0; i < MAX_PORTS; i++) {
		offsets[i] = offset;
		types[i] = tarr[i];
		pinfo[i] = &porttype_info::lookup(tarr[i]);
		offset += pinfo[i]->storage_size;
	}
	totalsize = offset;
}

void controller_frame::axis2(unsigned ctrl, short value) throw()
{
	axis(IINDEXOF_PORT(ctrl), IINDEXOF_PCID(ctrl), IINDEXOF_CTRL(ctrl), value);
}

short controller_frame::axis2(unsigned ctrl) throw()
{
	return axis(IINDEXOF_PORT(ctrl), IINDEXOF_PCID(ctrl), IINDEXOF_CTRL(ctrl));
}

unsigned controller_frame::maxbuttons() throw()
{
	//FIXME: Fix for more ports/controllers/controls
	return 96;
}

bool controller_frame::type_matches(const controller_frame& a)
{
	for(unsigned i = 0; i < MAX_PORTS; i++)
		if(a.types[i] != types[i])
			return false;
	return true;
}

size_t controller_frame_vector::walk_helper(size_t frame, bool sflag) throw()
{
	size_t ret = sflag ? frame : 0;
	if(frame >= frames)
		return ret;
	frame++;
	ret++;
	size_t page = frame / frames_per_page;
	size_t offset = frame_size * (frame % frames_per_page);
	size_t index = frame % frames_per_page;
	if(cache_page_num != page) {
		cache_page = &pages[page];
		cache_page_num = page;
	}
	while(frame < frames) {
		if(index == frames_per_page) {
			page++;
			cache_page = &pages[page];
			cache_page_num = page;
		}
		if(controller_frame::sync(cache_page->content + offset))
			break;
		index++;
		offset += frame_size;
		frame++;
		ret++;
	}
	return ret;
}

size_t controller_frame_vector::count_frames() throw()
{
	size_t ret = 0;
	if(!frames)
		return 0;
	cache_page_num = 0;
	cache_page = &pages[0];
	size_t offset = 0;
	size_t index = 0;
	for(size_t i = 0; i < frames; i++) {
		if(index == frames_per_page) {
			cache_page_num++;
			cache_page = &pages[cache_page_num];
			index = 0;
			offset = 0;
		}
		if(controller_frame::sync(cache_page->content + offset))
			ret++;
		index++;
		offset += frame_size;
		
	}
	return ret;
}

void controller_frame_vector::clear(enum porttype_t p1, enum porttype_t p2) throw(std::runtime_error)
{
	controller_frame check(p1, p2);
	frame_size = check.size();
	frames_per_page = CONTROLLER_PAGE_SIZE / frame_size;
	frames = 0;
	types[0] = p1;
	types[1] = p2;
	clear_cache();
	pages.clear();
}

controller_frame_vector::~controller_frame_vector() throw()
{
	pages.clear();
	cache_page = NULL;
}

controller_frame_vector::controller_frame_vector(enum porttype_t p1, enum porttype_t p2) throw(std::runtime_error)
{
	clear(p1, p2);
}

void controller_frame_vector::append(controller_frame frame) throw(std::bad_alloc, std::runtime_error)
{
	controller_frame check(types[0], types[1]);
	if(!check.types_match(frame))
		throw std::runtime_error("controller_frame_vector::append: Type mismatch");
	if(frames % frames_per_page == 0) {
		//Create new page.
		pages[frames / frames_per_page];
	}
	//Write the entry.
	size_t page = frames / frames_per_page;
	size_t offset = frame_size * (frames % frames_per_page);
	if(cache_page_num != page) {
		cache_page_num = page;
		cache_page = &pages[page];
	}
	controller_frame(cache_page->content + offset, types[0], types[1]) = frame;
	frames++;
}

controller_frame_vector::controller_frame_vector(const controller_frame_vector& vector) throw(std::bad_alloc)
{
	clear(vector.types[0], vector.types[1]);
	*this = vector;
}

controller_frame_vector& controller_frame_vector::operator=(const controller_frame_vector& v)
	throw(std::bad_alloc)
{
	if(this == &v)
		return *this;
	resize(v.frames);
	clear_cache();

	//Copy the fields.
	frame_size = v.frame_size;
	frames_per_page = v.frames_per_page;
	for(size_t i = 0; i < MAX_PORTS; i++)
		types[i] = v.types[i];

	//This can't fail anymore. Copy the raw page contents.
	size_t pagecount = (frames + frames_per_page - 1) / frames_per_page;
	for(size_t i = 0; i < pagecount; i++) {
		page& pg = pages[i];
		const page& pg2 = v.pages.find(i)->second;
		pg = pg2;
	}

	return *this;
}

size_t controller_frame::system_serialize(const unsigned char* buffer, char* textbuf)
{
	char tmp[128];
	if(buffer[1] || buffer[2] || buffer[3] || buffer[4])
		sprintf(tmp, "%c%c %i %i", ((buffer[0] & 1) ? 'F' : '.'), ((buffer[0] & 2) ? 'R' : '.'),
			unserialize_short(buffer + 1), unserialize_short(buffer + 3));
	else
		sprintf(tmp, "%c%c", ((buffer[0] & 1) ? 'F' : '.'), ((buffer[0] & 2) ? 'R' : '.'));
	size_t len = strlen(tmp);
	memcpy(textbuf, tmp, len);
	return len;
}

size_t controller_frame::system_deserialize(unsigned char* buffer, const char* textbuf)
{
	buffer[0] = 0;
	size_t idx = 0;
	if(read_button_value(textbuf, idx))
		buffer[0] |= 1;
	if(read_button_value(textbuf, idx))
		buffer[0] |= 2;
	serialize_short(buffer + 1, read_axis_value(textbuf, idx));
	serialize_short(buffer + 3, read_axis_value(textbuf, idx));
	skip_rest_of_field(textbuf, idx, false);
	return idx;
}

short read_axis_value(const char* buf, size_t& idx) throw()
{
		char ch;
		//Skip ws.
		while(is_nonterminator(buf[idx])) {
			char ch = buf[idx];
			if(ch != ' ' && ch != '\t')
				break;
			idx++;
		}
		//Read the sign if any.
		ch = buf[idx];
		if(!is_nonterminator(ch))
			return 0;
		bool negative = false;
		if(ch == '-') {
			negative = true;
			idx++;
		}
		if(ch == '+')
			idx++;

		//Read numeric value.
		int numval = 0;
		while(is_nonterminator(buf[idx]) && isdigit(static_cast<unsigned char>(ch = buf[idx]))) {
			numval = numval * 10 + (ch - '0');
			idx++;
		}
		if(negative)
			numval = -numval;

		return static_cast<short>(numval);
}

void controller_frame_vector::resize(size_t newsize) throw(std::bad_alloc)
{
	clear_cache();
	if(newsize == 0) {
		clear();
	} else if(newsize < frames) {
		//Shrink movie.
		size_t current_pages = (frames + frames_per_page - 1) / frames_per_page;
		size_t pages_needed = (newsize + frames_per_page - 1) / frames_per_page;
		for(size_t i = pages_needed; i < current_pages; i++)
			pages.erase(i);
		//Now zeroize the excess memory.
		if(newsize < pages_needed * frames_per_page) {
			size_t offset = frame_size * (newsize % frames_per_page);
			memset(pages[pages_needed - 1].content + offset, 0, CONTROLLER_PAGE_SIZE - offset);
		}
		frames = newsize;
	} else if(newsize > frames) {
		//Enlarge movie.
		size_t current_pages = (frames + frames_per_page - 1) / frames_per_page;
		size_t pages_needed = (newsize + frames_per_page - 1) / frames_per_page;
		//Create the needed pages.
		for(size_t i = current_pages; i < pages_needed; i++) {
			try {
				pages[i];
			} catch(...) {
				for(size_t i = current_pages; i < pages_needed; i++)
					if(pages.count(i))
						pages.erase(i);
				throw;
			}
		}
		frames = newsize;
	}
}

controller_frame::controller_frame() throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	for(unsigned i = 0; i < MAX_PORTS; i++) {
		offsets[i] = SYSTEM_BYTES;
		types[i] = PT_INVALID;
		pinfo[i] = NULL;
	}
	totalsize = SYSTEM_BYTES;
}

unsigned controller_frame::control_count()
{
	//FIXME: More controls.
	return 12;
}

void controller_frame::set_port_type(unsigned port, porttype_t ptype) throw(std::runtime_error)
{
	char tmp[MAXIMUM_CONTROLLER_FRAME_SIZE] = {0};
	if(!porttype_info::lookup(ptype).legal(port))
		throw std::runtime_error("Illegal port type for port index");
	if(memory != backing)
		throw std::runtime_error("Can't set port type on non-dedicated controller frame");
	if(port >= MAX_PORTS)
		return;
	const porttype_info* newpinfo[MAX_PORTS];
	size_t newoffsets[MAX_PORTS];
	size_t offset = SYSTEM_BYTES;
	for(size_t i = 0; i < MAX_PORTS; i++) {
		if(i != port)
			newpinfo[i] = pinfo[i];
		else
			newpinfo[i] = &porttype_info::lookup(ptype);
		newoffsets[i] = offset;
		if(newpinfo[i])
			offset += newpinfo[i]->storage_size;
		if(i != port && newpinfo[i] && newpinfo[i]->storage_size)
			memcpy(tmp + newoffsets[i], backing + offsets[i], newpinfo[i]->storage_size);
	}
	memcpy(memory, tmp, MAXIMUM_CONTROLLER_FRAME_SIZE);
	types[port] = ptype;
	pinfo[port] = newpinfo[port];
	for(size_t i = 0; i < MAX_PORTS; i++)
		offsets[i] = newoffsets[i];
	totalsize = offset;
}

controller_state::controller_state() throw()
{
	for(size_t i = 0; i < MAX_PORTS; i++) {
		porttypes[i] = PT_INVALID;
		porttypeinfo[i] = NULL;
	}
}

unsigned controller_state::lcid_count() throw()
{
	//FIXME: Support more ports/controllers.
	return 8;
}

std::pair<int, int> controller_state::lcid_to_pcid(unsigned lcid) throw()
{
	if(!porttypeinfo[0] || !porttypeinfo[1])
		return std::make_pair(-1, -1);
	//FIXME: Support more ports/controllers.
	unsigned p1devs = porttypeinfo[0]->controllers;
	unsigned p2devs = porttypeinfo[1]->controllers;
	if(lcid >= p1devs + p2devs)
		return std::make_pair(-1, -1);
	//Exceptional: If p1 is none, map all to p2.
	if(!p1devs)
		return std::make_pair(1, lcid);
	//LID 0 Is always PID 0 unless out of range.
	if(lcid == 0)
		return std::make_pair(0, 0);
	//LID 1-n are on port 2.
	else if(lcid < 1 + p2devs)
		return std::make_pair(1, lcid- 1);
	//From there, those are on port 1 (except for the first).
	else
		return std::make_pair(0, lcid - p2devs);
}

devicetype_t controller_state::pcid_to_type(unsigned port, unsigned pcid) throw()
{
	if(port >= MAX_PORTS)
		return DT_NONE;
	return porttypeinfo[port]->devicetype(pcid);
}

controller_frame controller_state::get(uint64_t framenum) throw()
{
	if(_autofire.size())
		return _input ^ _autohold ^ _autofire[framenum % _autofire.size()];
	else
		return _input ^ _autohold;
}

void controller_state::analog(unsigned port, unsigned pcid, int x, int y) throw()
{
	_input.axis(port, pcid, 0, x);
	_input.axis(port, pcid, 1, y);
}

void controller_state::reset(int32_t delay) throw()
{
	if(delay >= 0) {
		_input.reset(true);
		_input.delay(std::make_pair(delay / 10000, delay % 10000));
	} else {
		_input.reset(false);
		_input.delay(std::make_pair(0, 0));
	}
}

void controller_state::autohold(unsigned port, unsigned pcid, unsigned pbid, bool newstate) throw()
{
	_autohold.axis(port, pcid, pbid, newstate ? 1 : 0);
	information_dispatch::do_autohold_update(port, pcid, pbid, newstate);
}

bool controller_state::autohold(unsigned port, unsigned pcid, unsigned pbid) throw()
{
	return (_autohold.axis(port, pcid, pbid) != 0);
}

void controller_state::button(unsigned port, unsigned pcid, unsigned pbid, bool newstate) throw()
{
	_input.axis(port, pcid, pbid, newstate ? 1 : 0);
}

bool controller_state::button(unsigned port, unsigned pcid, unsigned pbid) throw()
{
	return (_input.axis(port, pcid, pbid) != 0);
}

void controller_state::autofire(std::vector<controller_frame> pattern) throw(std::bad_alloc)
{
	_autofire = pattern;
}

int controller_state::button_id(unsigned port, unsigned pcid, unsigned lbid) throw()
{
	if(port >= MAX_PORTS)
		return -1;
	return porttypeinfo[port]->button_id(pcid, lbid);
}

void controller_state::set_port(unsigned port, porttype_t ptype, bool set_core) throw(std::runtime_error)
{
	if(port >= MAX_PORTS)
		throw std::runtime_error("Port number invalid");
	const porttype_info* info = &porttype_info::lookup(ptype);
	if(!info->legal(port))
		throw std::runtime_error("Port type not valid for port");
	if(set_core)
		info->set_core_controller(port);
	porttype_t oldtype = porttypes[port];
	if(oldtype != ptype) {
		_input.set_port_type(port, ptype);
		_autohold.set_port_type(port, ptype);
		_committed.set_port_type(port, ptype);
		//The old autofire pattern no longer applies.
		_autofire.clear();
	}
	porttypes[port] = ptype;
	porttypeinfo[port] = info;
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

bool controller_state::is_analog(unsigned port, unsigned pcid) throw()
{
	return _input.is_analog(port, pcid);
}

bool controller_state::is_mouse(unsigned port, unsigned pcid) throw()
{
	return _input.is_mouse(port, pcid);
}
