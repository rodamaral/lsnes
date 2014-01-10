#include "memorywatch-list.hpp"

memorywatch_output_list::memorywatch_output_list()
{
}

memorywatch_output_list::~memorywatch_output_list()
{
}

void memorywatch_output_list::set_output(std::function<void(const std::string& n, const std::string& v)> _fn)
{
	fn = _fn;
}

void memorywatch_output_list::show(const std::string& iname, const std::string& val)
{
	if(cond_enable) {
		try {
			enabled->reset();
			auto e = enabled->evaluate();
			if(!e.type->toboolean(e.value))
				return;
		} catch(...) {
			return;
		}
	}
	fn(iname, val);
}

void memorywatch_output_list::reset()
{
}
