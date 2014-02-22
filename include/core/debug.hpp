#ifndef _debug__hpp__included__
#define _debug__hpp__included__

#include <functional>
#include <cstdint>

struct debug_handle
{
	void* handle;
};

enum debug_type
{
	DEBUG_READ,
	DEBUG_WRITE,
	DEBUG_EXEC,
	DEBUG_TRACE,
};

extern const uint64_t debug_all_addr;

debug_handle debug_add_callback(uint64_t addr, debug_type type,
	std::function<void(uint64_t addr, uint64_t value)> fn, std::function<void()> dtor);
debug_handle debug_add_trace_callback(uint64_t proc, std::function<void(uint64_t proc, const char* str,
	bool true_insn)> fn, std::function<void()> dtor);
void debug_remove_callback(uint64_t addr, debug_type type, debug_handle handle);
void debug_fire_callback_read(uint64_t addr, uint64_t value);
void debug_fire_callback_write(uint64_t addr, uint64_t value);
void debug_fire_callback_exec(uint64_t addr, uint64_t value);
void debug_fire_callback_trace(uint64_t proc, const char* str, bool true_insn = true);
void debug_set_cheat(uint64_t addr, uint64_t value);
void debug_clear_cheat(uint64_t addr);
void debug_setxmask(uint64_t mask);
void debug_tracelog(uint64_t proc, const std::string& filename);
bool debug_tracelogging(uint64_t proc);
void debug_set_tracelog_change_cb(std::function<void()> cb);
void debug_core_change();
void debug_request_break();

#endif
