#ifndef _library__joyfun__hpp__included__
#define _library__joyfun__hpp__included__

#include <cstdint>

/**
 * Perform axis calibration correction.
 *
 * Parameter v: The raw value read.
 * Parameter low: The low limit.
 * Parameter high: The high limit.
 * Returns: The calibrated read value.
 */
short calibration_correction(int64_t v, int64_t low, int64_t high);

/**
 * Translate hundredths of degree position into hat bitmask.
 *
 * 0 is assumed to be up, and values are assumed to be clockwise. Negative values are centered.
 *
 * Parameter angle: The angle.
 * Returns: The hat bitmask.
 */
short angle_to_bitmask(int angle);

/**
 * If a != b, a <- b and return true. Otherwise return false.
 *
 * Parameter a: The target.
 * Parameter b: The source.
 * Returns: a was not equal to b?
 */
template<typename T> bool make_equal(T& a, const T& b)
{
	bool r = (a != b);
	if(r)
		a = b;
	return r;
}

#endif
