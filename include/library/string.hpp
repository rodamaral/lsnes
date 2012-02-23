#ifndef _library__string__hpp__included__
#define _library__string__hpp__included__

#include <string>
#include <sstream>
#include <stdexcept>

/**
 * Strip trailing CR if any.
 */
std::string strip_CR(const std::string& str);

/**
 * Strip trailing CR if any.
 */
void istrip_CR(std::string& str);

/**
 * Return first character or -1 if empty.
 */
int firstchar(const std::string& str);

/**
 * String formatter
 */
class stringfmt
{
public:
	stringfmt() {}
	std::string str() { return x.str(); }
	template<typename T> stringfmt& operator<<(const T& y) { x << y; return *this; }
	void throwex() { throw std::runtime_error(x.str()); }
private:
	std::ostringstream x;
};

#endif
