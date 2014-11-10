#ifndef _library__string__hpp__included__
#define _library__string__hpp__included__

#include <string>
#include <sstream>
#include <set>
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
	std::u32string str32() { return utf8::to32(x.str()); }
	template<typename T> stringfmt& operator<<(const T& y) { x << y; return *this; }
	void throwex() { throw std::runtime_error(x.str()); }
private:
	std::ostringstream x;
};

/**
 * Lambda iterator.
 */
template<typename T> class lambda_output_iterator
{
public:
	template<typename U> class helper
	{
	public:
		helper(std::function<void(const U& val)> _fn)
			: fn(_fn)
		{
		}
		helper& operator=(const U& v)
		{
			fn(v);
			return *this;
		}
	private:
		std::function<void(const U& val)> fn;
	};
	typedef std::output_iterator_tag iterator_category;
	typedef helper<T> value_type;
	typedef int difference_type;
	typedef helper<T>& reference;
	typedef helper<T>* pointer;
/**
 * Constructor.
 */
	lambda_output_iterator(std::function<void(const T& val)> _fn)
		: h(_fn)
	{
	}
/**
 * Dereference.
 */
	helper<T>& operator*() throw()
	{
		return h;
	}
/**
 * Increment.
 */
	lambda_output_iterator<T>& operator++() throw()
	{
		return *this;
	}
/**
 * Increment.
 */
	lambda_output_iterator<T> operator++(int) throw()
	{
		return *this;
	}
private:
	helper<T> h;
};

/**
 * Token iterator.
 */
template<typename T> class token_iterator
{
public:
	typedef std::forward_iterator_tag iterator_category;
	typedef std::basic_string<T> value_type;
	typedef int difference_type;
	typedef const std::basic_string<T>& reference;
	typedef const std::basic_string<T>* pointer;
/**
 * Create new end-of-sequence iterator.
 */
	token_iterator() : str(tmp) { ctor_eos(); }
/**
 * Create a new start-of-sequence iterator.
 *
 * Parameter s: The string to iterate. Must remain valid during lifetime of iterator.
 * Parameter sep: The set of separators.
 * Parameter whole_sequence: If true, after seeing one separator, throw away separators until none more are found.
 */
	token_iterator(const std::basic_string<T>& s, std::initializer_list<const T*> sep,
		bool whole_sequence = false) throw(std::bad_alloc) : str(s) { ctor_itr(sep, whole_sequence); }
/**
 * Compare.
 */
	bool operator==(const token_iterator<T>& itr) const throw() { return equals_op(itr); }
/**
 * Compare.
 */
	bool operator!=(const token_iterator<T>& itr) const throw() { return !equals_op(itr); }
/**
 * Dereference.
 */
	const std::basic_string<T>& operator*() const throw() { return dereference(); }
/**
 * Increment.
 */
	token_iterator<T>& operator++() throw(std::bad_alloc) { return preincrement(); }
/**
 * Increment.
 */
	token_iterator<T> operator++(int) throw(std::bad_alloc) { return postincrement(); }
/**
 * Do nothing, pull everything.
 */
	static void pull_fn();
private:
/**
 * Foreach helper.
 */
	template<typename U> class _foreach
	{
	public:
/**
 * Create helper.
 */
		_foreach(const std::basic_string<U>& _s,
			std::initializer_list<const U*> sep, bool whole_sequence = false)
			: s(_s, sep, whole_sequence)
		{
		}
/**
 * Starting iterator.
 */
		token_iterator<U> begin() throw() { return s; }
/**
 * Ending iterator.
 */
		token_iterator<U> end() throw() { return e; }
	private:
		token_iterator<U> s;
		token_iterator<U> e;
	};

	void ctor_eos();
	void ctor_itr(std::initializer_list<const T*> sep, bool whole_sequence = false) throw(std::bad_alloc);
	token_iterator<T> postincrement() throw(std::bad_alloc);
	token_iterator<T>& preincrement() throw(std::bad_alloc);
	const std::basic_string<T>& dereference() const throw();
	bool equals_op(const token_iterator<T>& itr) const throw();
	size_t is_sep(size_t pos);
	void load_helper();
	const std::basic_string<T>& str;
	size_t bidx;
	size_t eidx;
	std::basic_string<T> tmp;
	std::set<std::basic_string<T>> spliton;
	bool is_end_iterator;
	bool whole_seq;
public:
/**
 * Return an container referencing tokens of string.
 */
	static _foreach<T> foreach(const std::basic_string<T>& _s,
		std::initializer_list<const T*> sep, bool whole_sequence = false)
	{
		return _foreach<T>(_s, sep, whole_sequence);
	}
};



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
