#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "library/globalwrap.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"

#include <map>
#include <sstream>
#include <iostream>

namespace
{
	function_ptr_command<const std::string&> set_setting(lsnes_cmd, "set-setting", "set a setting",
		"Syntax: set-setting <setting> [<value>]\nSet setting to a new value. Omit <value> to set to ''\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)([ \t]+(|[^ \t].*))?", t, "Setting name required.");
			lsnes_set.set(r[1], r[3]);
			messages << "Setting '" << r[1] << "' set to '" << r[3] << "'" << std::endl;
		});

	function_ptr_command<const std::string&> unset_setting(lsnes_cmd, "unset-setting", "unset a setting",
		"Syntax: unset-setting <setting>\nTry to unset a setting. Note that not all settings can be unset\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]*", t, "Expected setting name and nothing else");
			lsnes_set.blank(r[1]);
			messages << "Setting '" << r[1] << "' unset" << std::endl;
		});

	function_ptr_command<const std::string&> get_command(lsnes_cmd, "get-setting", "get value of a setting",
		"Syntax: get-setting <setting>\nShow value of setting\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]*", t, "Expected setting name and nothing else");
			if(lsnes_set.is_set(r[1]))
				messages << "Setting '" << r[1] << "' has value '"
					<< lsnes_set.get(r[1]) << "'" << std::endl;
			else
				messages << "Setting '" << r[1] << "' is unset" << std::endl;
		});

	function_ptr_command<> show_settings(lsnes_cmd, "show-settings", "Show values of all settings",
		"Syntax: show-settings\nShow value of all settings\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : lsnes_set.get_settings_set()) {
				if(!lsnes_set.is_set(i))
					messages << i << ": (unset)" << std::endl;
				else
					messages << i << ": " << lsnes_set.get(i) << std::endl;
			}
		});
}

setting_group lsnes_set;
