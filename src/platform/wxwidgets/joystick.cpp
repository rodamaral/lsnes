#ifdef WXWIDGETS_JOYSTICK_SUPPORT
#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/joystick.hpp"
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
			for(auto i : joystick_set()) {
				wxJoystick& j = *reinterpret_cast<wxJoystick*>(i);
				joystick_report_pov(i, 0, j.GetPOVCTSPosition());
				uint32_t bmask = j.GetButtonState();
				for(unsigned j = 0; j < 32; j++)
					joystick_report_button(i, j, (bmask >> j) & 1);
				wxPoint xy = j.GetPosition();
				joystick_report_axis(i, 0, xy.x);
				joystick_report_axis(i, 1, xy.y);
				joystick_report_axis(i, 2, j.GetZPosition());
				joystick_report_axis(i, 3, j.GetRudderPosition());
				joystick_report_axis(i, 4, j.GetUPosition());
				joystick_report_axis(i, 5, j.GetVPosition());
			}
			joystick_flush();
		}
	}* jtimer;
}

void joystick_driver_init() throw()
{
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
		uint64_t jid = reinterpret_cast<uint64_t>(joy);
		joystick_create(jid, joy->GetProductName());
		if(joy->HasPOV())
			joystick_new_hat(jid, 0, "POV");
		for(unsigned j = 0; j < joy->GetNumberButtons() && j < 32; j++)
			joystick_new_button(jid, j, buttonnames[j]);
		const char* R = "Rudder";
		joystick_new_axis(jid, 0, joy->GetXMin(), joy->GetXMax(), "X", 1);
		joystick_new_axis(jid, 1, joy->GetYMin(), joy->GetYMax(), "Y", 1);
		if(joy->HasZ())		joystick_new_axis(jid, 2, joy->GetZMin(), joy->GetZMax(), "Z", 1);
		if(joy->HasRudder())	joystick_new_axis(jid, 3, joy->GetRudderMin(), joy->GetRudderMax(), R, 1);
		if(joy->HasU()) 	joystick_new_axis(jid, 4, joy->GetUMin(), joy->GetUMax(), "U", 1);
		if(joy->HasV())		joystick_new_axis(jid, 5, joy->GetVMin(), joy->GetVMax(), "V", 1);
		joystick_message(jid);
	}
	ready = true;
	jtimer = new joystick_timer();
}

void joystick_driver_quit() throw()
{
	if(jtimer)
		jtimer->stop();
	delete jtimer;
	jtimer = NULL;
	ready = false;
	for(auto i : joystick_set())
		delete reinterpret_cast<wxJoystick*>(i);
	usleep(50000);
	joystick_quit();
}

void joystick_driver_thread_fn() throw()
{
	//We don't poll in this thread, so just quit instantly.
}

void joystick_driver_signal() throw()
{
	//We don't poll in dedicated thread, so nothing to do.
}

const char* joystick_driver_name = "Wxwidgets joystick plugin";
#endif
