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
		//Joystick axis.
		for(auto i : keygroup::get_axis_set()) {
			keygroup* k = keygroup::lookup_by_name(i);
			auto p = k->get_parameters();
			cfgfile << "set-axis " << i << " ";
			switch(p.ktype) {
			case keygroup::KT_DISABLED:		cfgfile << "disabled";		break;
			case keygroup::KT_AXIS_PAIR:		cfgfile << "axis";		break;
			case keygroup::KT_AXIS_PAIR_INVERSE:	cfgfile << "axis-inverse";	break;
			case keygroup::KT_PRESSURE_M0:		cfgfile << "pressure-0";	break;
			case keygroup::KT_PRESSURE_MP:		cfgfile << "pressure-+";	break;
			case keygroup::KT_PRESSURE_0M:		cfgfile << "pressure0-";	break;
			case keygroup::KT_PRESSURE_0P:		cfgfile << "pressure0+";	break;
			case keygroup::KT_PRESSURE_PM:		cfgfile << "pressure+-";	break;
			case keygroup::KT_PRESSURE_P0:		cfgfile << "pressure+0";	break;
			};
			cfgfile << " minus=" << p.cal_left << " zero=" << p.cal_center << " plus=" << p.cal_right
				<< " tolerance=" << p.cal_tolerance << std::endl;
		}
		//Settings.
		for(auto i : setting::get_settings_set()) {
			if(!setting::is_set(i))
				cfgfile << "unset-setting " << i << std::endl;
			else
				cfgfile << "set-setting " << i << " " << setting::get(i) << std::endl;
		}
		//Aliases.
		for(auto i : command::get_aliases()) {
			std::string old_alias_value = command::get_alias_for(i);
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