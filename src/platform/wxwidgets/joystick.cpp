#ifdef WXWIDGETS_JOYSTICK_SUPPORT
#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/joyfun.hpp"
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
	std::set<keygroup*> keygroups;
	std::map<std::pair<unsigned, unsigned>, keygroup*> buttons;
	std::map<std::pair<unsigned, unsigned>, keygroup*> axes;
	std::map<unsigned, keygroup*> hats;
	std::map<std::pair<unsigned, unsigned>, short> lbuttons;
	std::map<std::pair<unsigned, unsigned>, short> laxes;
	std::map<unsigned, short> lhats;
	std::map<unsigned, wxJoystick*> joysticks;
	volatile bool ready = false;

	void create_hat(unsigned i, unsigned j = 0)
	{
		std::string n = (stringfmt() << "joystick" << i << "hat" << j).str();
		keygroup* k = new keygroup(n, "joystick", keygroup::KT_HAT);
		hats[i] = k;
	}

	void create_button(unsigned i, unsigned j)
	{
		std::string n = (stringfmt() << "joystick" << i << "button" << j).str();
		keygroup* k = new keygroup(n, "joystick", keygroup::KT_KEY);
		buttons[std::make_pair(i, j)] = k;
	}

	void create_axis(unsigned i, unsigned j, int min, int max)
	{
		messages << "axis #" << j << ": " << min << " " << max << std::endl;
		std::string n = (stringfmt() << "joystick" << i << "axis" << j).str();
		keygroup* k;
		k = new keygroup(n, "joystick", keygroup::KT_AXIS_PAIR);
		axes[std::make_pair(i, j)] = k;
	}

	void read_axis(unsigned i, unsigned j, int pos, int pmin, int pmax)
	{
		auto key = std::make_pair(i, j);
		if(!axes.count(key))
			return;
		if(make_equal(laxes[key], calibration_correction(pos, pmin, pmax)))
			platform::queue(keypress(modifier_set(), *axes[key], laxes[key]));
	}

	void init_joysticks()
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
			joysticks[i] = joy;
			messages << "Joystick #" << i << ": " << joy->GetProductName() << std::endl;
			if(joy->HasPOV())
				create_hat(i);
			for(unsigned j = 0; j < joy->GetNumberButtons() && j < 32; j++)
				create_button(i, j);
			unsigned axcount = 2;
			create_axis(i, 0, joy->GetXMin(), joy->GetXMax());
			create_axis(i, 1, joy->GetYMin(), joy->GetYMax());
			if(joy->HasZ()) {
				create_axis(i, 2, joy->GetZMin(), joy->GetZMax());
				axcount++;
			}
			if(joy->HasRudder()) {
				create_axis(i, 3, joy->GetRudderMin(), joy->GetRudderMax());
				axcount++;
			}
			if(joy->HasU()) {
				create_axis(i, 4, joy->GetUMin(), joy->GetUMax());
				axcount++;
			}
			if(joy->HasV()) {
				create_axis(i, 5, joy->GetVMin(), joy->GetVMax());
				axcount++;
			}
			if(joy->HasPOV())
				messages << "1 hat, ";
			messages << axcount << " axes, " << min((int)joy->GetNumberButtons(), 32) << " buttons"
				<< std::endl;
		}
		ready = true;
	}

	void poll_joysticks()
	{
		if(!ready)
			return;
		modifier_set mod;
		for(auto i : joysticks) {
			unsigned jnum = i.first;
			wxJoystick* joy = i.second;
			if(joy->HasPOV())
				if(make_equal(lhats[jnum], angle_to_bitmask(joy->GetPOVCTSPosition())))
					platform::queue(keypress(modifier_set(), *hats[jnum], lhats[jnum]));
			uint32_t bmask = joy->GetButtonState();
			for(unsigned j = 0; j < 32; j++) {
				//Read buttons
				auto key = std::make_pair(jnum, j);
				if(buttons.count(key) && make_equal(lbuttons[key], static_cast<short>((bmask >> j) &
					1)))
					platform::queue(keypress(modifier_set(), *buttons[key], lbuttons[key]));
			}
			wxPoint xy = joy->GetPosition();
			read_axis(jnum, 0, xy.x, joy->GetXMin(), joy->GetXMax());
			read_axis(jnum, 1, xy.y, joy->GetYMin(), joy->GetYMax());
			if(joy->HasZ())
				read_axis(jnum, 2, joy->GetZPosition(), joy->GetZMin(), joy->GetZMax());
			if(joy->HasRudder())
				read_axis(jnum, 3, joy->GetRudderPosition(), joy->GetRudderMin(), joy->GetRudderMax());
			if(joy->HasU())
				read_axis(jnum, 4, joy->GetUPosition(), joy->GetUMin(), joy->GetUMax());
			if(joy->HasV())
				read_axis(jnum, 5, joy->GetVPosition(), joy->GetUMin(), joy->GetUMax());
		}
	}

	struct joystick_timer : public wxTimer
	{
		joystick_timer() { start(); }
		void start() { Start(POLL_WAIT / 1000); }
		void stop() { Stop(); }
		void Notify() { poll_joysticks(); }
	}* jtimer;
}

void joystick_plugin::init() throw()
{
	init_joysticks();
	jtimer = new joystick_timer();
}

void joystick_plugin::quit() throw()
{
	jtimer->stop();
	delete jtimer;
	jtimer = NULL;
	ready = false;
	usleep(50000);
	for(auto i : keygroups)
		delete i;
	for(auto i : joysticks)
		delete i.second;
	usleep(500000);
	buttons.clear();
	axes.clear();
	hats.clear();
	keygroups.clear();
	joysticks.clear();
}

void joystick_plugin::thread_fn() throw()
{
	//We don't poll in this thread, so just quit instantly.
}

void joystick_plugin::signal() throw()
{
	//We don't poll in dedicated thread, so nothing to do.
}

const char* joystick_plugin::name = "Wxwidgets joystick plugin";
#endif
