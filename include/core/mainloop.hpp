#ifndef _mainloop__hpp__included__
#define _mainloop__hpp__included__

#include "settings.hpp"
#include "rom.hpp"
#include "moviefile.hpp"
#include "movie.hpp"

/**
 * \brief Emulator main loop.
 */
void main_loop(struct loaded_rom& rom, struct moviefile& settings, bool load_has_to_succeed = false)
	throw(std::bad_alloc, std::runtime_error);
std::vector<text> get_jukebox_names();
void set_jukebox_names(const std::vector<text>& newj);
void init_main_callbacks();
extern text msu1_base_path;

/**
 * Signal that fast rewind operation is needed.
 *
 * Parameter ptr: If NULL, saves a state and calls lua_callback_do_unsafe_rewind() with quicksave, movie and NULL.
 *	If non-NULL, calls lua_callback_do_unsafe_rewind() with movie and this parameter, but without quicksave.
 */
void mainloop_signal_need_rewind(void* ptr);

void set_stop_at_frame(uint64_t frame = 0);
void switch_projects(const text& newproj);
void close_rom();
void load_new_rom(const romload_request& req);
void reload_current_rom();
void do_break_pause();
void convert_break_to_pause();

extern settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> movie_dflt_binary;
extern settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> save_dflt_binary;

#endif
