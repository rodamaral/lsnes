#ifdef WXWIDGETS_JOYSTICK_SUPPORT
#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/joystickapi.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include <wx/timer.h>
#include <wx/joystick.h>


#define POLL_WAIT 20000
#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>

namespace
{
	volatile bool ready;
	unsigned joysticks;
	std::map<unsigned, wxJoystick*> objs;

	const char* buttonnames[] = {"Button1", "Button2", "Button3", "Button4", "Button5", "Button6", "Button7",
		"Button8", "Button9", "Button10", "Button11", "Button12", "Button13", "Button14", "Button15",
		"Button16", "Button17", "Button18", "Button19", "Button20", "Button21", "Button22", "Button23",
		"Button24", "Button25", "Button26", "Button27", "Button28", "Button29", "Button30", "Button31",
		"Button32"};

	struct joystick_timer : public wxTimer
	{
		joystick_timer() { start(); }
		void start() { Start(POLL_WAIT / 1000); }
		void stop() { Stop(); }
		void Notify()
		{
			if(!ready)
				return;
			for(auto i : objs) {
				wxJoystick& j = *i.second;
				lsnes_gamepads[i.first].report_hat(0, j.GetPOVCTSPosition());
				uint32_t bmask = j.GetButtonState();
				for(unsigned j = 0; j < 32; j++)
					lsnes_gamepads[i.first].report_button(j, (bmask >> j) & 1);
				wxPoint xy = j.GetPosition();
				lsnes_gamepads[i.first].report_axis(0, xy.x);
				lsnes_gamepads[i.first].report_axis(1, xy.y);
				lsnes_gamepads[i.first].report_axis(2, j.GetZPosition());
				lsnes_gamepads[i.first].report_axis(3, j.GetRudderPosition());
				lsnes_gamepads[i.first].report_axis(4, j.GetUPosition());
				lsnes_gamepads[i.first].report_axis(5, j.GetVPosition());
			}
		}
	}* jtimer;

	struct _joystick_driver drv = {
		.init = []() -> void {
			unsigned max_joysticks = wxJoystick::GetNumberJoysticks();
			if(!max_joysticks)
				return;		//No joystick support.
			for(unsigned i = 0; i < max_joysticks; i++) {
				wxJoystick* joy = new wxJoystick(i);
				if(!joy->IsOk()) {
					//Not usable.
					delete joy;
					continue;
				}
				unsigned jid = lsnes_gamepads.add(joy->GetProductName());
				gamepad::pad& ngp = lsnes_gamepads[jid];
				objs[jid] = joy;

				if(joy->HasPOV())
					ngp.add_hat(0, "POV");
				for(unsigned j = 0; j < joy->GetNumberButtons() && j < 32; j++)
					ngp.add_button(j, buttonnames[j]);
				const char* R = "Rudder";
				ngp.add_axis(0, joy->GetXMin(), joy->GetXMax(), false, "X");
				ngp.add_axis(1, joy->GetYMin(), joy->GetYMax(), false, "Y");
				if(joy->HasZ())		ngp.add_axis(2, joy->GetZMin(), joy->GetZMax(), false, "Z");
				if(joy->HasRudder())	ngp.add_axis(3, joy->GetRudderMin(), joy->GetRudderMax(),
					false, R);
				if(joy->HasU()) 	ngp.add_axis(4, joy->GetUMin(), joy->GetUMax(), false, "U");
				if(joy->HasV())		ngp.add_axis(5, joy->GetVMin(), joy->GetVMax(), false, "V");
				messages << "Joystick #" << jid << " online: " << joy->GetProductName() << std::endl;
			}
			ready = true;
			jtimer = new joystick_timer();
		},
		.quit = []() -> void {
			if(jtimer)
				jtimer->stop();
			delete jtimer;
			jtimer = NULL;
			ready = false;
			for(auto i : objs)
				delete i.second;
			usleep(50000);
		},
		//We don't poll in this thread, so just quit instantly.
		.thread_fn = []() -> void {},
		.signal = []() -> void {},
		.name = []() -> const char* { return "Wxwidgets joystick plugin"; }
	};
	struct joystick_driver _drv(drv);
}

#endif
