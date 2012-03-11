#include "core/bsnes.hpp"
#include "interface/core.hpp"
#include <sstream>

std::string emucore_get_version()
{
	std::ostringstream x;
	x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
	return x.str();
}
