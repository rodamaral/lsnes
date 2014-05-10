#include "core/instance.hpp"
#include "core/settings.hpp"

emulator_instance::emulator_instance()
	: setcache(lsnes_vset), subtitles(&mlogic)
{
}

emulator_instance lsnes_instance;
