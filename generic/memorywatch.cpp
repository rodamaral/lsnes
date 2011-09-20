#include "memorywatch.hpp"
#include "memorymanip.hpp"
#include <cstdio>
#include <cstdlib>
#include <list>
#include <iomanip>
#include <stack>
#include <cmath>
#include <sstream>

std::string evaluate_watch(const std::string& expr) throw(std::bad_alloc)
{
	std::stack<double> s;
	size_t y;
	std::string _expr = expr;
	std::string t;
	double a;
	double b;
	int d;
	for(size_t i = 0; i < expr.length(); i++) {
		switch(expr[i]) {
		case 'C':
			y = expr.find_first_of("z", i);
			if(y > expr.length())
				return "#syntax (noterm)";
			t = _expr.substr(i + 1, y - i - 1);
			if(t.length() > 2 && t.substr(0, 2) == "0x") {
				char* end;
				s.push(strtoull(t.c_str() + 2, &end, 16));
				if(*end)
					return "#syntax (badhex)";
			} else {
				char* end;
				s.push(strtod(t.c_str(), &end));
				if(*end)
					return "#syntax (badnum)";
			}
			i = y;
			break;
		case 'R':
			if(i + 1 == expr.length())
				return "#syntax (noparam)";
			d = expr[++i] - '0';
			a = s.top();
			s.pop();
			b = pow(10, d);
			s.push(floor(b * a + 0.5) / b);
			break;
		case 'a':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(atan(a));
			break;
		case 'A':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(atan2(a, b));
			break;
		case 'c':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(cos(a));
			break;
		case 'r':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(sqrt(a));
			break;
		case 's':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(sin(a));
			break;
		case 't':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(tan(a));
			break;
		case 'u':
			if(s.size() < 1)
				return "#syntax (underflow)";
			s.push(s.top());
			break;
		case 'p':
			s.push(4 * atan(1));
			break;
		case '+':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(a + b);
			break;
		case '-':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(a - b);
			break;
		case '*':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(a * b);
			break;
		case 'i':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(a / b);
			break;
		case '/':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(static_cast<int64_t>(a / b));
			break;
		case '%':
			if(s.size() < 2)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			b = s.top();
			s.pop();
			s.push(a - static_cast<int64_t>(a / b) * b);
			break;
		case 'b':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<int8_t>(memory_read_byte(a)));
			break;
		case 'B':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<uint8_t>(memory_read_byte(a)));
			break;
		case 'w':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<int16_t>(memory_read_word(a)));
			break;
		case 'W':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<uint16_t>(memory_read_word(a)));
			break;
		case 'd':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<int32_t>(memory_read_dword(a)));
			break;
		case 'D':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<uint32_t>(memory_read_dword(a)));
			break;
		case 'q':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<int64_t>(memory_read_qword(a)));
			break;
		case 'Q':
			if(s.size() < 1)
				return "#syntax (underflow)";
			a = s.top();
			s.pop();
			s.push(static_cast<uint64_t>(memory_read_qword(a)));
			break;
		default:
			return "#syntax (illchar)";
		}
	}
	if(s.empty())
		return "#ERR";
	else {
		char buffer[512];
		sprintf(buffer, "%f", s.top());
		return buffer;
	}
}
