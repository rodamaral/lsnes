#ifndef _library__minmax__hpp__included__
#define _library__minmax__hpp__included__

template<typename T> T min(T a, T b)
{
	return (a < b) ? a : b;
}

template<typename T> T max(T a, T b)
{
	return (a < b) ? b : a;
}

#endif
