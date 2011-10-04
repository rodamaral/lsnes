#ifndef _controller__hpp__included__
#define _controller__hpp__included__

#include "controllerdata.hpp"

/**
 * Look up physcial controller ID by logical controller ID.
 *
 * parameter lid: Logical ID (0-7).
 * returns: Physical ID. -1 if there is no such logical controller.
 */
int controller_index_by_logical(unsigned lid) throw();
/**
 * Look up physcial controller ID by analog controller ID.
 *
 * parameter aid: Analog ID (0-2).
 * returns: Physical ID. -1 if there is no such controller.
 */
int controller_index_by_analog(unsigned aid) throw();
/**
 * Look up if controller is mouse by analog controller ID.
 *
 * parameter aid: Analog ID (0-2).
 * returns: True if ID points to mouse, otherwise false.
 */
bool controller_ismouse_by_analog(unsigned aid) throw();
/**
 * Look up type of controller by logical controller ID.
 *
 * parameter lid: Logical ID (0-7).
 * returns: The type of controller (not port!).
 */
devicetype_t controller_type_by_logical(unsigned lid) throw();
/**
 * Set port type.
 *
 * Parameter port: The port to set.
 * Parameter ptype: New port type.
 * Parameter set_core: If true, set port type in bsnes core, otherwise skip this step.
 */
void controller_set_port_type(unsigned port, porttype_t ptype, bool set_core = true) throw();

/**
 * Get current controls, assuming given frame.
 *
 * Parameter frame: Current frame number.
 * Returns: Current controls, taking autohold and autofire into account.
 */
controls_t get_current_controls(uint64_t frame);

/**
 * Send analog input.
 *
 * Parameter x: X position of click.
 * Parameter y: Y position of click.
 * Parameter index: Index of button (0-2).
 */
void send_analog_input(int32_t x, int32_t y, unsigned index);

/**
 * Manipulate the reset flag in current controls.
 *
 * Parameter delay: Delay for reset (-1 for no reset)
 */
void set_curcontrols_reset(int32_t delay);

#endif
