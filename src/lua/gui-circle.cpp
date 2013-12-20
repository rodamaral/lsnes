#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	struct render_object_circle : public framebuffer::object
	{
		render_object_circle(int32_t _x, int32_t _y, uint32_t _radius,
			framebuffer::color _outline, framebuffer::color _fill, uint32_t _thickness) throw()
			: x(_x), y(_y), outline(_outline), fill(_fill)
			{
				radius = _radius;
				radius2 = static_cast<uint64_t>(_radius) * _radius;
				if(_thickness > _radius)
					iradius2 = 0;
				else
					iradius2 = static_cast<uint64_t>(_radius - _thickness) *
						(_radius - _thickness);
			}
		~render_object_circle() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			outline.set_palette(scr);
			fill.set_palette(scr);
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			int32_t xmin = -radius;
			int32_t xmax = radius;
			int32_t ymin = -radius;
			int32_t ymax = radius;
			framebuffer::clip_range(originx, scr.get_width(), x, xmin, xmax);
			framebuffer::clip_range(originy, scr.get_height(), y, ymin, ymax);
			for(int32_t r = ymin; r < ymax; r++) {
				uint64_t pd2 = static_cast<int64_t>(r) * r;
				typename framebuffer::fb<X>::element_t* rptr = scr.rowptr(y + r + originy);
				size_t eptr = x + xmin + originx;
				for(int32_t c = xmin; c < xmax; c++, eptr++) {
					uint64_t fd2 = pd2 + static_cast<int64_t>(c) * c;
					if(fd2 > radius2)
						continue;
					else if(fd2 >= iradius2)
						outline.apply(rptr[eptr]);
					else
						fill.apply(rptr[eptr]);
				}
			}
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		int32_t radius;
		uint64_t radius2;
		uint64_t iradius2;
		framebuffer::color outline;
		framebuffer::color fill;
	};

	lua::fnptr gui_rectangle(lua_func_misc, "gui.circle", [](lua::state& L, const std::string& fname)
		-> int {
		if(!lua_render_ctx)
			return 0;
		int64_t outline = 0xFFFFFFU;
		int64_t fill = -1;
		uint32_t thickness = 1;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		uint32_t radius = L.get_numeric_argument<uint32_t>(3, fname.c_str());
		L.get_numeric_argument<uint32_t>(4, thickness, fname.c_str());
		L.get_numeric_argument<int64_t>(5, outline, fname.c_str());
		L.get_numeric_argument<int64_t>(6, fill, fname.c_str());
		framebuffer::color poutline(outline);
		framebuffer::color pfill(fill);
		lua_render_ctx->queue->create_add<render_object_circle>(x, y, radius, poutline, pfill, thickness);
		return 0;
	});
}
