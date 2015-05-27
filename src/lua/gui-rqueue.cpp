#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/misc.hpp"
#include "lua/internal.hpp"
#include "lua/bitmap.hpp"

namespace
{
	struct lua_renderqueue
	{
		lua_renderqueue(lua::state& L, uint32_t width, uint32_t height) throw();
		static size_t overcommit(uint32_t width, uint32_t height) { return 0; }
		~lua_renderqueue() throw() {}
		lua::render_context* get() { return &lctx; }
		std::string print()
		{
			size_t s = rqueue.get_object_count();
			return (stringfmt() << s << " " << ((s != 1) ? "objects" : "object")).str();
		}
		static int create(lua::state& L, lua::parameters& P);
		static int setnull(lua::state& L, lua::parameters& P);
		int run(lua::state& L, lua::parameters& P)
		{
			auto& core = CORE();
			if(!core.lua2->render_ctx) return 0;

			lua::render_context* ptr = get();
			if(ptr->top_gap != std::numeric_limits<uint32_t>::max())
				core.lua2->render_ctx->top_gap = ptr->top_gap;
			if(ptr->right_gap != std::numeric_limits<uint32_t>::max())
				core.lua2->render_ctx->right_gap = ptr->right_gap;
			if(ptr->bottom_gap != std::numeric_limits<uint32_t>::max())
				core.lua2->render_ctx->bottom_gap = ptr->bottom_gap;
			if(ptr->left_gap != std::numeric_limits<uint32_t>::max())
				core.lua2->render_ctx->left_gap = ptr->left_gap;
			core.lua2->render_ctx->queue->copy_from(*ptr->queue);
			return 0;
		}
		int synchronous_repaint(lua::state& L, lua::parameters& P)
		{
			auto& core = CORE();
			lua::objpin<lua_renderqueue> q;

			P(q);

			core.lua2->synchronous_paint_ctx = &*q;
			core.fbuf->redraw_framebuffer();
			core.lua2->synchronous_paint_ctx = NULL;
			return 0;
		}
		int clear(lua::state& L, lua::parameters& P)
		{
			lua::render_context* ptr = get();
			ptr->top_gap = std::numeric_limits<uint32_t>::max();
			ptr->right_gap = std::numeric_limits<uint32_t>::max();
			ptr->bottom_gap = std::numeric_limits<uint32_t>::max();
			ptr->left_gap = std::numeric_limits<uint32_t>::max();
			ptr->queue->clear();
			return 0;
		}
		int set(lua::state& L, lua::parameters& P)
		{
			auto& core = CORE();
			lua::objpin<lua_renderqueue> q;

			P(q);

			lua::render_context* ptr = q->get();
			if(!core.lua2->renderq_redirect || core.lua2->renderq_last != core.lua2->render_ctx)
				core.lua2->renderq_saved = core.lua2->render_ctx;
			core.lua2->render_ctx = core.lua2->renderq_last = ptr;
			core.lua2->renderq_redirect = true;
			return 0;
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
			for(auto y = 0U; y < rheight; y++) {
				const uint32_t* rowp = fb.rowptr(y);
				auto rowt = &b->pixels[y * rwidth];
				for(auto x = 0U; x < rwidth; x++) {
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
		lua::render_context lctx;
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
		auto& core = CORE();
		if(core.lua2->renderq_redirect && core.lua2->renderq_last == core.lua2->render_ctx)
			//If there is valid redirect, undo it.
			core.lua2->render_ctx = core.lua2->renderq_saved;
		core.lua2->renderq_redirect = false;
		core.lua2->renderq_last = NULL;
		core.lua2->renderq_saved = NULL;
		return 0;
	}

	lua::_class<lua_renderqueue> LUA_class_lua_renderqueue(lua_class_gui, "RENDERCTX", {
		{"new", lua_renderqueue::create},
		{"setnull", lua_renderqueue::setnull},
	}, {
		{"run", &lua_renderqueue::run},
		{"synchronous_repaint", &lua_renderqueue::synchronous_repaint},
		{"clear", &lua_renderqueue::clear},
		{"set", &lua_renderqueue::set},
		{"render", &lua_renderqueue::render},
	}, &lua_renderqueue::print);
}

void lua_renderq_run(lua::render_context* ctx, void* _sctx)
{
	lua_renderqueue* sctx = (lua_renderqueue*)_sctx;
	lua::render_context* ptr = sctx->get();
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
