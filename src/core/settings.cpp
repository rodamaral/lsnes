#include "core/command.hpp"
#include "core/settings.hpp"
#include "library/settings-cmd-bridge.hpp"

setting_group lsnes_set;
settings_command_bridge cmd_bridge(lsnes_set, lsnes_cmd, "set-setting", "unset-setting", "get-setting",
	"show-settings");
