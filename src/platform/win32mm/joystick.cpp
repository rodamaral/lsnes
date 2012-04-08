#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"

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
	std::set<keygroup*> keygroups;
	std::map<std::pair<unsigned, unsigned>, keygroup*> buttons;
	std::map<std::pair<unsigned, unsigned>, keygroup*> axes;
	std::map<unsigned, keygroup*> hats;
	std::map<std::pair<unsigned, unsigned>, short> lbuttons;
	std::map<std::pair<unsigned, unsigned>, short> laxes;
	std::map<unsigned, short> lhats;
	std::set<unsigned> joysticks;
	std::map<unsigned, JOYCAPS> capabilities;
	volatile bool quit_signaled;
	volatile bool quit_ack;

	void create_hat(unsigned i)
	{
		std::string n = (stringfmt() << "joystick" << i << "hat").str();
		keygroup* k = new keygroup(n, "joystick", keygroup::KT_HAT);
		hats[i] = k;
	}

	void create_button(unsigned i, unsigned j)
	{
		std::string n = (stringfmt() << "joystick" << i << "button" << j).str();
		keygroup* k = new keygroup(n, "joystick", keygroup::KT_KEY);
		buttons[std::make_pair(i, j)] = k;
	}

	void create_axis(unsigned i, unsigned j, unsigned min, unsigned max)
	{
		std::string n = (stringfmt() << "joystick" << i << "axis" << j).str();
		keygroup* k;
		k = new keygroup(n, "joystick", keygroup::KT_AXIS_PAIR);
		axes[std::make_pair(i, j)] = k;
	}

	void read_axis(unsigned i, unsigned j, unsigned pos, unsigned pmin, unsigned pmax)
	{
		auto key = std::make_pair(i, j);
		if(!axes.count(key))
			return;
		short cpos;
		double _pos = pos;
		double _pmin = pmin;
		double _pmax = pmax;
		_pos = 65535 * (_pos - _pmin) / (_pmax - _pmin) - 32768;
		if(_pos < -32768)
			cpos = -32768;
		else if(_pos > 32767)
			cpos = 32767;
		else
			cpos = _pos;
		if(laxes[key] != cpos) {
			platform::queue(keypress(modifier_set(), *axes[key], cpos));
			laxes[key] = cpos;
		}
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
			joysticks.insert(i);
			capabilities[i] = caps;
			messages << "Joystick #" << i << ": " << caps.szPname << " (by '" << caps.szOEMVxD << "')"
				<< std::endl;
			if(caps.wCaps & JOYCAPS_HASPOV)
				create_hat(i);
			for(unsigned j = 0; j < caps.wNumButtons && j < 32; j++)
				create_button(i, j);
			unsigned axcount = 2;
			create_axis(i, 0, caps.wXmin, caps.wXmax);
			create_axis(i, 1, caps.wYmin, caps.wYmax);
			if(caps.wCaps & JOYCAPS_HASZ) {
				create_axis(i, 2, caps.wZmin, caps.wZmax);
				axcount++;
			}
			if(caps.wCaps & JOYCAPS_HASR) {
				create_axis(i, 3, caps.wRmin, caps.wRmax);
				axcount++;
			}
			if(caps.wCaps & JOYCAPS_HASU) {
				create_axis(i, 4, caps.wUmin, caps.wUmax);
				axcount++;
			}
			if(caps.wCaps & JOYCAPS_HASV) {
				create_axis(i, 5, caps.wVmin, caps.wVmax);
				axcount++;
			}
			if(caps.wCaps & JOYCAPS_HASPOV)
				messages << "1 hat, ";
			messages << axcount << " axes, " << min((int)caps.wNumButtons, 32) << " buttons" << std::endl;
		}
	}

	void quit_joysticks()
	{
		for(auto i : keygroups)
			delete i;
		buttons.clear();
		axes.clear();
		hats.clear();
		keygroups.clear();
		joysticks.clear();
		capabilities.clear();
	}

	void poll_joysticks()
	{
		modifier_set mod;
		for(auto i : capabilities) {
			unsigned jnum = i.first;
			JOYINFOEX info;
			JOYCAPS caps = capabilities[jnum];
			info.dwSize = sizeof(info);
			info.dwFlags = JOY_RETURNALL;
			if(joyGetPosEx(jnum, &info) != JOYERR_NOERROR)
				continue;	//Not usable.
			if(caps.wCaps & JOYCAPS_HASPOV) {
				//Read POV hat.
				short m = 0;
				int pov = info.dwPOV;
				if((pov >= 0 && pov <= 6000) || (pov >= 30000  && pov <= 36000))
					m |= 1;
				if(pov >= 3000 && pov <= 15000)
					m |= 2;
				if(pov >= 12000 && pov <= 24000)
					m |= 4;
				if(pov >= 21000 && pov <= 33000)
					m |= 8;
				if(lhats[jnum] != m) {
					platform::queue(keypress(modifier_set(), *hats[jnum], m));
					lhats[jnum] = m;
				}
			}
			for(unsigned j = 0; j < caps.wMaxButtons; j++) {
				//Read buttons
				auto key = std::make_pair(jnum, j);
				short x = (info.dwButtons >> j) & 1;
				if(buttons.count(key) && lbuttons[key] != x) {
					platform::queue(keypress(modifier_set(), *buttons[key], x));
					lbuttons[key] = x;
				}
			}
			read_axis(jnum, 0, info.dwXpos, caps.wXmin, caps.wXmax);
			read_axis(jnum, 1, info.dwYpos, caps.wYmin, caps.wYmax);
			if(caps.wCaps & JOYCAPS_HASZ)
				read_axis(jnum, 2, info.dwZpos, caps.wZmin, caps.wZmax);
			if(caps.wCaps & JOYCAPS_HASR)
				read_axis(jnum, 3, info.dwRpos, caps.wRmin, caps.wRmax);
			if(caps.wCaps & JOYCAPS_HASU)
				read_axis(jnum, 4, info.dwUpos, caps.wUmin, caps.wUmax);
			if(caps.wCaps & JOYCAPS_HASV)
				read_axis(jnum, 5, info.dwVpos, caps.wVmin, caps.wVmax);
		}
	}
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
	quit_joysticks();
}

#define POLL_WAIT 20000

void joystick_plugin::thread_fn() throw()
{
	while(!quit_signaled) {
		poll_joysticks();
		usleep(POLL_WAIT);
	}
	quit_ack = true;
}

void joystick_plugin::signal() throw()
{
	quit_signaled = true;
}

const char* joystick_plugin::name = "Win32mm joystick plugin";
