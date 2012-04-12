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
	std::map<unsigned, joystick_model> joysticks;
	std::map<std::pair<unsigned, unsigned>, keygroup*> axes;
	std::map<std::pair<unsigned, unsigned>, keygroup*> buttons;
	std::map<std::pair<unsigned, unsigned>, keygroup*> hats;
	std::map<unsigned, wxJoystick*> jobjects;
	volatile bool quit_signaled;
	volatile bool quit_ack;
	volatile bool ready = false;

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

	void create_axis(unsigned i, unsigned j, int min, int max)
	{
		unsigned n = joysticks[i].new_axis(j, min, max, axisnames[j]);
		std::string name = (stringfmt() << "joystick" << i << "axis" << n).str();
		axes[std::make_pair(i, n)] = new keygroup(name, "joystick", keygroup::KT_AXIS_PAIR);
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
			jobjects[i] = joy;
			joysticks[i].name(joy->GetProductName());
			messages << "Joystick #" << i << ": " << joy->GetProductName() << std::endl;
			if(joy->HasPOV())
				create_hat(i);
			for(unsigned j = 0; j < joy->GetNumberButtons() && j < 32; j++)
				create_button(i, j);
			create_axis(i, 0, joy->GetXMin(), joy->GetXMax());
			create_axis(i, 1, joy->GetYMin(), joy->GetYMax());
			if(joy->HasZ())		create_axis(i, 2, joy->GetZMin(), joy->GetZMax());
			if(joy->HasRudder())	create_axis(i, 3, joy->GetRudderMin(), joy->GetRudderMax());
			if(joy->HasU()) 	create_axis(i, 4, joy->GetUMin(), joy->GetUMax());
			if(joy->HasV())		create_axis(i, 5, joy->GetVMin(), joy->GetVMax());
			if(joy->HasPOV())
				messages << "1 hat, ";
			messages << joysticks[i].axes() << " axes, " << joysticks[i].buttons() << " buttons"
				<< std::endl;
		}
		ready = true;
	}

	void poll_joysticks()
	{
		if(!ready)
			return;
		for(auto i : jobjects) {
			joystick_model& m = joysticks[i.first];
			wxJoystick& j = *i.second;
			m.report_pov(0, j.GetPOVCTSPosition());
			uint32_t bmask = j.GetButtonState();
			for(unsigned j = 0; j < 32; j++)
				m.report_button(j, (bmask >> j) & 1);
			wxPoint xy = j.GetPosition();
			m.report_axis(0, xy.x);
			m.report_axis(1, xy.y);
			m.report_axis(2, j.GetZPosition());
			m.report_axis(3, j.GetRudderPosition());
			m.report_axis(4, j.GetUPosition());
			m.report_axis(5, j.GetVPosition());
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
	}

	struct joystick_timer : public wxTimer
	{
		joystick_timer() { start(); }
		void start() { Start(POLL_WAIT / 1000); }
		void stop() { Stop(); }
		void Notify() { poll_joysticks(); }
	}* jtimer;

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
	jtimer = new joystick_timer();
}

void joystick_plugin::quit() throw()
{
	jtimer->stop();
	delete jtimer;
	jtimer = NULL;
	ready = false;
	usleep(50000);

	for(auto i : jobjects)	delete i.second;
	for(auto i : axes)	delete i.second;
	for(auto i : buttons)	delete i.second;
	for(auto i : hats)	delete i.second;
	joysticks.clear();
	jobjects.clear();
	axes.clear();
	buttons.clear();
	hats.clear();
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
