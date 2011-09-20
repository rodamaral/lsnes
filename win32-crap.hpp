#ifndef _win32_crap__hpp__included__
#define _win32_crap__hpp__included__

#if defined(_WIN32) || defined(_WIN64)
#ifndef _MAX_PATH
#define _MAX_PATH 8192
#endif

extern "C" char* strdup(const char* orig);

#endif
#endif