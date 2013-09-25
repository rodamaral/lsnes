#ifndef _romguess__hpp__included__
#define _romguess__hpp__included__

#include "core/romloader.hpp"
#include "core/window.hpp"

void try_guess_roms(rom_request& req);
void record_filehash(const std::string& file, uint64_t prefix, const std::string& hash);
std::string try_to_guess_rom(const std::string& hint, const std::string& hash, const std::string& xhash,
	core_type& type, unsigned i);

#endif

