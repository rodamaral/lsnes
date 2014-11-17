#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/joystickapi.hpp"
#include "core/messages.hpp"
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
	std::map<unsigned, unsigned> idx_to_jid;
	unsigned joysticks;

	const char* buttonnames[] = {"Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7",
		"Button8", "Button9", "Button10", "Button11", "Button12", "Button13", "Button14", "Button15",
		"Button16", "Button17", "Button18", "Button19", "Button20", "Button21", "Button22", "Button23",
		"Button24", "Button25", "Button26", "Button27", "Button28", "Button29", "Button30", "Button31",
		"Button32"};
#define POLL_WAIT 20000

	struct _joystick_driver drv = {
		.init = []() -> void {
			quit_signaled = false;
			quit_ack = false;
			unsigned max_joysticks = joyGetNumDevs();
			if(!max_joysticks)
				return;		//No joystick support.
			joysticks = max_joysticks;
			for(unsigned i = 0; i < max_joysticks; i++) {
				JOYINFOEX info;
				JOYCAPS caps;
				info.dwSize = sizeof(info);
				info.dwFlags = JOY_RETURNALL;
				if(joyGetPosEx(i, &info) != JOYERR_NOERROR)
					continue;	//Not usable.
				if(joyGetDevCaps(i, &caps, sizeof(caps)) != JOYERR_NOERROR)
					continue;	//Not usable.
				idx_to_jid[i] = lsnes_gamepads.add(caps.szPname);
				gamepad::pad& ngp = lsnes_gamepads[idx_to_jid[i]];
				if(caps.wCaps & JOYCAPS_HASPOV)
					ngp.add_hat(0, "POV");
				for(unsigned j = 0; j < caps.wNumButtons && j < 32; j++)
					ngp.add_button(j, buttonnames[j]);
				ngp.add_axis(0, caps.wXmin, caps.wXmax, false, "X");
				ngp.add_axis(1, caps.wYmin, caps.wYmax, false, "Y");
				if(caps.wCaps & JOYCAPS_HASZ)	ngp.add_axis(2, caps.wZmin, caps.wZmax, false, "Z");
				if(caps.wCaps & JOYCAPS_HASR)	ngp.add_axis(3, caps.wRmin, caps.wRmax, false,
					"Rudder");
				if(caps.wCaps & JOYCAPS_HASU)	ngp.add_axis(4, caps.wUmin, caps.wUmax, false, "U");
				if(caps.wCaps & JOYCAPS_HASV)	ngp.add_axis(5, caps.wVmin, caps.wVmax, false, "V");
				messages << "Joystick #" << idx_to_jid[i] << " online: " << caps.szPname << std::endl;
			}
			quit_ack = quit_signaled = false;
		},
		.quit = []() -> void {
			quit_signaled = true;
			while(!quit_ack);
		},
		.thread_fn = []() -> void {
			while(!quit_signaled) {
				for(unsigned i = 0; i < joysticks; i++) {
					JOYINFOEX info;
					info.dwSize = sizeof(info);
					info.dwFlags = JOY_RETURNALL;
					if(joyGetPosEx(i, &info) != JOYERR_NOERROR)
						continue;	//Not usable.
					lsnes_gamepads[idx_to_jid[i]].report_hat(0, info.dwPOV);
					for(unsigned j = 0; j < 32; j++)
						lsnes_gamepads[idx_to_jid[i]].report_button(j,
							(info.dwButtons >> j) & 1);
					lsnes_gamepads[idx_to_jid[i]].report_axis(0, info.dwXpos);
					lsnes_gamepads[idx_to_jid[i]].report_axis(1, info.dwYpos);
					lsnes_gamepads[idx_to_jid[i]].report_axis(2, info.dwZpos);
					lsnes_gamepads[idx_to_jid[i]].report_axis(3, info.dwRpos);
					lsnes_gamepads[idx_to_jid[i]].report_axis(4, info.dwUpos);
					lsnes_gamepads[idx_to_jid[i]].report_axis(5, info.dwVpos);
				}
				usleep(POLL_WAIT);
			}
			quit_ack = true;
		},
		.signal = []() -> void {
			quit_signaled = true;
			while(!quit_ack);
		},
		.name = []() -> const char* { return "Win32mm joystick plugin"; }
	};
	struct joystick_driver _drv(drv);
}
