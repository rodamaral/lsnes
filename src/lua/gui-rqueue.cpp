#include "core/framebuffer.hpp"
#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	lua_render_context* saved = NULL;
	lua_render_context* last = NULL;
	bool redirect = false;

	struct render_queue_obj
	{
		render_queue_obj(lua_state& L, uint32_t width, uint32_t height) throw()
		{
			lctx.left_gap = std::numeric_limits<uint32_t>::max();
			lctx.right_gap = std::numeric_limits<uint32_t>::max();
			lctx.bottom_gap = std::numeric_limits<uint32_t>::max();
			lctx.top_gap = std::numeric_limits<uint32_t>::max();
			lctx.queue = &rqueue;
			lctx.width = width;
			lctx.height = height;
		}
		~render_queue_obj() throw() {}
		lua_render_context* get() { return &lctx; }
		std::string print()
		{
			size_t s = rqueue.get_object_count();
			return (stringfmt() << s << " " << ((s != 1) ? "objects" : "object")).str();
		}
	private:
		framebuffer::queue rqueue;
		lua_render_context lctx;
	};

	function_ptr_luafun gui_rq_run(lua_func_misc, "gui.renderq_run", [](lua_state& L, const std::string& fname)
		-> int {
		if(!lua_render_ctx)
			return 0;
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
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
		} else
			throw std::runtime_error("Expected RENDERCTX as argument 1 for gui.renderq_run.");
		return 0;
	});

	function_ptr_luafun gui_srepaint(lua_func_misc, "gui.synchronous_repaint", [](lua_state& L,
		const std::string& fname) -> int {
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
			synchronous_paint_ctx = &*q;
			redraw_framebuffer();
		} else
			throw std::runtime_error("Expected RENDERCTX as argument 1 for gui.renderq_run.");
		return 0;
	});

	function_ptr_luafun gui_rq_clear(lua_func_misc, "gui.renderq_clear", [](lua_state& L,
		const std::string& fname) -> int {
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->get();
			ptr->top_gap = std::numeric_limits<uint32_t>::max();
			ptr->right_gap = std::numeric_limits<uint32_t>::max();
			ptr->bottom_gap = std::numeric_limits<uint32_t>::max();
			ptr->left_gap = std::numeric_limits<uint32_t>::max();
			ptr->queue->clear();
		} else
			throw std::runtime_error("Expected RENDERCTX as argument 1 for gui.renderq_clear.");
		return 0;
	});

	function_ptr_luafun gui_rq_new(lua_func_misc, "gui.renderq_new", [](lua_state& L, const std::string& fname)
		-> int {
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		lua_class<render_queue_obj>::create(L, x, y);
		return 1;
	});

	function_ptr_luafun gui_rq_set(lua_func_misc, "gui.renderq_set", [](lua_state& L, const std::string& fname)
		-> int {
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->get();
			if(!redirect || last != lua_render_ctx)
				saved = lua_render_ctx;
			lua_render_ctx = last = ptr;
			redirect = true;
		} else if(L.type(1) == LUA_TNIL) {
			if(redirect && last == lua_render_ctx)
				//If there is valid redirect, undo it.
				lua_render_ctx = saved;
			redirect = false;
			last = NULL;
			saved = NULL;
		} else
			throw std::runtime_error("Expected RENDERCTX or nil as argument 1 for gui.renderq_set.");
		return 0;
	});
}

DECLARE_LUACLASS(render_queue_obj, "RENDERCTX");

void lua_renderq_run(lua_render_context* ctx, void* _sctx)
{
	render_queue_obj* sctx = (render_queue_obj*)_sctx;
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
