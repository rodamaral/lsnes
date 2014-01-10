#ifndef _library__memorywatch_fb__hpp__included__
#define _library__memorywatch_fb__hpp__included__

#include "framebuffer.hpp"
#include "memorywatch.hpp"
#include "mathexpr.hpp"
#include "framebuffer-font2.hpp"

struct memorywatch_output_fb : public memorywatch_item_printer
{
	memorywatch_output_fb();
	~memorywatch_output_fb();
	void set_rqueue(framebuffer::queue& rqueue);
	void set_dtor_cb(std::function<void(memorywatch_output_fb&)> cb);
	void show(const std::string& iname, const std::string& val);
	void reset();
	bool cond_enable;
	gcroot_pointer<mathexpr> enabled;
	gcroot_pointer<mathexpr> pos_x;
	gcroot_pointer<mathexpr> pos_y;
	bool alt_origin_x;
	bool alt_origin_y;
	bool cliprange_x;
	bool cliprange_y;
	framebuffer::font2* font;
	framebuffer::color fg;
	framebuffer::color bg;
	framebuffer::color halo;
	//State variables.
	framebuffer::queue* queue;
	std::function<void(memorywatch_output_fb&)> dtor_cb;
};

#endif
