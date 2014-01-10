#ifndef _library__memorywatch_list__hpp__included__
#define _library__memorywatch_list__hpp__included__

#include "memorywatch.hpp"
#include "mathexpr.hpp"
#include <string>
#include <functional>

struct memorywatch_output_list : public memorywatch_item_printer
{
	memorywatch_output_list();
	~memorywatch_output_list();
	void set_output(std::function<void(const std::string& n, const std::string& v)> _fn);
	void show(const std::string& iname, const std::string& val);
	void reset();
	bool cond_enable;
	gcroot_pointer<mathexpr> enabled;
	//State variables.
	std::function<void(const std::string& n, const std::string& v)> fn;
};

#endif
