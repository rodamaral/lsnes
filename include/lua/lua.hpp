#ifndef _lua__hpp__included__
#define _lua__hpp__included__

#include <string>
#include <map>
#include <list>
#include "core/controllerframe.hpp"
#include "library/command.hpp"
#include "library/movie.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-base.hpp"
#include "library/lua-framebuffer.hpp"
#include "library/memtracker.hpp"
#include "library/settingvar.hpp"

namespace command { class group; }
namespace keyboard { class key; }

#define LUA_TIMED_HOOK_IDLE 0
#define LUA_TIMED_HOOK_TIMER 1

void init_lua() throw();
void quit_lua() throw();

struct lua_state
{
	lua_state(lua::state& _L, command::group& _command, settingvar::group& settings);
	~lua_state();

	lua::state::callback_list* on_paint;
	lua::state::callback_list* on_video;
	lua::state::callback_list* on_reset;
	lua::state::callback_list* on_frame;
	lua::state::callback_list* on_rewind;
	lua::state::callback_list* on_idle;
	lua::state::callback_list* on_timer;
	lua::state::callback_list* on_frame_emulated;
	lua::state::callback_list* on_readwrite;
	lua::state::callback_list* on_startup;
	lua::state::callback_list* on_pre_load;
	lua::state::callback_list* on_post_load;
	lua::state::callback_list* on_err_load;
	lua::state::callback_list* on_pre_save;
	lua::state::callback_list* on_post_save;
	lua::state::callback_list* on_err_save;
	lua::state::callback_list* on_input;
	lua::state::callback_list* on_snoop;
	lua::state::callback_list* on_snoop2;
	lua::state::callback_list* on_button;
	lua::state::callback_list* on_quit;
	lua::state::callback_list* on_keyhook;
	lua::state::callback_list* on_movie_lost;
	lua::state::callback_list* on_pre_rewind;
	lua::state::callback_list* on_post_rewind;
	lua::state::callback_list* on_set_rewind;
	lua::state::callback_list* on_latch;

	void callback_do_paint(struct lua::render_context* ctx, bool non_synthethic) throw();
	void callback_do_video(struct lua::render_context* ctx, bool& kill_frame, uint32_t& hscl, uint32_t& vscl)
		throw();
	void callback_do_input(portctrl::frame& data, bool subframe) throw();
	void callback_do_reset() throw();
	void callback_do_frame() throw();
	void callback_do_frame_emulated() throw();
	void callback_do_rewind() throw();
	void callback_do_readwrite() throw();
	void callback_do_idle() throw();
	void callback_do_timer() throw();
	void callback_pre_load(const text& name) throw();
	void callback_err_load(const text& name) throw();
	void callback_post_load(const text& name, bool was_state) throw();
	void callback_pre_save(const text& name, bool is_state) throw();
	void callback_err_save(const text& name) throw();
	void callback_post_save(const text& name, bool is_state) throw();
	void callback_snoop_input(uint32_t port, uint32_t controller, uint32_t index, short value) throw();
	void callback_quit() throw();
	void callback_keyhook(const text& key, keyboard::key& p) throw();
	void callback_do_unsafe_rewind(movie& mov, void* u);
	bool callback_do_button(uint32_t port, uint32_t controller, uint32_t index, const char* type);
	void callback_movie_lost(const char* what);
	void callback_do_latch(std::list<text>& args);
	void run_startup_scripts();
	void add_startup_script(const text& file);

	uint64_t timed_hook(int timer) throw();
	const std::map<text, text>& get_watch_vars();

	void do_eval_lua(const text& c) throw(std::bad_alloc);
	void do_run_lua(const text& c) throw(std::bad_alloc);
	void run_sysrc_lua(bool rerun);

	bool requests_repaint;
	bool requests_subframe_paint;
	lua::render_context* render_ctx;
	portctrl::frame* input_controllerdata;
	bool* kill_frame;
	void* synchronous_paint_ctx;
	uint32_t* hscl;
	uint32_t* vscl;
	bool* veto_flag;
	std::set<text> hooked_keys;
	uint64_t idle_hook_time;
	uint64_t timer_hook_time;
	lua::render_context* renderq_saved;
	lua::render_context* renderq_last;
	bool renderq_redirect;
	void set_memory_limit(size_t max_mb);

	std::list<text> startup_scripts;
	std::map<text, text> watch_vars;
private:
	void do_reset();
	void do_evaluate(const text& a);
	bool run_lua_fragment() throw(std::bad_alloc);
	template<typename... T> bool run_callback(lua::state::callback_list& list, T... args);
	void run_synchronous_paint(struct lua::render_context* ctx);
	lua::state& L;
	command::group& command;
	bool recursive_flag;
	const char* luareader_fragment;
	command::_fnptr<> resetcmd;
	command::_fnptr<const text&> evalcmd;
	command::_fnptr<const text&> evalcmd2;
	command::_fnptr<command::arg_filename> runcmd;
	struct _listener : public settingvar::listener
	{
		_listener(settingvar::group& group, lua_state& _obj)
			: obj(_obj), grp(group)
		{
			group.add_listener(*this);
		}
		~_listener() throw()
		{
			grp.remove_listener(*this);
		}
		void on_setting_change(settingvar::group& grp, const settingvar::base& val);
		lua_state& obj;
		settingvar::group& grp;
	};
	_listener listener;
};

#endif
