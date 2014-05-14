#ifndef _library__memorywatch_null__hpp__included__
#define _library__memorywatch_null__hpp__included__

#include "memorywatch.hpp"
#include <string>
#include <functional>

namespace memorywatch
{
struct output_null : public item_printer
{
	output_null();
	~output_null();
	void show(const std::string& iname, const std::string& val);
	void reset();
};
}

#endif
