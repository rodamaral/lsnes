#include "core/settings.hpp"

settingvar::set lsnes_setgrp;

settingvar::supervariable<settingvar::model_path> SET_rompath(lsnes_setgrp, "rompath", "Paths‣ROMs", "");
settingvar::supervariable<settingvar::model_path> SET_moviepath(lsnes_setgrp, "moviepath", "Paths‣Movies",
	"");
settingvar::supervariable<settingvar::model_path> SET_firmwarepath(lsnes_setgrp, "firmwarepath",
	"Paths‣Firmware", "");
settingvar::supervariable<settingvar::model_path> SET_slotpath(lsnes_setgrp, "slotpath", "Paths‣Save slots",
	"");
