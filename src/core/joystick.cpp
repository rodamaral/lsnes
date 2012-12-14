#include "core/command.hpp"
#include "core/joystick.hpp"
#include "core/window.hpp"
#include "library/string.hpp"
#include "library/joyfun.hpp"
#include <map>
#include <string>

namespace
{
	std::map<uint64_t, joystick_model> joysticks;
	std::map<uint64_t, unsigned> joynumbers;
	std::map<std::pair<uint64_t, unsigned>, keygroup*> axes;
	std::map<std::pair<uint64_t, unsigned>, keygroup*> buttons;
	std::map<std::pair<uint64_t, unsigned>, keygroup*> hats;
	unsigned joystick_count = 0;

	function_ptr_command<> show_joysticks(lsnes_cmd, "show-joysticks", "Show joysticks",
		"Syntax: show-joysticks\nShow joystick data.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			messages << "Driver: " << joystick_plugin::name << std::endl;
			messages << "--------------------------------------" << std::endl;
			for(auto i : joynumbers)
				messages << joysticks[i.first].compose_report(i.second) << std::endl;
			messages << "--------------------------------------" << std::endl;
		});
}

void joystick_create(uint64_t id, const std::string& xname)
{
	joynumbers[id] = joystick_count++;
	joysticks[id].name(xname);
}

void joystick_quit()
{
	for(auto i : axes)
		delete i.second;
	for(auto i : buttons)
		delete i.second;
	for(auto i : hats)
		delete i.second;
	joysticks.clear();
	joynumbers.clear();
	axes.clear();
	buttons.clear();
	hats.clear();
	joystick_count = 0;
}

void joystick_new_axis(uint64_t jid, uint64_t id, int64_t minv, int64_t maxv, const std::string& xname,
	enum keygroup::type atype)
{
	if(!joysticks.count(jid))
		return;
	unsigned jnum = joynumbers[jid];
	unsigned n = joysticks[jid].new_axis(id, minv, maxv, xname);
	std::string name = (stringfmt() << "joystick" << jnum << "axis" << n).str();
	axes[std::make_pair(jid, n)] = new keygroup(name, "joystick", atype);
}

void joystick_new_button(uint64_t jid, uint64_t id, const std::string& xname)
{
	if(!joysticks.count(jid))
		return;
	unsigned jnum = joynumbers[jid];
	unsigned n = joysticks[jid].new_button(id, xname);
	std::string name = (stringfmt() << "joystick" << jnum << "button" << n).str();
	buttons[std::make_pair(jid, n)] = new keygroup(name, "joystick", keygroup::KT_KEY);
}

void joystick_new_hat(uint64_t jid, uint64_t id_x, uint64_t id_y, int64_t min_dev, const std::string& xname_x,
	const std::string& xname_y)
{
	if(!joysticks.count(jid))
		return;
	unsigned jnum = joynumbers[jid];
	unsigned n = joysticks[jid].new_hat(id_x, id_y, min_dev, xname_x, xname_y);
	std::string name = (stringfmt() << "joystick" << jnum << "hat" << n).str();
	hats[std::make_pair(jid, n)] = new keygroup(name, "joystick", keygroup::KT_HAT);
}

void joystick_new_hat(uint64_t jid, uint64_t id, const std::string& xname)
{
	if(!joysticks.count(jid))
		return;
	unsigned jnum = joynumbers[jid];
	unsigned n = joysticks[jid].new_hat(id, xname);
	std::string name = (stringfmt() << "joystick" << jnum << "hat" << n).str();
	hats[std::make_pair(jid, n)] = new keygroup(name, "joystick", keygroup::KT_HAT);
}

void joystick_report_axis(uint64_t jid, uint64_t id, int64_t value)
{
	if(!joysticks.count(jid))
		return;
	joysticks[jid].report_axis(id, value);
}

void joystick_report_button(uint64_t jid, uint64_t id, bool value)
{
	if(!joysticks.count(jid))
		return;
	joysticks[jid].report_button(id, value);
}

void joystick_report_pov(uint64_t jid, uint64_t id, int angle)
{
	if(!joysticks.count(jid))
		return;
	joysticks[jid].report_pov(id, angle);
}

void joystick_message(uint64_t jid)
{
	if(!joysticks.count(jid))
		return;
	messages << "Found '" << joysticks[jid].name() << "' (" << joysticks[jid].buttons() << " buttons, "
		<< joysticks[jid].axes() << " axes, " << joysticks[jid].hats() << " hats)" << std::endl;
}

std::set<uint64_t> joystick_set()
{
	std::set<uint64_t> x;
	for(auto i : joynumbers)
		x.insert(i.first);
	return x;
}

void joystick_flush()
{
	short x;
	for(auto i : buttons)
		if(joysticks[i.first.first].button(i.first.second, x))
			platform::queue(keypress(keyboard_modifier_set(), *i.second, x));
	for(auto i : axes)
		if(joysticks[i.first.first].axis(i.first.second, x))
			platform::queue(keypress(keyboard_modifier_set(), *i.second, x));
	for(auto i : hats)
		if(joysticks[i.first.first].hat(i.first.second, x))
			platform::queue(keypress(keyboard_modifier_set(), *i.second, x));
}
