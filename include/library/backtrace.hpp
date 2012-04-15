#ifndef _library__backtrace__hpp__included__
#define _library__backtrace__hpp__included__

int lsnes_backtrace(void** buffer, int size);
void lsnes_backtrace_symbols_stderr(void* const* buffer, int size);

#endif
