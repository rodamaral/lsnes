#include "core/command.hpp"
#include "core/settings.hpp"

setting_var_group lsnes_vset;
setting_var_cache lsnes_vsetc(lsnes_vset);

namespace
{
	setting_var<setting_var_model_path> rompath(lsnes_vset, "rompath", "Paths‣ROMs", "");
	setting_var<setting_var_model_path> moviepath(lsnes_vset, "moviepath", "Paths‣Movies", "");
	setting_var<setting_var_model_path> firmwarepath(lsnes_vset, "firmwarepath", "Paths‣Firmware", "");
	setting_var<setting_var_model_path> slotpath(lsnes_vset, "slotpath", "Paths‣Save slots", "");
}
