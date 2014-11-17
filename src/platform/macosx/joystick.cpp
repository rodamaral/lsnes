#include "sdl-sysjoy-interface.h"
#include "core/keymapper.hpp"
#include "core/joystickapi.hpp"
#include "core/messages.hpp"
#include "core/misc.hpp"
#include "library/string.hpp"
#include <cstdlib>

namespace
{
	volatile bool quit_signaled = false;
	volatile bool quit_ack = false;
	const unsigned POLL_WAIT = 20000;
	std::map<uint8_t, SDL_Joystick*> joys;
	std::map<uint8_t, unsigned> idx_to_jid;

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
		idx_to_jid[index] = lsnes_gamepads.add(j->name);
		gamepad::pad& ngp = lsnes_gamepads[idx_to_jid[index]];

		for(int i = 0; i < j->nbuttons; i++)
			ngp.add_button(i, (stringfmt() << "Button #" << i).str());
		for(int i = 0; i < j->naxes; i++)
			ngp.add_axis(i, -32768, 32767, false, (stringfmt() << "Axis #" << i).str());
		for(int i = 0; i < j->nhats; i++)
			ngp.add_hat(i, (stringfmt() << "Hat #" << i).str());
	}

	struct _joystick_driver drv = {
		.init = []() -> void {
			quit_signaled = false;
			quit_ack = false;
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
		},
		.thread_fn = []() -> void {
			while(!quit_signaled) {
				for(auto i : joys)
					SDL_SYS_JoystickUpdate(i.second);
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
	lsnes_gamepads[j->index].report_axis(axis, value);
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
	lsnes_gamepads[j->index].report_hat(hat, angle);
	j->hats[hat] = val;
	return 0;
}

int SDL_PrivateJoystickButton(SDL_Joystick* j, uint8_t button, uint8_t state)
{
	lsnes_gamepads[j->index].report_button(button, state != 0);
	j->buttons[button] = state;
	return 0;
}

void SDL_SetError(const char* err)
{
	messages << "Joystick driver error: " << err << std::endl;
}
