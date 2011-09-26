#ifndef _lua__hpp__included__
#define _lua__hpp__included__

#include "render.hpp"
#include "controllerdata.hpp"

struct lua_State;

/**
 * Function implemented in C++ exported to Lua.
 */
class lua_function
{
public:
/**
 * Register function.
 */
	lua_function(const std::string& name) throw(std::bad_alloc);
/**
 * Unregister function.
 */
	virtual ~lua_function() throw();

/**
 * Invoke function.
 */
	virtual int invoke(lua_State* L) = 0;
protected:
	std::string fname;
};

/**
 * Register function pointer as lua function.
 */
class function_ptr_luafun : public lua_function
{
public:
/**
 * Register.
 */
	function_ptr_luafun(const std::string& name, int (*_fn)(lua_State* L, const std::string& fname))
		: lua_function(name)
	{
		fn = _fn;
	}
/**
 * Invoke function.
 */
	int invoke(lua_State* L)
	{
		return fn(L, fname);
	}
private:
	int (*fn)(lua_State* L, const std::string& fname);
};

struct lua_render_context
{
	uint32_t left_gap;
	uint32_t right_gap;
	uint32_t top_gap;
	uint32_t bottom_gap;
	struct render_queue* queue;
	uint32_t width;
	uint32_t height;
};

void init_lua() throw();
void lua_callback_do_paint(struct lua_render_context* ctx) throw();
void lua_callback_do_video(struct lua_render_context* ctx) throw();
void lua_callback_do_input(controls_t& data, bool subframe) throw();
void lua_callback_do_reset() throw();
void lua_callback_do_readwrite() throw();
void lua_callback_startup() throw();
void lua_callback_pre_load(const std::string& name) throw();
void lua_callback_err_load(const std::string& name) throw();
void lua_callback_post_load(const std::string& name, bool was_state) throw();
void lua_callback_pre_save(const std::string& name, bool is_state) throw();
void lua_callback_err_save(const std::string& name) throw();
void lua_callback_post_save(const std::string& name, bool is_state) throw();
void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw();
void lua_callback_quit() throw();

extern bool lua_requests_repaint;
extern bool lua_requests_subframe_paint;

#endif
