#ifndef _command__hpp__included__
#define _command__hpp__included__

#include <stdexcept>
#include <string>
#include <set>
#include "library/command.hpp"

extern command::set lsnes_cmds;

void refresh_alias_binds();
void kill_alias_binds();

#endif
