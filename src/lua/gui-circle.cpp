#include "lua/internal.hpp"
#include "core/render.hpp"

namespace
{
	struct render_object_circle : public render_object
	{
		render_object_circle(int32_t _x, int32_t _y, uint32_t _radius,
			premultiplied_color _outline, premultiplied_color _fill, uint32_t _thickness) throw()
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
		template<bool X> void op(struct screen<X>& scr) throw()
		{
			outline.set_palette(scr);
			fill.set_palette(scr);
			int32_t xmin = -radius;
			int32_t xmax = radius;
			int32_t ymin = -radius;
			int32_t ymax = radius;
			clip_range(scr.originx, scr.width, x, xmin, xmax);
			clip_range(scr.originy, scr.height, y, ymin, ymax);
			for(int32_t r = ymin; r < ymax; r++) {
				uint64_t pd2 = static_cast<int64_t>(r) * r;
				typename screen<X>::element_t* rptr = scr.rowptr(y + r + scr.originy);
				size_t eptr = x + xmin + scr.originx;
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
		void operator()(struct screen<true>& scr) throw()  { op(scr); }
		void operator()(struct screen<false>& scr) throw() { op(scr); }
	private:
		int32_t x;
		int32_t y;
		int32_t radius;
		uint64_t radius2;
		uint64_t iradius2;
		premultiplied_color outline;
		premultiplied_color fill;
	};

	function_ptr_luafun gui_rectangle("gui.circle", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int64_t outline = 0xFFFFFFU;
		int64_t fill = -1;
		uint32_t thickness = 1;
		int32_t x = get_numeric_argument<int32_t>(LS, 1, fname.c_str());
		int32_t y = get_numeric_argument<int32_t>(LS, 2, fname.c_str());
		uint32_t radius = get_numeric_argument<uint32_t>(LS, 3, fname.c_str());
		get_numeric_argument<uint32_t>(LS, 4, thickness, fname.c_str());
		get_numeric_argument<int64_t>(LS, 5, outline, fname.c_str());
		get_numeric_argument<int64_t>(LS, 6, fill, fname.c_str());
		premultiplied_color poutline(outline);
		premultiplied_color pfill(fill);
		lua_render_ctx->queue->add(*new render_object_circle(x, y, radius, poutline, pfill, thickness));
		return 0;
	});
}
