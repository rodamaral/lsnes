#include "memorywatch-list.hpp"
#include <functional>

namespace memorywatch
{
output_list::output_list()
{
}

output_list::~output_list()
{
}

void output_list::set_output(std::function<void(const std::string& n, const std::string& v)> _fn)
{
	fn = _fn;
}

void output_list::show(const std::string& iname, const std::string& val)
{
	if(cond_enable) {
		try {
			enabled->reset();
			auto e = enabled->evaluate();
			if(!e.type->toboolean(e._value))
				return;
		} catch(...) {
			return;
		}
	}
	fn(iname, val);
}

void output_list::reset()
{
}
}
