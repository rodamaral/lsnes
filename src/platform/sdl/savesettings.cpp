#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/zip.hpp"


namespace
{
	void write_configuration(const std::string& cfg)
	{
		std::ofstream cfgfile(cfg.c_str());
		//Joystick axes.
		for(auto i : lsnes_kbd.all_keys()) {
			keyboard_key_axis* j = i->cast_axis();
			if(!j)
				continue;
			auto p = j->get_calibration();
			cfgfile << "set-axis " << i->get_name() << " " << calibration_to_mode(p);
			cfgfile << " minus=" << p.left << " zero=" << p.center << " plus=" << p.right
				<< " tolerance=" << p.nullwidth << std::endl;
		}
		//Settings.
		for(auto i : lsnes_set.get_settings_set()) {
			if(!lsnes_set.is_set(i))
				cfgfile << "unset-setting " << i << std::endl;
			else
				cfgfile << "set-setting " << i << " " << lsnes_set.get(i) << std::endl;
		}
		for(auto i : lsnes_set.get_invalid_values())
			cfgfile << "set-setting " << i.first << " " << i.second << std::endl;
		//Aliases.
		for(auto i : lsnes_cmd.get_aliases()) {
			std::string old_alias_value = lsnes_cmd.get_alias_for(i);
			while(old_alias_value != "") {
				std::string aliasline;
				size_t s = old_alias_value.find_first_of("\n");
				if(s < old_alias_value.length()) {
					aliasline = old_alias_value.substr(0, s);
					old_alias_value = old_alias_value.substr(s + 1);
				} else {
					aliasline = old_alias_value;
					old_alias_value = "";
				}
				cfgfile << "alias-command " << i << " " << aliasline << std::endl;
			}
		}
		//Keybindings.
		for(auto i : keymapper::get_bindings()) {
			std::string i2 = i;
			size_t s = i2.find_first_of("|");
			size_t s2 = i2.find_first_of("/");
			if(s > i2.length() || s2 > s)
				continue;
			std::string key = i2.substr(s + 1);
			std::string mod = i2.substr(0, s2);
			std::string modspec = i2.substr(s2 + 1, s - s2 - 1);
			std::string old_command_value = keymapper::get_command_for(i);
			if(mod != "" || modspec != "")
				cfgfile << "bind-key " << mod << "/" << modspec << " " << key << " "
					<< old_command_value << std::endl;
			else
				cfgfile << "bind-key " << key << " " << old_command_value << std::endl;
		}
	}
}

void lsnes_sdl_save_config()
{
	std::string cfg = get_config_path() + "/lsnes.rc";
	std::string cfgn = cfg + ".new";
	write_configuration(cfg + ".new");
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
	//Grumble, Windows seemingly can't do this atomically.
	remove(cfg.c_str());
#endif
	rename(cfgn.c_str(), cfg.c_str());
	
}