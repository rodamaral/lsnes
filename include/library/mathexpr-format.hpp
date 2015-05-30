#ifndef _library__mathexpr_format__hpp__included__
#define _library__mathexpr_format__hpp__included__

#include "mathexpr.hpp"

namespace mathexpr
{
text format_bool(bool v, _format fmt);
text format_unsigned(uint64_t v, _format fmt);
text format_signed(int64_t v, _format fmt);
text format_float(double v, _format fmt);
text format_complex(double vr, double vi, _format fmt);
text format_string(text v, _format fmt);
}

#endif
