#include "binarystream.hpp"
#include "controller-data.hpp"
#include "threads.hpp"
#include "minmax.hpp"
#include "globalwrap.hpp"
#include "serialization.hpp"
#include "string.hpp"
#include "sha256.hpp"
#include <iostream>
#include <sys/time.h>
#include <sstream>
#include <list>
#include <deque>
#include <complex>

namespace
{
	port_controller simple_controller = {"(system)", "system", {}};
	port_controller_set simple_port = {"system", "system", "system", {simple_controller},{0}};

	struct porttype_basecontrol : public port_type
	{
		porttype_basecontrol() : port_type("basecontrol", "basecontrol", 1)
		{
			write = [](const port_type* _this, unsigned char* buffer, unsigned idx, unsigned ctrl,
				short x) -> void {
				if(idx > 0 || ctrl > 0) return;
				buffer[0] = x ? 1 : 0;
			};
			read = [](const port_type* _this, const unsigned char* buffer, unsigned idx, unsigned ctrl) ->
				short {
				if(idx > 0 || ctrl > 0) return 0;
				return buffer[0] ? 1 : 0;
			};
			serialize = [](const port_type* _this, const unsigned char* buffer, char* textbuf) -> size_t {
				textbuf[0] = buffer[0] ? 'F' : '-';
				textbuf[1] = '\0';
				return 1;
			};
			deserialize = [](const port_type* _this, unsigned char* buffer, const char* textbuf) ->
				size_t {
				size_t ptr = 0;
				buffer[0] = 0;
				if(read_button_value(textbuf, ptr))
					buffer[0] = 1;
				skip_rest_of_field(textbuf, ptr, false);
				return ptr;
			};
			controller_info = &simple_port;
		}
	};

	unsigned macro_random_bit()
	{
		static unsigned char state[32];
		static unsigned extracted = 256;
		if(extracted == 256) {
			timeval tv;
			gettimeofday(&tv, NULL);
			unsigned char buffer[48];
			memcpy(buffer, state, 32);
			serialization::u64b(buffer + 32, tv.tv_sec);
			serialization::u64b(buffer + 40, tv.tv_usec);
			sha256::hash(state, buffer, 48);
			extracted = 0;
		}
		unsigned bit = extracted++;
		return ((state[bit / 8] >> (bit % 8)) & 1);
	}
}

port_type& get_default_system_port_type()
{
	static porttype_basecontrol x;
	return x;
}

port_type::port_type(const std::string& iname, const std::string& _hname, size_t ssize) throw(std::bad_alloc)
	: hname(_hname), storage_size(ssize), name(iname)
{
}

port_type::~port_type() throw()
{
}

bool port_type::is_present(unsigned controller) const throw()
{
	return controller_info->controllers.size() > controller;
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
		for(unsigned j = 0; j < types[i]->controller_info->controllers.size(); j++)
			controller_multiplier = max(controller_multiplier, (size_t)types[i]->used_indices(j));
	//Count maximum number of controllers to determine the port multiplier.
	port_multiplier = 1;
	for(size_t i = 0; i < port_count; i++)
		port_multiplier = max(port_multiplier, controller_multiplier *
		(size_t)types[i]->controller_info->controllers.size());
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

size_t write_axis_value(char* buf, short _v)
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

namespace
{
	port_type_set& dummytypes()
	{
		static port_type_set x;
		return x;
	}

	size_t writeu32val(char32_t* buf, int val)
	{
		char c[12];
		size_t i;
		sprintf(c, "%d", val);
		for(i = 0; c[i]; i++)
			buf[i] = c[i];
		return i;
	}

	uint64_t find_next_sync(controller_frame_vector& movie, uint64_t after)
	{
		if(after >= movie.size())
			return after;
		do {
			after++;
		} while(after < movie.size() && !movie[after].sync());
		return after;
	}
}

void controller_frame::display(unsigned port, unsigned controller, char32_t* buf) throw()
{
	if(port >= types->ports()) {
		//Bad port.
		*buf = '\0';
		return;
	}
	uint8_t* backingmem = backing + types->port_offset(port);
	const port_type& ptype = types->port_type(port);
	if(controller >= ptype.controller_info->controllers.size()) {
		//Bad controller.
		*buf = '\0';
		return;
	}
	const port_controller& pc = ptype.controller_info->controllers[controller];
	bool need_space = false;
	short val;
	for(unsigned i = 0; i < pc.buttons.size(); i++) {
		const port_controller_button& pcb = pc.buttons[i];
		if(need_space && pcb.type != port_controller_button::TYPE_NULL) {
			need_space = false;
			*(buf++) = ' ';
		}
		switch(pcb.type) {
		case port_controller_button::TYPE_NULL:
			break;
		case port_controller_button::TYPE_BUTTON:
			*(buf++) = ptype.read(&ptype, backingmem, controller, i) ? pcb.symbol : U'-';
			break;
		case port_controller_button::TYPE_AXIS:
		case port_controller_button::TYPE_RAXIS:
		case port_controller_button::TYPE_TAXIS:
		case port_controller_button::TYPE_LIGHTGUN:
			val = ptype.read(&ptype, backingmem, controller, i);
			buf += writeu32val(buf, val);
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
	framepflag = p.framepflag;
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
	framepflag = p.framepflag;
	return *this;
}

pollcounter_vector::~pollcounter_vector() throw()
{
	delete[] ctrs;
}

void pollcounter_vector::clear() throw()
{
	memset(ctrs, 0, sizeof(uint32_t) * types->indices());
	framepflag = false;
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


void pollcounter_vector::set_framepflag(bool value) throw()
{
	framepflag = value;
}

bool pollcounter_vector::get_framepflag() const throw()
{
	return framepflag;
}

controller_frame::controller_frame(const port_type_set& p) throw(std::runtime_error)
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types = &p;
	host = NULL;
}

controller_frame::controller_frame(unsigned char* mem, const port_type_set& p, controller_frame_vector* _host)
	throw(std::runtime_error)
{
	if(!mem)
		throw std::runtime_error("NULL backing memory not allowed");
	memset(memory, 0, sizeof(memory));
	backing = mem;
	types = &p;
	host = _host;
}

controller_frame::controller_frame(const controller_frame& obj) throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types = obj.types;
	memcpy(backing, obj.backing, types->size());
	host = NULL;
}

controller_frame& controller_frame::operator=(const controller_frame& obj) throw(std::runtime_error)
{
	if(backing != memory && types != obj.types)
		throw std::runtime_error("Port types do not match");
	types = obj.types;
	short old = sync();
	memcpy(backing, obj.backing, types->size());
	if(host) host->notify_sync_change(sync() - old);
	return *this;
}

controller_frame_vector::fchange_listener::~fchange_listener()
{
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
			index = 0;
			offset = 0;
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

size_t controller_frame_vector::recount_frames() throw()
{
	uint64_t old_frame_count = real_frame_count;
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
	real_frame_count = ret;
	call_framecount_notification(old_frame_count);
	return ret;
}

void controller_frame_vector::clear(const port_type_set& p) throw(std::runtime_error)
{
	uint64_t old_frame_count = real_frame_count;
	frame_size = p.size();
	frames_per_page = CONTROLLER_PAGE_SIZE / frame_size;
	frames = 0;
	types = &p;
	clear_cache();
	pages.clear();
	real_frame_count = 0;
	call_framecount_notification(old_frame_count);
}

controller_frame_vector::~controller_frame_vector() throw()
{
	pages.clear();
	cache_page = NULL;
}

controller_frame_vector::controller_frame_vector() throw()
{
	real_frame_count = 0;
	freeze_count = 0;
	clear(dummytypes());
}

controller_frame_vector::controller_frame_vector(const port_type_set& p) throw()
{
	real_frame_count = 0;
	freeze_count = 0;
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
	if(frame.sync()) real_frame_count++;
	frames++;
}

controller_frame_vector::controller_frame_vector(const controller_frame_vector& vector) throw(std::bad_alloc)
{
	real_frame_count = 0;
	freeze_count = 0;
	clear(*vector.types);
	*this = vector;
}

controller_frame_vector& controller_frame_vector::operator=(const controller_frame_vector& v)
	throw(std::bad_alloc)
{
	if(this == &v)
		return *this;
	uint64_t old_frame_count = real_frame_count;
	resize(v.frames);
	clear_cache();

	//Copy the fields.
	frame_size = v.frame_size;
	frames_per_page = v.frames_per_page;
	types = v.types;
	real_frame_count = v.real_frame_count;

	//This can't fail anymore. Copy the raw page contents.
	size_t pagecount = (frames + frames_per_page - 1) / frames_per_page;
	for(size_t i = 0; i < pagecount; i++) {
		page& pg = pages[i];
		const page& pg2 = v.pages.find(i)->second;
		pg = pg2;
	}
	call_framecount_notification(old_frame_count);
	return *this;
}

void controller_frame_vector::resize(size_t newsize) throw(std::bad_alloc)
{
	clear_cache();
	if(newsize == 0) {
		clear();
	} else if(newsize < frames) {
		//Shrink movie.
		uint64_t old_frame_count = real_frame_count;
		for(size_t i = newsize; i < frames; i++)
			if((*this)[i].sync()) real_frame_count--;
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
		call_framecount_notification(old_frame_count);
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
		//This can use real_frame_count, because the real frame count won't change.
		call_framecount_notification(real_frame_count);
	}
}

bool controller_frame_vector::compatible(controller_frame_vector& with, uint64_t frame, const uint32_t* polls)
{
	//Types have to match.
	if(get_types() != with.get_types())
		return false;
	const port_type_set& pset = with.get_types();
	//If new movie is before first frame, anything with same project_id is compatible.
	if(frame == 0)
		return true;
	//Scan both movies until frame syncs are seen. Out of bounds reads behave as all neutral but frame
	//sync done.
	uint64_t syncs_seen = 0;
	uint64_t frames_read = 0;
	size_t old_size = size();
	size_t new_size = with.size();
	size_t pagenum = 0;
	size_t ocomplete_pages = old_size / frames_per_page;  //Round DOWN
	size_t ncomplete_pages = new_size / frames_per_page;  //Round DOWN
	size_t complete_pages = min(ocomplete_pages, ncomplete_pages);
	while(syncs_seen + frames_per_page < frame - 1 && pagenum < complete_pages) {
		//Fast process page. The above condition guarantees that these pages are completely used.
		auto opagedata = pages[pagenum].content;
		auto npagedata = with.pages[pagenum].content;
		size_t pagedataamt = frames_per_page * frame_size;
		if(memcmp(opagedata, npagedata, pagedataamt))
			return false;
		frames_read += frames_per_page;
		pagenum++;
		for(size_t i = 0; i < pagedataamt; i += frame_size)
			if(opagedata[i] & 1) syncs_seen++;
	}
	while(syncs_seen < frame - 1) {
		controller_frame oldc = blank_frame(true), newc = with.blank_frame(true);
		if(frames_read < old_size)
			oldc = (*this)[frames_read];
		if(frames_read < new_size)
			newc = with[frames_read];
		if(oldc != newc)
			return false;	//Mismatch.
		frames_read++;
		if(newc.sync())
			syncs_seen++;
	}
	//We increment the counter one time too many.
	frames_read--;
	//Current frame. We need to compare each control up to poll counter.
	uint64_t readable_old_subframes = 0, readable_new_subframes = 0;
	uint64_t oldlen = find_next_sync(*this, frames_read);
	uint64_t newlen = find_next_sync(with, frames_read);
	if(frames_read < oldlen)
		readable_old_subframes = oldlen - frames_read;
	if(frames_read < newlen)
		readable_new_subframes = newlen - frames_read;
	//Then rest of the stuff.
	for(unsigned i = 0; i < pset.indices(); i++) {
		uint32_t p = polls[i] & 0x7FFFFFFFUL;
		short ov = 0, nv = 0;
		for(uint32_t j = 0; j < p; j++) {
			if(j < readable_old_subframes)
				ov = (*this)[j + frames_read].axis2(i);
			if(j < readable_new_subframes)
				nv = with[j + frames_read].axis2(i);
			if(ov != nv)
				return false;
		}
	}
	return true;
}

uint64_t controller_frame_vector::binary_size() const throw()
{
	return size() * get_stride();
}

void controller_frame_vector::save_binary(binarystream::output& stream) const throw(std::runtime_error)
{
	uint64_t stride = get_stride();
	uint64_t pageframes = get_frames_per_page();
	uint64_t vsize = size();
	size_t pagenum = 0;
	while(vsize > 0) {
		uint64_t count = (vsize > pageframes) ? pageframes : vsize;
		size_t bytes = count * stride;
		const unsigned char* content = get_page_buffer(pagenum++);
		stream.raw(content, bytes);
		vsize -= count;
	}
}

void controller_frame_vector::load_binary(binarystream::input& stream) throw(std::bad_alloc, std::runtime_error)
{
	uint64_t stride = get_stride();
	uint64_t pageframes = get_frames_per_page();
	uint64_t vsize = 0;
	size_t pagenum = 0;
	uint64_t pagesize = stride * pageframes;
	while(stream.get_left()) {
		resize(vsize + pageframes);
		unsigned char* contents = get_page_buffer(pagenum++);
		uint64_t gcount = min(pagesize, stream.get_left());
		stream.raw(contents, gcount);
		vsize += (gcount / stride);
	}
	resize(vsize);
	recount_frames();
}

void controller_frame_vector::swap_data(controller_frame_vector& v) throw()
{
	uint64_t toldsize = real_frame_count;
	uint64_t voldsize = v.real_frame_count;
	std::swap(pages, v.pages);
	std::swap(frames_per_page, v.frames_per_page);
	std::swap(frame_size, v.frame_size);
	std::swap(frames, v.frames);
	std::swap(types, v.types);
	std::swap(cache_page_num, v.cache_page_num);
	std::swap(cache_page, v.cache_page);
	std::swap(real_frame_count, v.real_frame_count);
	if(!freeze_count)
		call_framecount_notification(toldsize);
	if(!v.freeze_count)
		v.call_framecount_notification(voldsize);
}

int64_t controller_frame_vector::find_frame(uint64_t n)
{
	if(!n) return -1;
	uint64_t stride = get_stride();
	uint64_t pageframes = get_frames_per_page();
	uint64_t vsize = size();
	size_t pagenum = 0;
	while(vsize > 0) {
		uint64_t count = (vsize > pageframes) ? pageframes : vsize;
		const unsigned char* content = get_page_buffer(pagenum++);
		size_t offset = 0;
		for(unsigned i = 0; i < count; i++) {
			if(controller_frame::sync(content + offset)) n--;
			if(n == 0) return (pagenum - 1) * pageframes + i;
			offset += stride;
		}
		vsize -= count;
	}
	return -1;
}

controller_frame::controller_frame() throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types = &dummytypes();
	host = NULL;
}

unsigned port_controller::analog_actions() const
{
	unsigned r = 0, s = 0;
	for(unsigned i = 0; i < buttons.size(); i++) {
		if(buttons[i].shadow)
			continue;
		switch(buttons[i].type) {
		case port_controller_button::TYPE_AXIS:
		case port_controller_button::TYPE_RAXIS:
		case port_controller_button::TYPE_LIGHTGUN:
			r++;
			break;
		case port_controller_button::TYPE_TAXIS:
			s++;
			break;
		case port_controller_button::TYPE_NULL:
		case port_controller_button::TYPE_BUTTON:
			;
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
	for(unsigned i = 0; i < buttons.size(); i++) {
		if(buttons[i].shadow)
			continue;
		switch(buttons[i].type) {
		case port_controller_button::TYPE_AXIS:
		case port_controller_button::TYPE_RAXIS:
		case port_controller_button::TYPE_LIGHTGUN:
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
		case port_controller_button::TYPE_NULL:
		case port_controller_button::TYPE_BUTTON:
			;
		};
	}
out:
	return std::make_pair(x1, x2);
}

namespace
{
	std::string macro_field_as_string(const JSON::node& parent, const std::string& path)
	{
		const JSON::node& n = parent.follow(path);
		if(n.type() != JSON::string)
			(stringfmt() << "Expected string as field '" << path << "'").throwex();
		return n.as_string8();
	}

	bool macro_field_as_boolean(const JSON::node& parent, const std::string& path)
	{
		const JSON::node& n = parent.follow(path);
		if(n.type() != JSON::boolean)
			(stringfmt() << "Expected boolean as field '" << path << "'").throwex();
		return n.as_bool();
	}

	const JSON::node& macro_field_as_array(const JSON::node& parent, const std::string& path)
	{
		const JSON::node& n = parent.follow(path);
		if(n.type() != JSON::array)
			(stringfmt() << "Expected array as field '" << path << "'").throwex();
		return n;
	}
}

controller_macro_data::controller_macro_data(const std::string& spec, const JSON::node& desc, unsigned inum)
{
	_descriptor = desc;
	unsigned btnnum = 0;
	std::map<std::string, unsigned> symbols;
	if(desc.type() != JSON::array)
		(stringfmt() << "Expected controller descriptor " << (inum + 1) << " to be an array");
	for(auto i = desc.begin(); i != desc.end(); ++i) {
		if(i->type() == JSON::string) {
			symbols[i->as_string8()] = btnnum++;
			btnmap.push_back(i.index());
		} else if(i->type() == JSON::number) {
			uint64_t anum = i->as_uint();
			if(anum > aaxes.size())
				(stringfmt() << "Descriptor axis number " << anum << " out of range in descriptor "
					<< (inum + 1)).throwex();
			else if(anum == aaxes.size())
				aaxes.push_back(std::make_pair(i.index(), std::numeric_limits<unsigned>::max()));
			else
				aaxes[anum].second = i.index();
		} else
			(stringfmt() << "Controller descriptor " << (inum + 1) << " contains element of unknown"
				<< "kind").throwex();
	}
	buttons = symbols.size();
	orig = spec;
	enabled = true;
	autoterminate = false;

	std::deque<size_t> stack;
	bool in_sparen = false;
	bool first = true;
	bool btn_token = false;
	bool btn_token_next = false;
	size_t last_bit = 0;
	size_t last_size = 0;
	size_t astride = aaxes.size();
	size_t stride = get_stride();
	size_t idx = 0;
	size_t len = spec.length();
	try {
		while(idx < len) {
			btn_token = btn_token_next;
			btn_token_next = false;
			unsigned char ch = spec[idx];
			if(autoterminate)
				throw std::runtime_error("Asterisk must be the last thing");
			if(ch == '(') {
				if(in_sparen)
					throw std::runtime_error("Parentheses in square brackets not allowed");
				stack.push_back(data.size());
			} else if(ch == ')') {
				if(in_sparen)
					throw std::runtime_error("Parentheses in square brackets not allowed");
				if(stack.empty())
					throw std::runtime_error("Unmatched right parenthesis");
				size_t x = stack.back();
				stack.pop_back();
				last_size = (data.size() - x) / stride;
			} else if(ch == '*') {
				autoterminate = true;
			} else if(ch == '?') {
				if(!btn_token)
					throw std::runtime_error("? needs button to apply to");
				if(!in_sparen)
					throw std::runtime_error("? needs to be in brackets");
				data[data.size() - stride + last_bit] |= 2;
			} else if(ch == '[') {
				if(in_sparen)
					throw std::runtime_error("Nested square brackets not allowed");
				in_sparen = true;
				data.resize(data.size() + stride);
				adata.resize(adata.size() + astride);
				last_size = 1;
			} else if(ch == ']') {
				if(!in_sparen)
					throw std::runtime_error("Unmatched right square bracket");
				in_sparen = false;
			} else if(ch == '.') {
				if(!in_sparen) {
					data.resize(data.size() + stride);
					adata.resize(adata.size() + astride);
					last_size = 1;
				}
			} else if(spec[idx] >= '0' && spec[idx] <= '9') {
				size_t rep = 0;
				unsigned i = 0;
				while(spec[idx + i] >= '0' && spec[idx + i] <= '9') {
					rep = 10 * rep + (spec[idx + i] - '0');
					i++;
				}
				if(in_sparen) {
					//This has special meaning: Axis transform.
					//Rep is the axis pair to operate on.
					if(spec[idx + i] != ':')
						throw std::runtime_error("Expected ':' in axis transform");
					size_t sep = i;
					while(idx + i < len && spec[idx + i] != '@')
						i++;
					if(idx + i >= len)
						throw std::runtime_error("Expected '@' in axis transform");
					std::string aexpr = spec.substr(idx + sep + 1, i - sep - 1);
					if(rep >= astride)
						throw std::runtime_error("Axis transform refers to invalid axis");
					adata[adata.size() - astride + rep] = axis_transform(aexpr);
					i++;
				} else {
					if(first)
						throw std::runtime_error("Repeat not allowed without frame to "
							"repeat");
					size_t o = data.size();
					size_t ao = adata.size();
					data.resize(o + (rep - 1) * last_size * stride);
					adata.resize(ao + (rep - 1) * last_size * astride);
					for(unsigned i = 1; i < rep; i++) {
						memcpy(&data[o + (i - 1) * last_size * stride], &data[o - last_size *
							stride], last_size * stride);
						memcpy(&data[ao + (i - 1) * last_size * astride], &data[ao -
							last_size * astride], last_size * astride);
					}
					last_size = last_size * rep;
				}
				idx = idx + (i - 1);
			} else {	//Symbol.
				bool found = false;
				for(auto k : symbols) {
					std::string key = k.first;
					size_t j;
					for(j = 0; idx + j < len && j < key.length(); j++)
						if(spec[idx + j] != key[j])
							break;
					if(j == key.length()) {
						idx += key.length() - 1;
						found = true;
						if(!in_sparen) {
							data.resize(data.size() + stride);
							adata.resize(adata.size() + astride);
						}
						last_bit = k.second;
						data[data.size() - stride + k.second] |= 1;
						if(!in_sparen)
							last_size = 1;
					}
				}
				if(!found)
					throw std::runtime_error("Unknown character or button");
				btn_token_next = true;
			}
			idx++;
			first = false;
		}
		if(in_sparen)
			throw std::runtime_error("Unmatched left square bracket");
		if(!stack.empty())
			throw std::runtime_error("Unmatched left parenthesis");
	} catch(std::exception& e) {
		(stringfmt() << "Error parsing macro for controller " << (inum + 1) << ": " << e.what()).throwex();
	}
}

bool controller_macro_data::syntax_check(const std::string& spec, const JSON::node& desc)
{
	unsigned buttons = 0;
	size_t astride = 0;
	if(desc.type() != JSON::array)
		return false;
	for(auto i = desc.begin(); i != desc.end(); ++i) {
		if(i->type() == JSON::string)
			buttons++;
		else if(i->type() == JSON::number) {
			uint64_t anum = i->as_uint();
			if(anum > astride)
				return false;
			else if(anum == astride)
				astride++;
		} else
			return false;
	}
	bool autoterminate = false;
	size_t depth = 0;
	bool in_sparen = false;
	bool first = true;
	bool btn_token = false;
	bool btn_token_next = false;
	size_t idx = 0;
	size_t len = spec.length();
	while(idx < len) {
		btn_token = btn_token_next;
		btn_token_next = false;
		unsigned char ch = spec[idx];
		if(autoterminate)
			return false;
		if(ch == '(') {
			if(in_sparen)
				return false;
			depth++;
		} else if(ch == ')') {
			if(in_sparen)
				return false;
			if(!depth)
				return false;
			depth--;
		} else if(ch == '*') {
			autoterminate = true;
		} else if(ch == '?') {
			if(!btn_token || !in_sparen)
				return false;
		} else if(ch == '[') {
			if(in_sparen)
				return false;
			in_sparen = true;
		} else if(ch == ']') {
			if(!in_sparen)
				return false;
			in_sparen = false;
		} else if(ch == '.') {
		} else if(spec[idx] >= '0' && spec[idx] <= '9') {
			size_t rep = 0;
			unsigned i = 0;
			while(spec[idx + i] >= '0' && spec[idx + i] <= '9') {
				rep = 10 * rep + (spec[idx + i] - '0');
				i++;
			}
			if(in_sparen) {
				//This has special meaning: Axis transform.
				//Rep is the axis pair to operate on.
				if(spec[idx + i] != ':')
					return false;
				size_t sep = i;
				while(idx + i < len && spec[idx + i] != '@')
					i++;
				if(idx + i >= len)
					return false;
				if(rep >= astride)
					return false;
				try {
					std::string aexpr = spec.substr(idx + sep + 1, i - sep - 1);
					axis_transform x(aexpr);
				} catch(...) {
					return false;
				}
				i++;
			} else {
				if(first)
					return false;
			}
			idx = idx + (i - 1);
		} else {	//Symbol.
			bool found = false;
			for(auto i = desc.begin(); i != desc.end(); ++i) {
				if(i->type() != JSON::string)
					continue;
				std::string key = i->as_string8();
				size_t j;
				for(j = 0; idx + j < len && j < key.length(); j++)
					if(spec[idx + j] != key[j])
						break;
				if(j == key.length()) {
					idx += key.length() - 1;
					found = true;
				}
			}
			if(!found)
				return false;
			btn_token_next = true;
		}
		idx++;
		first = false;
	}
	if(in_sparen)
		return false;
	if(depth)
		return false;
	return true;
}

void controller_macro_data::write(controller_frame& frame, unsigned port, unsigned controller, int64_t nframe,
	apply_mode amode)
{
	if(!enabled)
		return;
	if(autoterminate && (nframe < 0 || nframe >= (int64_t)get_frames()))
		return;
	if(nframe < 0)
		nframe += ((-nframe / get_frames()) + 3) * get_frames();
	nframe %= get_frames();
	for(size_t i = 0; i < buttons; i++) {
		unsigned lb = btnmap[i];
		unsigned st = data[nframe * get_stride() + i];
		if(st == 3)
			st = macro_random_bit();
		if(st == 1)
			switch(amode) {
			case AM_OVERWRITE:
			case AM_OR:
				frame.axis3(port, controller, lb, 1);
				break;
			case AM_XOR:
				frame.axis3(port, controller, lb, frame.axis3(port, controller, lb) ^ 1);
				break;
			}
		else
			switch(amode) {
			case AM_OVERWRITE:
				frame.axis3(port, controller, lb, 0);
				break;
			case AM_OR:
			case AM_XOR:
				;
			}
	}
	const port_controller* _ctrl = frame.porttypes().port_type(port).controller_info->get(controller);
	if(!_ctrl)
		return;
	size_t abuttons = aaxes.size();
	for(size_t i = 0; i < abuttons; i++) {
		unsigned ax = aaxes[i].first;
		unsigned ay = aaxes[i].second;
		if(ay != std::numeric_limits<unsigned>::max()) {
			if(ax > _ctrl->buttons.size()) continue;
			if(ay > _ctrl->buttons.size()) continue;
			auto g = adata[nframe * abuttons + i].transform(_ctrl->buttons[ax], _ctrl->buttons[ay],
				frame.axis3(port, controller, ax), frame.axis3(port, controller, ay));
			frame.axis3(port, controller, ax, g.first);
			frame.axis3(port, controller, ay, g.second);
		} else {
			if(ax > _ctrl->buttons.size()) continue;
			int16_t g = adata[nframe * abuttons + i].transform(_ctrl->buttons[ax],
				frame.axis3(port, controller, ax));
			frame.axis3(port, controller, ax, g);
		}
	}
}

std::string controller_macro_data::dump(const port_controller& ctrl)
{
	std::ostringstream o;
	for(size_t i = 0; i < get_frames(); i++) {
		o << "[";
		for(size_t j = 0; j < aaxes.size(); j++) {
			controller_macro_data::axis_transform& t = adata[i * aaxes.size() + j];
			o << j << ":";
			o << t.coeffs[0] << "," << t.coeffs[1] << "," << t.coeffs[2] << ",";
			o << t.coeffs[3] << "," << t.coeffs[4] << "," << t.coeffs[5] << "@";
		}
		for(size_t j = 0; j < buttons && j < ctrl.buttons.size(); j++) {
			unsigned st = data[i * get_stride() + j];
			if(ctrl.buttons[j].macro == "")
				continue;
			if(st == 1)
				o << ctrl.buttons[j].macro;
			if(st == 3)
				o << ctrl.buttons[j].macro << "?";
		}
		o << "]";
	}
	if(autoterminate)
		o << "*";
	return o.str();
}

void controller_macro::write(controller_frame& frame, int64_t nframe)
{
	for(auto& i : macros) {
		unsigned port;
		unsigned controller;
		try {
			auto g = frame.porttypes().lcid_to_pcid(i.first);
			port = g.first;
			controller = g.second;
		} catch(...) {
			continue;
		}
		i.second.write(frame, port, controller, nframe, amode);
	}
}

int16_t controller_macro_data::axis_transform::transform(const port_controller_button& b, int16_t v)
{
	return scale_axis(b, coeffs[0] * unscale_axis(b, v) + coeffs[4]);
}

std::pair<int16_t, int16_t> controller_macro_data::axis_transform::transform(const port_controller_button& b1,
	const port_controller_button& b2, int16_t v1, int16_t v2)
{
	double x, y, u, v, au, av, s;
	x = unscale_axis(b1, v1);
	y = unscale_axis(b2, v2);
	u = coeffs[0] * x + coeffs[1] * y + coeffs[4];
	v = coeffs[2] * x + coeffs[3] * y + coeffs[5];
	au = abs(u);
	av = abs(v);
	s = max(max(au, 1.0), max(av, 1.0));
	//If u and v exceed nominal range of [-1,1], those need to be projected to the edge.
	if(s > 1) {
		u /= s;
		v /= s;
	}
	auto g = std::make_pair(scale_axis(b1, u), scale_axis(b2, v));
	return g;
}

double controller_macro_data::axis_transform::unscale_axis(const port_controller_button& b, int16_t v)
{
	if(b.centers) {
		int32_t center = ((int32_t)b.rmin + (int32_t)b.rmax) / 2;
		if(v <= b.rmin)
			return -1;
		if(v < center)
			return -(center - (double)v) / (center - b.rmin);
		if(v == center)
			return 0;
		if(v < b.rmax)
			return ((double)v - center) / (b.rmax - center);
		return 1;
	} else {
		if(v <= b.rmin)
			return 0;
		if(v >= b.rmax)
			return 1;
		return ((double)v - b.rmin) / (b.rmax - b.rmin);
	}
}

int16_t controller_macro_data::axis_transform::scale_axis(const port_controller_button& b, double v)
{
	if(b.centers) {
		int32_t center = ((int32_t)b.rmin + (int32_t)b.rmax) / 2;
		if(v == 0)
			return center;
		if(v < 0) {
			double v2 = v * (center - b.rmin) + center;
			if(v2 < b.rmin)
				return b.rmin;
			return v2;
		}
		double v2 = v * (b.rmax - center) + center;
		if(v2 > b.rmax)
			return b.rmax;
		return v2;
	} else {
		double v2 = v * (b.rmax - b.rmin) + b.rmin;
		if(v2 < b.rmin)
			return b.rmin;
		if(v2 > b.rmax)
			return b.rmax;
		return v2;
	}
}

namespace
{
	std::complex<double> parse_complex(const std::string& expr)
	{
		regex_results r;
		if(r = regex("\\((.*),(.*)\\)", expr)) {
			//Real,Imaginary.
			return std::complex<double>(parse_value<double>(r[1]), parse_value<double>(r[2]));
		} else if(r = regex("\\((.*)<(.*)\\)", expr)) {
			return std::polar(parse_value<double>(r[1]), parse_value<double>(r[2]) * M_PI / 180);
		} else {
			return std::complex<double>(parse_value<double>(expr), 0.0);
		}
	}
}

controller_macro_data::axis_transform::axis_transform(const std::string& expr)
{
	regex_results r;
	if(r = regex("\\*(.*)\\+(.*)", expr)) {
		//Affine transform.
		std::complex<double> a = parse_complex(r[1]);
		std::complex<double> b = parse_complex(r[2]);
		coeffs[0] = a.real();
		coeffs[1] = -a.imag();
		coeffs[2] = a.imag();
		coeffs[3] = a.real();
		coeffs[4] = b.real();
		coeffs[5] = b.imag();
	} else if(r = regex("\\*(.*)", expr)) {
		//Linear transform.
		std::complex<double> a = parse_complex(r[1]);
		coeffs[0] = a.real();
		coeffs[1] = -a.imag();
		coeffs[2] = a.imag();
		coeffs[3] = a.real();
		coeffs[4] = 0;
		coeffs[5] = 0;
	} else if(r = regex("\\+(.*)", expr)) {
		//Relative
		std::complex<double> b = parse_complex(r[1]);
		coeffs[0] = 1;
		coeffs[1] = 0;
		coeffs[2] = 0;
		coeffs[3] = 1;
		coeffs[4] = b.real();
		coeffs[5] = b.imag();
	} else if(r = regex("(.*),(.*),(.*),(.*),(.*),(.*)", expr)) {
		//Full affine.
		coeffs[0] = parse_value<double>(r[1]);
		coeffs[1] = parse_value<double>(r[2]);
		coeffs[2] = parse_value<double>(r[3]);
		coeffs[3] = parse_value<double>(r[4]);
		coeffs[4] = parse_value<double>(r[5]);
		coeffs[5] = parse_value<double>(r[6]);
	} else {
		//Absolute.
		std::complex<double> b = parse_complex(expr);
		coeffs[0] = 0;
		coeffs[1] = 0;
		coeffs[2] = 0;
		coeffs[3] = 0;
		coeffs[4] = b.real();
		coeffs[5] = b.imag();
	}
}

JSON::node controller_macro::serialize()
{
	JSON::node v(JSON::object);
	switch(amode) {
	case controller_macro_data::AM_OVERWRITE:	v.insert("mode", JSON::s("overwrite")); break;
	case controller_macro_data::AM_OR:		v.insert("mode", JSON::s("or")); break;
	case controller_macro_data::AM_XOR:		v.insert("mode", JSON::s("xor")); break;
	};
	JSON::node& c = v.insert("data", JSON::array());
	for(auto& i : macros) {
		while(i.first > c.index_count())
			c.append(JSON::n());
		i.second.serialize(c.append(JSON::n()));
	}
	return v;
}

void controller_macro_data::serialize(JSON::node& v)
{
	v = JSON::object();
	v.insert("enable", JSON::b(enabled));
	v.insert("expr", JSON::s(orig));
	v.insert("desc", _descriptor);
}

JSON::node controller_macro_data::make_descriptor(const port_controller& ctrl)
{
	JSON::node n(JSON::array);
	for(size_t i = 0; i < ctrl.buttons.size(); i++) {
		if(ctrl.buttons[i].macro != "")
			n.append(JSON::s(ctrl.buttons[i].macro));
		else
			n.append(JSON::n()); //Placeholder.
	}
	for(size_t i = 0; i < ctrl.analog_actions(); i++) {
		auto g = ctrl.analog_action(i);
		n.index(g.first) = JSON::u(i);
		if(g.second != std::numeric_limits<unsigned>::max())
			n.index(g.second) = JSON::u(i);
	}
	return n;
}

controller_macro::controller_macro(const JSON::node& v)
{
	if(v.type() != JSON::object)
		throw std::runtime_error("Expected macro to be JSON object");
	std::string mode = macro_field_as_string(v, "mode");
	if(mode == "overwrite") amode = controller_macro_data::AM_OVERWRITE;
	else if(mode == "or") amode = controller_macro_data::AM_OR;
	else if(mode == "xor") amode = controller_macro_data::AM_XOR;
	else (stringfmt() << "Unknown button mode '" << mode << "'").throwex();
	const JSON::node& c = macro_field_as_array(v, "data");
	for(auto i = c.begin(); i != c.end(); ++i) {
		if(i->type() == JSON::object)
			macros[i.index()] = controller_macro_data(*i, i.index());
		else
			(stringfmt() << "Expected object as field 'data/" << i.index() << "'").throwex();
	}
}

controller_macro_data::controller_macro_data(const JSON::node& v, unsigned i)
	: controller_macro_data(macro_field_as_string(v, "expr"), macro_field_as_array(v, "desc"), i)
{
	enabled = macro_field_as_boolean(v, "enable");
}
