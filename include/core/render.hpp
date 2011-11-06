#ifndef _render__hpp__included__
#define _render__hpp__included__


#include <cstdint>
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

/**
 * Hicolor modifiable screen.
 */
struct screen
{
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
 * parameter _originx: X coordinate for origin.
 * parameter _originy: Y coordinate for origin.
 * parameter _pitch: Distance in bytes between successive scanlines.
 */
	void set(uint32_t* _memory, uint32_t _width, uint32_t _height, uint32_t _originx, uint32_t _originy,
		uint32_t _pitch) throw();

/**
 * Sets the size of the screen. The memory is freed if screen is reallocated or destroyed.
 *
 * parameter _width: Width of screen.
 * parameter _height: Height of screen.
 * parameter _originx: X coordinate for origin.
 * parameter _originy: Y coordinate for origin.
 * parameter upside_down: If true, image is upside down in memory.
 * throws std::bad_alloc: Not enough memory.
 */
	void reallocate(uint32_t _width, uint32_t _height, uint32_t _originx, uint32_t _originy,
		bool upside_down = false) throw(std::bad_alloc);

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
	uint32_t* rowptr(uint32_t row) throw();

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
	uint32_t* palette;
	uint32_t palette_r;
	uint32_t palette_g;
	uint32_t palette_b;

/**
 * Backing memory for this screen.
 */
	uint32_t* memory;

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

/**
 * Returns color value with specified (r,g,b) values (scale 0-255).
 *
 * parameter r: Red component.
 * parameter g: Green component.
 * parameter b: Blue component.
 * returns: color element value.
 */
	uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) throw();
private:
	screen(const screen&);
	screen& operator=(const screen&);
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
	virtual void operator()(struct screen& scr) throw() = 0;
};



/**
 * Premultiplied color.
 */
struct premultiplied_color
{
	uint32_t hi;
	uint32_t lo;
	uint16_t inv;
	premultiplied_color(int64_t color) throw()
	{
		if(color < 0) {
			//Transparent.
			hi = 0;
			lo = 0;
			inv = 256;
		} else {
			uint32_t c = (color & 0xFFFFFF);
			uint16_t a = 256 - ((color >> 24) & 0xFF);
			hi = c & 0xFF00FF;
			lo = c & 0x00FF00;
			hi *= a;
			lo *= a;
			inv = 256 - a;
		}
		//std::cerr << "Color " << color << " -> hi=" << hi << " lo=" << lo << " inv=" << inv << std::endl;
	}
	uint32_t blend(uint32_t color) throw()
	{
		uint32_t a, b;
		a = color & 0xFF00FF;
		b = color & 0x00FF00;
		return (((a * inv + hi) >> 8) & 0xFF00FF) | (((b * inv + lo) >> 8) & 0x00FF00);
	}
	void apply(uint32_t& x) throw()
	{
		x = blend(x);
	}
};

/**
 * Queue of render operations.
 */
struct render_queue
{
/**
 * Adds new object to render queue. The object must be allocated by new.
 *
 * parameter obj: The object to add
 * throws std::bad_alloc: Not enough memory.
 */
	void add(struct render_object& obj) throw(std::bad_alloc);

/**
 * Applies all objects in the queue in order, freeing them in progress.
 *
 * parameter scr: The screen to apply queue to.
 */
	void run(struct screen& scr) throw();

/**
 * Frees all objects in the queue without applying them.
 */
	void clear() throw();

/**
 * Destructor.
 */
	~render_queue() throw();
private:
	std::list<struct render_object*> q;
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
	int32_t& next_x, int32_t& next_y) throw();

/**
 * Render text into screen.
 *
 * parameter _x: The x position to render to (relative to origin).
 * parameter _y: The y position to render to (relative to origin).
 * parameter _text: The text to render (UTF-8).
 * parameter _fg: Foreground color.
 * parameter _bg: Background color.
 * throws std::bad_alloc: Not enough memory.
 */
void render_text(struct screen& scr, int32_t _x, int32_t _y, const std::string& _text, premultiplied_color _fg,
	premultiplied_color _bg) throw(std::bad_alloc);

#endif
