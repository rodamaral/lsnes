#ifndef _joystick__hpp__included__
#define _joystick__hpp__included__

#include "core/keymapper.hpp"

/**
 * Create a new joystick.
 *
 * Parameter id: The id of new joystick.
 * Parameter xname: The name of new joystick.
 */
void joystick_create(uint64_t id, const std::string& xname);
/**
 * Free all joysticks.
 */
void joystick_quit();
/**
 * Create a new axis.
 *
 * Parameter jid: The id of joystick.
 * Parameter id: The id of the axis.
 * Parameter minv: The minimal calibration value.
 * Parameter maxv: The maximal calibration value.
 * Parameter xname: The name of axis.
 * Parameter atype: The axis type (-1 => Disabled, 0 => Pressure, 1 => Axis).
 */
void joystick_new_axis(uint64_t jid, uint64_t id, int64_t minv, int64_t maxv, const std::string& xname, int atype);
/**
 * Create a new button.
 *
 * Parameter jid: The id of joystick.
 * Parameter id: The id of button.
 * Parameter xname: The name of button.
 * Returns: The number of button.
 */
void joystick_new_button(uint64_t jid, uint64_t id, const std::string& xname);
/**
 * Create a new hat from pair of axes.
 *
 * Parameter jid: The id of joystick.
 * Parameter id_x: The id of x axis of the hat.
 * Parameter id_y: The id of y axis of the hat.
 * Parameter min_dev: The smallest deviation from zero to react to.
 * Parameter xname_x: The name of x axis.
 * Parameter xname_y: The name of y axis.
 * Returns: The number of hat.
 */
void joystick_new_hat(uint64_t jid, uint64_t id_x, uint64_t id_y, int64_t min_dev, const std::string& xname_x,
	const std::string& xname_y);
/**
 * Create a new hat from POV control.
 *
 * Parameter jid: The id of joystick.
 * Parameter id: The id of POV control.
 * Parameter xname: The name of POV control.
 * Returns: The number of hat.
 */
void joystick_new_hat(uint64_t jid, uint64_t id, const std::string& xname);
/**
 * Report possible change in axis value.
 *
 * Requests to update unknown axes are ignored.
 *
 * Parameter jid: The id of joystick.
 * Parameter id: The ID of axis.
 * Parameter value: The value.
 */
void joystick_report_axis(uint64_t jid, uint64_t id, int64_t value);
/**
 * Report possible change in button value.
 *
 * Requests to update unknown buttons are ignored.
 *
 * Parameter jid: The id of joystick.
 * Parameter id: The ID of button.
 * Parameter value: The value.
 */
void joystick_report_button(uint64_t jid, uint64_t id, bool value);
/**
 * Report possible change in POV value.
 *
 * Requests to update unknown POVs are ignored.
 *
 * Parameter jid: The id of joystick.
 * Parameter id: The ID of POV.
 * Parameter value: The angle in hundredths of degree clockwise from up, or negative if centered.
 */
void joystick_report_pov(uint64_t jid, uint64_t id, int angle);
/**
 * Print message about joystick.
 *
 * Parameter jid: The joystick id.
 */
void joystick_message(uint64_t jid);
/**
 * Get set of all joysticks.
 */
std::set<uint64_t> joystick_set();
/**
 * Flush all pending joystick requests.
 */
void joystick_flush();

#endif
