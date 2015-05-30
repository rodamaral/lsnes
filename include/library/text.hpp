#ifndef _library__text__hpp__included__
#define _library__text__hpp__included__

#include <cstdint>
#include <string>
#include <cstdlib>
#include <exception>
#include <algorithm>
#include <iostream>
#include "memtracker.hpp"
#include "utf8.hpp"

/**
 * An UTF-32 string.
 */
class text
{
public:
	text() throw();
	text(const std::string& str) throw(std::bad_alloc);
	text(const std::u32string& str) throw(std::bad_alloc);
	text(const char* str) throw(std::bad_alloc);
	text(const char* str, size_t len) throw(std::bad_alloc);
	text(const char32_t* str) throw(std::bad_alloc);
	text(const char32_t* str, size_t len) throw(std::bad_alloc);
	text(const text& t) throw(std::bad_alloc);
	text& operator=(const text& t) throw(std::bad_alloc);
	~text() throw();
	size_t length() const throw() { return len; }
	operator std::string() const throw(std::bad_alloc) { return utf8::to8(data, len); }
	operator std::u32string() const throw(std::bad_alloc) { return std::u32string(data, data + len); }
	char32_t& operator[](size_t idx) throw(std::out_of_range)
	{
		if(idx >= len) throw std::out_of_range("String index out of range");
		return data[idx];
	}
	const char32_t& operator[](size_t idx) const throw(std::out_of_range)
	{
		if(idx >= len) throw std::out_of_range("String index out of range");
		return data[idx];
	}
	void set_tracking_category(const char* category);
	text substr(size_t start, size_t len) const throw(std::bad_alloc, std::out_of_range);
	text substr(size_t start) const throw(std::bad_alloc, std::out_of_range);
	size_t find_first_of(const char32_t* chlist, size_t ptr = 0) const throw();
	size_t find_first_not_of(const char32_t* chlist, size_t ptr = 0) const throw();
	size_t find_last_of(const char32_t* chlist, size_t ptr = 0) const throw();
	size_t find_last_not_of(const char32_t* chlist, size_t ptr = 0) const throw();
	text strip_CR() const throw(std::bad_alloc);
	static text& istrip_CR(text& t) throw();
	void ostream_helper(std::ostream& os) const;
	static text getline(std::istream& is);
	std::pair<size_t, size_t> output_utf8_fragment(size_t startidx, char* out, size_t outsize) const throw();
	size_t length_utf8() const throw();
	const char* c_str() const throw(std::bad_alloc);
	std::string to_cpp_str() const throw(std::bad_alloc) { return (std::string)*this; }
	void resize(size_t sz);
	
	static text concatenate(const text& a, const text& b) throw(std::bad_alloc);
	static text concatenate(const text& a, const std::string& b) throw(std::bad_alloc);
	static text concatenate(const text& a, const std::u32string& b) throw(std::bad_alloc);
	static text concatenate(const text& a, const char* b) throw(std::bad_alloc);
	static text concatenate(const text& a, const char32_t* b) throw(std::bad_alloc);
	static text concatenate(const std::string& a, const text& b) throw(std::bad_alloc);
	static text concatenate(const std::u32string& a, const text& b) throw(std::bad_alloc);
	static text concatenate(const char* a, const text& b) throw(std::bad_alloc);
	static text concatenate(const char32_t* a, const text& b) throw(std::bad_alloc);
	static text& concatenate_inplace(text& a, const text& b) throw(std::bad_alloc);
	static text& concatenate_inplace(text& a, const std::string& b) throw(std::bad_alloc);
	static text& concatenate_inplace(text& a, const std::u32string& b) throw(std::bad_alloc);
	static text& concatenate_inplace(text& a, const char* b) throw(std::bad_alloc);
	static text& concatenate_inplace(text& a, const char32_t* b) throw(std::bad_alloc);
	static int compare(const text& a, const text& b) throw();
	static int compare(const text& a, const std::string& b) throw();
	static int compare(const text& a, const std::u32string& b) throw();
	static int compare(const text& a, const char* b) throw();
	static int compare(const text& a, const char32_t* b) throw();
	static int compare(const std::string& a, const text& b) throw() { return -compare(b, a); }
	static int compare(const std::u32string& a, const text& b) throw() { return -compare(b, a); }
	static int compare(const char* a, const text& b) throw() { return -compare(b, a); }
	static int compare(const char32_t* a, const text& b) throw() { return -compare(b, a); }
	
	const char32_t* _internal_ptr() const { return data; }
	char32_t* _internal_ptr_mut() { return data; }
	void _internal_set_len(size_t _len) { len = _len; }
	void _internal_init() { data = NULL; len = 0; }
	void _internal_reallocate(size_t _cap) { reallocate(_cap); }
private:
	void reallocate(size_t newsize);
	char32_t* data;
	size_t len;
	size_t allocated;
	const char* c_str_buf;
	memtracker::autorelease tracker;
};

inline bool operator<(const text& a, const text& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const text& a, const text& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const text& a, const text& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const text& a, const text& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const text& a, const text& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const text& a, const text& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const text& a, const text& b) throw() { return text::concatenate(a, b); }
inline text& operator+=(text& a, const text& b) throw() { return text::concatenate_inplace(a, b); }
inline bool operator<(const std::string& a, const text& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const std::string& a, const text& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const std::string& a, const text& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const std::string& a, const text& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const std::string& a, const text& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const std::string& a, const text& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const std::string& a, const text& b) throw() { return text::concatenate(a, b); }
inline bool operator<(const std::u32string& a, const text& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const std::u32string& a, const text& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const std::u32string& a, const text& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const std::u32string& a, const text& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const std::u32string& a, const text& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const std::u32string& a, const text& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const std::u32string& a, const text& b) throw() { return text::concatenate(a, b); }
inline bool operator<(const char* a, const text& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const char* a, const text& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const char* a, const text& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const char* a, const text& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const char* a, const text& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const char* a, const text& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const char* a, const text& b) throw() { return text::concatenate(a, b); }
inline bool operator<(const char32_t* a, const text& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const char32_t* a, const text& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const char32_t* a, const text& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const char32_t* a, const text& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const char32_t* a, const text& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const char32_t* a, const text& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const char32_t* a, const text& b) throw() { return text::concatenate(a, b); }
inline bool operator<(const text& a, const std::string& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const text& a, const std::string& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const text& a, const std::string& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const text& a, const std::string& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const text& a, const std::string& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const text& a, const std::string& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const text& a, const std::string& b) throw() { return text::concatenate(a, b); }
inline text& operator+=(text& a, const std::string& b) throw() { return text::concatenate_inplace(a, b); }
inline bool operator<(const text& a, const std::u32string& b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const text& a, const std::u32string& b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const text& a, const std::u32string& b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const text& a, const std::u32string& b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const text& a, const std::u32string& b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const text& a, const std::u32string& b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const text& a, const std::u32string& b) throw() { return text::concatenate(a, b); }
inline text& operator+=(text& a, const std::u32string& b) throw() { return text::concatenate_inplace(a, b); }
inline bool operator<(const text& a, const char* b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const text& a, const char* b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const text& a, const char* b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const text& a, const char* b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const text& a, const char* b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const text& a, const char* b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const text& a, const char* b) throw() { return text::concatenate(a, b); }
inline text& operator+=(text& a, const char* b) throw() { return text::concatenate_inplace(a, b); }
inline bool operator<(const text& a, const char32_t* b) throw() { return text::compare(a, b) < 0; }
inline bool operator<=(const text& a, const char32_t* b) throw() { return text::compare(a, b) <= 0; }
inline bool operator==(const text& a, const char32_t* b) throw() { return text::compare(a, b) == 0; }
inline bool operator!=(const text& a, const char32_t* b) throw() { return text::compare(a, b) != 0; }
inline bool operator>=(const text& a, const char32_t* b) throw() { return text::compare(a, b) >= 0; }
inline bool operator>(const text& a, const char32_t* b) throw() { return text::compare(a, b) > 0; }
inline text operator+(const text& a, const char32_t* b) throw() { return text::concatenate(a, b); }
inline text& operator+=(text& a, const char32_t* b) throw() { return text::concatenate_inplace(a, b); }
inline std::ostream& operator<<(std::ostream& os, const text& a) { a.ostream_helper(os); return os; }


#endif
