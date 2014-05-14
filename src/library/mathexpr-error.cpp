#include "mathexpr-error.hpp"

namespace mathexpr
{
error::error(errorcode _code, const std::string& message)
	: std::runtime_error(message), code(_code)
{
}

error::errorcode error::get_code()
{
	return code;
}

const char* error::get_short_error()
{
	switch(code) {
	case UNDEFINED:		return "#Undefined";
	case CIRCULAR:		return "#Circular";
	case TYPE_MISMATCH:	return "#Type";
	case INTERNAL:		return "#Internal";
	case WDOMAIN:		return "#Domain";
	case DIV_BY_0:		return "#Div-by-0";
	case LOG_BY_0:		return "#Log-of-0";
	case ARGCOUNT:		return "#Argcount";
	case SIZE:		return "#Badsize";
	case ADDR:		return "#Badaddr";
	case FORMAT:		return "#Badformat";
	case UNKNOWN:		return "#???";
	default:		return "#Unknownerr";
	};
}
}
