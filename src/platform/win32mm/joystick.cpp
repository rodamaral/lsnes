#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/joystick.hpp"
#include "core/keymapper.hpp"
#include "core/joystickapi.hpp"
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
	volatile bool quit_signaled;
	volatile bool quit_ack;

	const char* buttonnames[] = {"Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7", 
		"Button8", "Button9", "Button10", "Button11", "Button12", "Button13", "Button14", "Button15",
		"Button16", "Button17", "Button18", "Button19", "Button20", "Button21", "Button22", "Button23", 
		"Button24", "Button25", "Button26", "Button27", "Button28", "Button29", "Button30", "Button31",
		"Button32"};
}

void joystick_driver_init() throw()
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
		joystick_create(i, caps.szPname);
		if(caps.wCaps & JOYCAPS_HASPOV)
			joystick_new_hat(i, 0, "POV");
		for(unsigned j = 0; j < caps.wNumButtons && j < 32; j++)
			joystick_new_button(i, j, buttonnames[j]);
		joystick_new_axis(i, 0, caps.wXmin, caps.wXmax, "X", 1);
		joystick_new_axis(i, 1, caps.wYmin, caps.wYmax, "Y", 1);
		if(caps.wCaps & JOYCAPS_HASZ)	joystick_new_axis(i, 2, caps.wZmin, caps.wZmax, "Z", 1);
		if(caps.wCaps & JOYCAPS_HASR)	joystick_new_axis(i, 3, caps.wRmin, caps.wRmax, "Rudder", 1);
		if(caps.wCaps & JOYCAPS_HASU)	joystick_new_axis(i, 4, caps.wUmin, caps.wUmax, "U", 1);
		if(caps.wCaps & JOYCAPS_HASV)	joystick_new_axis(i, 5, caps.wVmin, caps.wVmax, "V", 1);
		joystick_message(i);
	}
	quit_ack = quit_signaled = false;
}

void joystick_driver_quit() throw()
{
	quit_signaled = true;
	while(!quit_ack);
	joystick_quit();
}

#define POLL_WAIT 20000

void joystick_driver_thread_fn() throw()
{
	while(!quit_signaled) {
		for(auto i : joystick_set()) {
			JOYINFOEX info;
			info.dwSize = sizeof(info);
			info.dwFlags = JOY_RETURNALL;
			if(joyGetPosEx(i, &info) != JOYERR_NOERROR)
				continue;	//Not usable.
			joystick_report_pov(i, 0, info.dwPOV);
			for(unsigned j = 0; j < 32; j++)
				joystick_report_button(i, j, (info.dwButtons >> j) & 1);
			joystick_report_axis(i, 0, info.dwXpos);
			joystick_report_axis(i, 1, info.dwYpos);
			joystick_report_axis(i, 2, info.dwZpos);
			joystick_report_axis(i, 3, info.dwRpos);
			joystick_report_axis(i, 4, info.dwUpos);
			joystick_report_axis(i, 5, info.dwVpos);
		}
		joystick_flush();
		usleep(POLL_WAIT);
	}
	quit_ack = true;
}

void joystick_driver_signal() throw()
{
	quit_signaled = true;
	while(!quit_ack);
}

const char* joystick_driver_name = "Win32mm joystick plugin";
