#ifndef _sdump__hpp__included__
#define _sdump__hpp__included__

#include <string>
#include <cstdint>

#define SDUMP_FLAG_HIRES 1
#define SDUMP_FLAG_INTERLACED 2
#define SDUMP_FLAG_OVERSCAN 4
#define SDUMP_FLAG_PAL 8

void sdump_open(const std::string& prefix, bool ss);
void sdump_close();
void sdump_frame(const uint32_t* buffer, unsigned flags);
void sdump_sample(short left, short right);

#endif