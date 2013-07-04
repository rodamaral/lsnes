#ifndef _sdl__sysjoy__interface__included__
#define _sdl__sysjoy__interface__included__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct joystick_hwdata;

typedef struct _SDL_Joystick
{
	int index;
	const char* name;
	int naxes;
	int nhats;
	int nballs;
	int nbuttons;
	struct joystick_hwdata* hwdata;
	int16_t axes[256];
	uint8_t hats[256];
	uint8_t buttons[256];
} SDL_Joystick;

#define SDL_HAT_UP 1
#define SDL_HAT_RIGHTUP 2
#define SDL_HAT_RIGHT 3
#define SDL_HAT_RIGHTDOWN 4
#define SDL_HAT_DOWN 5
#define SDL_HAT_LEFTDOWN 6
#define SDL_HAT_LEFT 7
#define SDL_HAT_LEFTUP 8
#define SDL_HAT_CENTERED 0

typedef uint8_t Uint8;

//Driver -> Wrapper.
extern uint8_t SDL_numjoysticks;
int SDL_PrivateJoystickAxis(SDL_Joystick* j, uint8_t axis, int16_t value);
int SDL_PrivateJoystickBall(SDL_Joystick* j, uint8_t ball, int16_t dx, int16_t dy);
int SDL_PrivateJoystickHat(SDL_Joystick* j, uint8_t hat, uint8_t val);
int SDL_PrivateJoystickButton(SDL_Joystick* j, uint8_t button, uint8_t state);
void SDL_SetError(const char* err);
//Wrapper -> Driver.
int SDL_SYS_JoystickInit(void);
const char *SDL_SYS_JoystickName(int index);
int SDL_SYS_JoystickOpen(SDL_Joystick *joystick);
void SDL_SYS_JoystickUpdate(SDL_Joystick *joystick);
void SDL_SYS_JoystickClose(SDL_Joystick *joystick);
void SDL_SYS_JoystickQuit(void);

#ifdef __cplusplus
}
#endif
#endif
