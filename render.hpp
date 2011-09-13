#ifndef _render__hpp__included__
#define _render__hpp__included__

#include <cstdint>
#include <string>
#include <list>
#include <vector>
#include <stdexcept>

/**
 * \brief Low color (32768 colors) screen from buffer.
 */
struct lcscreen
{
/**
 * \brief Create new screen from bsnes output data.
 * 
 * \param mem The output buffer from bsnes.
 * \param hires True if in hires mode (512-wide lines instead of 256-wide).
 * \param interlace True if in interlace mode.
 * \param overscan True if overscan is enabled.
 * \param region True if PAL, false if NTSC.
 */
	lcscreen(const uint16_t* mem, bool hires, bool interlace, bool overscan, bool region) throw();

	lcscreen() throw();
	lcscreen(const uint16_t* mem, uint32_t _width, uint32_t _height) throw();
	lcscreen(const lcscreen& ls) throw(std::bad_alloc);
	lcscreen& operator=(const lcscreen& ls) throw(std::bad_alloc, std::runtime_error);
	void load(const std::vector<char>& data) throw(std::bad_alloc, std::runtime_error);
	void save(std::vector<char>& data) throw(std::bad_alloc);
	void save_png(const std::string& file) throw(std::bad_alloc, std::runtime_error);
	
/**
 * \brief Destructor.
 */
	~lcscreen();

/**
 * \brief True if memory is allocated by new[] and should be freed by the destructor., false otherwise.
 */
	bool user_memory;

/**
 * \brief Memory, 1 element per pixel in left-to-right, top-to-bottom order, 15 low bits of each element used.
 */
	const uint16_t* memory;

/**
 * \brief Number of elements (not bytes) between two successive scanlines.
 */
	uint32_t pitch;

/**
 * \brief Width of image.
 */
	uint32_t width;

/**
 * \brief Height of image.
 */
	uint32_t height;

/**
 * \brief Image allocated size (only valid for user_memory).
 */
	size_t allocated;
};

/**
 * \brief Truecolor modifiable screen.
 */
struct screen
{
/**
 * \brief Create new screen
 * 
 * Creates screen. The screen dimensions are initially 0x0.
 */
	screen() throw();

/**
 * \brief Destructor.
 */
	~screen() throw();
	
/**
 * \brief Set the screen to use specified backing memory.
 * 
 * Sets the backing memory for screen. The specified memory is not freed if screen is reallocated or destroyed.
 * 
 * \param _memory The memory buffer.
 * \param _width Width of screen.
 * \param _height Height of screen.
 * \param _originx X coordinate for origin.
 * \param _originy Y coordinate for origin.
 * \param _pitch Distance in bytes between successive scanlines.
 */
	void set(uint32_t* _memory, uint32_t _width, uint32_t _height, uint32_t _originx, uint32_t _originy,
		uint32_t _pitch) throw();
	
/**
 * \brief Set new size for screen, reallocating backing memory.
 * 
 * Sets the size of the screen. The memory is freed if screen is reallocated or destroyed.
 * 
 * \param _width Width of screen.
 * \param _height Height of screen.
 * \param _originx X coordinate for origin.
 * \param _originy Y coordinate for origin.
 * \param upside_down If true, image is upside down in memory.
 * \throws std::bad_alloc Not enough memory.
 */
	void reallocate(uint32_t _width, uint32_t _height, uint32_t _originx, uint32_t _originy,
		bool upside_down = false) throw(std::bad_alloc);

/**
 * \brief Paint low-color screen into screen.
 * 
 * Paints low-color screen into screen. The upper-left of image will be at origin. Scales the image by given factors.
 * If the image does not fit with specified scale factors, it is clipped.
 * 
 * \param scr The screen to paint.
 * \param hscale Horizontal scale factor.
 * \param vscale Vertical scale factor.
 */
	void copy_from(lcscreen& scr, uint32_t hscale, uint32_t vscale) throw();

/**
 * \brief Get pointer into specified row.
 * 
 * \param row Number of row (must be less than height).
 */
	uint32_t* rowptr(uint32_t row) throw();

/**
 * \brief Backing memory for this screen.
 */
	uint32_t* memory;

/**
 * \brief True if memory is given by user and must not be freed.
 */
	bool user_memory;

/**
 * \brief Width of screen.
 */
	uint32_t width;

/**
 * \brief Height of screen.
 */
	uint32_t height;

/**
 * \brief Distance between lines in bytes.
 */
	size_t pitch;

/**
 * \brief True if image is upside down in memory.
 */
	bool flipped;

/**
 * \brief X-coordinate of origin.
 */
	uint32_t originx;

/**
 * \brief Y-coordinate of origin.
 */
	uint32_t originy;

/**
 * \brief Palette.
 */
	uint32_t palette[32768];

/**
 * \brief Set the palette shifts.
 * 
 * Sets the palette shifts, converting the existing image.
 * 
 * \param rshift Shift for red component.
 * \param gshift Shift for green component.
 * \param bshift Shift for blue component.
 */
	void set_palette(uint32_t rshift, uint32_t gshift, uint32_t bshift) throw();

/**
 * \brief Return a color value.
 * 
 * Returns color value with specified (r,g,b) values (scale 0-255).
 * 
 * \param r Red component.
 * \param g Green component.
 * \param b Blue component.
 * \return color element value.
 */
	uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) throw();

/**
 * \brief Current red component shift.
 */
	uint32_t active_rshift;

/**
 * \brief Current green component shift.
 */
	uint32_t active_gshift;

/**
 * \brief Current blue component shift.
 */
	uint32_t active_bshift;
private:
	screen(const screen&);
	screen& operator=(const screen&);
};

/**
 * \brief Base class for objects to render.
 */
struct render_object
{
/**
 * \brief Destructor.
 */
	virtual ~render_object() throw();

/**
 * \brief Draw the object.
 * 
 * \param scr The screen to draw it on.
 */
	virtual void operator()(struct screen& scr) throw() = 0;
};

/**
 * \brief Queue of render operations.
 */
struct render_queue
{
/**
 * \brief Add object to render queue.
 * 
 * Adds new object to render queue. The object must be allocated by new.
 * 
 * \param obj The object to add
 * \throws std::bad_alloc Not enough memory.
 */
	void add(struct render_object& obj) throw(std::bad_alloc);

/**
 * \brief Apply all objects in order.
 * 
 * Applies all objects in the queue in order, freeing them in progress.
 * 
 * \param scr The screen to apply queue to.
 */
	void run(struct screen& scr) throw();

/**
 * \brief Clear the queue.
 * 
 * Frees all objects in the queue.
 * 
 */
	void clear() throw();

/**
 * \brief Destructor.
 */
	~render_queue() throw();
private:
	std::list<struct render_object*> q;
};

/**
 * \brief Render object rendering given text.
 */
struct render_object_text : public render_object
{
/**
 * \brief Constructor.
 * 
 * \param _x The x position to render to (relative to origin).
 * \param _y The y position to render to (relative to origin).
 * \param _text The text to render.
 * \param _fg Foreground color.
 * \param _fgalpha Foreground alpha (0-256).
 * \param _bg Background color.
 * \param _bgalpha Background alpha (0-256).
 */
	render_object_text(int32_t _x, int32_t _y, const std::string& _text, uint32_t _fg = 0xFFFFFFFFU,
		uint16_t _fgalpha = 255, uint32_t _bg = 0, uint16_t _bgalpha = 0) throw(std::bad_alloc);


	~render_object_text() throw();
/**
 * \brief Draw the text.
 */
	void operator()(struct screen& scr) throw();
private:
	int32_t x;
	int32_t y;
	uint32_t fg;
	uint16_t fgalpha;
	uint32_t bg;
	uint16_t bgalpha;
	std::string text;
};

/**
 * \brief Read font data for glyph.
 * 
 * \param codepoint Code point of glyph.
 * \param x X position to render into.
 * \param y Y position to render into.
 * \param orig_x X position at start of row.
 * \param next_x X position for next glyph is written here.
 * \param next_y Y position for next glyph is written here.
 * \return Two components: First is width of character, second is its offset in font data (0 if blank glyph).
 */
std::pair<uint32_t, size_t> find_glyph(uint32_t codepoint, int32_t x, int32_t y, int32_t orig_x,
	int32_t& next_x, int32_t& next_y) throw();


/**
 * \brief Render text into screen.
 * 
 * \param _x The x position to render to (relative to origin).
 * \param _y The y position to render to (relative to origin).
 * \param _text The text to render.
 * \param _fg Foreground color.
 * \param _fgalpha Foreground alpha (0-256).
 * \param _bg Background color.
 * \param _bgalpha Background alpha (0-256).
 */
void render_text(struct screen& scr, int32_t _x, int32_t _y, const std::string& _text, uint32_t _fg = 0xFFFFFFFFU,
		uint16_t _fgalpha = 255, uint32_t _bg = 0, uint16_t _bgalpha = 0) throw(std::bad_alloc);

#endif
