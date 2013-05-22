#ifndef _mainloop__hpp__included__
#define _mainloop__hpp__included__

#include "rom.hpp"
#include "moviefile.hpp"
#include "movie.hpp"

/**
 * \brief Emulator main loop.
 */
void main_loop(struct loaded_rom& rom, struct moviefile& settings, bool load_has_to_succeed = false)
	throw(std::bad_alloc, std::runtime_error);
std::vector<std::string> get_jukebox_names();
void set_jukebox_names(const std::vector<std::string>& newj);
void update_movie_state();
extern std::string msu1_base_path;

/**
 * Signal that fast rewind operation is needed.
 *
 * Parameter ptr: If NULL, saves a state and calls lua_callback_do_unsafe_rewind() with quicksave, movie and NULL.
 *	If non-NULL, calls lua_callback_do_unsafe_rewind() with movie and this parameter, but without quicksave.
 */
void mainloop_signal_need_rewind(void* ptr);

void set_stop_at_frame(uint64_t frame = 0);
void switch_projects(const std::string& newproj);

#endif
