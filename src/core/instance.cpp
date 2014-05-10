#include "core/instance.hpp"
#include "core/settings.hpp"

emulator_instance::emulator_instance()
	: setcache(lsnes_vset), subtitles(&mlogic), mbranch(&mlogic), mteditor(&mlogic)
{
}

emulator_instance lsnes_instance;

emulator_instance& CORE()
{
	return lsnes_instance;
}