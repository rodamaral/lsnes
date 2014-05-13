#include "core/instance.hpp"
#include "core/settings.hpp"
#include "core/command.hpp"
#include "core/keymapper.hpp"

emulator_instance::emulator_instance()
	: setcache(settings), subtitles(&mlogic), mbranch(&mlogic), mteditor(&mlogic),
	status(status_A, status_B, status_C), mapper(keyboard, command)
{
	status_A.valid = false;
	status_B.valid = false;
	status_C.valid = false;
	command.add_set(lsnes_cmds);
	mapper.add_invbind_set(lsnes_invbinds);
	settings.add_set(lsnes_setgrp);
}

emulator_instance lsnes_instance;

emulator_instance& CORE()
{
	return lsnes_instance;
}
