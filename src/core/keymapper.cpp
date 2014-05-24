#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "library/globalwrap.hpp"
#include "core/keymapper.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "core/queue.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <stdexcept>
#include <stdexcept>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <set>

keyboard::invbind_set lsnes_invbinds;
gamepad::set lsnes_gamepads;

namespace
{
	std::map<std::pair<unsigned, unsigned>, keyboard::key*> buttons;
	std::map<std::pair<unsigned, unsigned>, keyboard::key*> axes;
	std::map<std::pair<unsigned, unsigned>, keyboard::key*> hats;
}

void lsnes_gamepads_init()
{
	lsnes_gamepads.set_button_cb([](unsigned jnum, unsigned num, bool val) {
		if(!buttons.count(std::make_pair(jnum, num)))
			return;
		lsnes_instance.iqueue->queue(keypress_info(keyboard::modifier_set(), *buttons[std::make_pair(jnum,
			num)], val));
	});
	lsnes_gamepads.set_hat_cb([](unsigned jnum, unsigned num, unsigned val) {
		if(!hats.count(std::make_pair(jnum, num)))
			return;
		lsnes_instance.iqueue->queue(keypress_info(keyboard::modifier_set(), *hats[std::make_pair(jnum, num)],
			val));
	});
	lsnes_gamepads.set_axis_cb([](unsigned jnum, unsigned num, int16_t val) {
		if(!axes.count(std::make_pair(jnum, num)))
			return;
		lsnes_instance.iqueue->queue(keypress_info(keyboard::modifier_set(), *axes[std::make_pair(jnum, num)],
			val));
	});
	lsnes_gamepads.set_axismode_cb([](unsigned jnum, unsigned num, int mode, double tolerance) {
		if(!axes.count(std::make_pair(jnum, num)))
			return;
		axes[std::make_pair(jnum, num)]->cast_axis()->set_mode(mode, tolerance);
	});
	lsnes_gamepads.set_newitem_cb([](unsigned jnum, unsigned num, int type) {
		if(type == 0) {
			std::string name = (stringfmt() << "joystick" << jnum << "axis" << num).str();
			int mode = lsnes_gamepads[jnum].get_mode(num);
			axes[std::make_pair(jnum, num)] = new keyboard::key_axis(*lsnes_instance.keyboard, name,
				"joystick", mode);
			//Axis.
		} else if(type == 1) {
			std::string name = (stringfmt() << "joystick" << jnum << "button" << num).str();
			buttons[std::make_pair(jnum, num)] = new keyboard::key_key(*lsnes_instance.keyboard, name,
				"joystick");
			//Button.
		} else if(type == 2) {
			std::string name = (stringfmt() << "joystick" << jnum << "hat" << num).str();
			hats[std::make_pair(jnum, num)] = new keyboard::key_hat(*lsnes_instance.keyboard, name,
				"joystick");
			//Hat.
		}
	});
	try {
		auto cfg = zip::readrel(get_config_path() + "/gamepads.json", "");
		std::string _cfg(cfg.begin(), cfg.end());
		JSON::node config(_cfg);
		lsnes_gamepads.load(config);
	} catch(...) {
	}
}

void lsnes_gamepads_deinit()
{
	std::ofstream cfg(get_config_path() + "/gamepads.json");
	JSON::printer_indenting printer;
	if(cfg)
		cfg << lsnes_gamepads.save().serialize(&printer);
}

void cleanup_keymapper()
{
	for(auto i : buttons) delete i.second;
	for(auto i : axes) delete i.second;
	for(auto i : hats) delete i.second;
}

namespace
{
	command::fnptr<> show_joysticks(lsnes_cmds, "show-joysticks", "Show joystick info",
		"Syntax: show-joysticks\nShow joystick info.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			messages << "--------------------------------------------" << std::endl;
			messages << lsnes_gamepads.get_summary() << std::endl;
			messages << "--------------------------------------------" << std::endl;
		});
}
