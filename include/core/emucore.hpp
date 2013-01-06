#ifndef _emucore__hpp__included__
#define _emucore__hpp__included__

#include <map>
#include <list>
#include <cstdlib>
#include <string>
#include <set>
#include <vector>
#include "library/framebuffer.hpp"
#include "library/controller-data.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"

//Valid port types.
extern port_type* core_port_types[];
//Emulator core.
extern core_core* emulator_core;

#endif
