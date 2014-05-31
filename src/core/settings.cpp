#include "core/settings.hpp"

settingvar::set lsnes_setgrp;

namespace
{
	settingvar::supervariable<settingvar::model_path> rompath(lsnes_setgrp, "rompath", "Paths‣ROMs", "");
	settingvar::supervariable<settingvar::model_path> moviepath(lsnes_setgrp, "moviepath", "Paths‣Movies", "");
	settingvar::supervariable<settingvar::model_path> firmwarepath(lsnes_setgrp, "firmwarepath",
		"Paths‣Firmware", "");
	settingvar::supervariable<settingvar::model_path> slotpath(lsnes_setgrp, "slotpath", "Paths‣Save slots", "");
}
