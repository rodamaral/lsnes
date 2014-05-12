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
#include "library/keyboard-mapper.hpp"
#include "library/gamepad.hpp"

/**
 * Inverse bindings set.
 */
extern keyboard::invbind_set lsnes_invbinds;
/**
 * Gamepad HW.
 */
extern gamepad::set lsnes_gamepads;
/**
 * Initialize gamepads (need to be called before initializing joysticks).
 */
void lsnes_gamepads_init();
/**
 * Deinitialize gamepads (need to be called after deinitializing joysticks).
 */
void lsnes_gamepads_deinit();
/**
 * Cleanup the keymapper stuff.
 */
void cleanup_keymapper();

#endif
