#include "sdl-sysjoy-interface.h"
#include "core/joystick.hpp"
#include "core/joystickapi.hpp"
#include "core/misc.hpp"
#include "library/string.hpp"
#include <cstdlib>

namespace
{
	volatile bool quit_signaled = false;
	volatile bool quit_ack = false;
	const unsigned POLL_WAIT = 20000;
	std::map<uint8_t, SDL_Joystick*> joys;

	void probe_joystick(int index)
	{
		SDL_Joystick* j = new SDL_Joystick;
		memset(j, 0, sizeof(*j));
		j->index = index;
		if(SDL_SYS_JoystickOpen(j) < 0) {
			delete j;
			return;
		}
		uint64_t joyid = reinterpret_cast<size_t>(j);
		joys[index] = j;
		joystick_create(joyid, j->name);
		for(int i = 0; i < j->nbuttons; i++)
			joystick_new_button(joyid, i, (stringfmt() << "Button #" << i).str());
		for(int i = 0; i < j->naxes; i++)
			joystick_new_axis(joyid, i, -32768, 32767, (stringfmt() << "Axis #" << i).str(), 1);
		for(int i = 0; i < j->nhats; i++)
			joystick_new_hat(joyid, i, (stringfmt() << "Hat #" << i).str());
	}

	struct _joystick_driver drv = {
		.init = []() -> void {
			int jcnt = SDL_SYS_JoystickInit();
			if(jcnt < 0)
				return;
			for(int i = 0; i < jcnt; i++)
				probe_joystick(i);
			quit_ack = quit_signaled = false;
		},
		.quit = []() -> void {
			quit_signaled = true;
			while(!quit_ack);
			for(auto i : joys) {
				SDL_SYS_JoystickClose(i.second);
				delete i.second;
			}
			SDL_SYS_JoystickQuit();
			joystick_quit();
		},
		.thread_fn = []() -> void {
			while(!quit_signaled) {
				for(auto i : joys)
					SDL_SYS_JoystickUpdate(i.second);
				joystick_flush();
				usleep(POLL_WAIT);
			}
			quit_ack = true;
		},
		.signal = []() -> void {
			quit_signaled = true;
			while(!quit_ack);
		},
		.name = []() -> const char* { return "Mac OS X joystick plugin"; }
	};
	struct joystick_driver _drv(drv);
}

uint8_t SDL_numjoysticks;


int SDL_PrivateJoystickAxis(SDL_Joystick* j, uint8_t axis, int16_t value)
{
	joystick_report_axis(reinterpret_cast<size_t>(j), axis, value);
	j->axes[axis] = value;
	return 0;
}

int SDL_PrivateJoystickBall(SDL_Joystick* j, uint8_t ball, int16_t dx, int16_t dy)
{
	//We don't support balls.
	return 0;
}

int SDL_PrivateJoystickHat(SDL_Joystick* j, uint8_t hat, uint8_t val)
{
	int angle = -1;
	if(val > 0)
		angle = 4500 * (val - 1);
	joystick_report_pov(reinterpret_cast<size_t>(j), hat, angle);
	j->hats[hat] = val;
	return 0;
}

int SDL_PrivateJoystickButton(SDL_Joystick* j, uint8_t button, uint8_t state)
{
	joystick_report_button(reinterpret_cast<size_t>(j), button, state != 0);
	j->buttons[button] = state;
	return 0;
}

void SDL_SetError(const char* err)
{
	messages << "Joystick driver error: " << err << std::endl;
}
