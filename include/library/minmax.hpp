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

/**
 * Clip v to [a,b].
 */
template<typename T> T clip(T v, T a, T b)
{
	return (v < a) ? a : ((v > b) ? b : v);
}

template<typename T, typename U>
class pair_assign_helper
{
public:
	pair_assign_helper(T& _a, U& _b) : a(_a), b(_b) {}
	const std::pair<T, U>& operator=(const std::pair<T, U>& r)
	{
		a = r.first;
		b = r.second;
		return r;
	}
private:
	T& a;
	U& b;
};

/**
 * Create a rvalue from components of pair.
 */
template<typename T, typename U>
pair_assign_helper<T, U> rpair(T& a, U& b)
{
	return pair_assign_helper<T, U>(a, b);
}



#endif
