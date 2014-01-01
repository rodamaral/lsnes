#include "framebuffer.hpp"
#include "png.hpp"
#include "serialization.hpp"
#include "string.hpp"
#include "minmax.hpp"
#include "utf8.hpp"
#include <cstring>
#include <iostream>
#include <list>

#define TABSTOPS 64
#define SCREENSHOT_RGB_MAGIC	0x74212536U

namespace framebuffer
{
namespace
{
	std::list<pixfmt*>& pixfmts()
	{
		static std::list<pixfmt*> x;
		return x;
	}

	template<size_t c> void decode_words(uint8_t* target, const uint8_t* src, size_t srcsize)
	{
		if(c == 1 || c == 2 || c == 3 || c == 4)
			srcsize /= c;
		else
			srcsize /= 3;
		if(c == 1) {
			for(size_t i = 0; i < srcsize; i++)
				target[i] = src[i];
		} else if(c == 2) {
			uint16_t* _target = reinterpret_cast<uint16_t*>(target);
			for(size_t i = 0; i < srcsize; i++) {
				_target[i] = static_cast<uint16_t>(src[2 * i + 0]) << 8;
				_target[i] |= static_cast<uint16_t>(src[2 * i + 1]);
			}
		} else if(c == 3) {
			for(size_t i = 0; i < srcsize; i++) {
				target[3 * i + 0] = src[3 * i + 0];
				target[3 * i + 1] = src[3 * i + 1];
				target[3 * i + 2] = src[3 * i + 2];
			}
		} else if(c == 4) {
			uint32_t* _target = reinterpret_cast<uint32_t*>(target);
			for(size_t i = 0; i < srcsize; i++) {
				_target[i] = static_cast<uint32_t>(src[4 * i + 0]) << 24;
				_target[i] |= static_cast<uint32_t>(src[4 * i + 1]) << 16;
				_target[i] |= static_cast<uint32_t>(src[4 * i + 2]) << 8;
				_target[i] |= static_cast<uint32_t>(src[4 * i + 3]);
			}
		} else if(c == 5) {
			uint32_t* _target = reinterpret_cast<uint32_t*>(target);
			for(size_t i = 0; i < srcsize; i++) {
				_target[i] = (static_cast<uint32_t>(src[3 * i + 0]) << 16);
				_target[i] |= (static_cast<uint32_t>(src[3 * i + 1]) << 8);
				_target[i] |= (static_cast<uint32_t>(src[3 * i + 2]));
			}
		}
	}

	template<size_t c> void encode_words(uint8_t* target, const uint8_t* src, size_t elts)
	{
		if(c == 1)
			for(size_t i = 0; i < elts; i++)
				target[i] = src[i];
		else if(c == 2) {
			const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
			for(size_t i = 0; i < elts; i++) {
				target[2 * i + 0] = _src[i] >> 8;
				target[2 * i + 1] = _src[i];
			}
		} else if(c == 3) {
			for(size_t i = 0; i < elts; i++) {
				target[3 * i + 0] = src[3 * i + 0];
				target[3 * i + 1] = src[3 * i + 1];
				target[3 * i + 2] = src[3 * i + 2];
			}
		} else if(c == 4) {
			const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
			for(size_t i = 0; i < elts; i++) {
				target[4 * i + 0] = _src[i] >> 24;
				target[4 * i + 1] = _src[i] >> 16;
				target[4 * i + 2] = _src[i] >> 8;
				target[4 * i + 3] = _src[i];
			}
		} else if(c == 5) {
			const uint32_t* _src = reinterpret_cast<const uint32_t*>(src);
			for(size_t i = 0; i < elts; i++) {
				target[3 * i + 0] = _src[i] >> 16;
				target[3 * i + 1] = _src[i] >> 8;
				target[3 * i + 2] = _src[i];
			}
		}
	}

	struct color_modifier
	{
		const char* name;
		std::function<void(int64_t& v)> fn;
		bool modifier;
	};

	std::map<std::string, std::pair<std::function<void(int64_t& v)>, bool>>& colornames()
	{
		static std::map<std::string, std::pair<std::function<void(int64_t& v)>, bool>> c;
		return c;
	}
}

basecolor::basecolor(const std::string& name, int64_t value)
{
	colornames()[name] = std::make_pair([value](int64_t& v) { v = value; }, false);
}

color_mod::color_mod(const std::string& name, std::function<void(int64_t&)> fn)
{
	colornames()[name] = std::make_pair(fn, true);
}

pixfmt::pixfmt() throw(std::bad_alloc)
{
	pixfmts().push_back(this);
}

pixfmt::~pixfmt() throw()
{
	for(auto i = pixfmts().begin(); i != pixfmts().end(); i++)
		if(*i == this) {
			pixfmts().erase(i);
			break;
		}
}

raw::raw(const info& info) throw(std::bad_alloc)
{
	size_t unit = info.type->get_bpp();
	size_t pixel_offset = info.offset_y * info.physstride + unit * info.offset_x;
	user_memory = false;
	addr = info.mem + pixel_offset;
	fmt = info.type;
	width = info.width;
	height = info.height;
	stride = info.stride;
	allocated = 0;
}

raw::raw() throw(std::bad_alloc)
{
	user_memory = true;
	fmt = NULL;
	addr = NULL;
	width = 0;
	height = 0;
	stride = 0;
	allocated = 0;
}

raw::raw(const raw& f) throw(std::bad_alloc)
{
	user_memory = true;
	fmt = f.fmt;
	width = f.width;
	height = f.height;
	size_t unit = f.fmt->get_bpp();
	stride = f.width * unit;
	allocated = unit * width * height;
	addr = new char[allocated];
	for(size_t i = 0; i < height; i++)
		memcpy(addr + stride * i, f.addr + f.stride * i, unit * width);
}

raw& raw::operator=(const raw& f) throw(std::bad_alloc, std::runtime_error)
{
	if(!user_memory)
		throw std::runtime_error("Target framebuffer is not writable");
	if(this == &f)
		return *this;
	size_t unit = f.fmt->get_bpp();
	size_t newallocated = unit * f.width * f.height;
	if(newallocated > allocated) {
		char* newaddr = new char[newallocated];
		delete[] addr;
		addr = newaddr;
		allocated = newallocated;
	}
	fmt = f.fmt;
	width = f.width;
	height = f.height;
	stride = f.width * unit;
	for(size_t i = 0; i < height; i++)
		memcpy(addr + stride * i, f.addr + f.stride * i, unit * width);
	return *this;
}

raw::~raw()
{
	if(user_memory)
		delete[] addr;
}

void raw::load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error)
{
	if(data.size() < 2)
		throw std::runtime_error("Bad screenshot data");
	if(!user_memory)
		throw std::runtime_error("Target framebuffer is not writable");
	pixfmt* nfmt = NULL;
	const uint8_t* data2 = reinterpret_cast<const uint8_t*>(&data[0]);
	size_t legacy_width = serialization::u16b(data2);
	size_t dataoffset;
	size_t _width;
	size_t _height;

	if(legacy_width > 0 && data.size() % (3 * legacy_width) == 2) {
		//Legacy screenshot.
		for(pixfmt* f : pixfmts())
			if(f->get_magic() == 0)
				nfmt = f;
		if(!nfmt)
			throw std::runtime_error("Unknown screenshot format");
		_width = legacy_width;
		_height = (data.size() - 2) / (3 * legacy_width);
		dataoffset = 2;
	} else {
		//New format.
		if(data.size() < 8)
			throw std::runtime_error("Bad screenshot data");
		dataoffset = 8;
		uint32_t magic = serialization::u32b(data2 + 2);
		for(pixfmt* f : pixfmts())
			if(f->get_magic() == magic)
				nfmt = f;
		if(!nfmt)
			throw std::runtime_error("Unknown screenshot format");
		_width = serialization::u16b(data2 + 6);
		_height = (data.size() - 8) / (nfmt->get_ss_bpp() * _width);
	}
	if(data.size() < dataoffset + nfmt->get_ss_bpp() * _width * _height)
		throw std::runtime_error("Bad screenshot data");

	size_t bpp = nfmt->get_bpp();
	size_t sbpp = nfmt->get_ss_bpp();
	if(allocated < bpp * _width * _height) {
		//Allocate more memory.
		size_t newalloc = bpp * _width * _height;
		char* addr2 = new char[newalloc];
		delete[] addr;
		addr = addr2;
		allocated = newalloc;
	}
	fmt = nfmt;
	width = _width;
	height = _height;
	stride = _width * bpp;
	if(bpp == 1)
		decode_words<1>(reinterpret_cast<uint8_t*>(addr), data2 + dataoffset, data.size() - dataoffset);
	else if(bpp == 2)
		decode_words<2>(reinterpret_cast<uint8_t*>(addr), data2 + dataoffset, data.size() - dataoffset);
	else if(bpp == 3)
		decode_words<3>(reinterpret_cast<uint8_t*>(addr), data2 + dataoffset, data.size() - dataoffset);
	else if(bpp == 4 && sbpp == 3)
		decode_words<5>(reinterpret_cast<uint8_t*>(addr), data2 + dataoffset, data.size() - dataoffset);
	else if(bpp == 4 && sbpp == 4)
		decode_words<4>(reinterpret_cast<uint8_t*>(addr), data2 + dataoffset, data.size() - dataoffset);
}

void raw::save(std::vector<char>& data) throw(std::bad_alloc)
{
	uint8_t* memory = reinterpret_cast<uint8_t*>(addr);
	unsigned m;
	size_t bpp = fmt->get_bpp();
	size_t sbpp = fmt->get_ss_bpp();
	size_t offset;
	uint8_t* data2;
	uint32_t magic = fmt->get_magic();
	switch(magic) {
	case 0:
		//Save in legacy format.
		offset = 2;
		data.resize(offset + sbpp * static_cast<size_t>(width) * height);
		data2 = reinterpret_cast<uint8_t*>(&data[0]);
		serialization::u16b(&data[0], width);
		break;
	default:
		//Choose the first two bytes so that screenshot is bad in legacy format.
		m = 2;
		while((sbpp * width * height + 8) % (3 * m) == 2)
			m++;
		offset = 8;
		data.resize(offset + sbpp * static_cast<size_t>(width) * height);
		serialization::u16b(&data[0], m);
		serialization::u32b(&data[2], magic);
		serialization::u16b(&data[6], width);
		break;
	}
	data2 = reinterpret_cast<uint8_t*>(&data[0]);
	for(size_t i = 0; i < height; i++) {
		if(bpp == 1)
			encode_words<1>(data2 + offset + sbpp * width * i, memory + stride * i, width);
		else if(bpp == 2)
			encode_words<2>(data2 + offset + sbpp * width * i, memory + stride * i, width);
		else if(bpp == 3)
			encode_words<3>(data2 + offset + sbpp * width * i, memory + stride * i, width);
		else if(bpp == 4 && sbpp == 3)
			encode_words<5>(data2 + offset + sbpp * width * i, memory + stride * i, width);
		else if(bpp == 4 && sbpp == 4)
			encode_words<4>(data2 + offset + sbpp * width * i, memory + stride * i, width);
	}
}

void raw::save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error)
{
	uint8_t* memory = reinterpret_cast<uint8_t*>(addr);
	png::encoder img;
	img.width = width;
	img.height = height;
	img.has_palette = false;
	img.has_alpha = false;
	img.data.resize(static_cast<size_t>(width) * height);
	for(size_t i = 0; i < height; i++)
		fmt->decode(&img.data[width * i], memory + stride * i, width);
	try {
		img.encode(file);
	} catch(...) {
		throw;
	}
}

size_t raw::get_stride() const throw() { return stride; }
unsigned char* raw::get_start() const throw() { return reinterpret_cast<uint8_t*>(addr); }
pixfmt* raw::get_format() const throw() { return fmt; }


template<bool X>
fb<X>::fb() throw()
{
	width = 0;
	height = 0;
	last_blit_w = 0;
	last_blit_h = 0;
	stride = 0;
	offset_x = 0;
	offset_y = 0;
	mem = NULL;
	user_mem = false;
	upside_down = false;
	current_fmt = NULL;
	active_rshift = (X ? 32 : 16);
	active_gshift = (X ? 16 : 8);
	active_bshift = 0;
}


template<bool X>
fb<X>::~fb() throw()
{
	if(user_mem)
		delete[] mem;
}

#define DECBUF_SIZE 4096

template<bool X>
void fb<X>::copy_from(raw& scr, size_t hscale, size_t vscale) throw()
{
	typename fb<X>::element_t decbuf[DECBUF_SIZE];
	last_blit_w = scr.width * hscale;
	last_blit_h = scr.height * vscale;

	if(!scr.fmt) {
		for(size_t y = 0; y < height; y++)
			memset(rowptr(y), 0, sizeof(typename fb<X>::element_t) * width);
		return;
	}
	if(scr.fmt != current_fmt || active_rshift != auxpal.rshift || active_gshift != auxpal.gshift ||
		active_bshift != auxpal.bshift) {
		scr.fmt->set_palette(auxpal, active_rshift, active_gshift, active_bshift);
		current_fmt = scr.fmt;
	}

	for(size_t y = 0; y < height; y++)
		memset(rowptr(y), 0, sizeof(typename fb<X>::element_t) * width);
	if(width < offset_x || height < offset_y) {
		//Just clear the screen.
		return;
	}
	size_t copyable_width = 0, copyable_height = 0;
	if(hscale)
		copyable_width = (width - offset_x) / hscale;
	if(vscale)
		copyable_height = (height - offset_y) / vscale;
	copyable_width = (copyable_width > scr.width) ? scr.width : copyable_width;
	copyable_height = (copyable_height > scr.height) ? scr.height : copyable_height;

	for(size_t y = 0; y < copyable_height; y++) {
		size_t line = y * vscale + offset_y;
		const uint8_t* sbase = reinterpret_cast<uint8_t*>(scr.addr) + y * scr.stride;
		typename fb<X>::element_t* ptr = rowptr(line) + offset_x;
		size_t bpp = scr.fmt->get_bpp();
		size_t xptr = 0;
		size_t old_copyable_width = copyable_width;
		while(copyable_width > DECBUF_SIZE) {
			scr.fmt->decode(decbuf, sbase + xptr * bpp, DECBUF_SIZE, auxpal);
			for(size_t k = 0; k < DECBUF_SIZE; k++)
				for(size_t i = 0; i < hscale; i++)
					*(ptr++) = decbuf[k];
			xptr += DECBUF_SIZE;
			copyable_width -= DECBUF_SIZE;
		}
		scr.fmt->decode(decbuf, sbase + xptr * bpp, copyable_width, auxpal);
		for(size_t k = 0; k < copyable_width; k++)
			for(size_t i = 0; i < hscale; i++)
				*(ptr++) = decbuf[k];
		copyable_width = old_copyable_width;
		for(size_t j = 1; j < vscale; j++)
			memcpy(rowptr(line + j) + offset_x, rowptr(line) + offset_x,
				sizeof(typename fb<X>::element_t) * hscale * copyable_width);
	};
}

template<bool X>
void fb<X>::set_palette(uint32_t r, uint32_t g, uint32_t b) throw(std::bad_alloc)
{
	typename fb<X>::element_t R, G, B;
	if(r == active_rshift && g == active_gshift && b == active_bshift)
		return;
	for(size_t i = 0; i < static_cast<size_t>(width) * height; i++) {
		typename fb<X>::element_t word = mem[i];
		R = (word >> active_rshift) & (X ? 0xFFFF : 0xFF);
		G = (word >> active_gshift) & (X ? 0xFFFF : 0xFF);
		B = (word >> active_bshift) & (X ? 0xFFFF : 0xFF);
		mem[i] = (R << r) | (G << g) | (B << b);
	}
	active_rshift = r;
	active_gshift = g;
	active_bshift = b;
}

template<bool X>
void fb<X>::set(element_t* _memory, size_t _width, size_t _height, size_t _pitch) throw()
{
	if(user_mem && mem)
		delete[] mem;
	mem = _memory;
	width = _width;
	height = _height;
	stride = _pitch;
	user_mem = false;
	upside_down = false;
}

template<bool X>
void fb<X>::reallocate(size_t _width, size_t _height, bool _upside_down) throw(std::bad_alloc)
{
	if(width != _width || height != _height) {
		if(user_mem) {
			element_t* newmem = new element_t[_width * _height];
			delete[] mem;
			mem = newmem;
		} else
			mem = new element_t[_width * _height];
	}
	memset(mem, 0, sizeof(element_t) * _width * _height);
	width = _width;
	height = _height;
	stride = _width;
	upside_down = _upside_down;
	user_mem = true;
}

template<bool X>
void fb<X>::set_origin(size_t _offset_x, size_t _offset_y) throw()
{
	offset_x = _offset_x;
	offset_y = _offset_y;
}

template<bool X>
size_t fb<X>::get_width() const throw()
{
	return width;
}

template<bool X>
size_t fb<X>::get_height() const throw()
{
	return height;
}

template<bool X>
size_t fb<X>::get_last_blit_width() const throw()
{
	return last_blit_w;
}

template<bool X>
size_t fb<X>::get_last_blit_height() const throw()
{
	return last_blit_h;
}


template<bool X>
typename fb<X>::element_t* fb<X>::rowptr(size_t row) throw()
{
	if(upside_down)
		row = height - row - 1;
	return mem + stride * row;
}

template<bool X>
const typename fb<X>::element_t* fb<X>::rowptr(size_t row) const throw()
{
	if(upside_down)
		row = height - row - 1;
	return mem + stride * row;
}

template<bool X> uint8_t fb<X>::get_palette_r() const throw() { return auxpal.rshift; }
template<bool X> uint8_t fb<X>::get_palette_g() const throw() { return auxpal.gshift; }
template<bool X> uint8_t fb<X>::get_palette_b() const throw() { return auxpal.bshift; }

size_t raw::get_width() const throw() { return width; }
size_t raw::get_height() const throw() { return height; }
template<bool X> size_t fb<X>::get_origin_x() const throw() { return offset_x; }
template<bool X> size_t fb<X>::get_origin_y() const throw() { return offset_y; }

void clip_range(uint32_t origin, uint32_t size, int32_t base, int32_t& minc, int32_t& maxc) throw()
{
	int64_t _origin = origin;
	int64_t _size = size;
	int64_t _base = base;
	int64_t _minc = minc;
	int64_t _maxc = maxc;
	int64_t mincoordinate = _base + _origin + _minc;
	int64_t maxcoordinate = _base + _origin + _maxc;
	if(mincoordinate < 0)
		_minc = _minc - mincoordinate;
	if(maxcoordinate > _size)
		_maxc = _maxc - (maxcoordinate - _size);
	if(_minc >= maxc) {
		minc = 0;
		maxc = 0;
	} else {
		minc = _minc;
		maxc = _maxc;
	}
}

void queue::add(struct object& obj) throw(std::bad_alloc)
{
	struct node* n = reinterpret_cast<struct node*>(alloc(sizeof(node)));
	n->obj = &obj;
	n->next = NULL;
	n->killed = false;
	if(queue_tail)
		queue_tail = queue_tail->next = n;
	else
		queue_head = queue_tail = n;
}

void queue::copy_from(queue& q) throw(std::bad_alloc)
{
	struct node* tmp = q.queue_head;
	while(tmp) {
		try {
			tmp->obj->clone(*this);
			tmp = tmp->next;
		} catch(...) {
		}
	}
}

template<bool X> void queue::run(struct fb<X>& scr) throw()
{
	struct node* tmp = queue_head;
	while(tmp) {
		try {
			if(!tmp->killed)
				(*(tmp->obj))(scr);
			tmp = tmp->next;
		} catch(...) {
		}
	}
}

void queue::clear() throw()
{
	while(queue_head) {
		if(!queue_head->killed)
			queue_head->obj->~object();
		queue_head = queue_head->next;
	}
	//Release all memory for reuse.
	memory_allocated = 0;
	pages = 0;
	queue_tail = NULL;
}

void* queue::alloc(size_t block) throw(std::bad_alloc)
{
	block = (block + 15) / 16 * 16;
	if(block > RENDER_PAGE_SIZE)
		throw std::bad_alloc();
	if(pages == 0 || memory_allocated + block > pages * RENDER_PAGE_SIZE) {
		memory_allocated = pages * RENDER_PAGE_SIZE;
		memory[pages++];
	}
	void* mem = memory[memory_allocated / RENDER_PAGE_SIZE].content + (memory_allocated % RENDER_PAGE_SIZE);
	memory_allocated += block;
	return mem;
}

void queue::kill_request(void* obj) throw()
{
	struct node* tmp = queue_head;
	while(tmp) {
		try {
			if(!tmp->killed && tmp->obj->kill_request(obj)) {
				//Kill this request.
				tmp->killed = true;
				tmp->obj->~object();
			}
			tmp = tmp->next;
		} catch(...) {
		}
	}
}

queue::queue() throw()
{
	queue_head = NULL;
	queue_tail = NULL;
	memory_allocated = 0;
	pages = 0;
}

queue::~queue() throw()
{
	clear();
}

object::object() throw()
{
}

object::~object() throw()
{
}

bool object::kill_request_ifeq(void* myobj, void* killobj)
{
	if(!killobj)
		return false;
	if(myobj == killobj)
		return true;
	return false;
}

bool object::kill_request(void* obj) throw()
{
	return false;
}

font::font() throw(std::bad_alloc)
{
	bad_glyph_data[0] = 0x018001AAU;
	bad_glyph_data[1] = 0x01800180U;
	bad_glyph_data[2] = 0x01800180U;
	bad_glyph_data[3] = 0x55800180U;
	bad_glyph.wide = false;
	bad_glyph.data = bad_glyph_data;
}

void font::load_hex_glyph(const char* data, size_t size) throw(std::bad_alloc, std::runtime_error)
{
	char buf2[8];
	std::string line(data, data + size);
	regex_results r;
	if(r = regex("([0-9A-Fa-f]+):([0-9A-Fa-f]{32})", line)) {
	} else if(r = regex("([0-9A-Fa-f]+):([0-9A-Fa-f]{64})", line)) {
	} else
		(stringfmt() << "Invalid line '" << line << "'").throwex();
	std::string codepoint = r[1];
	std::string cdata = r[2];
	if(codepoint.length() > 7)
		(stringfmt() << "Invalid line '" << line << "'").throwex();
	strcpy(buf2, codepoint.c_str());
	char* end2;
	unsigned long cp = strtoul(buf2, &end2, 16);
	if(*end2 || cp > 0x10FFFF)
		(stringfmt() << "Invalid line '" << line << "'").throwex();
	glyphs[cp].wide = (cdata.length() == 64);
	size_t p = memory.size();
	for(size_t i = 0; i < cdata.length(); i += 8) {
		char buf[9] = {0};
		char* end;
		for(size_t j = 0; j < 8; j++)
			buf[j] = cdata[i + j];
		memory.push_back(strtoul(buf, &end, 16));
	}
	glyphs[cp].offset = p;
}

void font::load_hex(const char* data, size_t size) throw(std::bad_alloc, std::runtime_error)
{
	const char* enddata = data + size;
	while(data != enddata) {
		size_t linesize = 0;
		while(data + linesize != enddata && data[linesize] != '\n' && data[linesize] != '\r')
			linesize++;
		if(linesize && data[0] != '#')
			load_hex_glyph(data, linesize);
		data += linesize;
		if(data != enddata)
			data++;
	}
	memory.push_back(0);
	memory.push_back(0);
	memory.push_back(0);
	memory.push_back(0);
	glyphs[32].wide = false;
	glyphs[32].offset = memory.size() - 4;
	for(auto& i : glyphs)
		i.second.data = &memory[i.second.offset];
}

const font::glyph& font::get_glyph(uint32_t glyph) throw()
{
	if(glyphs.count(glyph))
		return glyphs[glyph];
	else
		return bad_glyph;
}

std::set<uint32_t> font::get_glyphs_set()
{
	std::set<uint32_t> out;
	for(auto& i : glyphs)
		out.insert(i.first);
	return out;
}

const font::glyph& font::get_bad_glyph() throw()
{
	return bad_glyph;
}

std::pair<size_t, size_t> font::get_metrics(const std::string& string) throw()
{
	size_t commit_width = 0;
	size_t commit_height = 0;
	size_t linelength = 0;
	utf8::to32i(string.begin(), string.end(), lambda_output_iterator<int32_t>([this, &linelength, &commit_width,
		&commit_height](const int32_t& cp) -> void {
		const glyph& g = get_glyph(cp);
		switch(cp) {
		case 9:
			linelength = (linelength + TABSTOPS) / TABSTOPS * TABSTOPS;
			commit_width = max(commit_width, linelength);
			break;
		case 10:
			commit_height += 16;
			break;
		default:
			linelength = linelength + (g.wide ? 16 : 8);
			commit_width = max(commit_width, linelength);
			break;
		};
	}));
	return std::make_pair(commit_width, commit_height);
}

std::vector<font::layout> font::dolayout(const std::string& string) throw(std::bad_alloc)
{
	//First, calculate the number of glyphs to draw.
	size_t chars = 0;
	utf8::to32i(string.begin(), string.end(), lambda_output_iterator<int32_t>([&chars](const int32_t& cp)
		-> void {
		if(cp != 9 && cp != 10)
			chars++;
	}));
	//Allocate space.
	std::vector<layout> l;
	l.resize(chars);
	size_t gtr = 0;
	size_t layout_x = 0;
	size_t layout_y = 0;
	utf8::to32i(string.begin(), string.end(), lambda_output_iterator<int32_t>([this, &layout_x, &layout_y,
		&l, &gtr](const int32_t cp) {
		const glyph& g = get_glyph(cp);
		switch(cp) {
		case 9:
			layout_x = (layout_x + TABSTOPS) / TABSTOPS * TABSTOPS;
			break;
		case 10:
			layout_x = 0;
			layout_y = layout_y + 16;
			break;
		default:
			l[gtr].x = layout_x;
			l[gtr].y = layout_y;
			l[gtr++].dglyph = &g;
			layout_x = layout_x + (g.wide ? 16 : 8);;
		}
	}));
	return l;
}

template<bool X> void font::render(struct fb<X>& scr, int32_t x, int32_t y, const std::string& text,
	color fg, color bg, bool hdbl, bool vdbl) throw()
{
	x += scr.get_origin_x();
	y += scr.get_origin_y();
	size_t layout_x = 0;
	size_t layout_y = 0;
	size_t swidth = scr.get_width();
	size_t sheight = scr.get_height();
	utf8::to32i(text.begin(), text.end(), lambda_output_iterator<int32_t>([this, x, y, &scr, &layout_x,
		&layout_y, swidth, sheight, hdbl, vdbl, &fg, &bg](const int32_t& cp) {
		const glyph& g = get_glyph(cp);
		switch(cp) {
		case 9:
			layout_x = (layout_x + TABSTOPS) / TABSTOPS * TABSTOPS;
			break;
		case 10:
			layout_x = 0;
			layout_y = layout_y + (vdbl ? 32 : 16);
			break;
		default:
			//Render this glyph at x + layout_x, y + layout_y.
			int32_t gx = x + layout_x;
			int32_t gy = y + layout_y;
			//Don't draw characters completely off-screen.
			if(gy <= (vdbl ? -32 : -16) || gy >= (ssize_t)sheight)
				break;
			if(gx <= -(hdbl ? 2 : 1) * (g.wide ? 16 : 8) || gx >= (ssize_t)swidth)
				break;
			//Compute the bounding box.
			uint32_t xstart = 0;
			uint32_t ystart = 0;
			uint32_t xlength = (hdbl ? 2 : 1) * (g.wide ? 16 : 8);
			uint32_t ylength = (vdbl ? 32 : 16);
			if(gx < 0)	xstart = -gx;
			if(gy < 0)	ystart = -gy;
			xlength -= xstart;
			ylength -= ystart;
			if(gx + xstart + xlength > swidth)	xlength = swidth - (gx + xstart);
			if(gy + ystart + ylength > sheight)	ylength = sheight - (gy + ystart);
			if(g.data)
				for(size_t i = 0; i < ylength; i++) {
					typename fb<X>::element_t* r = scr.rowptr(gy + ystart + i) +
						(gx + xstart);
					uint32_t _y = (i + ystart) >> (vdbl ? 1 : 0);
					uint32_t d = g.data[_y >> (g.wide ? 1 : 2)];
					if(g.wide)
						d >>= 16 - ((_y & 1) << 4);
					else
						d >>= 24 - ((_y & 3) << 3);
					if(hdbl)
						for(size_t j = 0; j < xlength; j++) {
							uint32_t b = (g.wide ? 15 : 7) - ((j + xstart) >> 1);
							if(((d >> b) & 1) != 0)
								fg.apply(r[j]);
							else
								bg.apply(r[j]);
						}
					else
						for(size_t j = 0; j < xlength; j++) {
							uint32_t b = (g.wide ? 15 : 7) - (j + xstart);
							if(((d >> b) & 1) != 0)
								fg.apply(r[j]);
							else
								bg.apply(r[j]);
						}
				}
			else
				for(size_t i = 0; i < ylength; i++) {
					typename fb<X>::element_t* r = scr.rowptr(gy + ystart + i) +
						(gx + xstart);
					for(size_t j = 0; j < xlength; j++)
						bg.apply(r[j]);
				}
			layout_x += (hdbl ? 2 : 1) * (g.wide ? 16 : 8);
		}
	}));
}

color::color(const std::string& clr) throw(std::bad_alloc, std::runtime_error)
{
	int64_t col = -1;
	bool first = true;
	auto& cspecs = colornames();
	for(auto& t : token_iterator_foreach(clr, {" ","\t"}, true)) {
		if(!cspecs.count(t))
			throw std::runtime_error("Invalid color (modifier) '" + t + "'");
		if(!first && !cspecs[t].second)
			throw std::runtime_error("Base color (" + t + ") can't be used as modifier");
		if(first && cspecs[t].second)
			throw std::runtime_error("Modifier (" + t + ") can't be used as base color");
		(cspecs[t].first)(col);
		first = false;
	}
	*this = color(col);
}

void color::set_palette(unsigned rshift, unsigned gshift, unsigned bshift, bool X) throw()
{
	if(X) {
		uint64_t r = ((orig >> 16) & 0xFF) * 257;
		uint64_t g = ((orig >> 8) & 0xFF) * 257;
		uint64_t b = (orig & 0xFF) * 257;
		uint64_t a = 65535;
		uint64_t fullc = ~0ULL & ~((a << rshift) | (a << gshift) | (a << bshift));
		uint64_t color = (r << rshift) | (g << gshift) | (b << bshift) | fullc;
		hiHI = color & 0xFFFF0000FFFFULL;
		loHI = (color & 0xFFFF0000FFFF0000ULL) >> 16;
		hiHI *= (static_cast<uint32_t>(origa) * 256);
		loHI *= (static_cast<uint32_t>(origa) * 256);
	} else {
		uint32_t r = (orig >> 16) & 0xFF;
		uint32_t g = (orig >> 8) & 0xFF;
		uint32_t b = orig & 0xFF;
		uint32_t a = 255;
		uint64_t fullc = ~0UL & ~((a << rshift) | (a << gshift) | (a << bshift));
		uint32_t color = (r << rshift) | (g << gshift) | (b << bshift) | fullc;
		hi = color & 0xFF00FF;
		lo = (color & 0xFF00FF00) >> 8;
		hi *= origa;
		lo *= origa;
	}
}

namespace
{
	void adjust_hmM_hue(int16_t& hue, int16_t& m, int16_t& M, double adj)
	{
		if(m == M)
			return;
		int16_t S = M - m;
		hue = (hue + static_cast<uint32_t>(adj * S)) % (6 * S);
	}
	void adjust_ls_saturation(double& s, double& l, double adj)
	{
		s = clip(s + adj, 0.0, 1.0);
	}
	void adjust_ls_lightness(double& s, double& l, double adj)
	{
		l = clip(l + adj, 0.0, 1.0);
	}
	template<void(*adjustfn)(double& s, double& l, double adj)>
	void adjust_hmM_sl(int16_t& hue, int16_t& m, int16_t& M, double adj)
	{
		int16_t S1 = M - m;
		double _m = m / 255.0;
		double _M = M / 255.0;
		double l = (_m + _M) / 2;
		double s;
		if(l == 0 || l == 1) s = 0;
		else if(l <= 0.5) s = _M / l - 1;
		else s = (_M - l) / (1 - l);
		adjustfn(s, l, adj);
		if(l <= 0.5) _M = l * (s + 1);
		else _M = l + s - l * s;
		_m = 2 * l -_M;
		m = _m * 255;
		M = _M * 255;
		int32_t S2 = M - m;
		hue = S1 ? (S2 * hue / S1) : 0;
	}
	//0: m
	//1: M
	//2: m + phue
	//3: M - phue
	const uint8_t hsl2rgb_flags[] = {24, 52, 6, 13, 33, 19};
	template<void(*adjustfn)(int16_t& hue, int16_t& m, int16_t& M, double adj)>
	uint32_t adjustcolor(uint32_t color, double shift)
	{
		int16_t R = (color >> 16) & 0xFF;
		int16_t G = (color >> 8) & 0xFF;
		int16_t B = color & 0xFF;
		int16_t m = min(R, min(G, B));
		int16_t M = max(R, max(G, B));
		int16_t S1 = M - m;
		int16_t hue;
		if(R == M)
			hue = G - B + 6 * S1;
		else if(G == M)
			hue = B - R + 2 * S1;
		else
			hue = R - G + 4 * S1;
		adjustfn(hue, m, M, shift);
		if(m == M)
			return ((uint32_t)m << 16) | ((uint32_t)m << 8) | (uint32_t)m;
		int16_t S2 = M - m;
		hue %= (6 * S2);
		uint32_t V[4];
		V[0] = m;
		V[1] = M;
		V[2] = m + hue % S2;
		V[3] = M - hue % S2;
		uint8_t flag = hsl2rgb_flags[hue / S2];
		return (V[(flag >> 4) & 3] << 16) | (V[(flag >> 2) & 3] << 8) | (V[flag & 3]);
	}
}

int64_t color_rotate_hue(int64_t basecolor, int step, int steps)
{
	if(!steps)
		throw std::runtime_error("Expected nonzero steps for hue rotation");
	if(basecolor < 0) {
		//Special: Any rotation of transparent is transparent.
		return -1;
	}
	uint32_t asteps = std::abs(steps);
	if(steps < 0)
		step = asteps - step % asteps;	//Reverse order.
	double hueshift = 6.0 * (step % asteps) / asteps;
	basecolor = adjustcolor<adjust_hmM_hue>(basecolor & 0xFFFFFF, hueshift) | (basecolor & 0xFF000000);
	return basecolor;
}

int64_t color_adjust_saturation(int64_t color, double adjust)
{
	if(color < 0) return color;
	return adjustcolor<adjust_hmM_sl<adjust_ls_saturation>>(color & 0xFFFFFF, adjust) | (color & 0xFF000000);
}

int64_t color_adjust_lightness(int64_t color, double adjust)
{
	if(color < 0) return color;
	return adjustcolor<adjust_hmM_sl<adjust_ls_lightness>>(color & 0xFFFFFF, adjust) | (color & 0xFF000000);
}


template class fb<false>;
template class fb<true>;
template void queue::run(struct fb<false>&);
template void queue::run(struct fb<true>&);
template void font::render(struct fb<false>& scr, int32_t x, int32_t y, const std::string& text,
	color fg, color bg, bool hdbl, bool vdbl) throw();
template void font::render(struct fb<true>& scr, int32_t x, int32_t y, const std::string& text,
	color fg, color bg, bool hdbl, bool vdbl) throw();
}
