#include "mathexpr-format.hpp"
#include "string.hpp"

namespace mathexpr
{
namespace
{
	std::string pad(const std::string& orig, int width, bool zeropad, bool nosign)
	{
		std::string out = orig;
		if(!nosign && (orig == "" || (orig[0] != '+' && orig[0] != '-')))
			nosign = true;
		if(nosign) {
			while((int)out.length() < width)
				out = (zeropad ? "0" : " ") + out;
		} else {
			if(zeropad)
				while((int)out.length() < width)
					out = out.substr(0,1) + "0" + out.substr(1);
			else
				while((int)out.length() < width)
					out = " " + out;
		}
		return out;
	}

	std::string int_to_bits(uint64_t v)
	{
		if(v < 2) return std::string(1, 48 + v);
		return int_to_bits(v >> 1) + std::string(1, 48 + (v & 1));
	}

	std::string format_float_10(double v, _format fmt)
	{
		std::string format;
		format += "%";
		if(fmt.showsign)
			format += "+";
		if(fmt.fillzeros)
			format += "0";
		if(fmt.width >= 0)
			format += (stringfmt() << fmt.width).str();
		if(fmt.precision >= 0)
			format += (stringfmt() << "." << fmt.precision).str();
		format += "g";
		char buffer[256];
		snprintf(buffer, sizeof(buffer) - 1, format.c_str(), v);
		return buffer;
	}
}

std::string format_bool(bool v, _format fmt)
{
	if(fmt.type == _format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	return format_unsigned(v ? 1 : 0, fmt);
}

std::string format_unsigned(uint64_t v, _format fmt)
{
	if(fmt.type == _format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	if(fmt.type == _format::STRING)
		return "<#Badformat>";
	if(fmt.type == _format::DEFAULT)
		return (stringfmt() << v).str();
	std::ostringstream _out;
	switch(fmt.type) {
	case _format::BINARY:
		_out << int_to_bits(v);
		break;
	case _format::OCTAL:
		_out << std::oct << v;
		break;
	case _format::DECIMAL:
		_out << v;
		break;
	case _format::HEXADECIMAL:
		if(fmt.uppercasehex) _out << std::uppercase;
		_out << std::hex << v;
		break;
	case _format::BOOLEAN:
	case _format::STRING:
	case _format::DEFAULT:
		;
	}
	std::string out = _out.str();
	if(fmt.showsign)
		out = "+" + out;
	if(fmt.precision > 0) {
		out += ".";
		for(int i = 0; i < fmt.precision; i++)
			out += "0";
	}
	return pad(out, fmt.width, fmt.fillzeros, false);
}

std::string format_signed(int64_t v, _format fmt)
{
	if(fmt.type == _format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	if(fmt.type == _format::STRING)
		return "<#Badformat>";
	if(fmt.type == _format::DEFAULT)
		return (stringfmt() << v).str();
	std::ostringstream _out;
	switch(fmt.type) {
	case _format::BINARY:
		if(v < 0) _out << "-";
		_out << int_to_bits(std::abs(v));
		break;
	case _format::OCTAL:
		if(v < 0) _out << "-";
		_out << std::oct << std::abs(v);
		break;
	case _format::DECIMAL:
		_out << v;
		break;
	case _format::HEXADECIMAL:
		if(v < 0) _out << "-";
		if(fmt.uppercasehex) _out << std::uppercase;
		_out << std::hex << std::abs(v);
		break;
	case _format::BOOLEAN:
	case _format::STRING:
	case _format::DEFAULT:
		;
	}
	std::string out = _out.str();
	if(fmt.showsign && v >= 0)
		out = "+" + out;
	if(fmt.precision > 0) {
		out += ".";
		for(int i = 0; i < fmt.precision; i++)
			out += "0";
	}
	return pad(out, fmt.width, fmt.fillzeros, false);
}

std::string format_float(double v, _format fmt)
{
	if(fmt.type == _format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	if(fmt.type == _format::STRING)
		return "<#Badformat>";
	if(fmt.type == _format::DEFAULT)
		return (stringfmt() << v).str();
	std::string out;
	switch(fmt.type) {
	case _format::BINARY:
		//out = format_float_base(v, fmt, 2);
		//break;
		return "<#Badbase>";
	case _format::OCTAL:
		//out = format_float_base(v, fmt, 8);
		//break;
		return "<#Badbase>";
	case _format::DECIMAL:
		//out = format_float_base(v, fmt, 10);
		out = format_float_10(v, fmt);
		break;
	case _format::HEXADECIMAL:
		//out = format_float_base(v, fmt, 16);
		//break;
		return "<#Badbase>";
	case _format::BOOLEAN:
	case _format::STRING:
	case _format::DEFAULT:
		;
	}
	return pad(out, fmt.width, fmt.fillzeros, false);
}

std::string format_complex(double vr, double vi, _format fmt)
{
	if(fmt.type == _format::BOOLEAN)
		return pad((vr || vi) ? "true" : "false", fmt.width, false, true);
	if(fmt.type == _format::STRING)
		return "<#Badformat>";
	if(fmt.type == _format::DEFAULT) {
		if(vi >= 0)
			return (stringfmt() << vr << "+" << vi << "i").str();
		else
			return (stringfmt() << vr << vi << "i").str();
	}
	std::string xr = format_float(vr, fmt);
	fmt.showsign = true;
	std::string xi = format_float(vi, fmt);
	return xr + xi + "i";
}

std::string format_string(std::string v, _format fmt)
{
	if(fmt.type == _format::BOOLEAN)
		return pad((v != "") ? "true" : "false", fmt.width, false, true);
	if(fmt.type != _format::STRING)
		return "<#Badformat>";
	if(fmt.precision > 0 && (ssize_t)v.length() > fmt.precision)
		v = v.substr(0, fmt.precision);
	return pad(v, fmt.width, false, true);
}
}
