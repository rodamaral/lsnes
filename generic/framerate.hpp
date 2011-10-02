#ifndef _framerate__hpp__included__
#define _framerate__hpp__included__

#include <cstdint>

/**
 * Nominal framerate of NTSC SNES.
 */
#define FRAMERATE_SNES_NTSC (10738636.0/178683.0)
/**
 * Nominal framerate of PAL SNES.
 */
#define FRAMERATE_SNES_PAL (322445.0/6448.0)

/**
 * Sets the nominal frame rate. Framerate limiting tries to maintain the nominal framerate when there is no other
 * explict framerate to maintain.
 */
void set_nominal_framerate(double fps) throw();

/**
 * Returns the current realized framerate.
 *
 * returns: The framerate the system is currently archiving.
 */
double get_framerate() throw();

/**
 * Acknowledge frame start for timing purposes.
 *
 * parameter usec: Current time (relative to some unknown epoch) in microseconds.
 */
void ack_frame_tick(uint64_t usec) throw();

/**
 * Computes the number of microseconds to wait for next frame.
 *
 * parameter usec: Current time (relative to some unknown epoch) in microseconds.
 * returns: Number of more microseconds to wait.
 */
uint64_t to_wait_frame(uint64_t usec) throw();

/**
 * Return microsecond-resolution time since unix epoch.
 */
uint64_t get_utime();

/**
 * Wait specified number of microseconds.
 */
void wait_usec(uint64_t usec);

#endif
