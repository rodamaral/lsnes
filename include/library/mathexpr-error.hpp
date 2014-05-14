#ifndef _library__mathexpr_error__hpp__included__
#define _library__mathexpr_error__hpp__included__

#include <string>
#include <stdexcept>

namespace mathexpr
{
class error : public std::runtime_error
{
public:
	enum errorcode
	{
		UNDEFINED,		//Evaluation encountered undefined value.
		CIRCULAR,		//Evaluation encountered circular reference.
		TYPE_MISMATCH,		//Evaluation encountered mismatching types.
		INTERNAL,		//Evaluation encountered internal error.
		WDOMAIN,		//Evaluation encountered domain error.
		DIV_BY_0,		//Evaluation encountered division by zero.
		LOG_BY_0,		//Evaluation encountered logarithm of zero.
		ARGCOUNT,		//Evaluation encountered wrong argument count.
		SIZE,			//Bad size for memory watch.
		ADDR,			//Bad address for memory watch.
		FORMAT,			//Bad format string.
		UNKNOWN,		//Unknown error.
	};
	error(errorcode code, const std::string& message);
	errorcode get_code();
	const char* get_short_error();
private:
	errorcode code;
};
}

#endif
