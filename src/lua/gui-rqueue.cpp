#include "core/framebuffer.hpp"
#include "lua/internal.hpp"
#include "lua/bitmap.hpp"
#include "library/framebuffer.hpp"

namespace
{
	lua_render_context* saved = NULL;
	lua_render_context* last = NULL;
	bool redirect = false;

	struct lua_renderqueue
	{
		lua_renderqueue(lua::state& L, uint32_t width, uint32_t height) throw();
		~lua_renderqueue() throw() {}
		lua_render_context* get() { return &lctx; }
		std::string print()
		{
			size_t s = rqueue.get_object_count();
			return (stringfmt() << s << " " << ((s != 1) ? "objects" : "object")).str();
		}
		static int create(lua::state& L, lua::parameters& P);
		static int setnull(lua::state& L, lua::parameters& P);
		int run(lua::state& L, lua::parameters& P)
		{
			if(!lua_render_ctx) return 0;

			lua_render_context* ptr = get();
			if(ptr->top_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->top_gap = ptr->top_gap;
			if(ptr->right_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->right_gap = ptr->right_gap;
			if(ptr->bottom_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->bottom_gap = ptr->bottom_gap;
			if(ptr->left_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->left_gap = ptr->left_gap;
			lua_render_ctx->queue->copy_from(*ptr->queue);
			return 0;
		}
		int synchronous_repaint(lua::state& L, lua::parameters& P)
		{
			lua::objpin<lua_renderqueue> q;

			P(q);

			synchronous_paint_ctx = &*q;
			redraw_framebuffer();
			synchronous_paint_ctx = NULL;
			return 0;
		}
		int clear(lua::state& L, lua::parameters& P)
		{
			lua_render_context* ptr = get();
			ptr->top_gap = std::numeric_limits<uint32_t>::max();
			ptr->right_gap = std::numeric_limits<uint32_t>::max();
			ptr->bottom_gap = std::numeric_limits<uint32_t>::max();
			ptr->left_gap = std::numeric_limits<uint32_t>::max();
			ptr->queue->clear();
		}
		int set(lua::state& L, lua::parameters& P)
		{
			lua::objpin<lua_renderqueue> q;

			P(q);

			lua_render_context* ptr = q->get();
			if(!redirect || last != lua_render_ctx)
				saved = lua_render_ctx;
			lua_render_ctx = last = ptr;
			redirect = true;
		}
		int render(lua::state& L, lua::parameters& P)
		{
			uint32_t rwidth = lctx.width + rdgap(lctx.left_gap) + rdgap(lctx.right_gap);
			uint32_t rheight = lctx.height + rdgap(lctx.top_gap) + rdgap(lctx.bottom_gap);
			uint32_t xoff = rdgap(lctx.left_gap);
			uint32_t yoff = rdgap(lctx.top_gap);
			framebuffer::fb<false> fb;
			fb.reallocate(rwidth, rheight);
			fb.set_origin(xoff, yoff);
			rqueue.run(fb);
			lua_dbitmap* b = lua::_class<lua_dbitmap>::create(L, rwidth, rheight);
			for(auto y = 0; y < rheight; y++) {
				const uint32_t* rowp = fb.rowptr(y);
				auto rowt = &b->pixels[y * rwidth];
				for(auto x = 0; x < rwidth; x++) {
					uint32_t v = rowp[x];
					uint64_t c = -1;
					if(v >> 24)
						c = ((256 - (v >> 24) - (v >> 31)) << 24) + (v & 0xFFFFFF);
					rowt[x] = c;
				}
			}
			return 1;
		}
	private:
		uint32_t rdgap(uint32_t v)
		{
			return (v + 1) ? v : 0;
		}
		framebuffer::queue rqueue;
		lua_render_context lctx;
	};

	lua_renderqueue::lua_renderqueue(lua::state& L, uint32_t width, uint32_t height) throw()
	{
		lctx.left_gap = std::numeric_limits<uint32_t>::max();
		lctx.right_gap = std::numeric_limits<uint32_t>::max();
		lctx.bottom_gap = std::numeric_limits<uint32_t>::max();
		lctx.top_gap = std::numeric_limits<uint32_t>::max();
		lctx.queue = &rqueue;
		lctx.width = width;
		lctx.height = height;
	}

	int lua_renderqueue::create(lua::state& L, lua::parameters& P)
	{
		int32_t x, y;

		P(x, y);

		lua::_class<lua_renderqueue>::create(L, x, y);
		return 1;
	}

	int lua_renderqueue::setnull(lua::state& L, lua::parameters& P)
	{
		if(redirect && last == lua_render_ctx)
			//If there is valid redirect, undo it.
			lua_render_ctx = saved;
		redirect = false;
		last = NULL;
		saved = NULL;
		return 0;
	}

	lua::_class<lua_renderqueue> class_lua_renderqueue(lua_class_gui, "RENDERCTX", {
		{"new", lua_renderqueue::create},
		{"setnull", lua_renderqueue::setnull},
	}, {
		{"run", &lua_renderqueue::run},
		{"synchronous_repaint", &lua_renderqueue::synchronous_repaint},
		{"clear", &lua_renderqueue::clear},
		{"set", &lua_renderqueue::set},
		{"render", &lua_renderqueue::render},
	});
}

void lua_renderq_run(lua_render_context* ctx, void* _sctx)
{
	lua_renderqueue* sctx = (lua_renderqueue*)_sctx;
	lua_render_context* ptr = sctx->get();
	if(ptr->top_gap != std::numeric_limits<uint32_t>::max())
		ctx->top_gap = ptr->top_gap;
	if(ptr->right_gap != std::numeric_limits<uint32_t>::max())
		ctx->right_gap = ptr->right_gap;
	if(ptr->bottom_gap != std::numeric_limits<uint32_t>::max())
		ctx->bottom_gap = ptr->bottom_gap;
	if(ptr->left_gap != std::numeric_limits<uint32_t>::max())
		ctx->left_gap = ptr->left_gap;
	ctx->queue->copy_from(*ptr->queue);
}
