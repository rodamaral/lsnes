#ifndef _library__int24__hpp__incuded__
#define _library__int24__hpp__incuded__

#include <cstdint>

class ss_uint24_t
{
public:
	ss_uint24_t() throw();
	ss_uint24_t(uint32_t v) throw();
	operator uint32_t() const throw();
	bool operator<(const ss_uint24_t& v) const throw() { return (uint32_t)*this < (uint32_t)v; }
	bool operator<=(const ss_uint24_t& v) const throw() { return (uint32_t)*this <= (uint32_t)v; }
	bool operator==(const ss_uint24_t& v) const throw() { return (uint32_t)*this == (uint32_t)v; }
	bool operator!=(const ss_uint24_t& v) const throw() { return (uint32_t)*this != (uint32_t)v; }
	bool operator>=(const ss_uint24_t& v) const throw() { return (uint32_t)*this >= (uint32_t)v; }
	bool operator>(const ss_uint24_t& v) const throw() { return (uint32_t)*this > (uint32_t)v; }
	bool operator==(uint32_t v) const throw() { return (uint32_t)*this == (uint32_t)v; }
	bool operator!=(uint32_t v) const throw() { return (uint32_t)*this != (uint32_t)v; }
private:
	char v[3];
};

class ss_int24_t
{
public:
	ss_int24_t() throw();
	ss_int24_t(int32_t v) throw();
	ss_int24_t(const ss_uint24_t& v) throw();
	operator int32_t() const throw();
	bool operator<(const ss_int24_t& v) const throw() { return (int32_t)*this < (int32_t)v; }
	bool operator<=(const ss_int24_t& v) const throw() { return (int32_t)*this <= (int32_t)v; }
	bool operator==(const ss_int24_t& v) const throw() { return (int32_t)*this == (int32_t)v; }
	bool operator!=(const ss_int24_t& v) const throw() { return (int32_t)*this != (int32_t)v; }
	bool operator>=(const ss_int24_t& v) const throw() { return (int32_t)*this >= (int32_t)v; }
	bool operator>(const ss_int24_t& v) const throw() { return (int32_t)*this > (int32_t)v; }
	bool operator==(int32_t v) const throw() { return (uint32_t)*this == (uint32_t)v; }
	bool operator!=(int32_t v) const throw() { return (uint32_t)*this != (uint32_t)v; }
private:
	char v[3];
};

template<typename T> struct is_ss_int24 { const static bool flag = false; };
template<> struct is_ss_int24<ss_uint24_t> { const static bool flag = true; };
template<> struct is_ss_int24<ss_int24_t> { const static bool flag = true; };


#endif
