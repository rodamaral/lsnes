#ifndef _keymapper__hpp__included__
#define _keymapper__hpp__included__

#include <string>
#include <sstream>
#include <stdexcept>
#include <list>
#include <set>
#include <map>
#include <iostream>
#include "misc.hpp"
#include "library/keyboard.hpp"
#include "library/keymapper.hpp"
#include "library/joystick2.hpp"

/**
 * Our keyboard
 */
extern keyboard lsnes_kbd;
/**
 * Our key mapper.
 */
extern keyboard_mapper lsnes_mapper;

/**
 * Gamepad HW.
 */
extern hw_gamepad_set lsnes_gamepads;
/**
 * Initialize gamepads (need to be called before initializing joysticks).
 */
void lsnes_gamepads_init();
/**
 * Deinitialize gamepads (need to be called after deinitializing joysticks).
 */
void lsnes_gamepads_deinit();

#endif
