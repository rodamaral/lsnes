#ifndef _framerate__hpp__included__
#define _framerate__hpp__included__

#include <cstdint>


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
 * Freeze time.
 *
 * Parameter usec: Current time in microseconds.
 */
void freeze_time(uint64_t usec);

/**
 * Unfreeze time.
 *
 * Parameter usec: Current time in microseconds.
 */
void unfreeze_time(uint64_t usec);

/**
 * Acknowledge frame start for timing purposes. If time is frozen, it is automatically unfrozen.
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
