#ifndef _library__string__hpp__included__
#define _library__string__hpp__included__

#include <string>
#include <sstream>
#include <list>
#include <stdexcept>
#include <vector>
#include <boost/lexical_cast.hpp>
#include "utf8.hpp"
#include "int24.hpp"

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
	regex_results(std::vector<std::string> res, std::vector<std::pair<size_t, size_t>> mch);
	operator bool() const;
	bool operator!() const;
	size_t size() const;
	const std::string& operator[](size_t i) const;
	std::pair<size_t, size_t> match(size_t i) const;
private:
	bool matched;
	std::vector<std::string> results;
	std::vector<std::pair<size_t, size_t>> matches;
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
	//Floating-point case.
	try {
		if(std::numeric_limits<T>::is_integer) {
			if(!std::numeric_limits<T>::is_signed && value.length() && value[0] == '-') {
				throw std::runtime_error("Unsigned values can't be negative");
			}
			size_t idx = 0;
			if(value[idx] == '-' || value[idx] == '+')
				idx++;
			bool sign = (value[0] == '-');
			T mult = sign ? -1 : 1;
			T bound = sign ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
			T val = 0;
			if(value.length() > idx + 2 && value[idx] == '0' && value[idx + 1] == 'x') {
				//Hexadecimal
				for(size_t i = idx + 2; i < value.length(); i++) {
					char ch = value[i];
					T v = 0;
					if(ch >= '0' && ch <= '9')
						v = ch - '0';
					else if(ch >= 'A' && ch <= 'F')
						v = ch - 'A' + 10;
					else if(ch >= 'a' && ch <= 'f')
						v = ch - 'a' + 10;
					else
						throw std::runtime_error("Invalid character in number");
					if((sign && (bound + v) / 16 > val) || (!sign && (bound - v) / 16 < val))
						throw std::runtime_error("Value exceeds range");
					val = 16 * val + (sign ? -v : v);
				}
			} else {
				//Decimal.
				for(size_t i = idx; i < value.length(); i++) {
					char ch = value[i];
					T v = 0;
					if(ch >= '0' && ch <= '9')
						v = ch - '0';
					else
						throw std::runtime_error("Invalid character in number");
					if((sign && (bound + v) / 10 > val) || (!sign && (bound - v) / 10 < val))
						throw std::runtime_error("Value exceeds range");
					val = 10 * val + (sign ? -v : v);
				}
			}
			return val;
		}
		return boost::lexical_cast<T>(value);
	} catch(std::exception& e) {
		throw std::runtime_error("Can't parse value '" + value + "': " + e.what());
	}
}

template<> inline ss_int24_t parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	int32_t v = parse_value<int32_t>(value);
	if(v < -8388608 || v > 8388607)
		throw std::runtime_error("Can't parse value '" + value + "': Value out of valid range");
	return v;
}

template<> inline ss_uint24_t parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	uint32_t v = parse_value<uint32_t>(value);
	if(v > 0xFFFFFF)
		throw std::runtime_error("Can't parse value '" + value + "': Value out of valid range");
	return v;
}

template<> inline std::string parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	return value;
}

template<typename T>
class string_list
{
public:
	string_list();
	string_list(const std::list<std::basic_string<T>>& list);
	bool empty();
	string_list strip_one() const;
	size_t size() const;
	const std::basic_string<T>& operator[](size_t idx) const;
	bool operator<(const string_list<T>& x) const;
	bool operator==(const string_list<T>& x) const;
	bool prefix_of(const string_list<T>& x) const;
	std::basic_string<T> debug_name() const;
private:
	string_list(const std::basic_string<T>* array, size_t arrsize);
	std::vector<std::basic_string<T>> v;
};

/**
 * Split a string into substrings on some unicode codepoint.
 */
string_list<char> split_on_codepoint(const std::string& s, char32_t cp);
/**
 * Split a string into substrings on some unicode codepoint.
 */
string_list<char32_t> split_on_codepoint(const std::u32string& s, char32_t cp);

#endif
