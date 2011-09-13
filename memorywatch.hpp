#ifndef _memorywatch__hpp__included__
#define _memorywatch__hpp__included__

#include <string>
#include <stdexcept>

std::string evaluate_watch(const std::string& expr) throw(std::bad_alloc);

#endif