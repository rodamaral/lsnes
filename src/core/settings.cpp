#include "core/command.hpp"
#include "core/settings.hpp"

settingvar::group lsnes_vset;
settingvar::cache lsnes_vsetc(lsnes_vset);

namespace
{
	settingvar::variable<settingvar::model_path> rompath(lsnes_vset, "rompath", "Paths‣ROMs", "");
	settingvar::variable<settingvar::model_path> moviepath(lsnes_vset, "moviepath", "Paths‣Movies", "");
	settingvar::variable<settingvar::model_path> firmwarepath(lsnes_vset, "firmwarepath", "Paths‣Firmware", "");
	settingvar::variable<settingvar::model_path> slotpath(lsnes_vset, "slotpath", "Paths‣Save slots", "");
}
