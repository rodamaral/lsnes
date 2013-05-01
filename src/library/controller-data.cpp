#include "controller-data.hpp"
#include "threadtypes.hpp"
#include "minmax.hpp"
#include "globalwrap.hpp"
#include <iostream>
#include <list>

namespace
{
	const char* basecontrol_chars = "F";

	port_controller_button* null_buttons[] = {};
	port_controller simple_controller = {"(system)", "system", 0, null_buttons};
	port_controller* simple_controllers[] = {&simple_controller};
	port_controller_set simple_port = {1, simple_controllers};

	struct porttype_basecontrol : public port_type
	{
		porttype_basecontrol() : port_type("basecontrol", "basecontrol", 999998, 1)
		{
			write = [](unsigned char* buffer, unsigned idx, unsigned ctrl, short x) -> void {
				if(idx > 0 || ctrl > 0) return;
				buffer[0] = x ? 1 : 0;
			};
			read = [](const unsigned char* buffer, unsigned idx, unsigned ctrl) -> short {
				if(idx > 0 || ctrl > 0) return 0;
				return buffer[0] ? 1 : 0;
			};
			serialize = [](const unsigned char* buffer, char* textbuf) -> size_t {
				textbuf[0] = buffer[0] ? 'F' : '-';
				textbuf[1] = '\0';
				return 1;
			};
			deserialize = [](unsigned char* buffer, const char* textbuf) -> size_t {
				size_t ptr = 0;
				buffer[0] = 0;
				if(read_button_value(textbuf, ptr))
					buffer[0] = 1;
				skip_rest_of_field(textbuf, ptr, false);
				return ptr;
			};
			legal = [](unsigned port) -> int { return (port == 0); };
			controller_info = &simple_port;
			used_indices = [](unsigned controller) -> unsigned { return (controller == 0) ? 1 : 0; };
		}
	};
}

port_type& get_default_system_port_type()
{
	static porttype_basecontrol x;
	return x;
}

port_type::port_type(const std::string& iname, const std::string& _hname,  unsigned id,
	size_t ssize) throw(std::bad_alloc)
	: hname(_hname), storage_size(ssize), name(iname), pt_id(id)
{
}

port_type::~port_type() throw()
{
}

bool port_type::is_present(unsigned controller) const throw()
{
	return controller_info->controller_count > controller;
}

namespace
{
	size_t dummy_offset = 0;
	port_type* dummy_type = &get_default_system_port_type();
	unsigned dummy_index = 0;
	struct binding
	{
		std::vector<port_type*> types;
		port_type_set* stype;
		bool matches(const std::vector<class port_type*>& x)
		{
			if(x.size() != types.size())
				return false;
			for(size_t i = 0; i < x.size(); i++)
				if(x[i] != types[i])
					return false;
			return true;
		}
	};
	std::list<binding>& bindings()
	{
		static std::list<binding> x;
		return x;
	}
}

port_type_set::port_type_set() throw()
{
	
	port_offsets = &dummy_offset;
	port_types = &dummy_type;
	port_count = 1;
	total_size = 1;
	_indices.resize(1);
	_indices[0].valid = true;
	_indices[0].port = 0;
	_indices[0].controller = 0;
	_indices[0].control = 0;
	
	port_multiplier = 1;
	controller_multiplier = 1;
	indices_size = 1;
	indices_tab = &dummy_index;
}

port_type_set& port_type_set::make(std::vector<class port_type*> types, struct port_index_map control_map)
	throw(std::bad_alloc, std::runtime_error)
{
	for(auto i : bindings())
		if(i.matches(types))
			return *(i.stype);
	//Not found, create new.
	port_type_set& ret = *new port_type_set(types, control_map);
	binding b;
	b.types = types;
	b.stype = &ret;
	bindings().push_back(b);
	return ret;
}

port_type_set::port_type_set(std::vector<class port_type*> types, struct port_index_map control_map)
{
	port_count = types.size();
	//Verify legality of port types.
	for(size_t i = 0; i < port_count; i++)
		if(!types[i] || !types[i]->legal(i))
			throw std::runtime_error("Illegal port types");
	//Count maximum number of controller indices to determine the controller multiplier.
	controller_multiplier = 1;
	for(size_t i = 0; i < port_count; i++)
		for(unsigned j = 0; j < types[i]->controller_info->controller_count; j++)
			controller_multiplier = max(controller_multiplier, (size_t)types[i]->used_indices(j));
	//Count maximum number of controllers to determine the port multiplier.
	port_multiplier = 1;
	for(size_t i = 0; i < port_count; i++)
		port_multiplier = max(port_multiplier, controller_multiplier *
		(size_t)types[i]->controller_info->controller_count);
	//Allocate the per-port tables.
	port_offsets = new size_t[types.size()];
	port_types = new class port_type*[types.size()];
	//Determine the total size and offsets.
	size_t offset = 0;
	for(size_t i = 0; i < port_count; i++) {
		port_offsets[i] = offset;
		offset += types[i]->storage_size;
		port_types[i] = types[i];
	}
	total_size = offset;
	//Determine the index size and allocate it.
	indices_size = port_multiplier * port_count;
	indices_tab = new unsigned[indices_size];
	for(size_t i = 0; i < indices_size; i++)
		indices_tab[i] = 0xFFFFFFFFUL;
	//Copy the index data (and reverse it).
	controllers = control_map.logical_map;
	legacy_pcids = control_map.pcid_map;
	_indices = control_map.indices;
	for(size_t j = 0; j < _indices.size(); j++) {
		auto& i = _indices[j];
		if(i.valid)
			indices_tab[i.port * port_multiplier + i.controller * controller_multiplier + i.control] = j;
	}
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

namespace
{
	port_type_set& dummytypes()
	{
		static port_type_set x;
		return x;
	}
}

void controller_frame::display(unsigned port, unsigned controller, char* buf) throw()
{
	if(port >= types->ports()) {
		//Bad port.
		*buf = '\0';
		return;
	}
	uint8_t* backingmem = backing + types->port_offset(port);
	const port_type& ptype = types->port_type(port);
	if(controller >= ptype.controller_info->controller_count) {
		//Bad controller.
		*buf = '\0';
		return;
	}
	const port_controller* pc = ptype.controller_info->controllers[controller];
	bool need_space = false;
	for(unsigned i = 0; i < pc->button_count; i++) {
		const port_controller_button* pcb = pc->buttons[i];
		if(need_space && pcb->type != port_controller_button::TYPE_NULL) {
			need_space = false;
			*(buf++) = ' ';
		}
		switch(pcb->type) {
		case port_controller_button::TYPE_NULL:
			break;
		case port_controller_button::TYPE_BUTTON:
			*(buf++) = ptype.read(backingmem, controller, i) ? pcb->symbol : '-';
			break;
		case port_controller_button::TYPE_AXIS:
		case port_controller_button::TYPE_RAXIS:
		case port_controller_button::TYPE_TAXIS:
			buf += sprintf(buf, "%d", ptype.read(backingmem, controller, i));
			need_space = true;
			break;
		}
	}
	*buf = '\0';
}

pollcounter_vector::pollcounter_vector() throw(std::bad_alloc)
{
	types = &dummytypes();
	ctrs = new uint32_t[types->indices()];
	clear();
}

pollcounter_vector::pollcounter_vector(const port_type_set& p) throw(std::bad_alloc)
{
	types = &p;
	ctrs = new uint32_t[types->indices()];
	clear();
}

pollcounter_vector::pollcounter_vector(const pollcounter_vector& p) throw(std::bad_alloc)
{
	ctrs = new uint32_t[p.types->indices()];
	types = p.types;
	memcpy(ctrs, p.ctrs, sizeof(uint32_t) * p.types->indices());
}

pollcounter_vector& pollcounter_vector::operator=(const pollcounter_vector& p) throw(std::bad_alloc)
{
	if(this == &p)
		return *this;
	uint32_t* n = new uint32_t[p.types->indices()];
	types = p.types;
	memcpy(n, p.ctrs, sizeof(uint32_t) * p.types->indices());
	delete[] ctrs;
	ctrs = n;
	return *this;
}

pollcounter_vector::~pollcounter_vector() throw()
{
	delete[] ctrs;
}

void pollcounter_vector::clear() throw()
{
	memset(ctrs, 0, sizeof(uint32_t) * types->indices());
}

void pollcounter_vector::set_all_DRDY() throw()
{
	for(size_t i = 0; i < types->indices(); i++)
		ctrs[i] |= 0x80000000UL;
}

void pollcounter_vector::clear_DRDY(unsigned idx) throw()
{
	ctrs[idx] &= 0x7FFFFFFFUL;
}

bool pollcounter_vector::get_DRDY(unsigned idx) throw()
{
	return ((ctrs[idx] & 0x80000000UL) != 0);
}

bool pollcounter_vector::has_polled() throw()
{
	uint32_t res = 0;
	for(size_t i = 0; i < types->indices() ; i++)
		res |= ctrs[i];
	return ((res & 0x7FFFFFFFUL) != 0);
}

uint32_t pollcounter_vector::get_polls(unsigned idx) throw()
{
	return ctrs[idx] & 0x7FFFFFFFUL;
}

uint32_t pollcounter_vector::increment_polls(unsigned idx) throw()
{
	uint32_t x = ctrs[idx] & 0x7FFFFFFFUL;
	++ctrs[idx];
	return x;
}

uint32_t pollcounter_vector::max_polls() throw()
{
	uint32_t max = 0;
	for(unsigned i = 0; i < types->indices(); i++) {
		uint32_t tmp = ctrs[i] & 0x7FFFFFFFUL;
		max = (max < tmp) ? tmp : max;
	}
	return max;
}

void pollcounter_vector::save_state(std::vector<uint32_t>& mem) throw(std::bad_alloc)
{
	mem.resize(types->indices());
	//Compatiblity fun.
	for(size_t i = 0; i < types->indices(); i++)
		mem[i] = ctrs[i];
}

void pollcounter_vector::load_state(const std::vector<uint32_t>& mem) throw()
{
	for(size_t i = 0; i < types->indices(); i++)
		ctrs[i] = mem[i];
}

bool pollcounter_vector::check(const std::vector<uint32_t>& mem) throw()
{
	return (mem.size() == types->indices());
}


controller_frame::controller_frame(const port_type_set& p) throw(std::runtime_error)
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types = &p;
}

controller_frame::controller_frame(unsigned char* mem, const port_type_set& p) throw(std::runtime_error)
{
	if(!mem)
		throw std::runtime_error("NULL backing memory not allowed");
	memset(memory, 0, sizeof(memory));
	backing = mem;
	types = &p;
}

controller_frame::controller_frame(const controller_frame& obj) throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types = obj.types;
	memcpy(backing, obj.backing, types->size());
}

controller_frame& controller_frame::operator=(const controller_frame& obj) throw(std::runtime_error)
{
	if(backing != memory && types != obj.types)
		throw std::runtime_error("Port types do not match");
	types = obj.types;
	memcpy(backing, obj.backing, types->size());
	return *this;
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

void controller_frame_vector::clear(const port_type_set& p) throw(std::runtime_error)
{
	frame_size = p.size();
	frames_per_page = CONTROLLER_PAGE_SIZE / frame_size;
	frames = 0;
	types = &p;
	clear_cache();
	pages.clear();
}

controller_frame_vector::~controller_frame_vector() throw()
{
	pages.clear();
	cache_page = NULL;
}

controller_frame_vector::controller_frame_vector() throw()
{
	clear(dummytypes());
}

controller_frame_vector::controller_frame_vector(const port_type_set& p) throw()
{
	clear(p);
}

void controller_frame_vector::append(controller_frame frame) throw(std::bad_alloc, std::runtime_error)
{
	controller_frame check(*types);
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
	controller_frame(cache_page->content + offset, *types) = frame;
	frames++;
}

controller_frame_vector::controller_frame_vector(const controller_frame_vector& vector) throw(std::bad_alloc)
{
	clear(*vector.types);
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
	types = v.types;

	//This can't fail anymore. Copy the raw page contents.
	size_t pagecount = (frames + frames_per_page - 1) / frames_per_page;
	for(size_t i = 0; i < pagecount; i++) {
		page& pg = pages[i];
		const page& pg2 = v.pages.find(i)->second;
		pg = pg2;
	}

	return *this;
}

/*
*/

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
	types = &dummytypes();
}

unsigned port_controller::analog_actions() const
{
	unsigned r = 0, s = 0;
	for(unsigned i = 0; i < button_count; i++) {
		if(buttons[i]->shadow)
			continue;
		switch(buttons[i]->type) {
		case port_controller_button::TYPE_AXIS:
		case port_controller_button::TYPE_RAXIS:
			r++;
			break;
		case port_controller_button::TYPE_TAXIS:
			s++;
			break;
		};
	}
	return (r + 1)/ 2 + s;
}

std::pair<unsigned, unsigned> port_controller::analog_action(unsigned k) const
{
	unsigned x1 = std::numeric_limits<unsigned>::max();
	unsigned x2 = std::numeric_limits<unsigned>::max();
	unsigned r = 0;
	bool second = false;
	bool selecting = false;
	for(unsigned i = 0; i < button_count; i++) {
		if(buttons[i]->shadow)
			continue;
		switch(buttons[i]->type) {
		case port_controller_button::TYPE_AXIS:
		case port_controller_button::TYPE_RAXIS:
			if(selecting) {
				x2 = i;
				goto out;
			}
			if(r == k && !second) {
				//This and following.
				x1 = i;
				selecting = true;
			}
			if(!second)
				r++;
			second = !second;
			break;
		case port_controller_button::TYPE_TAXIS:
			if(selecting)
				break;
			if(r == k) {
				x1 = i;
				goto out;
			}
			r++;
			break;
		};
	}
out:
	return std::make_pair(x1, x2);
}
