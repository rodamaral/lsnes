#ifndef _interface__c_interface__hpp__included__
#define _interface__c_interface__hpp__included__

#define LSNES_BUILD_AS_BUILTIN_CORE
#include "c-interface.h"
#include <library/loadlib.hpp>

void try_init_c_module(const loadlib::module& module);
void initialize_all_builtin_c_cores();

template<typename T> struct ccore_call_param_map { static int id; static const char* name; };

#endif
