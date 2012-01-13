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

#endif