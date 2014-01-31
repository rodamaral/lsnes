#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"
#include "library/minmax.hpp"

namespace
{
	int _dx[8] =  {-1,-1, 0, 1, 1, 1, 0,-1};
	int _dy[8] =  { 0, 1, 1, 1, 0,-1,-1,-1};
	int _dxn[8] = { 0, 0, 1, 1, 0, 0,-1,-1};
	int _dyn[8] = { 1, 1, 0, 0,-1,-1, 0, 0};
	int _dxp[8] = { 0, 1, 1, 0, 0,-1,-1, 0};
	int _dyp[8] = { 1, 0, 0,-1,-1, 0, 0, 1};
	struct render_object_arrow : public framebuffer::object
	{
		render_object_arrow(int32_t _x, int32_t _y, uint32_t _length, uint32_t _width,
			uint32_t _headwidth, uint32_t _headthickness, int _direction, bool _fill,
			framebuffer::color _color) throw()
			: x(_x), y(_y), length(_length), width(_width), headwidth(_headwidth),
			headthickness(_headthickness), direction(_direction), fill(_fill), color(_color) {}
		~render_object_arrow() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			color.set_palette(scr);
			uint32_t originx = scr.get_origin_x();
			uint32_t originy = scr.get_origin_y();
			auto orange = offsetrange();
			for(int32_t o = orange.first; o < orange.second; o++) {
				int32_t bpx, bpy;
				bpx = x + originx + ((o < 0) ? _dxn[direction & 7] : _dxp[direction & 7]) * o;
				bpy = y + originy + ((o < 0) ? _dyn[direction & 7] : _dyp[direction & 7]) * o;
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
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
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
				if(fill) {
					if(direction & 1) {
						int32_t fedge = hmax - minc;
						maxc = max(maxc, minc + fedge / 2 + 1);
					} else
						maxc = max(maxc, hmax);
				} else
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
		framebuffer::color color;
	};

	lua::fnptr2 gui_box(lua_func_misc, "gui.arrow", [](lua::state& L, lua::parameters& P) -> int {
		int32_t x, y;
		uint32_t length, headwidth, width, headthickness;
		int direction;
		bool fill;
		framebuffer::color color;

		if(!lua_render_ctx) return 0;

		P(x, y, length, headwidth, direction, P.optional(fill, false), P.optional(color, 0xFFFFFF),
			P.optional(width, 1), P.optional2(headthickness, width));

		lua_render_ctx->queue->create_add<render_object_arrow>(x, y, length, width, headwidth, headthickness,
			direction, fill, color);
		return 0;
	});
}
