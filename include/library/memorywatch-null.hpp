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
	void show(const text& iname, const text& val);
	void reset();
};
}

#endif
