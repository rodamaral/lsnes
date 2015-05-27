#ifndef _lua_debug__hpp__included__
#define _lua_debug__hpp__included__

#include "internal.hpp"
#include "core/debug.hpp"

template<debug_context::etype type>
void handle_registerX(lua::state& L, uint64_t addr, int lfn);

template<debug_context::etype type>
void handle_unregisterX(lua::state& L, uint64_t addr, int lfn);

#endif
