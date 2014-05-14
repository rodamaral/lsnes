#ifndef _lua__hpp__included__
#define _lua__hpp__included__

#include <string>
#include <map>
#include "core/controllerframe.hpp"
#include "library/movie.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-framebuffer.hpp"

namespace keyboard
{
	class key;
}

void init_lua() throw();
void quit_lua() throw();
void lua_callback_do_paint(struct lua::render_context* ctx, bool non_synthethic) throw();
void lua_callback_do_video(struct lua::render_context* ctx, bool& kill_frame, uint32_t& hscl, uint32_t& vscl) throw();
void lua_callback_do_input(controller_frame& data, bool subframe) throw();
void lua_callback_do_reset() throw();
void lua_callback_do_frame() throw();
void lua_callback_do_frame_emulated() throw();
void lua_callback_do_rewind() throw();
void lua_callback_do_readwrite() throw();
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
void lua_callback_keyhook(const std::string& key, keyboard::key& p) throw();
void lua_callback_do_unsafe_rewind(const std::vector<char>& save, uint64_t secs, uint64_t ssecs, movie& mov, void* u);
bool lua_callback_do_button(uint32_t port, uint32_t controller, uint32_t index, const char* type);
void lua_callback_movie_lost(const char* what);
void lua_callback_do_latch(std::list<std::string>& args);
void lua_run_startup_scripts();
void lua_add_startup_script(const std::string& file);


#define LUA_TIMED_HOOK_IDLE 0
#define LUA_TIMED_HOOK_TIMER 1

uint64_t lua_timed_hook(int timer) throw();
const std::map<std::string, std::u32string>& get_lua_watch_vars();

extern bool lua_requests_repaint;
extern bool lua_requests_subframe_paint;

#endif
