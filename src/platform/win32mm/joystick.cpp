#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/joyfun.hpp"

#include <windows.h>
#include <mmsystem.h>
#include <regstr.h>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>

namespace
{
	std::map<unsigned, joystick_model> joysticks;
	std::map<std::pair<unsigned, unsigned>, keygroup*> axes;
	std::map<std::pair<unsigned, unsigned>, keygroup*> buttons;
	std::map<std::pair<unsigned, unsigned>, keygroup*> hats;
	volatile bool quit_signaled;
	volatile bool quit_ack;

	const char* axisnames[] = {"X", "Y", "Z", "Rudder", "U", "V"};
	const char* buttonnames[] = {"Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7", 
		"Button8", "Button9", "Button10", "Button11", "Button12", "Button13", "Button14", "Button15",
		"Button16", "Button17", "Button18", "Button19", "Button20", "Button21", "Button22", "Button23", 
		"Button24", "Button25", "Button26", "Button27", "Button28", "Button29", "Button30", "Button31",
		"Button32"};

	void create_hat(unsigned i, unsigned j = 0)
	{
		unsigned n = joysticks[i].new_hat(j, "POV");
		std::string name = (stringfmt() << "joystick" << i << "hat" << n).str();
		hats[std::make_pair(i, n)] = new keygroup(name, "joystick", keygroup::KT_HAT);
	}

	void create_button(unsigned i, unsigned j)
	{
		unsigned n = joysticks[i].new_button(j, buttonnames[j]);
		std::string name = (stringfmt() << "joystick" << i << "button" << n).str();
		buttons[std::make_pair(i, n)] = new keygroup(name, "joystick", keygroup::KT_KEY);
	}

	void create_axis(unsigned i, unsigned j, unsigned min, unsigned max)
	{
		unsigned n = joysticks[i].new_axis(j, min, max, axisnames[j]);
		std::string name = (stringfmt() << "joystick" << i << "axis" << n).str();
		axes[std::make_pair(i, n)] = new keygroup(name, "joystick", keygroup::KT_AXIS_PAIR);
	}

	void init_joysticks()
	{
		unsigned max_joysticks = joyGetNumDevs();
		if(!max_joysticks)
			return;		//No joystick support.
		for(unsigned i = 0; i < max_joysticks; i++) {
			JOYINFOEX info;
			JOYCAPS caps;
			info.dwSize = sizeof(info);
			info.dwFlags = JOY_RETURNALL;
			if(joyGetPosEx(i, &info) != JOYERR_NOERROR)
				continue;	//Not usable.
			if(joyGetDevCaps(i, &caps, sizeof(caps)) != JOYERR_NOERROR)
				continue;	//Not usable.
			messages << "Joystick #" << i << ": " << caps.szPname << " (by '" << caps.szOEMVxD << "')"
				<< std::endl;
			joystick_model& m = joysticks[i];
			m.name(caps.szPname);
			if(caps.wCaps & JOYCAPS_HASPOV)
				create_hat(i);
			for(unsigned j = 0; j < caps.wNumButtons && j < 32; j++)
				create_button(i, j);
			create_axis(i, 0, caps.wXmin, caps.wXmax);
			create_axis(i, 1, caps.wYmin, caps.wYmax);
			if(caps.wCaps & JOYCAPS_HASZ)	create_axis(i, 2, caps.wZmin, caps.wZmax);
			if(caps.wCaps & JOYCAPS_HASR)	create_axis(i, 3, caps.wRmin, caps.wRmax);
			if(caps.wCaps & JOYCAPS_HASU)	create_axis(i, 4, caps.wUmin, caps.wUmax);
			if(caps.wCaps & JOYCAPS_HASV)	create_axis(i, 5, caps.wVmin, caps.wVmax);
			if(caps.wCaps & JOYCAPS_HASPOV)
				messages << "1 hat, ";
			messages << m.axes() << " axes, " << m.buttons() << " buttons" << std::endl;
		}
	}

	function_ptr_command<> show_joysticks("show-joysticks", "Show joysticks",
		"Syntax: show-joysticks\nShow joystick data.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : joysticks)
				messages << i.second.compose_report(i.first) << std::endl;
		});
}

void joystick_plugin::init() throw()
{
	init_joysticks();
	quit_ack = quit_signaled = false;
}

void joystick_plugin::quit() throw()
{
	quit_signaled = true;
	while(!quit_ack);
	for(auto i : joysticks)	close(i.first);
	for(auto i : axes)	delete i.second;
	for(auto i : buttons)	delete i.second;
	for(auto i : hats)	delete i.second;
	joysticks.clear();
	axes.clear();
	buttons.clear();
	hats.clear();
}

#define POLL_WAIT 20000

void joystick_plugin::thread_fn() throw()
{
	while(!quit_signaled) {
		for(auto i : joysticks) {
			joystick_model& m = joysticks[i.first];
			JOYINFOEX info;
			info.dwSize = sizeof(info);
			info.dwFlags = JOY_RETURNALL;
			if(joyGetPosEx(i.first, &info) != JOYERR_NOERROR)
				continue;	//Not usable.
			m.report_pov(0, info.dwPOV);
			for(unsigned j = 0; j < 32; j++)
				m.report_button(j, (info.dwButtons >> j) & 1);
			m.report_axis(0, info.dwXpos);
			m.report_axis(1, info.dwYpos);
			m.report_axis(2, info.dwZpos);
			m.report_axis(3, info.dwRpos);
			m.report_axis(4, info.dwUpos);
			m.report_axis(5, info.dwVpos);
		}
		short x;
		for(auto i : buttons)
			if(joysticks[i.first.first].button(i.first.second, x))
				platform::queue(keypress(modifier_set(), *i.second, x));
		for(auto i : axes)
			if(joysticks[i.first.first].axis(i.first.second, x))
				platform::queue(keypress(modifier_set(), *i.second, x));
		for(auto i : hats)
			if(joysticks[i.first.first].hat(i.first.second, x))
				platform::queue(keypress(modifier_set(), *i.second, x));
		usleep(POLL_WAIT);
	}
	quit_ack = true;
}

void joystick_plugin::signal() throw()
{
	quit_signaled = true;
}

const char* joystick_plugin::name = "Win32mm joystick plugin";
