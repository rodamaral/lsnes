#include "framebuffer-font2.hpp"
#include "memorywatch-fb.hpp"
#include "utf8.hpp"
#include "minmax.hpp"

namespace memorywatch
{
namespace
{
	struct fb_object : public framebuffer::object
	{
		struct params
		{
			int64_t x;
			int64_t y;
			bool cliprange_x;
			bool cliprange_y;
			bool alt_origin_x;
			bool alt_origin_y;
			framebuffer::font2* font;
			framebuffer::color fg;
			framebuffer::color bg;
			framebuffer::color halo;
		};
		fb_object(const params& _p, const std::string& _msg);
		~fb_object() throw();
		void operator()(struct framebuffer::fb<false>& scr) throw();
		void operator()(struct framebuffer::fb<true>& scr) throw();
		void clone(framebuffer::queue& q) const throw(std::bad_alloc);
	private:
		template<bool ext> void draw(struct framebuffer::fb<ext>& scr) throw();
		params p;
		std::u32string msg;
	};

	fb_object::fb_object(const fb_object::params& _p, const std::string& _msg)
		: p(_p)
	{
		msg = utf8::to32(_msg);
	}

	fb_object::~fb_object() throw() {}
	void fb_object::operator()(struct framebuffer::fb<false>& scr) throw() { draw(scr); }
	void fb_object::operator()(struct framebuffer::fb<true>& scr) throw() { draw(scr); }

	void fb_object::clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }

	template<bool ext> void fb_object::draw(struct framebuffer::fb<ext>& scr) throw()
	{
		p.x += scr.get_origin_x();
		p.y += scr.get_origin_y();
		if(p.alt_origin_x)
			p.x += scr.get_last_blit_width();
		if(p.alt_origin_y)
			p.y += scr.get_last_blit_height();
		p.fg.set_palette(scr);
		p.bg.set_palette(scr);
		p.halo.set_palette(scr);

		bool has_halo = p.halo;
		//Layout the text.
		int64_t orig_x = p.x;
		int64_t drawx = p.x;
		int64_t drawy = p.y;
		int64_t max_drawx = p.x;
		for(size_t i = 0; i < msg.size();) {
			uint32_t cp = msg[i];
			std::u32string k = p.font->best_ligature_match(msg, i);
			const framebuffer::font2::glyph& glyph = p.font->lookup_glyph(k);
			if(k.length())
				i += k.length();
			else
				i++;
			if(cp == 9) {
				drawx = (drawx + 64) >> 6 << 6;
			} else if(cp == 10) {
				drawx = orig_x;
				drawy += p.font->get_rowadvance();
			} else {
				drawx += glyph.width;
				max_drawx = max(max_drawx, drawx);
			}
		}
		uint64_t width = max_drawx - p.x;
		uint64_t height = drawy - p.y;
		if(has_halo) {
			width += 2;
			height += 2;
		}
		drawx = p.x;
		drawy = p.y;
		orig_x = p.x;
		if(p.cliprange_x) {
			if(drawx < 0)
				drawx = 0;
			else if(drawx + width > scr.get_width())
				drawx = scr.get_width() - width;
		}
		if(p.cliprange_y) {
			if(drawy < 0)
				drawy = 0;
			else if(drawy + height > scr.get_height())
				drawy = scr.get_height() - height;
		}
		if(has_halo) {
			orig_x++;
			drawx++;
			drawy++;
		}
		for(size_t i = 0; i < msg.size();) {
			uint32_t cp = msg[i];
			std::u32string k = p.font->best_ligature_match(msg, i);
			const framebuffer::font2::glyph& glyph = p.font->lookup_glyph(k);
			if(k.length())
				i += k.length();
			else
				i++;
			if(cp == 9) {
				drawx = (drawx + 64) >> 6 << 6;
			} else if(cp == 10) {
				drawx = orig_x;
				drawy += p.font->get_rowadvance();
			} else {
				glyph.render(scr, drawx, drawy, p.fg, p.bg, p.halo);
				drawx += glyph.width;
			}
		}
	}
}

output_fb::output_fb()
{
	font = NULL;
}

output_fb::~output_fb()
{
	if(dtor_cb)
		dtor_cb(*this);
}

void output_fb::set_rqueue(framebuffer::queue& rqueue)
{
	queue = &rqueue;
}

void output_fb::set_dtor_cb(std::function<void(output_fb&)> cb)
{
	dtor_cb = cb;
}

void output_fb::show(const std::string& iname, const std::string& val)
{
	fb_object::params p;
	try {
		if(cond_enable) {
			enabled->reset();
			auto e = enabled->evaluate();
			if(!e.type->toboolean(e.value))
				return;
		}
		pos_x->reset();
		pos_y->reset();
		auto x = pos_x->evaluate();
		auto y = pos_y->evaluate();
		p.x = x.type->tosigned(x.value);
		p.y = y.type->tosigned(y.value);
		p.alt_origin_x = alt_origin_x;
		p.alt_origin_y = alt_origin_y;
		p.cliprange_x = cliprange_x;
		p.cliprange_y = cliprange_y;
		p.font = font;
		p.fg = fg;
		p.bg = bg;
		p.halo = halo;
		queue->create_add<fb_object>(p, val);
	} catch(...) {
	}
}

void output_fb::reset()
{
	enabled->reset();
	pos_x->reset();
	pos_y->reset();
}
}