#ifndef _library__string__hpp__included__
#define _library__string__hpp__included__

#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <library/utf8.hpp>

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
	std::u32string str32() { return to_u32string(x.str()); }
	template<typename T> stringfmt& operator<<(const T& y) { x << y; return *this; }
	void throwex() { throw std::runtime_error(x.str()); }
private:
	std::ostringstream x;
};

/**
 * Extract token out of string.
 *
 * Parameter str: The original string and the rest of the string on return.
 * Parameter tok: The extracted token will be written here.
 * Parameter sep: The characters to split on (empty always extracts the rest).
 * Parameter seq: If true, skip whole sequence of token ending characters.
 * Returns: The character token splitting occured on (-1 if end of string, -2 if string is empty).
 */
int extract_token(std::string& str, std::string& tok, const char* sep, bool seq = false) throw(std::bad_alloc);

class regex_results
{
public:
	regex_results();
	regex_results(std::vector<std::string> res);
	operator bool() const;
	bool operator!() const;
	size_t size() const;
	const std::string& operator[](size_t i) const;
private:
	bool matched;
	std::vector<std::string> results;
};

/**
 * Regexp a string and return matches.
 *
 * Parameter regex: The regexp to apply.
 * Parameter str: The string to apply the regexp to.
 * Parameter ex: If non-null and string does not match, throw this as std::runtime_error.
 * Returns: The captures.
 */
regex_results regex(const std::string& regex, const std::string& str, const char* ex = NULL)
	throw(std::bad_alloc, std::runtime_error);

/**
 * Regexp a string and return match result.
 *
 * Parameter regex: The regexp to apply.
 * Parameter str: The string to apply the regexp to.
 * Returns: True if matches, false if not.
 */
bool regex_match(const std::string& regex, const std::string& str) throw(std::bad_alloc, std::runtime_error);

/**
 * Cast string to bool.
 *
 * The following is true: 'on', 'true', 'yes', '1', 'enable', 'enabled'.
 * The following is false: 'off', 'false', 'no', '0', 'disable', 'disabled'.
 * Parameter str: The string to cast.
 * Returns: -1 if string is bad, 0 if false, 1 if true.
 */
int string_to_bool(const std::string& cast_to_bool);

/**
 * \brief Typeconvert string.
 */
template<typename T> inline T parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	try {
		//Hack, since lexical_cast lets negative values slip through.
		if(!std::numeric_limits<T>::is_signed && value.length() && value[0] == '-') {
			throw std::runtime_error("Unsigned values can't be negative");
		}
		return boost::lexical_cast<T>(value);
	} catch(std::exception& e) {
		throw std::runtime_error("Can't parse value '" + value + "': " + e.what());
	}
}

template<> inline std::string parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	return value;
}

#endif
