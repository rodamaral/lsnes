#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "library/globalwrap.hpp"
#include "core/keymapper.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"

#include <stdexcept>
#include <stdexcept>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <set>

keyboard lsnes_kbd;
keyboard_mapper lsnes_mapper(lsnes_kbd, lsnes_cmd);

std::string calibration_to_mode(keyboard_axis_calibration p)
{
	if(p.mode == -1) return "disabled";
	if(p.mode == 1 && p.esign_b == 1) return "axis";
	if(p.mode == 1 && p.esign_b == -1) return "axis-inverse";
	if(p.mode == 0 && p.esign_a == -1 && p.esign_b == 0) return "pressure-0";
	if(p.mode == 0 && p.esign_a == -1 && p.esign_b == 1) return "pressure-+";
	if(p.mode == 0 && p.esign_a == 0 && p.esign_b == -1) return "pressure0-";
	if(p.mode == 0 && p.esign_a == 0 && p.esign_b == 1) return "pressure0+";
	if(p.mode == 0 && p.esign_a == 1 && p.esign_b == -1) return "pressure+-";
	if(p.mode == 0 && p.esign_a == 1 && p.esign_b == 0) return "pressure+0";
	return "";
}
