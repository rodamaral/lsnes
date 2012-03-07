#include "lua/lua.hpp"

#ifdef NO_LUA
struct lua_State { int x; };
lua_function::lua_function(const std::string& name) throw(std::bad_alloc) {}
lua_function::~lua_function() throw() {}
void lua_callback_do_paint(struct lua_render_context* ctx, bool nonsynth) throw() {}
void lua_callback_do_video(struct lua_render_context* ctx) throw() {}
void lua_callback_do_input(controller_frame& data, bool subframe) throw() {}
void lua_callback_do_reset() throw() {}
void lua_callback_do_frame() throw() {}
void lua_callback_do_frame_emulated() throw() {}
void lua_callback_do_rewind() throw() {}
void lua_callback_do_idle() throw() {}
void lua_callback_do_timer() throw() {}
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
void lua_callback_keyhook(const std::string& key, const struct keygroup::parameters& p) throw() {}
void init_lua() throw() {}
void quit_lua() throw() {}
uint64_t lua_timed_hook(int timer) throw() { return 0x7EFFFFFFFFFFFFFFULL; }

bool lua_requests_repaint = false;
bool lua_requests_subframe_paint = false;
bool lua_supported = false;
uint64_t lua_idle_hook_time = 0x7EFFFFFFFFFFFFFFULL;
uint64_t lua_timer_hook_time = 0x7EFFFFFFFFFFFFFFULL;

#endif

char SYMBOL_3263572374236473826587375832743243264346;
