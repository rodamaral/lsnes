#include "framebuffer.hpp"
#include "png.hpp"
#include "serialization.hpp"
#include "string.hpp"
#include "utf8.hpp"
#include <cstring>
#include <iostream>
#include <list>

#define TABSTOPS 64
#define SCREENSHOT_RGB_MAGIC	0x74212536U

namespace
{
	std::list<pixel_format*>& pixel_formats()
	{
		static std::list<pixel_format*> x;
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
}

pixel_format::pixel_format() throw(std::bad_alloc)
{
	pixel_formats().push_back(this);
}

pixel_format::~pixel_format() throw()
{
	for(auto i = pixel_formats().begin(); i != pixel_formats().end(); i++)
		if(*i == this) {
			pixel_formats().erase(i);
			break;
		}
}

framebuffer_raw::framebuffer_raw(const framebuffer_info& info) throw(std::bad_alloc)
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

framebuffer_raw::framebuffer_raw() throw(std::bad_alloc)
{
	user_memory = true;
	fmt = NULL;
	addr = NULL;
	width = 0;
	height = 0;
	stride = 0;
	allocated = 0;
}

framebuffer_raw::framebuffer_raw(const framebuffer_raw& f) throw(std::bad_alloc)
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

framebuffer_raw& framebuffer_raw::operator=(const framebuffer_raw& f) throw(std::bad_alloc, std::runtime_error)
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

framebuffer_raw::~framebuffer_raw()
{
	if(user_memory)
		delete[] addr;
}

void framebuffer_raw::load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error)
{
	if(data.size() < 2)
		throw std::runtime_error("Bad screenshot data");
	if(!user_memory)
		throw std::runtime_error("Target framebuffer is not writable");
	pixel_format* nfmt = NULL;
	const uint8_t* data2 = reinterpret_cast<const uint8_t*>(&data[0]);
	size_t legacy_width = read16ube(data2);
	size_t dataoffset;
	size_t _width;
	size_t _height;

	if(legacy_width > 0 && data.size() % (3 * legacy_width) == 2) {
		//Legacy screenshot.
		for(pixel_format* f : pixel_formats())
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
		uint32_t magic = read32ube(data2 + 2);
		for(pixel_format* f : pixel_formats())
			if(f->get_magic() == magic)
				nfmt = f;
		if(!nfmt)
			throw std::runtime_error("Unknown screenshot format");
		_width = read16ube(data2 + 6);
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

void framebuffer_raw::save(std::vector<char>& data) throw(std::bad_alloc)
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
		write16ube(&data[0], width);
		break;
	default:
		//Choose the first two bytes so that screenshot is bad in legacy format.
		m = 2;
		while((sbpp * width * height + 8) % (3 * m) == 2)
			m++;
		offset = 8;
		data.resize(offset + sbpp * static_cast<size_t>(width) * height);
		write16ube(&data[0], m);
		write32ube(&data[2], magic);
		write16ube(&data[6], width);
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

void framebuffer_raw::save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error)
{
	uint8_t* memory = reinterpret_cast<uint8_t*>(addr);
	uint8_t* buffer = new uint8_t[3 * static_cast<size_t>(width) * height];
	for(size_t i = 0; i < height; i++)
		fmt->decode(buffer + 3 * width * i, memory + stride * i, width);
	try {
		save_png_data(file, buffer, width, height);
		delete[] buffer;
	} catch(...) {
		delete[] buffer;
		throw;
	}
}

template<bool X>
framebuffer<X>::framebuffer() throw()
{
	width = 0;
	height = 0;
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
framebuffer<X>::~framebuffer() throw()
{
	if(user_mem)
		delete[] mem;
}

#define DECBUF_SIZE 4096

template<bool X>
void framebuffer<X>::copy_from(framebuffer_raw& scr, size_t hscale, size_t vscale) throw()
{
	typename framebuffer<X>::element_t decbuf[DECBUF_SIZE];

	if(!scr.fmt) {
		for(size_t y = 0; y < height; y++)
			memset(rowptr(y), 0, sizeof(typename framebuffer<X>::element_t) * width);
		return;
	}
	if(scr.fmt != current_fmt || active_rshift != auxpal.rshift || active_gshift != auxpal.gshift ||
		active_bshift != auxpal.bshift) {
		scr.fmt->set_palette(auxpal, active_rshift, active_gshift, active_bshift);
		current_fmt = scr.fmt;
	}

	for(size_t y = 0; y < height; y++)
		memset(rowptr(y), 0, sizeof(typename framebuffer<X>::element_t) * width);
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
		typename framebuffer<X>::element_t* ptr = rowptr(line) + offset_x;
		size_t bpp = scr.fmt->get_bpp();
		size_t xptr = 0;
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
		for(size_t j = 1; j < vscale; j++)
			memcpy(rowptr(line + j) + offset_x, rowptr(line) + offset_x,
				sizeof(typename framebuffer<X>::element_t) * hscale * copyable_width);
	};
}

template<bool X>
void framebuffer<X>::set_palette(uint32_t r, uint32_t g, uint32_t b) throw(std::bad_alloc)
{
	typename framebuffer<X>::element_t R, G, B;
	if(r == active_rshift && g == active_gshift && b == active_bshift)
		return;
	for(size_t i = 0; i < static_cast<size_t>(width) * height; i++) {
		typename framebuffer<X>::element_t word = mem[i];
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
void framebuffer<X>::set(element_t* _memory, size_t _width, size_t _height, size_t _pitch) throw()
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
void framebuffer<X>::reallocate(size_t _width, size_t _height, bool _upside_down) throw(std::bad_alloc)
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
void framebuffer<X>::set_origin(size_t _offset_x, size_t _offset_y) throw()
{
	offset_x = _offset_x;
	offset_y = _offset_y;
}

template<bool X>
size_t framebuffer<X>::get_width() const throw()
{
	return width;
}

template<bool X>
size_t framebuffer<X>::get_height() const throw()
{
	return height;
}

template<bool X>
typename framebuffer<X>::element_t* framebuffer<X>::rowptr(size_t row) throw()
{
	if(upside_down)
		row = height - row - 1;
	return mem + stride * row;
}

template<bool X>
const typename framebuffer<X>::element_t* framebuffer<X>::rowptr(size_t row) const throw()
{
	if(upside_down)
		row = height - row - 1;
	return mem + stride * row;
}

template<bool X> uint8_t framebuffer<X>::get_palette_r() const throw() { return auxpal.rshift; }
template<bool X> uint8_t framebuffer<X>::get_palette_g() const throw() { return auxpal.gshift; }
template<bool X> uint8_t framebuffer<X>::get_palette_b() const throw() { return auxpal.bshift; }

size_t framebuffer_raw::get_width() const throw() { return width; }
size_t framebuffer_raw::get_height() const throw() { return height; }
template<bool X> size_t framebuffer<X>::get_origin_x() const throw() { return offset_x; }
template<bool X> size_t framebuffer<X>::get_origin_y() const throw() { return offset_y; }

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

void render_queue::add(struct render_object& obj) throw(std::bad_alloc)
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

void render_queue::copy_from(render_queue& q) throw(std::bad_alloc)
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

template<bool X> void render_queue::run(struct framebuffer<X>& scr) throw()
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

void render_queue::clear() throw()
{
	while(queue_head) {
		if(!queue_head->killed)
			queue_head->obj->~render_object();
		queue_head = queue_head->next;
	}
	//Release all memory for reuse.
	memory_allocated = 0;
	pages = 0;
	queue_tail = NULL;
}

void* render_queue::alloc(size_t block) throw(std::bad_alloc)
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

void render_queue::kill_request(void* obj) throw()
{
	struct node* tmp = queue_head;
	while(tmp) {
		try {
			if(!tmp->killed && tmp->obj->kill_request(obj)) {
				//Kill this request.
				tmp->killed = true;
				tmp->obj->~render_object();
			}
			tmp = tmp->next;
		} catch(...) {
		}
	}
}

render_queue::render_queue() throw()
{
	queue_head = NULL;
	queue_tail = NULL;
	memory_allocated = 0;
	pages = 0;
}

render_queue::~render_queue() throw()
{
	clear();
}

render_object::render_object() throw()
{
}

render_object::~render_object() throw()
{
}

bool render_object::kill_request_ifeq(void* myobj, void* killobj)
{
	if(!killobj)
		return false;
	if(myobj == killobj)
		return true;
	return false;
}

bool render_object::kill_request(void* obj) throw()
{
	return false;
}

bitmap_font::bitmap_font() throw(std::bad_alloc)
{
	bad_glyph_data[0] = 0x018001AAU;
	bad_glyph_data[1] = 0x01800180U;
	bad_glyph_data[2] = 0x01800180U;
	bad_glyph_data[3] = 0x55800180U;
	bad_glyph.wide = false;
	bad_glyph.data = bad_glyph_data;
}

void bitmap_font::load_hex_glyph(const char* data, size_t size) throw(std::bad_alloc, std::runtime_error)
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

void bitmap_font::load_hex(const char* data, size_t size) throw(std::bad_alloc, std::runtime_error)
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

const bitmap_font::glyph& bitmap_font::get_glyph(uint32_t glyph) throw()
{
	if(glyphs.count(glyph))
		return glyphs[glyph];
	else
		return bad_glyph;
}

std::pair<size_t, size_t> bitmap_font::get_metrics(const std::string& string) throw()
{
	size_t commit_width = 0;
	size_t commit_height = 0;
	int32_t lineminy = 0;
	int32_t linemaxy = 0;
	size_t linelength = 0;
	uint16_t utfstate = utf8_initial_state;
	size_t itr = 0;
	size_t maxitr = string.length();
	while(true) {
		int ch = (itr < maxitr) ? static_cast<unsigned char>(string[itr++]) : -1;
		int32_t cp = utf8_parse_byte(ch, utfstate);
		if(cp < 0 && ch < 0) {
			//The end.
			commit_width = (commit_width < linelength) ? linelength : commit_width;
			commit_height += (linemaxy - lineminy + 1);
			break;
		}
		if(cp < 0)
			continue;
		const glyph& g = get_glyph(cp);
		switch(cp) {
		case 9:
			linelength = (linelength + TABSTOPS) / TABSTOPS * TABSTOPS;
			break;
		case 10:
			commit_width = (commit_width < linelength) ? linelength : commit_width;
			commit_height += 16;
			break;
		default:
			linelength = linelength + (g.wide ? 16 : 8);
			break;
		};
	}
	return std::make_pair(commit_width, commit_height);
}

std::vector<bitmap_font::layout> bitmap_font::dolayout(const std::string& string) throw(std::bad_alloc)
{
	//First, calculate the number of glyphs to draw.
	uint16_t utfstate = utf8_initial_state;
	size_t itr = 0;
	size_t maxitr = string.length();
	size_t chars = 0;
	while(true) {
		int ch = (itr < maxitr) ? static_cast<unsigned char>(string[itr++]) : -1;
		int32_t cp = utf8_parse_byte(ch, utfstate);
		if(cp < 0 && ch < 0)
			break;
		if(cp != 9 && cp != 10)
			chars++;
	}
	//Allocate space.
	std::vector<layout> l;
	l.resize(chars);
	itr = 0;
	size_t gtr = 0;
	size_t layout_x = 0;
	size_t layout_y = 0;
	utfstate = utf8_initial_state;
	while(true) {
		int ch = (itr < maxitr) ? static_cast<unsigned char>(string[itr++]) : -1;
		int32_t cp = utf8_parse_byte(ch, utfstate);
		if(cp < 0 && ch < 0)
			break;
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
	}
	return l;
}

template<bool X> void bitmap_font::render(struct framebuffer<X>& scr, int32_t x, int32_t y, const std::string& text,
	premultiplied_color fg, premultiplied_color bg, bool hdbl, bool vdbl) throw()
{
	x += scr.get_origin_x();
	y += scr.get_origin_y();
	uint16_t utfstate = utf8_initial_state;
	size_t itr = 0;
	size_t maxitr = text.length();
	size_t layout_x = 0;
	size_t layout_y = 0;
	size_t swidth = scr.get_width();
	size_t sheight = scr.get_height();
	while(true) {
		int ch = (itr < maxitr) ? static_cast<unsigned char>(text[itr++]) : -1;
		int32_t cp = utf8_parse_byte(ch, utfstate);
		if(cp < 0 && ch < 0)
			break;
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
					typename framebuffer<X>::element_t* r = scr.rowptr(gy + ystart + i) +
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
					typename framebuffer<X>::element_t* r = scr.rowptr(gy + ystart + i) +
						(gx + xstart);
					for(size_t j = 0; j < xlength; j++)
						bg.apply(r[j]);
				}
			layout_x += (hdbl ? 2 : 1) * (g.wide ? 16 : 8);
		}
	}
}

void premultiplied_color::set_palette(unsigned rshift, unsigned gshift, unsigned bshift, bool X) throw()
{
	if(X) {
		uint64_t r = ((orig >> 16) & 0xFF) * 257;
		uint64_t g = ((orig >> 8) & 0xFF) * 257;
		uint64_t b = (orig & 0xFF) * 257;
		uint64_t color = (r << rshift) | (g << gshift) | (b << bshift);
		hiHI = color & 0xFFFF0000FFFFULL;
		loHI = (color & 0xFFFF0000FFFF0000ULL) >> 16;
		hiHI *= (static_cast<uint32_t>(origa) * 256);
		loHI *= (static_cast<uint32_t>(origa) * 256);
	} else {
		uint32_t r = (orig >> 16) & 0xFF;
		uint32_t g = (orig >> 8) & 0xFF;
		uint32_t b = orig & 0xFF;
		uint32_t color = (r << rshift) | (g << gshift) | (b << bshift);
		hi = color & 0xFF00FF;
		lo = (color & 0xFF00FF00) >> 8;
		hi *= origa;
		lo *= origa;
	}
}

template class framebuffer<false>;
template class framebuffer<true>;
template void render_queue::run(struct framebuffer<false>&);
template void render_queue::run(struct framebuffer<true>&);
template void bitmap_font::render(struct framebuffer<false>& scr, int32_t x, int32_t y, const std::string& text,
	premultiplied_color fg, premultiplied_color bg, bool hdbl, bool vdbl) throw();
template void bitmap_font::render(struct framebuffer<true>& scr, int32_t x, int32_t y, const std::string& text,
	premultiplied_color fg, premultiplied_color bg, bool hdbl, bool vdbl) throw();
