#ifndef _library__mathexpr_format__hpp__included__
#define _library__mathexpr_format__hpp__included__

#include "mathexpr.hpp"

namespace mathexpr
{
std::string format_bool(bool v, _format fmt);
std::string format_unsigned(uint64_t v, _format fmt);
std::string format_signed(int64_t v, _format fmt);
std::string format_float(double v, _format fmt);
std::string format_complex(double vr, double vi, _format fmt);
std::string format_string(std::string v, _format fmt);
}

#endif
