#ifndef _memorywatch__hpp__included__
#define _memorywatch__hpp__included__

#include <string>
#include <stdexcept>
#include <set>

std::string evaluate_watch(const std::string& expr) throw(std::bad_alloc);
std::set<std::string> get_watches() throw(std::bad_alloc);
std::string get_watchexpr_for(const std::string& w) throw(std::bad_alloc);
void set_watchexpr_for(const std::string& w, const std::string& expr) throw(std::bad_alloc);
void do_watch_memory();

#endif