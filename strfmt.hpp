#pragma once
#include <sstream>

class stringfmt
{
public:
	stringfmt() {}
	std::string str() { return x.str(); }
	template<typename T> stringfmt& operator<<(const T& y) { x << y; return *this; }
	void throwex() { throw std::runtime_error(x.str()); }
private:
	std::ostringstream x;
};

class message_output
{
public:
	void end() {
		std::string X = x.str();
		cb_message(X.c_str(), X.length());
	}
	template<typename T> inline message_output& operator<<(T value)
	{
		x << value;
		return *this;
	}
	inline message_output& operator<<(std::ostream& (*fn)(std::ostream& o))
	{
		fn(x);
		return *this;
	}
private:
	std::ostringstream x;
};

