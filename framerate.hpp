#ifndef _framerate__hpp__included__
#define _framerate__hpp__included__

#include <cstdint>

/**
 * \brief Nominal framerate of NTSC SNES.
 */
#define FRAMERATE_SNES_NTSC (10738636.0/178683.0)
/**
 * \brief Nominal framerate of PAL SNES.
 */
#define FRAMERATE_SNES_PAL (322445.0/6448.0)

/**
 * \brief Set the nominal framerate
 *
 * Sets the nominal frame rate. Framerate limiting tries to maintain the nominal framerate when there is no other
 * explict framerate to maintain.
 */
void set_nominal_framerate(double fps) throw();

/**
 * \brief Get the current realized framerate.
 *
 * Returns the current realized framerate.
 *
 * \return The framerate the system is currently archiving.
 */
double get_framerate() throw();

/**
 * \brief ACK frame start.
 *
 * Acknowledge frame start for timing purposes.
 *
 * \param msec Current time.
 */
void ack_frame_tick(uint64_t msec) throw();

/**
 * \brief Obtain how long to wait for next frame.
 *
 * Computes the number of milliseconds to wait for next frame.
 *
 * \param msec Current time.
 * \return Number of more milliseconds to wait.
 */
uint64_t to_wait_frame(uint64_t msec) throw();

#endif
