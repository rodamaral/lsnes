#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/project.hpp"
#include "core/window.hpp"
#include <library/string.hpp>

#include <cstdio>
#include <cstdlib>
#include <list>
#include <iomanip>
#include <stack>
#include <cmath>
#include <sstream>
#include <map>

namespace
{
	std::map<std::string, std::string> watches;

	struct numeric_type
	{
		numeric_type() { t = VT_NAN; hex = 0; }
		numeric_type(int8_t x) { t = VT_SIGNED; s = x; hex = 0; }
		numeric_type(uint8_t x) { t = VT_UNSIGNED; u = x; hex = 0; }
		numeric_type(int16_t x) { t = VT_SIGNED; s = x; hex = 0; }
		numeric_type(uint16_t x) { t = VT_UNSIGNED; u = x; hex = 0; }
		numeric_type(int32_t x) { t = VT_SIGNED; s = x; hex = 0; }
		numeric_type(uint32_t x) { t = VT_UNSIGNED; u = x; hex = 0; }
		numeric_type(int64_t x) { t = VT_SIGNED; s = x; hex = 0; }
		numeric_type(uint64_t x) { t = VT_UNSIGNED; u = x; hex = 0; }
		numeric_type(double x) { t = VT_FLOAT; f = x; }
		numeric_type(const std::string& _s)
		{
			char* end;
			if(_s.length() > 2 && _s[0] == '0' && _s[1] == 'x') {
				t = VT_UNSIGNED;
				u = strtoull(_s.c_str() + 2, &end, 16);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			} else if(_s.length() > 3 && _s[0] == '+' && _s[1] == '0' && _s[2] == 'x') {
				t = VT_SIGNED;
				s = (int64_t)strtoull(_s.c_str() + 3, &end, 16);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			} else if(_s.length() > 3 && _s[0] == '-' && _s[1] == '0' && _s[2] == 'x') {
				t = VT_SIGNED;
				s = -(int64_t)strtoull(_s.c_str() + 3, &end, 16);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			} else if(_s.find_first_of(".") < _s.length()) {
				t = VT_FLOAT;
				f = strtod(_s.c_str(), &end);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			} else if(_s.length() > 1 && _s[0] == '+') {
				t = VT_SIGNED;
				s = (int64_t)strtoull(_s.c_str() + 1, &end, 10);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			} else if(_s.length() > 1 && _s[0] == '-') {
				t = VT_SIGNED;
				s = -(int64_t)strtoull(_s.c_str() + 1, &end, 10);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			} else {
				t = VT_UNSIGNED;
				u = strtoull(_s.c_str(), &end, 10);
				if(*end)
					throw std::runtime_error("#syntax (badval)");
			}
			hex = 0;
		}
		uint64_t as_address() const
		{
			switch(t) {
			case VT_SIGNED:		return s;
			case VT_UNSIGNED:	return u;
			case VT_FLOAT:		return f;
			case VT_NAN:		throw std::runtime_error("#NAN");
			};
			return 0;
		}
		int64_t as_integer() const
		{
			switch(t) {
			case VT_SIGNED:		return s;
			case VT_UNSIGNED:	return u;
			case VT_FLOAT:		return f;
			case VT_NAN:		throw std::runtime_error("#NAN");
			};
			return 0;
		}
		double as_double() const
		{
			switch(t) {
			case VT_SIGNED:		return s;
			case VT_UNSIGNED:	return u;
			case VT_FLOAT:		return f;
			case VT_NAN:		throw std::runtime_error("#NAN");
			};
			return 0;
		}
		std::string str() const
		{
			uint64_t wmasks[] = {
				0x0000000000000000ULL, 0x000000000000000FULL, 0x00000000000000FFULL,
				0x0000000000000FFFULL, 0x000000000000FFFFULL, 0x00000000000FFFFFULL,
				0x0000000000FFFFFFULL, 0x000000000FFFFFFFULL, 0x00000000FFFFFFFFULL,
				0x0000000FFFFFFFFFULL, 0x000000FFFFFFFFFFULL, 0x00000FFFFFFFFFFFULL,
				0x0000FFFFFFFFFFFFULL, 0x000FFFFFFFFFFFFFULL, 0x00FFFFFFFFFFFFFFULL,
				0x0FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL
			};
			std::ostringstream x;
			if(hex && (t == VT_SIGNED || t == VT_UNSIGNED)) {
				uint64_t wmask = wmasks[hex];
				uint64_t w = (t == VT_SIGNED) ? s : u;
				x << std::hex << std::setw(hex) << std::setfill('0') << (w & wmask);
				return x.str();
			}
			switch(t) {
			case VT_SIGNED:		x << s;		break;
			case VT_UNSIGNED:	x << u;		break;
			case VT_FLOAT:		x << f;		break;
			case VT_NAN:		x << "#NAN";	break;
			};
			return x.str();
		}
		numeric_type round(int prec) const
		{
			double b = 0, c = 0;
			switch(t) {
			case VT_FLOAT:
				b = pow(10, prec);
				c = floor(b * f + 0.5) / b;
				return numeric_type(c);
			default:
				return *this;
			}
		}
		numeric_type operator+(const numeric_type& b) const
		{
			if(t == VT_NAN || b.t == VT_NAN)
				return numeric_type();
			else if(t == VT_FLOAT || b.t == VT_FLOAT)
				return numeric_type(as_double() + b.as_double());
			else if(t == VT_SIGNED || b.t == VT_SIGNED)
				return numeric_type(as_integer() + b.as_integer());
			else
				return numeric_type(as_address() + b.as_address());
		}
		numeric_type operator-(const numeric_type& b) const
		{
			if(t == VT_NAN || b.t == VT_NAN)
				return numeric_type();
			else if(t == VT_FLOAT || b.t == VT_FLOAT)
				return numeric_type(as_double() - b.as_double());
			else if(t == VT_SIGNED || b.t == VT_SIGNED)
				return numeric_type(as_integer() - b.as_integer());
			else
				return numeric_type(as_address() - b.as_address());
		}
		numeric_type operator*(const numeric_type& b) const
		{
			if(t == VT_NAN || b.t == VT_NAN)
				return numeric_type();
			else if(t == VT_FLOAT || b.t == VT_FLOAT)
				return numeric_type(as_double() * b.as_double());
			else if(t == VT_SIGNED || b.t == VT_SIGNED)
				return numeric_type(as_integer() * b.as_integer());
			else
				return numeric_type(as_address() * b.as_address());
		}
		numeric_type operator/(const numeric_type& b) const
		{
			if(b.t != VT_NAN && fabs(b.as_double()) < 1e-30)
				throw std::runtime_error("#DIV-BY-0");
			if(t == VT_NAN || b.t == VT_NAN)
				return numeric_type();
			else
				return numeric_type(as_double() / b.as_double());
		}
		numeric_type operator%(const numeric_type& b) const
		{
			return numeric_type(*this - b * idiv(b));
		}
		numeric_type idiv(const numeric_type& b) const
		{
			if(b.t != VT_NAN && fabs(b.as_double()) < 1e-30)
				throw std::runtime_error("#DIV-BY-0");
			if(t == VT_NAN || b.t == VT_NAN)
				return numeric_type();
			else if(t == VT_FLOAT || b.t == VT_FLOAT)
				return numeric_type(floor(as_double() / b.as_double()));
			else if(t == VT_SIGNED || b.t == VT_SIGNED)
				return numeric_type(as_integer() / b.as_integer());
			else
				return numeric_type(as_address() / b.as_address());
		}
		void sethex(char ch)
		{
			if(ch >= '0' && ch <= '9')
				hex = (ch - '0');
			if(ch >= 'A' && ch <= 'G')
				hex = (ch - 'A') + 10;
			if(ch >= 'a' && ch <= 'g')
				hex = (ch - 'a') + 10;
		}
	private:
		enum value_type
		{
			VT_SIGNED,
			VT_UNSIGNED,
			VT_FLOAT,
			VT_NAN
		} t;
		int64_t s;
		uint64_t u;
		double f;
		unsigned hex;
	};

	numeric_type stack_pop(std::stack<numeric_type>& s, bool norm = false)
	{
		if(s.size() < 1)
			throw std::runtime_error("#syntax (underflow)");
		numeric_type r = s.top();
		if(!norm)
			s.pop();
		return r;
	}

	template<typename T> void stack_push(std::stack<numeric_type>& s, T val)
	{
		s.push(numeric_type(val));
	}
}

std::string evaluate_watch(const std::string& expr) throw(std::bad_alloc)
{
	std::stack<numeric_type> s;
	size_t y;
	std::string _expr = expr;
	std::string t;
	numeric_type a;
	numeric_type b;
	int d;
	try {
		for(size_t i = 0; i < expr.length(); i++) {
			numeric_type r;
			switch(expr[i]) {
			case 'C':
				y = expr.find_first_of("z", i);
				if(y > expr.length())
					return "#syntax (noterm)";
				t = _expr.substr(i + 1, y - i - 1);
				stack_push(s, numeric_type(t));
				i = y;
				break;
			case 'R':
				if(i + 1 == expr.length())
					throw std::runtime_error("#syntax (noparam)");
				a = stack_pop(s);
				d = expr[++i] - '0';
				stack_push(s, a.round(d));
				break;
			case 'H':
				if(i + 1 == expr.length())
					throw std::runtime_error("#syntax (noparam)");
				s.top().sethex(expr[++i]);
				break;
			case 'a':
				stack_push<double>(s, atan(stack_pop(s).as_double()));
				break;
			case 'A':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push<double>(s, atan2(a.as_double(), b.as_double()));
				break;
			case 'c':
				stack_push<double>(s, cos(stack_pop(s).as_double()));
				break;
			case 'r':
				a = stack_pop(s);
				if(a.as_double() < 0)
					throw std::runtime_error("#NAN");
				stack_push<double>(s, sqrt(a.as_double()));
				break;
			case 's':
				stack_push<double>(s, sin(stack_pop(s).as_double()));
				break;
			case 't':
				stack_push<double>(s, tan(stack_pop(s).as_double()));
				break;
			case 'u':
				stack_push(s, stack_pop(s, true));
				break;
			case 'p':
				stack_push(s, 4 * atan(1));
				break;
			case '+':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push(s, a + b);
				break;
			case '-':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push(s, a - b);
				break;
			case '*':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push(s, a * b);
				break;
			case 'i':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push(s, a.idiv(b));
				break;
			case '/':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push(s, a / b);
				break;
			case '%':
				a = stack_pop(s);
				b = stack_pop(s);
				stack_push(s, a % b);
				break;
			case 'b':
				stack_push<int8_t>(s, lsnes_memory.read<uint8_t>(stack_pop(s).as_address()));
				break;
			case 'B':
				stack_push<uint8_t>(s, lsnes_memory.read<uint8_t>(stack_pop(s).as_address()));
				break;
			case 'w':
				stack_push<int16_t>(s, lsnes_memory.read<uint16_t>(stack_pop(s).as_address()));
				break;
			case 'W':
				stack_push<uint16_t>(s, lsnes_memory.read<uint16_t>(stack_pop(s).as_address()));
				break;
			case 'd':
				stack_push<int32_t>(s, lsnes_memory.read<uint32_t>(stack_pop(s).as_address()));
				break;
			case 'D':
				stack_push<uint32_t>(s, lsnes_memory.read<uint32_t>(stack_pop(s).as_address()));
				break;
			case 'q':
				stack_push<int64_t>(s, lsnes_memory.read<uint64_t>(stack_pop(s).as_address()));
				break;
			case 'Q':
				stack_push<uint64_t>(s, lsnes_memory.read<uint64_t>(stack_pop(s).as_address()));
				break;
			default:
				throw std::runtime_error("#syntax (illchar)");
			}
		}
		if(s.empty())
			return "#ERR";
		else
			return s.top().str();
	} catch(std::exception& e) {
		return e.what();
	}
}

std::set<std::string> get_watches() throw(std::bad_alloc)
{
	std::set<std::string> r;
	auto p = project_get();
	std::map<std::string, std::string>* ws;
	if(p)
		ws = &p->watches;
	else
		ws = &watches;
	for(auto i : *ws)
		r.insert(i.first);
	return r;
}

std::string get_watchexpr_for(const std::string& w) throw(std::bad_alloc)
{
	auto p = project_get();
	std::map<std::string, std::string>* ws;
	if(p)
		ws = &p->watches;
	else
		ws = &watches;
	if(ws->count(w))
		return (*ws)[w];
	else
		return "";
}

void set_watchexpr_for(const std::string& w, const std::string& expr) throw(std::bad_alloc)
{
	auto& status = platform::get_emustatus();
	auto p = project_get();
	if(expr != "") {
		if(p)
			p->watches[w] = expr;
		else
			watches[w] = expr;
		status.set("M[" + w + "]", evaluate_watch(expr));
	} else {
		if(p)
			p->watches.erase(w);
		else
			watches.erase(w);
		status.erase("M[" + w + "]");
	}
	if(p)
		project_flush(p);
	notify_status_update();
}

void do_watch_memory()
{
	auto& status = platform::get_emustatus();
	auto p = project_get();
	auto w = p ? &(p->watches) : &watches;
	for(auto i : *w)
		status.set("M[" + i.first + "]", evaluate_watch(i.second));
}

namespace
{
	function_ptr_command<const std::string&> add_watch(lsnes_cmd, "add-watch", "Add a memory watch",
		"Syntax: add-watch <name> <expression>\nAdds a new memory watch\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]+(|[^ \t].*)", t, "Name and expression required.");
			set_watchexpr_for(r[1], r[2]);
		});

	function_ptr_command<const std::string&> remove_watch(lsnes_cmd, "remove-watch", "Remove a memory watch",
		"Syntax: remove-watch <name>\nRemoves a memory watch\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([^ \t]+)[ \t]*", t, "Name required.");
			set_watchexpr_for(r[1], "");
		});
}
