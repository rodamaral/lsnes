#ifndef _library__mathexpr_format__hpp__included__
#define _library__mathexpr_format__hpp__included__

#include "mathexpr.hpp"

std::string math_format_bool(bool v, mathexpr_format fmt);
std::string math_format_unsigned(uint64_t v, mathexpr_format fmt);
std::string math_format_signed(int64_t v, mathexpr_format fmt);
std::string math_format_float(double v, mathexpr_format fmt);
std::string math_format_complex(double vr, double vi, mathexpr_format fmt);
std::string math_format_string(std::string v, mathexpr_format fmt);

#endif
