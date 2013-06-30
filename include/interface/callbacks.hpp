#ifndef _interface__callbacks__hpp__included__
#define _interface__callbacks__hpp__included__

#include <cstdint>
#include "library/framebuffer.hpp"

/**
 * Callbacks emulator binding can use.
 */
struct emucore_callbacks
{
public:
	virtual ~emucore_callbacks() throw();
/**
 * Get input from specified control.
 */
	virtual int16_t get_input(unsigned port, unsigned index, unsigned control) = 0;
/**
 * Set input for specified control. Only works in readwrite mode.
 *
 * Returns the actual input value used.
 */
	virtual int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value) = 0;
/**
 * Tick the RTC timer.
 */
	virtual void timer_tick(uint32_t increment, uint32_t per_second) = 0;
/**
 * Get path for firmware.
 */
	virtual std::string get_firmware_path() = 0;
/**
 * Get the base filename for ROM.
 */
	virtual std::string get_base_path() = 0;
/**
 * Get current RTC time.
 */
	virtual time_t get_time() = 0;
/**
 * Get the RNG seed to use.
 */
	virtual time_t get_randomseed() = 0;
/**
 * Output a frame. Call once for each call to emulate().
 */
	virtual void output_frame(framebuffer_raw& screen, uint32_t fps_n, uint32_t fps_d) = 0;
/**
 * Set state of action.
 */
	virtual void set_action_state(unsigned id, bool enabled) = 0;
/**
 * Set reset actions.
 */
	virtual void set_reset_actions(signed soft, signed hard) = 0;
};

extern struct emucore_callbacks* ecore_callbacks;

#endif
