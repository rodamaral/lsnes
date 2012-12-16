#include "core/window.hpp"
#include "platform/sdl/platform.hpp"

#include <set>

#include <SDL.h>

namespace
{
	std::map<unsigned, keyboard_key*> joyaxis;
	std::map<unsigned, keyboard_key*> joybutton;
	std::map<unsigned, keyboard_key*> joyhat;
	std::set<SDL_Joystick*> joysticksx;
}

unsigned translate_sdl_joystick(SDL_Event& e, keypress& k)
{
	if(e.type == SDL_JOYAXISMOTION) {
		unsigned num = static_cast<unsigned>(e.jaxis.which) * 256 +
			static_cast<unsigned>(e.jaxis.axis);
		if(joyaxis.count(num)) {
			k = keypress(keyboard_modifier_set(), *joyaxis[num], e.jaxis.value);
			return 1;
		}
	} else if(e.type == SDL_JOYHATMOTION) {
		unsigned num = static_cast<unsigned>(e.jhat.which) * 256 +
			static_cast<unsigned>(e.jhat.hat);
		short v = 0;
		if(e.jhat.value & SDL_HAT_UP)
			v |= 1;
		if(e.jhat.value & SDL_HAT_RIGHT)
			v |= 2;
		if(e.jhat.value & SDL_HAT_DOWN)
			v |= 4;
		if(e.jhat.value & SDL_HAT_LEFT)
			v |= 8;
		if(joyhat.count(num)) {
			k = keypress(keyboard_modifier_set(), *joyhat[num], v);
			return 1;
		}
	} else if(e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
		unsigned num = static_cast<unsigned>(e.jbutton.which) * 256 +
			static_cast<unsigned>(e.jbutton.button);
		if(joybutton.count(num)) {
			k = keypress(keyboard_modifier_set(), *joybutton[num], (e.type == SDL_JOYBUTTONDOWN) ? 1 : 0);
			return 1;
		}
	}
	return 0;

}

void joystick_plugin::init() throw()
{
	int joysticks = SDL_NumJoysticks();
	if(!joysticks) {
		messages << "No joysticks detected." << std::endl;
	} else {
		messages << joysticks << " joystick(s) detected." << std::endl;
		for(int i = 0; i < joysticks; i++) {
			SDL_Joystick* j = SDL_JoystickOpen(i);
			if(!j) {
				messages << "Joystick #" << i << ": Can't open!" << std::endl;
				continue;
			}
			joysticksx.insert(j);
			messages << "Joystick #" << i << ": " << SDL_JoystickName(i) << "("
				<< SDL_JoystickNumAxes(j) << " axes, " << SDL_JoystickNumButtons(j)
				<< " buttons, " << SDL_JoystickNumHats(j) << " hats)." << std::endl;
			for(int k = 0; k < SDL_JoystickNumAxes(j); k++) {
				unsigned num = 256 * i + k;
				std::ostringstream x;
				x << "joystick" << i << "axis" << k;
				keyboard_axis_calibration cal;
				cal.mode = 1;
				cal.esign_a = -1;
				cal.esign_b = 1;
				cal.left = -32767;
				cal.center = 0;
				cal.right = 32767;
				cal.nullwidth = 0.3;
				joyaxis[num] = new keyboard_key_axis(lsnes_kbd, x.str(), "joystick", cal);
			}
			for(int k = 0; k < SDL_JoystickNumButtons(j); k++) {
				unsigned num = 256 * i + k;
				std::ostringstream x;
				x << "joystick" << i << "button" << k;
				joybutton[num] = new keyboard_key_key(lsnes_kbd, x.str(), "joystick");
			}
			for(int k = 0; k < SDL_JoystickNumHats(j); k++) {
				unsigned num = 256 * i + k;
				std::ostringstream x;
				x << "joystick" << i << "hat" << k;
				joyhat[num] = new keyboard_key_hat(lsnes_kbd, x.str(), "joystick");
			}
		}
	}
}

void joystick_plugin::quit() throw()
{
	for(auto i : joyaxis)
		delete i.second;
	for(auto i : joyhat)
		delete i.second;
	for(auto i : joybutton)
		delete i.second;
	for(auto i : joysticksx)
		SDL_JoystickClose(i);
	joyaxis.clear();
	joyhat.clear();
	joybutton.clear();
	joysticksx.clear();
}

void joystick_plugin::thread_fn() throw()
{
	//Exit instantly.
}

void joystick_plugin::signal() throw()
{
	//Nothing to do.
}

const char* joystick_plugin::name = "SDL joystick plugin";
