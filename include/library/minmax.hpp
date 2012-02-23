#ifndef _library__minmax__hpp__included__
#define _library__minmax__hpp__included__

#include <map>

/**
 * Return minimum of a and b.
 */
template<typename T> T min(T a, T b)
{
	return (a < b) ? a : b;
}

/**
 * Return maximum of a and b.
 */
template<typename T> T max(T a, T b)
{
	return (a < b) ? b : a;
}

#endif
