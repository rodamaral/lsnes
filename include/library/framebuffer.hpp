#ifndef _library__framebuffer__hpp__included__
#define _library__framebuffer__hpp__included__

#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include "framebuffer-pixfmt.hpp"
#include "threads.hpp"
#include "memtracker.hpp"

namespace framebuffer
{
template<bool X> struct elem {};
template<> struct elem<false> { typedef uint32_t t; };
template<> struct elem<true> { typedef uint64_t t; };

extern unsigned default_shift_r;
extern unsigned default_shift_g;
extern unsigned default_shift_b;

/**
 * Pixel format auxillary palette.
 */
template<bool X>
struct auxpalette
{
	typedef typename elem<X>::t element_t;
	uint8_t rshift;			//Red shift.
	uint8_t gshift;			//Green shift.
	uint8_t bshift;			//Blue shift.
	std::vector<element_t> pcache;	//Palette cache.
};

/**
 * Game framebuffer information.
 */
struct info
{
/**
 * Pixel format of framebuffer.
 */
	pixfmt* type;
/**
 * The physical memory backing the framebuffer.
 */
	char* mem;
/**
 * Physical width of framebuffer.
 */
	size_t physwidth;
/**
 * Physical height of framebuffer.
 */
	size_t physheight;
/**
 * Physical stride of framebuffer (in bytes).
 */
	size_t physstride;
/**
 * Visible width of framebuffer.
 */
	size_t width;
/**
 * Visible height of framebuffer.
 */
	size_t height;
/**
 * Visible stride of framebuffer (in bytes).
 */
	size_t stride;
/**
 * Visible X offset of framebuffer.
 */
	size_t offset_x;
/**
 * Visible Y offset of framebuffer.
 */
	size_t offset_y;
};

template<bool X> struct framebuffer;

/**
 * Raw framebuffer.
 *
 * This framebuffer is in system format, and can either be backed either by temporary buffer or memory buffer.
 *
 * Any copying only preserves the visible part.
 */
struct raw
{
/**
 * Create a new framebuffer backed by temporary buffer.
 *
 * The resulting framebuffer is read-only.
 *
 * Parameter info: The framebuffer info.
 */
	raw(const info& finfo) throw(std::bad_alloc);
/**
 * Create a new framebuffer backed by memory buffer.
 *
 * The resulting framebuffer can be written to.
 */
	raw() throw(std::bad_alloc);
/**
 * Copy a framebuffer.
 *
 * The resulting copy is writable.
 *
 * Parameter f: The framebuffer.
 */
	raw(const raw& f) throw(std::bad_alloc);
/**
 * Assign a framebuffer.
 *
 * Parameter f: The framebuffer.
 * Throws std::runtime_error: The target framebuffer is not writable.
 */
	raw& operator=(const raw& f) throw(std::bad_alloc, std::runtime_error);
/**
 * Load contents of framebuffer.
 *
 * parameter data: The data to load.
 * throws std::runtime_error: The target framebuffer is not writable.
 */
	void load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error);
/**
 * Save contents of framebuffer.
 *
 * parameter data: The vector to write the data to (in format compatible with load()).
 */
	void save(std::vector<char>& data) throw(std::bad_alloc);
/**
 * Save contents of framebuffer as a PNG.
 *
 * parameter file: The filename to save to.
 * throws std::runtime_error: Can't save the PNG.
 */
	void save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error);
/**
 * Get width.
 *
 * Returns: The width.
 */
	size_t get_width() const throw();
/**
 * Get height.
 *
 * Returns: The height.
 */
	size_t get_height() const throw();
/**
 * Get stride.
 */
	size_t get_stride() const throw();
/**
 * Get starting address.
 */
	unsigned char* get_start() const throw();
/**
 * Get pixel format.
 */
	pixfmt* get_format() const throw();
/**
 * Destructor.
 */
	~raw();
private:
	bool user_memory;		//True if allocated in user memory, false if aliases framebuffer.
	char* addr;			//Address of framebuffer start.
	pixfmt* fmt;		//Format of framebuffer.
	size_t width;			//Width of framebuffer.
	size_t height;			//Height of framebuffer.
	size_t stride;			//Stride in pixels.
	size_t allocated;		//Amount of memory allocated (only meaningful if user_memory=true).
	template<bool X> friend class fb;
};


struct color;

/**
 * Rendered framebuffer.
 *
 * This framebuffer is in RGB32/RGB64 format, and is always backed by memory buffer.
 */
template<bool X>
struct fb
{
	typedef typename elem<X>::t element_t;
/**
 * Creates framebuffer. The framebuffer dimensions are initially 0x0.
 */
	fb() throw();

/**
 * Destructor.
 */
	~fb() throw();

/**
 * Sets the backing memory for framebuffer. The specified memory is not freed if framebuffer is reallocated or
 * destroyed.
 *
 * parameter _memory: The memory buffer.
 * parameter _width: Width of framebuffer.
 * parameter _height: Height of framebuffer.
 * parameter _pitch: Distance in bytes between successive scanlines (in pixels).
 *
 * Note: The _pitch should be multiple of 4, and _memory aligned to 16 bytes.
 */
	void set(element_t* _memory, size_t _width, size_t _height, size_t _pitch) throw();

/**
 * Sets the size of the framebuffer. The memory is freed if framebuffer is reallocated or destroyed.
 *
 * The pitch of resulting framebuffer is the smallest possible.
 *
 * parameter _width: Width of framebuffer.
 * parameter _height: Height of framebuffer.
 * parameter upside_down: If true, image is upside down in memory.
 * throws std::bad_alloc: Not enough memory.
 */
	void reallocate(size_t _width, size_t _height, bool upside_down = false) throw(std::bad_alloc);

/**
 * Set origin
 *
 * parameter _originx: X coordinate for origin.
 * parameter _originy: Y coordinate for origin.
 */
	void set_origin(size_t _originx, size_t _originy) throw();
/**
 * Get X origin.
 *
 * Returns: The X origin.
 */
	size_t get_origin_x() const throw();
/**
 * Get Y origin.
 *
 * Returns: The Y origin.
 */
	size_t get_origin_y() const throw();
/**
 * Paints raw framebuffer into framebuffer. The upper-left of image will be at origin. Scales the image by given
 * factors. If the image does not fit with specified scale factors, it is clipped.
 *
 * parameter scr The framebuffer to paint.
 * parameter hscale Horizontal scale factor.
 * parameter vscale Vertical scale factor.
 */
	void copy_from(raw& scr, size_t hscale, size_t vscale) throw();

/**
 * Get pointer into specified row.
 *
 * parameter row: Number of row (must be less than height).
 * Returns: Pointer into row data.
 */
	element_t* rowptr(size_t row) throw();
/**
 * Get pointer into specified row.
 *
 * parameter row: Number of row (must be less than height).
 * Returns: Pointer into row data.
 */
	const element_t* rowptr(size_t row) const throw();
/**
 * Set palette. Also converts the image data.
 *
 * parameter r Shift for red component
 * parameter g Shift for green component
 * parameter b Shift for blue component
 */
	void set_palette(uint32_t r, uint32_t g, uint32_t b) throw(std::bad_alloc);
/**
 * Get stride of image.
 *
 * Returns: The stride.
 */
	size_t get_stride() const throw() { return stride; }
/**
 * Get width of image.
 *
 * Returns: The width.
 */
	size_t get_width() const throw() { return width; }
/**
 * Get height of image.
 *
 * Returns: The height.
 */
	size_t get_height() const throw() { return height; }
/**
 * Get last blit width
 *
 * Returns: The width.
 */
	size_t get_last_blit_width() const throw() { return last_blit_w; }
/**
 * Get last blit height
 *
 * Returns: The height.
 */
	size_t get_last_blit_height() const throw() { return last_blit_h; }
/**
 * Get R palette offset.
 */
	uint8_t get_palette_r() const throw();
/**
 * Get G palette offset.
 */
	uint8_t get_palette_g() const throw();
/**
 * Get B palette offset.
 */
	uint8_t get_palette_b() const throw();
private:
	fb(const fb& f);
	fb& operator=(const fb& f);
	size_t width;		//Width of framebuffer.
	size_t height;		//Height of framebuffer.
	size_t stride;		//Stride in pixels.
	size_t offset_x;	//X offset.
	size_t offset_y;	//Y offset.
	size_t last_blit_w;	//Width of last blit.
	size_t last_blit_h;	//Height of last blit.
	element_t* mem;		//The memory of framebuffer.
	pixfmt* current_fmt;	//Current format of framebuffer.
	auxpalette<X> auxpal;	//Aux palette.
	bool user_mem;		//True if internal memory is used.
	bool upside_down;	//Upside down flag.
	uint8_t active_rshift;			//Red shift.
	uint8_t active_gshift;			//Green shift.
	uint8_t active_bshift;			//Blue shift.
	friend struct color;
};

struct queue;

/**
 * Base class for objects to render.
 */
struct object
{
/**
 * Constructor.
 */
	object() throw();
/**
 * Destructor.
 */
	virtual ~object() throw();
/**
 * Kill object function. If it returns true, kill the request. Default is to return false.
 */
	virtual bool kill_request(void* obj) throw();
/**
 * Return true if myobj and killobj are equal and not NULL.
 */
	bool kill_request_ifeq(void* myobj, void* killobj);
/**
 * Draw the object.
 *
 * parameter scr: The screen to draw it on.
 */
	virtual void operator()(struct fb<false>& scr) throw() = 0;
	virtual void operator()(struct fb<true>& scr) throw() = 0;
/**
 * Clone the object.
 */
	virtual void clone(struct queue& q) const throw(std::bad_alloc) = 0;
};

/**
 * Premultiplied color.
 */
struct color
{
	uint32_t hi;
	uint32_t lo;
	uint64_t hiHI;
	uint64_t loHI;
	uint32_t orig;
	uint16_t origa;
	uint16_t inv;
	uint32_t invHI;

	operator bool() const throw() { return (origa != 0); }
	bool operator!() const throw() { return (origa == 0); }

	color() throw()
	{
		hi = lo = 0;
		hiHI = loHI = 0;
		orig = 0;
		origa = 0;
		inv = 256;
		invHI = 65536;
	}

	color(int64_t color) throw()
	{
		if(color < 0) {
			//Transparent.
			orig = 0;
			origa = 0;
			inv = 256;
		} else {
			orig = color & 0xFFFFFF;
			origa = 256 - ((color >> 24) & 0xFF);;
			inv = 256 - origa;
		}
		invHI = 256 * static_cast<uint32_t>(inv);
		set_palette(default_shift_r, default_shift_g, default_shift_b, false);
		set_palette(default_shift_r << 1, default_shift_g << 1, default_shift_b << 1, true);
		//std::cerr << "Color " << color << " -> hi=" << hi << " lo=" << lo << " inv=" << inv << std::endl;
	}
	color(const std::string& color) throw(std::bad_alloc, std::runtime_error);
	static std::string stringify(int64_t number);
	void set_palette(unsigned rshift, unsigned gshift, unsigned bshift, bool X) throw();
	template<bool X> void set_palette(struct fb<X>& s) throw()
	{
		set_palette(s.active_rshift, s.active_gshift, s.active_bshift, X);
	}
	uint32_t blend(uint32_t color) throw()
	{
		uint32_t a, b;
		a = color & 0xFF00FF;
		b = (color & 0xFF00FF00) >> 8;
		return (((a * inv + hi) >> 8) & 0xFF00FF) | ((b * inv + lo) & 0xFF00FF00);
	}
	void apply(uint32_t& x) throw()
	{
		x = blend(x);
	}
	uint64_t blend(uint64_t color) throw()
	{
		uint64_t a, b;
		a = color & 0xFFFF0000FFFFULL;
		b = (color & 0xFFFF0000FFFF0000ULL) >> 16;
		return (((a * invHI + hiHI) >> 16) & 0xFFFF0000FFFFULL) | ((b * invHI + loHI) &
			0xFFFF0000FFFF0000ULL);
	}
	void apply(uint64_t& x) throw()
	{
		x = blend(x);
	}
	int64_t asnumber() const throw()
	{
		if(!origa)
			return -1;
		else
			return orig | ((uint32_t)(256 - origa) << 24);
	}
};

int64_t color_rotate_hue(int64_t color, int count, int steps);
int64_t color_adjust_saturation(int64_t color, double adjust);
int64_t color_adjust_lightness(int64_t color, double adjust);

/**
 * Base color name.
 */
struct basecolor
{
	basecolor(const std::string& name, int64_t value);
};

/**
 * Color modifier.
 */
struct color_mod
{
	color_mod(const std::string& name, std::function<void(int64_t&)> fn);
};

/**
 * Bitmap font (8x16).
 */
struct font
{
	/**
	 * Bitmap font glyph.
	 */
	struct glyph
	{
		bool wide;		//If set, 16 wide instead of 8.
		uint32_t* data;		//Glyph data. Bitpacked with element padding between rows.
		size_t offset;		//Glyph offset.
		uint32_t get_width() const throw() { return wide ? 16 : 8; }
		uint32_t get_height() const throw() { return 16; }
		bool read_pixel(uint32_t x, uint32_t y) const throw()
		{
			if(wide) {
				return ((data[y >> 1] >> (31 - (((y & 1) << 4) + x))) & 1) != 0;
			} else {
				return ((data[y >> 2] >> (31 - (((y & 3) << 3) + x))) & 1) != 0;
			}
		}
	};

	/**
	 * Bitmap font layout.
	 */
	struct layout
	{
		size_t x;		//X position.
		size_t y;		//Y position.
		const glyph* dglyph;	//The glyph itself.
	};
/**
 * Constructor.
 */
	font() throw(std::bad_alloc);
/**
 * Load a .hex format font.
 *
 * Parameter data: The font data.
 * Parameter size: The font data size in bytes.
 * Throws std::runtime_error: Bad font data.
 */
	void load_hex(const char* data, size_t size) throw(std::bad_alloc, std::runtime_error);
/**
 * Locate glyph.
 *
 * Parameter glyph: Number of glyph to locate.
 * Returns: Glyph parameters.
 */
	const glyph& get_glyph(uint32_t glyph) throw();
/**
 * Get metrics of string.
 *
 * Parameter string: The string to get metrics of.
 * Returns: A pair. First element is width of string, the second is height of string.
 */
	std::pair<size_t, size_t> get_metrics(const std::string& string, uint32_t xalign, bool xdbl, bool ydbl)
		throw();
/**
 * Layout a string.
 *
 * Parameter string: The string to get layout of.
 * Returns: String layout.
 */
	std::vector<layout> dolayout(const std::string& string) throw(std::bad_alloc);
/**
 * Get width of string.
 *
 * Parameter string: The string to get width of.
 * Returns: The width.
 */
	uint32_t get_width(const std::string& string);
/**
 * Get set of all glyph numbers.
 */
	std::set<uint32_t> get_glyphs_set();
/**
 * Get bad glyph.
 */
	const glyph& get_bad_glyph() throw();
/**
 * Render string to framebuffer.
 *
 * Parameter framebuffer: The framebuffer to render on.
 * Parameter x: The x-coordinate to start from.
 * Parameter y: The y-coordinate to start from.
 * Parameter text: The text to render.
 * Parameter fg: The foreground color.
 * Parameter bg: The background color.
 * Parameter hdbl: If set, double width horizontally.
 * Parameter vdbl: If set, double height vertically.
 */
	template<bool X> void render(struct fb<X>& scr, int32_t x, int32_t y, const std::string& text,
		color fg, color bg, bool hdbl, bool vdbl) throw();
/**
 * Call function on every glyph.
 *
 * Parameter str: The string to call on.
 * Parameter alignx: The x alignment.
 * Parameter cb: The callback to call.
 */
	void for_each_glyph(const std::string& str, uint32_t alignx, bool xdbl, bool ydbl,
		std::function<void(uint32_t x, uint32_t y, const glyph& g, bool xdbl, bool ydbl)> cb);
/**
 * Render to bitmap.
 *
 * Parameter buf: The bufer to render on.
 * Parameter stride: The stride on buffer.
 * Parameter str: The string to render.
 * Parameter alignx: The x alignment.
 * Parameter hdbl: If set, double width horizontally.
 * Parameter vdbl: If set, double height vertically.
 */
	void render(uint8_t* buf, size_t stride, const std::string& str, uint32_t alignx, bool hdbl, bool vdbl);
private:
	glyph bad_glyph;
	uint32_t bad_glyph_data[4];
	std::map<uint32_t, glyph> glyphs;
	size_t tabstop;
	std::vector<uint32_t> memory;
	void load_hex_glyph(const char* data, size_t size) throw(std::bad_alloc, std::runtime_error);
};


#define RENDER_PAGE_SIZE 65500

/**
 * Queue of render operations.
 */
struct queue
{
/**
 * Applies all objects in the queue in order.
 *
 * parameter scr: The screen to apply queue to.
 */
	template<bool X> void run(struct fb<X>& scr) throw();

/**
 * Frees all objects in the queue without applying them.
 */
	void clear() throw();
/**
 * Call kill_request on all objects in queue.
 */
	void kill_request(void* obj) throw();
/**
 * Get memory from internal allocator.
 */
	void* alloc(size_t block) throw(std::bad_alloc);

/**
 * Call object constructor on internal memory.
 */
	template<class T, typename... U> void create_add(U... args)
	{
		add(*new(alloc(sizeof(T))) T(args...));
	}
/**
 * Copy objects from another render queue.
 */
	void copy_from(queue& q) throw(std::bad_alloc);
/**
 * Helper for clone.
 */
	template<typename T> void clone_helper(const T* obj)
	{
		create_add<T>(*obj);
	}
/**
 * Get number of objects.
 */
	size_t get_object_count()
	{
		size_t c = 0;
		struct node* n = queue_head;
		while(n != NULL) {
			n = n->next;
			c++;
		}
		return c;
	}
/**
 * Constructor.
 */
	queue(memtracker& _tracker) throw();
/**
 * Destructor.
 */
	~queue() throw();
private:
	void add(struct object& obj) throw(std::bad_alloc);
	struct node { struct object* obj; struct node* next; bool killed; };
	struct page { char content[RENDER_PAGE_SIZE]; };
	struct node* queue_head;
	struct node* queue_tail;
	size_t memory_allocated;
	size_t pages;
	threads::lock display_mutex; //Synchronize display and kill.
	std::map<size_t, page> memory;
	memtracker& tracker;
};

/**
 * Drop every fourth byte of specified buffer.
 *
 * Parameter dest: Destination buffer, should be 16-byte aligned.
 * Parameter src: Source buffer, should be 16-byte aligned.
 * Parameter units: Number of 4 byte units to copy. Must be multiple of 4.
 */
void copy_drop4(uint8_t* dest, const uint32_t* src, size_t units);

/**
 * Drop every fourth byte of specified buffer and swap first and third.
 *
 * Parameter dest: Destination buffer, should be 16-byte aligned.
 * Parameter src: Source buffer, should be 16-byte aligned.
 * Parameter units: Number of 4 byte units to copy. Must be multiple of 4.
 */
void copy_drop4s(uint8_t* dest, const uint32_t* src, size_t units);

/**
 * Swap every first and third byte out of four
 *
 * Parameter dest: Destination buffer, should be 16-byte aligned.
 * Parameter src: Source buffer, should be 16-byte aligned.
 * Parameter units: Number of 4 byte units to copy. Must be multiple of 4.
 */
void copy_swap4(uint8_t* dest, const uint32_t* src, size_t units);

/**
 * Drop every fourth word of specified buffer.
 *
 * Parameter dest: Destination buffer, should be 16-byte aligned.
 * Parameter src: Source buffer, should be 16-byte aligned.
 * Parameter units: Number of 8 byte units to copy. Must be multiple of 2.
 */
void copy_drop4(uint16_t* dest, const uint64_t* src, size_t units);

/**
 * Drop every fourth byte of specified buffer and swap first and third.
 *
 * Parameter dest: Destination buffer, should be 16-byte aligned.
 * Parameter src: Source buffer, should be 16-byte aligned.
 * Parameter units: Number of 8 byte units to copy. Must be multiple of 2.
 */
void copy_drop4s(uint16_t* dest, const uint64_t* src, size_t units);

/**
 * Swap every first and third byte out of four
 *
 * Parameter dest: Destination buffer, should be 16-byte aligned.
 * Parameter src: Source buffer, should be 16-byte aligned.
 * Parameter units: Number of 8 byte units to copy. Must be multiple of 2.
 */
void copy_swap4(uint16_t* dest, const uint64_t* src, size_t units);
}


#endif
