#include "mathexpr-ntype.hpp"
#include "mathexpr-error.hpp"
#include "mathexpr-format.hpp"
#include "string.hpp"
#include <functional>
#include <iostream>
#include <map>

namespace mathexpr
{
namespace
{
	void throw_domain(const std::string& err)
	{
		throw error(error::WDOMAIN, err);
	}

	template<typename T> int _cmp_values(T a, T b)
	{
		if(a < b)
			return -1;
		if(a > b)
			return 1;
		return 0;
	}

	class expr_val;

	class expr_val_numeric
	{
		enum _type
		{
			T_UNSIGNED,
			T_SIGNED,
			T_FLOAT,
			T_COMPLEX,
		};
	public:
		struct unsigned_tag {};
		struct signed_tag {};
		struct float_tag {};
		struct complex_tag {};
		expr_val_numeric()
		{
			type = T_SIGNED;
			v_signed = 0;
			v_imag = 0;
		}
		expr_val_numeric(unsigned_tag, uint64_t v)
		{
			type = T_UNSIGNED;
			v_unsigned = v;
			v_imag = 0;
		}
		expr_val_numeric(signed_tag, int64_t v)
		{
			type = T_SIGNED;
			v_signed = v;
			v_imag = 0;
		}
		expr_val_numeric(float_tag, double v)
		{
			type = T_FLOAT;
			v_float = v;
			v_imag = 0;
		}
		expr_val_numeric(complex_tag, double re, double im)
		{
			type = T_COMPLEX;
			v_float = re;
			v_imag = im;
		}
		expr_val_numeric(const std::string& str)
		{
			if(str == "i") {
				type = T_COMPLEX;
				v_float = 0;
				v_imag = 1;
			} else if(regex("[0-9]+|0x[0-9a-fA-F]+", str)) {
				//UNSIGNED.
				v_unsigned = parse_value<uint64_t>(str);
				v_imag = 0;
				type = T_UNSIGNED;
			} else if(regex("[+-][0-9]+|[+-]0x[0-9a-fA-F]+", str)) {
				//SIGNED.
				v_signed = parse_value<int64_t>(str);
				v_imag = 0;
				type = T_SIGNED;
			} else if(regex("[+-]?([0-9]+|[0-9]*\\.[0-9]+)([eE][0-9]+)?", str)) {
				//FLOAT.
				v_float = parse_value<double>(str);
				v_imag = 0;
				type = T_FLOAT;
			} else
				throw std::runtime_error("Bad number '" + str + "'");
		}
		double as_float() const
		{
			switch(type) {
			case T_UNSIGNED:	return v_unsigned;
			case T_SIGNED:		return v_signed;
			case T_FLOAT:		return v_float;
			case T_COMPLEX:		return v_float;
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		int64_t as_signed() const
		{
			switch(type) {
			case T_UNSIGNED:	return v_unsigned;
			case T_SIGNED:		return v_signed;
			case T_FLOAT:		return v_float;
			case T_COMPLEX:		return v_float;
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		uint64_t as_unsigned() const
		{
			switch(type) {
			case T_UNSIGNED:	return v_unsigned;
			case T_SIGNED:		return v_signed;
			case T_FLOAT:		return v_float;
			case T_COMPLEX:		return v_float;
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		std::string tostring()
		{
			switch(type) {
			case T_UNSIGNED:
				return (stringfmt() << v_unsigned).str();
			case T_SIGNED:
				return (stringfmt() << v_signed).str();
			case T_FLOAT:
				//FIXME: Saner formatting.
				return (stringfmt() << v_float).str();
			case T_COMPLEX:
				//FIXME: Saner formatting.
				if(v_imag < 0)
					return (stringfmt() << v_float << v_imag << "*i").str();
				if(v_imag > 0)
					return (stringfmt() << v_float << "+" << v_imag << "*i").str();
				return (stringfmt() << v_float).str();
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		uint64_t tounsigned()
		{
			return as_unsigned();
		}
		int64_t tosigned()
		{
			return as_signed();
		}
		void scale(uint64_t scale)
		{
			switch(type) {
			case T_UNSIGNED:
				type = T_FLOAT;
				v_float = (1.0 * v_unsigned / scale);
				break;
			case T_SIGNED:
				type = T_FLOAT;
				v_float = (1.0 * v_signed / scale);
				break;
			case T_COMPLEX:
				v_imag /= scale;
			case T_FLOAT:
				v_float /= scale;
				break;
			}
		}
		bool toboolean()
		{
			switch(type) {
			case T_UNSIGNED:	return (v_unsigned != 0);
			case T_SIGNED:		return (v_signed != 0);
			case T_FLOAT:		return (v_float != 0);
			case T_COMPLEX:		return (v_float != 0) || (v_imag != 0);
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		std::string format(_format fmt)
		{
			switch(type) {
			case T_UNSIGNED: return format_unsigned(v_unsigned, fmt);
			case T_SIGNED: return format_signed(v_signed, fmt);
			case T_FLOAT: return format_float(v_float, fmt);
			case T_COMPLEX: return format_complex(v_float, v_imag, fmt);
			}
			throw error(error::INTERNAL, "Don't know how to print numeric type");
		}
		expr_val_numeric operator~() const
		{
			switch(type) {
			case T_UNSIGNED:	return expr_val_numeric(unsigned_tag(), ~v_unsigned);
			case T_SIGNED:		return expr_val_numeric(signed_tag(), ~v_signed);
			case T_FLOAT:		throw_domain("Bit operations are only for integers");
			case T_COMPLEX:		throw_domain("Bit operations are only for integers");
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		expr_val_numeric operator&(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				throw_domain("Bit operations are only for integers");
			if(type == T_FLOAT || b.type == T_FLOAT)
				throw_domain("Bit operations are only for integers");
			if(type == T_SIGNED || b.type == T_SIGNED)
				return expr_val_numeric(signed_tag(), as_signed() & b.as_signed());
			return expr_val_numeric(unsigned_tag(), as_unsigned() & b.as_unsigned());
		}
		expr_val_numeric operator|(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				throw_domain("Bit operations are only for integers");
			if(type == T_FLOAT || b.type == T_FLOAT)
				throw_domain("Bit operations are only for integers");
			if(type == T_SIGNED || b.type == T_SIGNED)
				return expr_val_numeric(signed_tag(), as_signed() | b.as_signed());
			return expr_val_numeric(unsigned_tag(), as_unsigned() | b.as_unsigned());
		}
		expr_val_numeric operator^(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				throw_domain("Bit operations are only for integers");
			if(type == T_FLOAT || b.type == T_FLOAT)
				throw_domain("Bit operations are only for integers");
			if(type == T_SIGNED || b.type == T_SIGNED)
				return expr_val_numeric(signed_tag(), as_signed() ^ b.as_signed());
			return expr_val_numeric(unsigned_tag(), as_unsigned() ^ b.as_unsigned());
		}
		expr_val_numeric operator-() const
		{
			switch(type) {
			case T_COMPLEX:		return expr_val_numeric(complex_tag(), -v_float, -v_imag);
			case T_UNSIGNED:	return expr_val_numeric(signed_tag(), -(int64_t)v_unsigned);
			case T_SIGNED:		return expr_val_numeric(signed_tag(), -v_signed);
			case T_FLOAT:		return expr_val_numeric(float_tag(), -v_float);
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		expr_val_numeric operator+(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				return expr_val_numeric(complex_tag(), as_float() + b.as_float(),
					v_imag + b.v_imag);
			if(type == T_FLOAT || b.type == T_FLOAT)
				return expr_val_numeric(float_tag(), as_float() + b.as_float());
			if(type == T_SIGNED || b.type == T_SIGNED)
				return expr_val_numeric(signed_tag(), as_signed() + b.as_signed());
			return expr_val_numeric(unsigned_tag(), as_unsigned() + b.as_unsigned());
		}
		expr_val_numeric operator-(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				return expr_val_numeric(complex_tag(), as_float() - b.as_float(),
					v_imag - b.v_imag);
			if(type == T_FLOAT || b.type == T_FLOAT)
				return expr_val_numeric(float_tag(), as_float() - b.as_float());
			if(type == T_SIGNED || b.type == T_SIGNED)
				return expr_val_numeric(signed_tag(), as_signed() - b.as_signed());
			return expr_val_numeric(unsigned_tag(), as_unsigned() - b.as_unsigned());
		}
		expr_val_numeric operator*(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				return expr_val_numeric(complex_tag(), as_float() * b.as_float() - v_imag * b.v_imag,
					as_float() * b.v_imag + b.as_float() * v_imag);
			if(type == T_FLOAT || b.type == T_FLOAT)
				return expr_val_numeric(float_tag(), as_float() * b.as_float());
			if(type == T_SIGNED || b.type == T_SIGNED)
				return expr_val_numeric(signed_tag(), as_signed() * b.as_signed());
			return expr_val_numeric(unsigned_tag(), as_unsigned() * b.as_unsigned());
		}
		expr_val_numeric operator/(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX) {
				double div = b.as_float() * b.as_float() + b.v_imag * b.v_imag;
				if(div == 0)
					throw error(error::DIV_BY_0, "Division by 0");
				return expr_val_numeric(complex_tag(),
					(as_float() * b.as_float() + v_imag * b.v_imag) / div,
					(v_imag * b.as_float() - as_float() * b.v_imag) / div);
			}
			if(type == T_FLOAT || b.type == T_FLOAT) {
				if(b.as_float() == 0)
					throw error(error::DIV_BY_0, "Division by 0");
				return expr_val_numeric(float_tag(), as_float() / b.as_float());
			}
			if(type == T_SIGNED || b.type == T_SIGNED) {
				if(b.as_signed() == 0)
					throw error(error::DIV_BY_0, "Division by 0");
				return expr_val_numeric(signed_tag(), as_signed() / b.as_signed());
			}
			if(b.as_unsigned() == 0)
				throw error(error::DIV_BY_0, "Division by 0");
			return expr_val_numeric(unsigned_tag(), as_unsigned() / b.as_unsigned());
		}
		expr_val_numeric operator%(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				throw_domain("Remainder is only for integers");
			if(type == T_FLOAT || b.type == T_FLOAT)
				throw_domain("Remainder is only for integers");
			if(type == T_SIGNED || b.type == T_SIGNED) {
				if(b.as_signed() == 0)
					throw error(error::DIV_BY_0, "Division by 0");
				return expr_val_numeric(signed_tag(), as_signed() / b.as_signed());
			}
			if(b.as_unsigned() == 0)
				throw error(error::DIV_BY_0, "Division by 0");
			return expr_val_numeric(unsigned_tag(), as_unsigned() / b.as_unsigned());
		}
		expr_val_numeric log() const
		{
			if(type == T_COMPLEX) {
				double mag = as_float() * as_float() + v_imag * v_imag;
				if(mag == 0)
					throw error(error::LOG_BY_0, "Can't take logarithm of 0");
				double r = 0.5 * ::log(mag);
				double i = ::atan2(v_imag, as_float());
				return expr_val_numeric(complex_tag(), r, i);
			}
			if(as_float() == 0)
				throw error(error::LOG_BY_0, "Can't take logarithm of 0");
			if(as_float() <= 0)
				return expr_val_numeric(complex_tag(), ::log(std::abs(as_float())), 4 * ::atan(1));
			return expr_val_numeric(float_tag(), ::log(as_float()));
		}
		static expr_val_numeric log2(expr_val_numeric a, expr_val_numeric b)
		{
			return b.log() / a.log();
		}
		expr_val_numeric exp() const
		{
			if(type == T_COMPLEX) {
				double mag = ::exp(as_float());
				return expr_val_numeric(complex_tag(), mag * ::cos(v_imag),
					mag * ::sin(v_imag));
			}
			return expr_val_numeric(float_tag(), ::exp(as_float()));
		}
		static expr_val_numeric exp2(expr_val_numeric a, expr_val_numeric b)
		{
			expr_val_numeric tmp = b * a.log();
			return tmp.exp();
		}
		expr_val_numeric sqrt() const
		{
			if(as_float() < 0 && type != T_COMPLEX)
				return expr_val_numeric(complex_tag(), 0, ::sqrt(-as_float()));
			if(type == T_COMPLEX) {
				double mag = ::sqrt(::sqrt(as_float() * as_float() + v_imag * v_imag));
				double ar = 0.5 * ::atan2(v_imag, as_float());
				return expr_val_numeric(complex_tag(), mag * ::cos(ar), mag * ::sin(ar));
			}
			return expr_val_numeric(float_tag(), ::sqrt(as_float()));
		}
		expr_val_numeric sin() const
		{
			if(type == T_COMPLEX) {
				return expr_val_numeric(complex_tag(), ::sin(as_float()) * ::cosh(v_imag),
					::cos(as_float()) * ::sinh(v_imag));
			}
			return expr_val_numeric(float_tag(), ::sin(as_float()));
		}
		expr_val_numeric cos() const
		{
			if(type == T_COMPLEX) {
				return expr_val_numeric(complex_tag(), ::cos(as_float()) * ::cosh(v_imag),
					-::sin(as_float()) * ::sinh(v_imag));
			}
			return expr_val_numeric(float_tag(), ::cos(as_float()));
		}
		expr_val_numeric tan() const
		{
			return sin()/cos();
		}
		expr_val_numeric atan() const
		{
			if(type == T_COMPLEX) {
				expr_val_numeric x = expr_val_numeric(complex_tag(), 0, 1) * *this;
				expr_val_numeric n = expr_val_numeric(complex_tag(), 1, 0) + x;
				expr_val_numeric d = expr_val_numeric(complex_tag(), 1, 0) - x;
				expr_val_numeric y = n / d;
				expr_val_numeric w = y.log();
				return w / expr_val_numeric(complex_tag(), 0, 2);
			}
			return expr_val_numeric(float_tag(), ::atan(as_float()));
		}
		expr_val_numeric acos() const
		{
			expr_val_numeric sinesqr = (expr_val_numeric(float_tag(), 1) - *this * *this);
			expr_val_numeric sine = sinesqr.sqrt();
			expr_val_numeric tangent = sine / *this;
			return tangent.atan();
		}
		expr_val_numeric asin() const
		{
			expr_val_numeric cosinesqr = (expr_val_numeric(float_tag(), 1) - *this * *this);
			expr_val_numeric cosine = cosinesqr.sqrt();
			expr_val_numeric tangent = *this / cosine;
			return tangent.atan();
		}
		expr_val_numeric sinh() const
		{
			return (exp() - (-*this).exp()) / expr_val_numeric(float_tag(), 2);
		}
		expr_val_numeric cosh() const
		{
			return (exp() + (-*this).exp()) / expr_val_numeric(float_tag(), 2);
		}
		expr_val_numeric tanh() const
		{
			return sinh() / cosh();
		}
		expr_val_numeric arsinh() const
		{
			//x - 1/x = 2u
			//x^2 - 2ux - 1 = 0
			//(x-u)^2 - x^2 + 2ux - u^2 + x^2 - 2ux - 1 = 0
			//(x-u)^2 = u^2 + 1
			expr_val_numeric xmu = (*this * *this) + expr_val_numeric(float_tag(), 1);
			expr_val_numeric x = xmu.sqrt() + *this;
			return x.log();
		}
		expr_val_numeric arcosh() const
		{
			expr_val_numeric xmu = (*this * *this) - expr_val_numeric(float_tag(), 1);
			expr_val_numeric x = xmu.sqrt() + *this;
			return x.log();
		}
		expr_val_numeric artanh() const
		{
			//(x-1/x)/(x+1/x)=u
			//x^2=u+1/(1-u)
			expr_val_numeric t(float_tag(), 1);
			return ((t + *this) / (t - *this)).sqrt().log();
		}
		static expr_val_numeric atan2(expr_val_numeric a, expr_val_numeric b)
		{
			if(a.type == T_COMPLEX || b.type == T_COMPLEX)
				throw_domain("atan2 is only for reals");
			return expr_val_numeric(float_tag(), ::atan2(a.as_float(), b.as_float()));
		}
		expr_val_numeric torad() const
		{
			return expr_val_numeric(float_tag(), ::atan(1) / 45 * as_float());
		}
		expr_val_numeric todeg() const
		{
			return expr_val_numeric(float_tag(), 45 / ::atan(1) * as_float());
		}
		static expr_val_numeric shift(expr_val_numeric a, expr_val_numeric b, bool inv)
		{
			int64_t s = b.as_signed();
			if(inv)
				s = -s;
			if(a.type == T_SIGNED) {
				int64_t _a = a.v_signed;
				if(s < -63)
					return expr_val_numeric(signed_tag(), -1);
				if(s > 63)
					return expr_val_numeric(signed_tag(), 0);
				if(s < 0) {
					uint64_t r = _a;
					uint64_t r2 = r >> -s;
					uint64_t m = 0xFFFFFFFFFFFFFFFFULL - ((1ULL << (64 - s)) - 1);
					return expr_val_numeric(signed_tag(), m | r2);
				} else if(s > 0)
					return expr_val_numeric(signed_tag(), _a << s);
				else
					return expr_val_numeric(signed_tag(), _a);
			} else if(a.type == T_UNSIGNED) {
				uint64_t _a = a.v_unsigned;
				if(s < -63 || s > 63)
					return expr_val_numeric(unsigned_tag(), 0);
				if(s < 0)
					return expr_val_numeric(unsigned_tag(), _a >> -s);
				else if(s > 0)
					return expr_val_numeric(unsigned_tag(), _a << s);
				else
					return expr_val_numeric(unsigned_tag(), _a);
			} else
				throw_domain("Bit operations are only for integers");
			return expr_val_numeric(unsigned_tag(), 0); //NOTREACHED
		}
		static expr_val_numeric op_pi()
		{
			return expr_val_numeric(float_tag(), 4 * ::atan(1));
		}
		static expr_val_numeric op_e()
		{
			return expr_val_numeric(float_tag(), ::exp(1));
		}
		bool operator==(const expr_val_numeric& b) const
		{
			if(type == T_COMPLEX || b.type == T_COMPLEX)
				return as_float() == b.as_float() && v_imag == b.v_imag;
			return (_cmp(*this, b) == 0);
		}
		static int _cmp_float_unsigned(uint64_t a, float b)
		{
			if(b < 0)
				return 1;
			//TODO: Handle values too large for exact integer representation.
			if((double)a < b)
				return -1;
			if((double)a > b)
				return 1;
			return 0;
		}
		static int _cmp_float_signed(int64_t a, float b)
		{
			//TODO: Handle values too large for exact integer representation.
			if((double)a < b)
				return -1;
			if((double)a > b)
				return 1;
			return 0;
		}
		static int _cmp(expr_val_numeric a, expr_val_numeric b)
		{
			if(a.type == T_COMPLEX || b.type == T_COMPLEX)
				throw_domain("Can't compare complex numbers");
			switch(a.type) {
			case T_UNSIGNED:
				switch(b.type) {
				case T_UNSIGNED:
					return _cmp_values(a.v_unsigned, b.v_unsigned);
				case T_SIGNED:
					if(b.v_signed < 0)
						return 1;
					if((int64_t)a.v_unsigned < 0)
						return 1;
					return _cmp_values((int64_t)a.v_unsigned, b.v_signed);
				case T_FLOAT:
					return _cmp_float_unsigned(a.v_unsigned, b.v_float);
				case T_COMPLEX:
					throw error(error::INTERNAL,
						"Internal error (shouldn't be here)");
				}
			case T_SIGNED:
				switch(b.type) {
				case T_UNSIGNED:
					if(a.v_signed < 0)
						return -1;
					if((int64_t)b.v_unsigned < 0)
						return -1;
					return _cmp_values(a.v_signed, (int64_t)b.v_unsigned);
				case T_SIGNED:
					return _cmp_values(a.v_signed, b.v_signed);
				case T_FLOAT:
					return _cmp_float_signed(a.v_signed, b.v_float);
				case T_COMPLEX:
					throw error(error::INTERNAL,
						"Internal error (shouldn't be here)");
				}
			case T_FLOAT:
				switch(b.type) {
				case T_UNSIGNED:
					return -_cmp_float_unsigned(b.v_unsigned, a.v_float);
				case T_SIGNED:
					return -_cmp_float_signed(b.v_signed, a.v_float);
				case T_FLOAT:
					if(a.v_float < b.v_float)
						return -1;
					if(a.v_float > b.v_float)
						return 1;
					return 0;
				case T_COMPLEX:
					throw error(error::INTERNAL,
						"Internal error (shouldn't be here)");
				}
			case T_COMPLEX:
				throw error(error::INTERNAL, "Internal error (shouldn't be here)");
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		static expr_val_numeric x_unsigned(expr_val_numeric a)
		{
			switch(a.type) {
			case T_UNSIGNED:	return expr_val_numeric(unsigned_tag(), a.v_unsigned);
			case T_SIGNED:		return expr_val_numeric(unsigned_tag(), a.v_signed);
			case T_FLOAT:		return expr_val_numeric(unsigned_tag(), a.v_float);
			default:		throw_domain("Can't convert non-real into unsigned");
			}
			return expr_val_numeric(unsigned_tag(), 0); //NOTREACHED.
		}
		static expr_val_numeric x_signed(expr_val_numeric a)
		{
			switch(a.type) {
			case T_UNSIGNED:	return expr_val_numeric(signed_tag(), a.v_unsigned);
			case T_SIGNED:		return expr_val_numeric(signed_tag(), a.v_signed);
			case T_FLOAT:		return expr_val_numeric(signed_tag(), a.v_float);
			default:		throw_domain("Can't convert non-real into signed");
			}
			return expr_val_numeric(signed_tag(), 0); //NOTREACHED.
		}
		static expr_val_numeric x_float(expr_val_numeric a)
		{
			switch(a.type) {
			case T_UNSIGNED:	return expr_val_numeric(float_tag(), a.v_unsigned);
			case T_SIGNED:		return expr_val_numeric(float_tag(), a.v_signed);
			case T_FLOAT:		return expr_val_numeric(float_tag(), a.v_float);
			default:		throw_domain("Can't convert non-real into float");
			}
			return expr_val_numeric(float_tag(), 0); //NOTREACHED.
		}
		expr_val_numeric re() const
		{
			if(type == T_COMPLEX)
				return expr_val_numeric(float_tag(), v_float);
			return expr_val_numeric(float_tag(), as_float());
		}
		expr_val_numeric im() const
		{
			if(type == T_COMPLEX)
				return expr_val_numeric(float_tag(), v_imag);
			return expr_val_numeric(float_tag(), 0);
		}
		expr_val_numeric conj() const
		{
			if(type == T_COMPLEX)
				return expr_val_numeric(complex_tag(), v_float, -v_imag);
			return expr_val_numeric(float_tag(), as_float());
		}
		expr_val_numeric abs() const
		{
			switch(type) {
			case T_COMPLEX:  return expr_val_numeric(float_tag(), v_float * v_float + v_imag * v_imag);
			case T_FLOAT:    return expr_val_numeric(float_tag(), ::fabs(v_float));
			case T_SIGNED:   return expr_val_numeric(signed_tag(), ::abs(v_signed));
			case T_UNSIGNED: return expr_val_numeric(unsigned_tag(), v_unsigned);
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		expr_val_numeric arg() const
		{
			switch(type) {
			case T_COMPLEX:  return expr_val_numeric(float_tag(), ::atan2(v_imag, v_float));
			default:
				if(as_float() < 0)
					return expr_val_numeric(float_tag(), 4 * ::atan(1));
				else
					return expr_val_numeric(float_tag(), 0);
			}
		}
	private:
		enum _type type;
		union {
			uint64_t v_unsigned;
			int64_t v_signed;
			double v_float;
		};
		double v_imag;
	};

	class expr_val
	{
		struct number_tag {};
		struct string_tag {};
		struct boolean_tag {};
		enum _type
		{
			T_BOOLEAN,
			T_NUMERIC,
			T_STRING,
		};
		expr_val_numeric& as_numeric()
		{
			if(type != T_NUMERIC)
				throw_domain("Can't operate with non-numbers");
			return v_numeric;
		}
	public:
		expr_val()
		{
			type = T_NUMERIC;
		}
		expr_val(const std::string& str, bool string)
		{
			if(string) {
				v_string = str;
				type = T_STRING;
			} else if(str == "false") {
				v_boolean = false;
				type = T_BOOLEAN;
			} else if(str == "true") {
				v_boolean = true;
				type = T_BOOLEAN;
			} else if(str == "e") {
				v_numeric = expr_val_numeric::op_e();
				type = T_NUMERIC;
			} else if(str == "pi") {
				v_numeric = expr_val_numeric::op_pi();
				type = T_NUMERIC;
			} else {
				v_numeric = expr_val_numeric(str);
				type = T_NUMERIC;
			}
		}
		expr_val(typeinfo_wrapper<expr_val>::unsigned_tag t, uint64_t v)
			: type(T_NUMERIC), v_numeric(expr_val_numeric::unsigned_tag(), v)
		{
		}
		expr_val(typeinfo_wrapper<expr_val>::signed_tag t, int64_t v)
			: type(T_NUMERIC), v_numeric(expr_val_numeric::signed_tag(), v)
		{
		}
		expr_val(typeinfo_wrapper<expr_val>::float_tag t, double v)
			: type(T_NUMERIC), v_numeric(expr_val_numeric::float_tag(), v)
		{
		}
		expr_val(typeinfo_wrapper<expr_val>::boolean_tag, bool b)
			: type(T_BOOLEAN), v_boolean(b)
		{
		}
		expr_val(expr_val_numeric v)
			: type(T_NUMERIC), v_numeric(v)
		{
		}
		expr_val(string_tag, std::string s)
			: type(T_STRING), v_string(s)
		{
		}
		expr_val(boolean_tag, bool b)
			: type(T_BOOLEAN), v_boolean(b)
		{
		}
		std::string tostring()
		{
			switch(type) {
			case T_BOOLEAN:
				if(v_boolean)
					return "true";
				else
					return "false";
			case T_NUMERIC:
				return v_numeric.tostring();
			case T_STRING:
				return v_string;
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		uint64_t tounsigned()
		{
			if(type != T_NUMERIC)
				throw_domain("Can't convert non-number into unsigned");
			return v_numeric.tounsigned();
		}
		int64_t tosigned()
		{
			if(type != T_NUMERIC)
				throw_domain("Can't convert non-number into signed");
			return v_numeric.tosigned();
		}
		void scale(uint64_t _scale)
		{
			if(type != T_NUMERIC)
				throw_domain("Can't scale non-number");
			v_numeric.scale(_scale);
		}
		bool toboolean()
		{
			switch(type) {
			case T_BOOLEAN:
				return v_boolean;
			case T_NUMERIC:
				return v_numeric.toboolean();
			case T_STRING:
				return (v_string.length() != 0);
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		static expr_val op_lnot(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() != 1)
				throw error(error::ARGCOUNT, "logical not takes 1 argument");
			return expr_val(boolean_tag(), !(promises[0]().toboolean()));
		}
		static expr_val op_lor(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() != 2)
				throw error(error::ARGCOUNT, "logical or takes 2 arguments");
			if(promises[0]().toboolean())
				return expr_val(boolean_tag(), true);
			return expr_val(boolean_tag(), promises[1]().toboolean());
		}
		static expr_val op_land(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() != 2)
				throw error(error::ARGCOUNT, "logical and takes 2 arguments");
			if(!(promises[0]().toboolean()))
				return expr_val(boolean_tag(), false);
			return expr_val(boolean_tag(), promises[1]().toboolean());
		}
		static expr_val fun_if(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() == 2) {
				if((promises[0]().toboolean()))
					return promises[1]();
				else
					return expr_val(boolean_tag(), false);
			} else if(promises.size() == 3) {
				if((promises[0]().toboolean()))
					return promises[1]();
				else
					return promises[2]();
			} else
				throw error(error::ARGCOUNT, "if takes 2 or 3 arguments");
		}
		static expr_val fun_select(std::vector<std::function<expr_val&()>> promises)
		{
			for(auto& i : promises) {
				expr_val v = i();
				if(v.type != T_BOOLEAN || v.v_boolean)
					return v;
			}
			return expr_val(boolean_tag(), false);
		}
		static expr_val fun_pyth(std::vector<std::function<expr_val&()>> promises)
		{
			std::vector<expr_val> v;
			for(auto& i : promises)
				v.push_back(i());
			expr_val_numeric n(expr_val_numeric::float_tag(), 0);
			expr_val_numeric one(expr_val_numeric::float_tag(), 1);
			for(auto& i : v) {
				if(i.type != T_NUMERIC)
					throw error(error::WDOMAIN, "pyth requires numeric args");
				n = n + one * i.v_numeric * i.v_numeric;
			}
			return n.sqrt();
		}
		template<expr_val (*T)(expr_val& a, expr_val& b)>
		static expr_val fun_fold(std::vector<std::function<expr_val&()>> promises)
		{
			if(!promises.size())
				return expr_val(boolean_tag(), false);
			expr_val v = promises[0]();
			for(size_t i = 1; i < promises.size(); i++)
				v = T(v, promises[i]());
			return v;
		}
		static expr_val fold_min(expr_val& a, expr_val& b)
		{
			int t = _cmp_values(a.type, b.type);
			if(t < 0)
				return a;
			if(t > 0)
				return b;
			return (_cmp(a, b) < 0) ? a : b;
		}
		static expr_val fold_max(expr_val& a, expr_val& b)
		{
			int t = _cmp_values(a.type, b.type);
			if(t < 0)
				return a;
			if(t > 0)
				return b;
			return (_cmp(a, b) > 0) ? a : b;
		}
		static expr_val fold_sum(expr_val& a, expr_val& b)
		{
			return add(a, b);
		}
		static expr_val fold_prod(expr_val& a, expr_val& b)
		{
			return mul(a, b);
		}
		template<expr_val (*T)(expr_val a, expr_val b)>
		static expr_val op_binary(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() != 2)
				throw error(error::ARGCOUNT, "Operation takes 2 arguments");
			expr_val a = promises[0]();
			expr_val b = promises[1]();
			return T(a, b);
		}
		template<expr_val (*T)(expr_val a)>
		static expr_val op_unary(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() != 1)
				throw error(error::ARGCOUNT, "Operation takes 1 argument");
			expr_val a = promises[0]();
			return T(a);
		}
		template<expr_val (*T)(expr_val a),expr_val (*U)(expr_val a, expr_val b)>
		static expr_val op_unary_binary(std::vector<std::function<expr_val&()>> promises)
		{
			if(promises.size() == 1)
				return T(promises[0]());
			if(promises.size() == 2)
				return U(promises[0](), promises[1]());
			throw error(error::ARGCOUNT, "Operation takes 1 or 2 arguments");
		}
		static expr_val bnot(expr_val a)
		{
			return ~a.as_numeric();
		}
		static expr_val band(expr_val a, expr_val b)
		{
			return a.as_numeric() & b.as_numeric();
		}
		static expr_val bor(expr_val a, expr_val b)
		{
			return a.as_numeric() | b.as_numeric();
		}
		static expr_val bxor(expr_val a, expr_val b)
		{
			return a.as_numeric() ^ b.as_numeric();
		}
		static expr_val neg(expr_val a)
		{
			return -a.as_numeric();
		}
		template<expr_val_numeric (expr_val_numeric::*T)() const>
		static expr_val f_n_fn(expr_val a)
		{
			return (a.as_numeric().*T)();
		}
		template<expr_val_numeric (*T)(expr_val_numeric x, expr_val_numeric y)>
		static expr_val f_n_fn2(expr_val a, expr_val b)
		{
			return T(a.as_numeric(), b.as_numeric());
		}
		static expr_val lshift(expr_val a, expr_val b)
		{
			return expr_val_numeric::shift(a.as_numeric(), b.as_numeric(), false);
		}
		static expr_val rshift(expr_val a, expr_val b)
		{
			return expr_val_numeric::shift(a.as_numeric(), b.as_numeric(), true);
		}
		static expr_val op_pi(std::vector<std::function<expr_val&()>> promises)
		{
			return expr_val_numeric::op_pi();
		}
		static expr_val add(expr_val a, expr_val b)
		{
			if(a.type == T_STRING && b.type == T_STRING)
				return expr_val(string_tag(), a.v_string + b.v_string);
			return a.as_numeric() + b.as_numeric();
		}
		static expr_val sub(expr_val a, expr_val b)
		{
			return a.as_numeric() - b.as_numeric();
		}
		static expr_val mul(expr_val a, expr_val b)
		{
			return a.as_numeric() * b.as_numeric();
		}
		static expr_val div(expr_val a, expr_val b)
		{
			return a.as_numeric() / b.as_numeric();
		}
		static expr_val rem(expr_val a, expr_val b)
		{
			return a.as_numeric() % b.as_numeric();
		}
		static bool _eq(const expr_val& a, const expr_val& b)
		{
			if(a.type != b.type)
				return false;
			switch(a.type) {
			case T_BOOLEAN: return (a.v_boolean == b.v_boolean);
			case T_STRING: return (a.v_string == b.v_string);
			case T_NUMERIC: return (a.v_numeric == b.v_numeric);
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		static expr_val eq(expr_val a, expr_val b)
		{
			return expr_val(boolean_tag(), _eq(a, b));
		}
		static expr_val ne(expr_val a, expr_val b)
		{
			return expr_val(boolean_tag(), !_eq(a, b));
		}
		static int _cmp(expr_val a, expr_val b)
		{
			if(a.type != b.type)
				throw_domain("Can't compare distinct value types");
			switch(a.type) {
			case T_BOOLEAN:		return _cmp_values(a.v_boolean, b.v_boolean);
			case T_STRING:		return _cmp_values(a.v_string, b.v_string);
			case T_NUMERIC:		return expr_val_numeric::_cmp(a.v_numeric, b.v_numeric);
			}
			throw error(error::INTERNAL, "Internal error (shouldn't be here)");
		}
		template<expr_val_numeric(*T)(expr_val_numeric v)>
		static expr_val x_nconv(expr_val a)
		{
			return T(a.as_numeric());
		}
		static expr_val lt(expr_val a, expr_val b)
		{
			return expr_val(boolean_tag(), _cmp(a, b) < 0);
		}
		static expr_val le(expr_val a, expr_val b)
		{
			return expr_val(boolean_tag(), _cmp(a, b) <= 0);
		}
		static expr_val ge(expr_val a, expr_val b)
		{
			return expr_val(boolean_tag(), _cmp(a, b) >= 0);
		}
		static expr_val gt(expr_val a, expr_val b)
		{
			return expr_val(boolean_tag(), _cmp(a, b) > 0);
		}
		std::string format_string(std::string val, _format fmt)
		{
			if((int)val.length() > fmt.precision && fmt.precision >= 0)
				val = val.substr(0, fmt.precision);
			while((int)val.length() < fmt.width)
				val = " " + val;
			return val;
		}
		std::string print_bool_numeric(bool val, _format fmt)
		{
			std::string out = val ? "1" : "0";
			if(fmt.precision > 0) {
				out += ".";
				for(int i = 0; i < fmt.precision; i++)
					out += "0";
			}
			while((int)out.length() < fmt.width)
				out = ((fmt.fillzeros) ? "0" : " ") + out;
			return out;
		}
		std::string format(_format fmt)
		{
			switch(type) {
			case T_BOOLEAN: return format_bool(v_boolean, fmt);
			case T_NUMERIC: return v_numeric.format(fmt);
			case T_STRING: return format_string(v_string, fmt);
			default:
				return "#Notprintable";
			}
		}
		static std::set<operinfo*> operations()
		{
			static operinfo_set<expr_val> x({
				{"-", expr_val::op_unary<expr_val::neg>, true, 1, -3, true},
				{"!", expr_val::op_lnot, true, 1, -3, true},
				{"~", expr_val::op_unary<expr_val::bnot>, true, 1, -3, true},
				{"*", expr_val::op_binary<expr_val::mul>, true, 2, -5, false},
				{"/", expr_val::op_binary<expr_val::div>, true, 2, -5, false},
				{"%", expr_val::op_binary<expr_val::rem>, true, 2, -5, false},
				{"+", expr_val::op_binary<expr_val::add>, true, 2, -6, false},
				{"-", expr_val::op_binary<expr_val::sub>, true, 2, -6, false},
				{"<<", expr_val::op_binary<expr_val::lshift>, true, 2, -7, false},
				{">>", expr_val::op_binary<expr_val::rshift>, true, 2, -7, false},
				{"<", expr_val::op_binary<expr_val::lt>, true, 2, -8, false},
				{"<=", expr_val::op_binary<expr_val::le>, true, 2, -8, false},
				{">", expr_val::op_binary<expr_val::gt>, true, 2, -8, false},
				{">=", expr_val::op_binary<expr_val::ge>, true, 2, -8, false},
				{"==", expr_val::op_binary<expr_val::eq>, true, 2, -9, false},
				{"!=", expr_val::op_binary<expr_val::ne>, true, 2, -9, false},
				{"&", expr_val::op_binary<expr_val::band>, true, 2, -10, false},
				{"^", expr_val::op_binary<expr_val::bxor>, true, 2, -11, false},
				{"|", expr_val::op_binary<expr_val::bor>, true, 2, -12, false},
				{"&&", expr_val::op_land, true, 2, -13, false},
				{"||", expr_val::op_lor, true, 2, -14, false},
				{"π", expr_val::op_pi, true, 0, 0, false},
				{"if", expr_val::fun_if},
				{"select", expr_val::fun_select},
				{"unsigned", expr_val::op_unary<expr_val::x_nconv<expr_val_numeric::x_unsigned>>},
				{"signed", expr_val::op_unary<expr_val::x_nconv<expr_val_numeric::x_signed>>},
				{"float", expr_val::op_unary<expr_val::x_nconv<expr_val_numeric::x_float>>},
				{"min", expr_val::fun_fold<expr_val::fold_min>},
				{"max", expr_val::fun_fold<expr_val::fold_max>},
				{"sum", expr_val::fun_fold<expr_val::fold_sum>},
				{"prod", expr_val::fun_fold<expr_val::fold_prod>},
				{"sqrt", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::sqrt>>},
				{"log", expr_val::op_unary_binary<expr_val::f_n_fn<&expr_val_numeric::log>,
					expr_val::f_n_fn2<expr_val_numeric::log2>>},
				{"exp", expr_val::op_unary_binary<expr_val::f_n_fn<&expr_val_numeric::exp>,
					expr_val::f_n_fn2<expr_val_numeric::exp2>>},
				{"sin", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::sin>>},
				{"cos", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::cos>>},
				{"tan", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::tan>>},
				{"atan", expr_val::op_unary_binary<expr_val::f_n_fn<&expr_val_numeric::atan>,
					expr_val::f_n_fn2<expr_val_numeric::atan2>>},
				{"asin", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::asin>>},
				{"acos", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::acos>>},
				{"sinh", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::sinh>>},
				{"cosh", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::cosh>>},
				{"tanh", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::tanh>>},
				{"artanh", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::artanh>>},
				{"arsinh", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::arsinh>>},
				{"arcosh", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::arcosh>>},
				{"torad", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::torad>>},
				{"todeg", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::todeg>>},
				{"re", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::re>>},
				{"im", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::im>>},
				{"conj", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::conj>>},
				{"abs", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::abs>>},
				{"arg", expr_val::op_unary<expr_val::f_n_fn<&expr_val_numeric::arg>>},
				{"pyth", expr_val::fun_pyth},
			});
			return x.get_set();
		}
	private:
		_type type;
		bool v_boolean;
		expr_val_numeric v_numeric;
		std::string v_string;
	};
}

struct typeinfo* expression_value()
{
	static typeinfo_wrapper<expr_val> expession_value_obj;
	return &expession_value_obj;
}
}
