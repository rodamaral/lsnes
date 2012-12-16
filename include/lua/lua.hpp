#ifndef _lua__hpp__included__
#define _lua__hpp__included__

#include "core/controllerframe.hpp"
#include "core/keymapper.hpp"
#include "core/movie.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"

void init_lua(bool soft = false) throw();
void quit_lua() throw();
void lua_callback_do_paint(struct lua_render_context* ctx, bool non_synthethic) throw();
void lua_callback_do_video(struct lua_render_context* ctx) throw();
void lua_callback_do_input(controller_frame& data, bool subframe) throw();
void lua_callback_do_reset() throw();
void lua_callback_do_frame() throw();
void lua_callback_do_frame_emulated() throw();
void lua_callback_do_rewind() throw();
void lua_callback_do_readwrite() throw();
void lua_callback_startup() throw();
void lua_callback_do_idle() throw();
void lua_callback_do_timer() throw();
void lua_callback_pre_load(const std::string& name) throw();
void lua_callback_err_load(const std::string& name) throw();
void lua_callback_post_load(const std::string& name, bool was_state) throw();
void lua_callback_pre_save(const std::string& name, bool is_state) throw();
void lua_callback_err_save(const std::string& name) throw();
void lua_callback_post_save(const std::string& name, bool is_state) throw();
void lua_callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw();
void lua_callback_quit() throw();
void lua_callback_keyhook(const std::string& key, keyboard_key& p) throw();
void lua_callback_do_unsafe_rewind(const std::vector<char>& save, uint64_t secs, uint64_t ssecs, movie& mov, void* u);

#define LUA_TIMED_HOOK_IDLE 0
#define LUA_TIMED_HOOK_TIMER 1

uint64_t lua_timed_hook(int timer) throw();

extern bool lua_requests_repaint;
extern bool lua_requests_subframe_paint;

#endif
