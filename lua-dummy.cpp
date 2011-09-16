#include "lua.hpp"

struct lua_State { int x; };
lua_function::lua_function(const std::string& name) throw(std::bad_alloc) {}
lua_function::~lua_function() throw() {}
void lua_callback_do_paint(struct lua_render_context* ctx) throw() {}
void lua_callback_do_video(struct lua_render_context* ctx) throw() {}
void lua_callback_do_input(controls_t& data, bool subframe) throw() {}
void lua_callback_do_reset() throw() {}
void lua_callback_do_readwrite() throw() {}
void lua_callback_startup() throw() {}
void lua_callback_pre_load(const std::string& name) throw() {}
void lua_callback_err_load(const std::string& name) throw() {}
void lua_callback_post_load(const std::string& name, bool was_state) throw() {}
void lua_callback_pre_save(const std::string& name, bool is_state) throw() {}
void lua_callback_err_save(const std::string& name) throw() {}
void lua_callback_post_save(const std::string& name, bool is_state) throw() {}
void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw() {}
void lua_callback_quit() throw() {}
void init_lua(window* win) throw() {}
bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;
