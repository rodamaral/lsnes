#include "core/framebuffer.hpp"
#include "lua/internal.hpp"
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
		int run(lua::state& L, const std::string& fname)
		{
			if(!lua_render_ctx)
				return 0;
			auto q = lua::_class<lua_renderqueue>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->get();
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
		int synchronous_repaint(lua::state& L, const std::string& fname)
		{
			auto q = lua::_class<lua_renderqueue>::pin(L, 1, fname.c_str());
			synchronous_paint_ctx = &*q;
			redraw_framebuffer();
			synchronous_paint_ctx = NULL;
			return 0;
		}
		int clear(lua::state& L, const std::string& fname)
		{
			auto q = lua::_class<lua_renderqueue>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->get();
			ptr->top_gap = std::numeric_limits<uint32_t>::max();
			ptr->right_gap = std::numeric_limits<uint32_t>::max();
			ptr->bottom_gap = std::numeric_limits<uint32_t>::max();
			ptr->left_gap = std::numeric_limits<uint32_t>::max();
			ptr->queue->clear();
		}
		int set(lua::state& L, const std::string& fname)
		{
			auto q = lua::_class<lua_renderqueue>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->get();
			if(!redirect || last != lua_render_ctx)
				saved = lua_render_ctx;
			lua_render_ctx = last = ptr;
			redirect = true;
		}
	private:
		framebuffer::queue rqueue;
		lua_render_context lctx;
	};

	lua_renderqueue::lua_renderqueue(lua::state& L, uint32_t width, uint32_t height) throw()
	{
		lua::objclass<lua_renderqueue>().bind_multi(L, {
			{"run", &lua_renderqueue::run},
			{"synchronous_repaint", &lua_renderqueue::synchronous_repaint},
			{"clear", &lua_renderqueue::clear},
			{"set", &lua_renderqueue::set},
		});
		lctx.left_gap = std::numeric_limits<uint32_t>::max();
		lctx.right_gap = std::numeric_limits<uint32_t>::max();
		lctx.bottom_gap = std::numeric_limits<uint32_t>::max();
		lctx.top_gap = std::numeric_limits<uint32_t>::max();
		lctx.queue = &rqueue;
		lctx.width = width;
		lctx.height = height;
	}

	lua::fnptr gui_rq_run(lua_func_misc, "gui.renderq_run", [](lua::state& L, const std::string& fname)
		-> int {
		if(!lua_render_ctx)
			return 0;
		if(lua::_class<lua_renderqueue>::is(L, 1)) {
			return lua::_class<lua_renderqueue>::get(L, 1, fname.c_str())->run(L, fname);
		} else
			throw std::runtime_error("Expected RENDERCTX as argument 1 for gui.renderq_run.");
		return 0;
	});

	lua::fnptr gui_srepaint(lua_func_misc, "gui.synchronous_repaint", [](lua::state& L,
		const std::string& fname) -> int {
		if(lua::_class<lua_renderqueue>::is(L, 1)) {
			return lua::_class<lua_renderqueue>::get(L, 1, fname.c_str())->synchronous_repaint(L, fname);
		} else
			throw std::runtime_error("Expected RENDERCTX as argument 1 for gui.renderq_run.");
		return 0;
	});

	lua::fnptr gui_rq_clear(lua_func_misc, "gui.renderq_clear", [](lua::state& L,
		const std::string& fname) -> int {
		if(lua::_class<lua_renderqueue>::is(L, 1)) {
			return lua::_class<lua_renderqueue>::get(L, 1, fname.c_str())->clear(L, fname);
		} else
			throw std::runtime_error("Expected RENDERCTX as argument 1 for gui.renderq_clear.");
		return 0;
	});

	lua::fnptr gui_rq_new(lua_func_misc, "gui.renderq_new", [](lua::state& L, const std::string& fname)
		-> int {
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		lua::_class<lua_renderqueue>::create(L, x, y);
		return 1;
	});

	lua::fnptr gui_rq_set(lua_func_misc, "gui.renderq_set", [](lua::state& L, const std::string& fname)
		-> int {
		if(lua::_class<lua_renderqueue>::is(L, 1)) {
			return lua::_class<lua_renderqueue>::get(L, 1, fname.c_str())->set(L, fname);
		} else if(L.type(1) == LUA_TNIL || L.type(1) == LUA_TNONE) {
			if(redirect && last == lua_render_ctx)
				//If there is valid redirect, undo it.
				lua_render_ctx = saved;
			redirect = false;
			last = NULL;
			saved = NULL;
		} else
			throw std::runtime_error("Expected RENDERCTX or nil as argument 1 for " + fname);
		return 0;
	});

	lua::_class<lua_renderqueue> class_lua_renderqueue("RENDERCTX");
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
