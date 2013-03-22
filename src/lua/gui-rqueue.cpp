#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	lua_render_context* saved = NULL;
	lua_render_context* last = NULL;
	bool redirect = false;

	struct render_queue_obj
	{
		render_queue_obj(uint32_t width, uint32_t height) throw()
		{
			lctx.left_gap = std::numeric_limits<uint32_t>::max();
			lctx.right_gap = std::numeric_limits<uint32_t>::max();
			lctx.bottom_gap = std::numeric_limits<uint32_t>::max();
			lctx.top_gap = std::numeric_limits<uint32_t>::max();
			lctx.queue = &rqueue;
			lctx.width = 512;
			lctx.height = 448;
		}
		~render_queue_obj() throw() {}
		lua_render_context* get() { return &lctx; }
	private:
		render_queue rqueue;
		lua_render_context lctx;
	};

	function_ptr_luafun gui_rq_run(LS, "gui.renderq_run", [](lua_state& L, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->object()->get();
			if(ptr->top_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->top_gap = ptr->top_gap;
			if(ptr->right_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->right_gap = ptr->right_gap;
			if(ptr->bottom_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->bottom_gap = ptr->bottom_gap;
			if(ptr->left_gap != std::numeric_limits<uint32_t>::max())
				lua_render_ctx->left_gap = ptr->left_gap;
			lua_render_ctx->queue->copy_from(*ptr->queue);
		} else {
			L.pushstring("Expected RENDERCTX as argument 1 for gui.renderq_run.");
			L.error();
		}
		return 0;
	});

	function_ptr_luafun gui_rq_clear(LS, "gui.renderq_clear", [](lua_state& L, const std::string& fname) -> int {
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->object()->get();
			ptr->top_gap = std::numeric_limits<uint32_t>::max();
			ptr->right_gap = std::numeric_limits<uint32_t>::max();
			ptr->bottom_gap = std::numeric_limits<uint32_t>::max();
			ptr->left_gap = std::numeric_limits<uint32_t>::max();
			ptr->queue->clear();
		} else {
			L.pushstring("Expected RENDERCTX as argument 1 for gui.renderq_clear.");
			L.error();
		}
		return 0;
	});

	function_ptr_luafun gui_rq_new(LS, "gui.renderq_new", [](lua_state& L, const std::string& fname) -> int {
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		lua_class<render_queue_obj>::create(L, x, y);
		return 1;
	});

	function_ptr_luafun gui_rq_set(LS, "gui.renderq_set", [](lua_state& L, const std::string& fname) -> int {
		if(lua_class<render_queue_obj>::is(L, 1)) {
			lua_class<render_queue_obj>::get(L, 1, fname.c_str());
			auto q = lua_class<render_queue_obj>::pin(L, 1, fname.c_str());
			lua_render_context* ptr = q->object()->get();
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
		} else {
			L.pushstring("Expected RENDERCTX or nil as argument 1 for gui.renderq_set.");
			L.error();
		}
		return 0;
	});
}

DECLARE_LUACLASS(render_queue_obj, "RENDERCTX");