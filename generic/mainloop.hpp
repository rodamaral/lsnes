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

#endif