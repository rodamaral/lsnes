#ifndef _library__memorywatch_null__hpp__included__
#define _library__memorywatch_null__hpp__included__

#include "memorywatch.hpp"
#include "mathexpr.hpp"
#include <string>
#include <functional>

struct memorywatch_output_null : public memorywatch_item_printer
{
	memorywatch_output_null();
	~memorywatch_output_null();
	void show(const std::string& iname, const std::string& val);
	void reset();
};

#endif
