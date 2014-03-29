#include "mathexpr-format.hpp"
#include "string.hpp"

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

	const char* numbers_l = "0123456789abcdef";
	const char* numbers_u = "0123456789ABCDEF";

	std::string print_decimals(double tail, unsigned base, int places, bool trimzero, bool uppercase)
	{
		std::string y;
		for(int i = 0; i < places; i++) {
			uint32_t n = floor(tail);
			y += std::string(1, (uppercase ? numbers_u : numbers_l)[n % base]);
			tail *= base;
			tail = tail - n;
		}
		if(trimzero) {
			size_t p = y.find_last_not_of("0");
			if(p < y.length())
				y = y.substr(0, p);
		}
		return y;
	}

	std::string format_float_base(double v, mathexpr_format fmt, unsigned base)
	{
		bool exponential = false;
		std::string out;
		char expsep = (base == 16) ? 'g' : 'e';
		if(fmt.uppercasehex) expsep -= 32;
		int exponent = 0;
		v += 0.5 * pow(base, fmt.precision);  //Round.
again:
		out = "";
		if(fmt.showsign || v < 0)
			out += (v < 0) ? "-" : "+";
		if(exponential) {
			//TODO.
			out += std::string(1, expsep);
			out += (stringfmt() << exponent).str();
		} else {
			//TODO: Print whole part.
			double tail = v - floor(v);
			if(fmt.precision < 0) {
				//Print up to 5 places.
				std::string y = print_decimals(tail, base, fmt.precision, true, fmt.uppercasehex);
				if(y != "")
					out = out + "." + y;
			} else if(fmt.precision > 0) {
				//Print . and fmt.precision places.
				out += ".";
				out += print_decimals(tail, base, fmt.precision, false, fmt.uppercasehex);
			}
		}
		if(!exponential && (ssize_t)out.length() > fmt.width) {
			exponential = true;
			goto again;
		}
		return out;
	}

	std::string format_float_10(double v, mathexpr_format fmt)
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

std::string math_format_bool(bool v, mathexpr_format fmt)
{
	if(fmt.type == mathexpr_format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	return math_format_unsigned(v ? 1 : 0, fmt);
}

std::string math_format_unsigned(uint64_t v, mathexpr_format fmt)
{
	if(fmt.type == mathexpr_format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	if(fmt.type == mathexpr_format::STRING)
		return "<#Badformat>";
	if(fmt.type == mathexpr_format::DEFAULT)
		return (stringfmt() << v).str();
	std::ostringstream _out;
	switch(fmt.type) {
	case mathexpr_format::BINARY:
		_out << int_to_bits(v);
		break;
	case mathexpr_format::OCTAL:
		_out << std::oct << v;
		break;
	case mathexpr_format::DECIMAL:
		_out << v;
		break;
	case mathexpr_format::HEXADECIMAL:
		if(fmt.uppercasehex) _out << std::uppercase;
		_out << std::hex << v;
		break;
	case mathexpr_format::BOOLEAN:
	case mathexpr_format::STRING:
	case mathexpr_format::DEFAULT:
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

std::string math_format_signed(int64_t v, mathexpr_format fmt)
{
	if(fmt.type == mathexpr_format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	if(fmt.type == mathexpr_format::STRING)
		return "<#Badformat>";
	if(fmt.type == mathexpr_format::DEFAULT)
		return (stringfmt() << v).str();
	std::ostringstream _out;
	switch(fmt.type) {
	case mathexpr_format::BINARY:
		if(v < 0) _out << "-";
		_out << int_to_bits(std::abs(v));
		break;
	case mathexpr_format::OCTAL:
		if(v < 0) _out << "-";
		_out << std::oct << std::abs(v);
		break;
	case mathexpr_format::DECIMAL:
		_out << v;
		break;
	case mathexpr_format::HEXADECIMAL:
		if(v < 0) _out << "-";
		if(fmt.uppercasehex) _out << std::uppercase;
		_out << std::hex << std::abs(v);
		break;
	case mathexpr_format::BOOLEAN:
	case mathexpr_format::STRING:
	case mathexpr_format::DEFAULT:
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

std::string math_format_float(double v, mathexpr_format fmt)
{
	if(fmt.type == mathexpr_format::BOOLEAN)
		return pad(v ? "true" : "false", fmt.width, false, true);
	if(fmt.type == mathexpr_format::STRING)
		return "<#Badformat>";
	if(fmt.type == mathexpr_format::DEFAULT)
		return (stringfmt() << v).str();
	std::string out;
	switch(fmt.type) {
	case mathexpr_format::BINARY:
		//out = format_float_base(v, fmt, 2);
		//break;
		return "<#Badbase>";
	case mathexpr_format::OCTAL:
		//out = format_float_base(v, fmt, 8);
		//break;
		return "<#Badbase>";
	case mathexpr_format::DECIMAL:
		//out = format_float_base(v, fmt, 10);
		out = format_float_10(v, fmt);
		break;
	case mathexpr_format::HEXADECIMAL:
		//out = format_float_base(v, fmt, 16);
		//break;
		return "<#Badbase>";
	case mathexpr_format::BOOLEAN:
	case mathexpr_format::STRING:
	case mathexpr_format::DEFAULT:
		;
	}
	return pad(out, fmt.width, fmt.fillzeros, false);
}

std::string math_format_complex(double vr, double vi, mathexpr_format fmt)
{
	if(fmt.type == mathexpr_format::BOOLEAN)
		return pad((vr || vi) ? "true" : "false", fmt.width, false, true);
	if(fmt.type == mathexpr_format::STRING)
		return "<#Badformat>";
	if(fmt.type == mathexpr_format::DEFAULT) {
		if(vi >= 0)
			return (stringfmt() << vr << "+" << vi << "i").str();
		else
			return (stringfmt() << vr << vi << "i").str();
	}
	std::string xr = math_format_float(vr, fmt);
	fmt.showsign = true;
	std::string xi = math_format_float(vi, fmt);
	return xr + xi + "i";
}

std::string math_format_string(std::string v, mathexpr_format fmt)
{
	if(fmt.type == mathexpr_format::BOOLEAN)
		return pad((v != "") ? "true" : "false", fmt.width, false, true);
	if(fmt.type != mathexpr_format::STRING)
		return "<#Badformat>";
	if(fmt.precision > 0 && (ssize_t)v.length() > fmt.precision)
		v = v.substr(0, fmt.precision);
	return pad(v, fmt.width, false, true);
}
