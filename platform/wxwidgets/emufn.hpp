#ifndef _wxwdigets_emufn__hpp__included__
#define _wxwdigets_emufn__hpp__included__

#include "rom.hpp"
#include "moviefile.hpp"

void boot_emulator(loaded_rom& rom, moviefile& movie);
void exec_command(const std::string& cmd);

#endif