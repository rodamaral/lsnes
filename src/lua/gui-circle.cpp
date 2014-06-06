#include "core/instance.hpp"
#include "lua/internal.hpp"
#include "library/framebuffer.hpp"
#include "library/range.hpp"
#include "library/lua-framebuffer.hpp"

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
			uint32_t oX = x + scr.get_origin_x();
			uint32_t oY = y + scr.get_origin_y();
			range bX = (range::make_w(scr.get_width()) - oX) & range::make_b(-radius, radius + 1);
			range bY = (range::make_w(scr.get_height()) - oY) & range::make_b(-radius, radius + 1);
			for(uint32_t r = bY.low(); r != bY.high(); r++) {
				uint64_t pd2 = static_cast<int64_t>(r) * r;
				typename framebuffer::fb<X>::element_t* rptr = scr.rowptr(oY + r);
				size_t eptr = oX + bX.low();
				for(uint32_t c = bX.low(); c != bX.high(); c++, eptr++) {
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

	int circle(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		int32_t x, y;
		uint32_t radius, thickness;
		framebuffer::color poutline, pfill;

		if(!core.lua2->render_ctx) return 0;

		P(x, y, radius, P.optional(thickness, 1), P.optional(poutline, 0xFFFFFFU), P.optional(pfill, -1));

		core.lua2->render_ctx->queue->create_add<render_object_circle>(x, y, radius, poutline, pfill,
			thickness);
		return 0;
	}

	lua::functions circle_fns(lua_func_misc, "gui", {
		{"circle", circle},
	});
}
