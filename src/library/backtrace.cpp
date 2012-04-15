#include "library/backtrace.hpp"

#ifdef __linux__
#include <execinfo.h>

int lsnes_backtrace(void** buffer, int size)
{
	return backtrace(buffer, size);
}

void lsnes_backtrace_symbols_stderr(void* const* buffer, int size)
{
	backtrace_symbols_fd(buffer, size, 2);
}
#else
int lsnes_backtrace(void** buffer, int size)
{
	return 0;
}

void lsnes_backtrace_symbols_stderr(void* const* buffer, int size)
{
}

#endif
