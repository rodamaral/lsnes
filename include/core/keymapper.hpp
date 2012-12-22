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

/**
 * Our keyboard
 */
extern keyboard lsnes_kbd;
/**
 * Our key mapper.
 */
extern keyboard_mapper lsnes_mapper;

/**
 * Translate axis calibration into mode name.
 */
std::string calibration_to_mode(keyboard_axis_calibration p);

#endif
