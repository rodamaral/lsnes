#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/minmax.hpp"

namespace
{
	int _dx[8] = {-1,-1,0,1,1,1,0,-1};
	int _dy[8] = {0,1,1,1,0,-1,-1,-1};
	struct render_object_arrow : public render_object
	{
		render_object_arrow(int32_t _x, int32_t _y, uint32_t _length, uint32_t _width,
			uint32_t _headwidth, uint32_t _headthickness, int _direction, bool _fill,
			premultiplied_color _color) throw()
			: x(_x), y(_y), length(_length), width(_width), headwidth(_headwidth),
			headthickness(_headthickness), direction(_direction), fill(_fill), color(_color) {}
		~render_object_arrow() throw() {}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
		{
			color.set_palette(scr);
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			auto orange = offsetrange();
			for(int32_t o = orange.first; o < orange.second; o++) {
				int32_t bpx = x + originx + _dx[(direction + 2) & 7] * o;
				int32_t bpy = y + originy + _dy[(direction + 2) & 7] * o;
				int dx = _dx[direction & 7];
				int dy = _dy[direction & 7];
				auto drange = drawrange(o);
				for(unsigned d = drange.first; d < drange.second; d++) {
					int32_t xc = bpx + dx * d;
					int32_t yc = bpy + dy * d;
					if(xc < 0 || xc >= scr.get_width())
						continue;
					if(yc < 0 || yc >= scr.get_height())
						continue;
					color.apply(scr.rowptr(yc)[xc]);
				}
			}
		}
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
		void clone(render_queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		std::pair<int32_t, int32_t> offsetrange()
		{
			int32_t cmin = -static_cast<int32_t>(width / 2);
			int32_t cmax = static_cast<int32_t>((width + 1) / 2);
			int32_t hmin = -static_cast<int32_t>(headwidth / 2);
			int32_t hmax = static_cast<int32_t>((headwidth + 1) / 2);
			return std::make_pair(min(cmin, hmin), max(cmax, hmax));
		}
		std::pair<int32_t, int32_t> drawrange(int32_t offset)
		{
			int32_t cmin = -static_cast<int32_t>(width / 2);
			int32_t cmax = static_cast<int32_t>((width + 1) / 2);
			int32_t hmin = -static_cast<int32_t>(headwidth / 2);
			int32_t hmax = static_cast<int32_t>((headwidth + 1) / 2);
			bool in_center = (offset >= cmin && offset < cmax);
			bool in_head = (offset >= hmin && offset < hmax);
			int32_t minc = std::abs(offset);	//Works for head&tail.
			int32_t maxc = 0;
			if(in_center) maxc = max(maxc, static_cast<int32_t>(length));
			if(in_head) {
				if(fill)
					maxc = max(maxc, hmax);
				else
					maxc = max(maxc, static_cast<int32_t>(minc + headthickness));
			}
			return std::make_pair(minc, maxc);
		}
		int32_t x;
		int32_t y;
		uint32_t length;
		uint32_t width;
		uint32_t headwidth;
		uint32_t headthickness;
		int direction;
		bool fill;
		premultiplied_color color;
	};

	function_ptr_luafun gui_box(lua_func_misc, "gui.arrow", [](lua_state& L, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		uint32_t length = L.get_numeric_argument<int32_t>(3, fname.c_str());
		uint32_t headwidth = L.get_numeric_argument<int32_t>(4, fname.c_str());
		int direction = L.get_numeric_argument<int>(5, fname.c_str());
		int64_t color = 0xFFFFFFU;
		bool fill = false;
		if(L.type(6) == LUA_TBOOLEAN && L.toboolean(6))
			fill = true;
		L.get_numeric_argument<int64_t>(7, color, fname.c_str());
		uint32_t width = 1;
		L.get_numeric_argument<uint32_t>(8, width, fname.c_str());
		uint32_t headthickness = width;
		L.get_numeric_argument<uint32_t>(9, headthickness, fname.c_str());
		lua_render_ctx->queue->create_add<render_object_arrow>(x, y, length, width, headwidth, headthickness,
			direction, fill, color);
		return 0;
	});
	
}
