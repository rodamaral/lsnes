#ifndef _render__hpp__included__
#define _render__hpp__included__


#include <cstdint>
#include <map>
#include <string>
#include <list>
#include <vector>
#include <stdexcept>
#include <iostream>

/**
 * Low color (32768 colors) screen from buffer.
 */
struct lcscreen
{
/**
 * Create new screen from bsnes output data.
 *
 * parameter mem The output buffer from bsnes.
 * parameter hires True if in hires mode (512-wide lines instead of 256-wide).
 * parameter interlace True if in interlace mode.
 * parameter overscan True if overscan is enabled.
 * parameter region True if PAL, false if NTSC.
 */
	lcscreen(const uint32_t* mem, bool hires, bool interlace, bool overscan, bool region) throw();

/**
 * Create new memory-backed screen. The resulting screen can be written to.
 */
	lcscreen() throw();
/**
 * Create new screen with specified contents and size.
 *
 * parameter mem: Memory to use as frame data. 1 element per pixel. Left-to-Right, top-to-bottom order.
 * parameter _width: Width of the screen to create.
 * parameter _height: Height of the screen to create.
 */
	lcscreen(const uint32_t* mem, uint32_t _width, uint32_t _height) throw();
/**
 * Copy the screen.
 *
 * The assigned copy is always writable.
 *
 * parameter ls: The source screen.
 * throws std::bad_alloc: Not enough memory.
 */
	lcscreen(const lcscreen& ls) throw(std::bad_alloc);
/**
 * Assign the screen.
 *
 * parameter ls: The source screen.
 * returns: Reference to target screen.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The target screen is not writable.
 */
	lcscreen& operator=(const lcscreen& ls) throw(std::bad_alloc, std::runtime_error);
/**
 * Load contents of screen.
 *
 * parameter data: The data to load.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The target screen is not writable.
 */
	void load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error);
/**
 * Save contents of screen.
 *
 * parameter data: The vector to write the data to (in format compatible with load()).
 * throws std::bad_alloc: Not enough memory.
 */
	void save(std::vector<char>& data) throw(std::bad_alloc);
/**
 * Save contents of screen as a PNG.
 *
 * parameter file: The filename to save to.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't save the PNG.
 */
	void save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error);

/**
 * Destructor.
 */
	~lcscreen();

/**
 * True if memory is allocated by new[] and should be freed by the destructor., false otherwise. Also signals
 * writablity.
 */
	bool user_memory;

/**
 * Memory, 1 element per pixel in left-to-right, top-to-bottom order, 15 low bits of each element used.
 */
	const uint32_t* memory;

/**
 * Number of elements (not bytes) between two successive scanlines.
 */
	uint32_t pitch;

/**
 * Width of image.
 */
	uint32_t width;

/**
 * Height of image.
 */
	uint32_t height;

/**
 * Image allocated size (only valid for user_memory=true).
 */
	size_t allocated;
};

template<bool X> struct screenelem {};
template<> struct screenelem<false> { typedef uint32_t t; };
template<> struct screenelem<true> { typedef uint64_t t; };

/**
 * Hicolor modifiable screen.
 */
template<bool X>
struct screen
{
	typedef typename screenelem<X>::t element_t;
/**
 * Creates screen. The screen dimensions are initially 0x0.
 */
	screen() throw();

/**
 * Destructor.
 */
	~screen() throw();

/**
 * Sets the backing memory for screen. The specified memory is not freed if screen is reallocated or destroyed.
 *
 * parameter _memory: The memory buffer.
 * parameter _width: Width of screen.
 * parameter _height: Height of screen.
 * parameter _pitch: Distance in bytes between successive scanlines.
 */
	void set(element_t* _memory, uint32_t _width, uint32_t _height, uint32_t _pitch) throw();

/**
 * Sets the size of the screen. The memory is freed if screen is reallocated or destroyed.
 *
 * parameter _width: Width of screen.
 * parameter _height: Height of screen.
 * parameter upside_down: If true, image is upside down in memory.
 * throws std::bad_alloc: Not enough memory.
 */
	void reallocate(uint32_t _width, uint32_t _height, bool upside_down = false) throw(std::bad_alloc);

/**
 * Set origin
 *
 * parameter _originx: X coordinate for origin.
 * parameter _originy: Y coordinate for origin.
 */
	void set_origin(uint32_t _originx, uint32_t _originy) throw();

/**
 * Paints low-color screen into screen. The upper-left of image will be at origin. Scales the image by given factors.
 * If the image does not fit with specified scale factors, it is clipped.
 *
 * parameter scr The screen to paint.
 * parameter hscale Horizontal scale factor.
 * parameter vscale Vertical scale factor.
 */
	void copy_from(lcscreen& scr, uint32_t hscale, uint32_t vscale) throw();

/**
 * Get pointer into specified row.
 *
 * parameter row: Number of row (must be less than height).
 */
	element_t* rowptr(uint32_t row) throw();

/**
 * Set palette. Also converts the image data.
 *
 * parameter r Shift for red component
 * parameter g Shift for green component
 * parameter b Shift for blue component
 */
	void set_palette(uint32_t r, uint32_t g, uint32_t b);

/**
 * Active palette
 */
	element_t* palette;
	uint32_t palette_r;
	uint32_t palette_g;
	uint32_t palette_b;

/**
 * Backing memory for this screen.
 */
	element_t* memory;

/**
 * True if memory is given by user and must not be freed.
 */
	bool user_memory;

/**
 * Width of screen.
 */
	uint32_t width;

/**
 * Height of screen.
 */
	uint32_t height;

/**
 * Distance between lines in bytes.
 */
	size_t pitch;

/**
 * True if image is upside down in memory.
 */
	bool flipped;

/**
 * X-coordinate of origin.
 */
	uint32_t originx;

/**
 * Y-coordinate of origin.
 */
	uint32_t originy;
private:
	screen(const screen<X>&);
	screen& operator=(const screen<X>&);
};

/**
 * Base class for objects to render.
 */
struct render_object
{
/**
 * Destructor.
 */
	virtual ~render_object() throw();

/**
 * Draw the object.
 *
 * parameter scr: The screen to draw it on.
 */
	virtual void operator()(struct screen<false>& scr) throw() = 0;
	virtual void operator()(struct screen<true>& scr) throw() = 0;
};



/**
 * Premultiplied color.
 */
struct premultiplied_color
{
	uint32_t hi;
	uint32_t lo;
	uint64_t hiHI;
	uint64_t loHI;
	uint32_t orig;
	uint16_t origa;
	uint16_t inv;
	uint32_t invHI;

	premultiplied_color() throw()
	{
		hi = lo = 0;
		hiHI = loHI = 0;
		orig = 0;
		origa = 0;
		inv = 256;
		invHI = 65536;
	}

	premultiplied_color(int64_t color) throw()
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
		set_palette(16, 8, 0, false);
		set_palette(32, 16, 0, true);
		//std::cerr << "Color " << color << " -> hi=" << hi << " lo=" << lo << " inv=" << inv << std::endl;
	}
	void set_palette(unsigned rshift, unsigned gshift, unsigned bshift, bool X) throw();
	template<bool X> void set_palette(struct screen<X>& s) throw()
	{
		set_palette(s.palette_r, s.palette_g, s.palette_b, X);
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
		return (((a * invHI + hiHI) >> 16) & 0xFFFF0000FFFFULL) | ((b * invHI + loHI) & 0xFFFF0000FFFF0000ULL);
	}
	void apply(uint64_t& x) throw()
	{
		x = blend(x);
	}
};

#define RENDER_PAGE_SIZE 65500

/**
 * Queue of render operations.
 */
struct render_queue
{
/**
 * Applies all objects in the queue in order.
 *
 * parameter scr: The screen to apply queue to.
 */
	template<bool X> void run(struct screen<X>& scr) throw();

/**
 * Frees all objects in the queue without applying them.
 */
	void clear() throw();

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
 * Constructor.
 */
	render_queue() throw();
/**
 * Destructor.
 */
	~render_queue() throw();
private:
	void add(struct render_object& obj) throw(std::bad_alloc);
	struct node { struct render_object* obj; struct node* next; };
	struct page { char content[RENDER_PAGE_SIZE]; };
	struct node* queue_head;
	struct node* queue_tail;
	size_t memory_allocated;
	size_t pages;
	std::map<size_t, page> memory;
};


/**
 * Clip range inside another.
 *
 * parameter origin: Origin coordinate.
 * parameter size: Dimension size.
 * parameter base: Base coordinate.
 * parameter minc: Minimum coordinate relative to base. Updated.
 * parameter maxc: Maximum coordinate relative to base. Updated.
 */
void clip_range(uint32_t origin, uint32_t size, int32_t base, int32_t& minc, int32_t& maxc) throw();

/**
 * Initialize font data.
 */
void do_init_font();

/**
 * Read font data for glyph.
 *
 * parameter codepoint: Code point of glyph.
 * parameter x: X position to render into.
 * parameter y: Y position to render into.
 * parameter orig_x: X position at start of row.
 * parameter next_x: X position for next glyph is written here.
 * parameter next_y: Y position for next glyph is written here.
 * returns: Two components: First is width of character, second is pointer to font data (NULL if blank glyph).
 */
std::pair<uint32_t, const uint32_t*> find_glyph(uint32_t codepoint, int32_t x, int32_t y, int32_t orig_x,
	int32_t& next_x, int32_t& next_y, bool hdbl = false, bool vdbl = false) throw();

/**
 * Render text into screen.
 *
 * parameter _x: The x position to render to (relative to origin).
 * parameter _y: The y position to render to (relative to origin).
 * parameter _text: The text to render (UTF-8).
 * parameter _fg: Foreground color.
 * parameter _bg: Background color.
 * parameter _hdbl: If true, draw text using double width.
 * parameter _vdbl: If true, draw text using double height.
 * throws std::bad_alloc: Not enough memory.
 */
template<bool X> void render_text(struct screen<X>& scr, int32_t _x, int32_t _y, const std::string& _text,
	premultiplied_color _fg, premultiplied_color _bg, bool _hdbl = false, bool _vdbl = false)
	throw(std::bad_alloc);

#endif
