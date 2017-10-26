#include "exrethrow.hpp"
#include <functional>
#include <list>
#include <iostream>
#include <future>

namespace exrethrow
{
ex_spec<std::exception, 0> std_exception;
ex_spec<std::logic_error, 1> std_logic_error;
ex_spec<std::domain_error, 2> std_domain_error;
ex_spec<std::invalid_argument, 2> std_invalid_argument;
ex_spec<std::length_error, 2> std_length_error;
ex_spec<std::out_of_range, 2> std_out_of_range;
ex_spec<std::future_error, 2> std_future_error;
ex_spec<std::runtime_error, 1> std_runtime_error;
ex_spec<std::range_error, 2> std_range_error;
ex_spec<std::overflow_error, 2> std_overflow_error;
ex_spec<std::underflow_error, 2> std_underflow_error;
ex_spec<std::system_error, 2> std_system_error;
ex_spec<std::bad_cast, 1> std_bad_cast;
ex_spec<std::bad_exception, 1> std_bad_exception;
ex_spec<std::bad_function_call, 1> std_bad_function_call;
ex_spec<std::bad_typeid, 1> std_bad_typeid;
ex_spec<std::bad_weak_ptr, 1> std_bad_weak_ptr;
ex_spec<std::ios_base::failure, 1> std_ios_base_failure;

struct exspec_item {
	unsigned prio;
	std::function<bool(std::exception& e)> identify;
	std::function<void()> (*throwfn)(std::exception& e);
};
std::list<exspec_item>* exspecs;

void add_ex_spec(unsigned prio, std::function<bool(std::exception& e)> identify,
	std::function<void()> (*throwfn)(std::exception& e))
{
	exspec_item i;
	i.prio = prio;
	i.identify = identify;
	i.throwfn = throwfn;
	if(!exspecs)
		exspecs = new std::list<exspec_item>;
	for(auto j = exspecs->begin(); j != exspecs->end(); j++) {
		if(j->prio <= prio) {
			exspecs->insert(j, i);
			return;
		}
	}
	exspecs->push_back(i);
}

std::function<void()> get_throw_fn(std::exception& e)
{
	for(auto& i : *exspecs) {
		if(i.identify(e))
			return i.throwfn(e);
	}
	return []() -> void { throw std::runtime_error("Unknown exception"); };
}

storage::storage()
{
	null = true;
}

storage::storage(std::exception& e)
{
	null = false;
	oom = false;
	if(dynamic_cast<std::bad_alloc*>(&e)) {
		oom = true;
		return;
	}
	try {
		do_rethrow = get_throw_fn(e);
	} catch(...) {
		oom = true;
	}
}

void storage::rethrow()
{
	if(null) return;
	if(oom)
		throw std::bad_alloc();
	do_rethrow();
}

storage::operator bool()
{
	return !null;
}
}
