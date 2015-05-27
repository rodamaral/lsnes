#include "arch-detect.hpp"
#include "crandom.hpp"
#include <stdexcept>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

namespace
{
	typedef BOOLEAN (WINAPI *rtlgenrandom_t)(PVOID buf, ULONG buflen);
	HMODULE advapi;
	rtlgenrandom_t rtlgenrandom_fn;
}

namespace crandom
{
void init()
{
	if(rtlgenrandom_fn) return;
	advapi = LoadLibraryA("advapi32.dll");
	if(!advapi)
		throw std::runtime_error("Can't load advapi32.dll");
	rtlgenrandom_fn = (rtlgenrandom_t)GetProcAddress(advapi, "SystemFunction036");
	if(!rtlgenrandom_fn)
		throw std::runtime_error("Can't find rtlgenrandom");
}

void generate(void* buffer, size_t buffersize)
{
	if(!rtlgenrandom_fn)
		init();
	while(!rtlgenrandom_fn(buffer, buffersize));
}
}

#else
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

namespace
{
	int fd = -1;
	void try_file(const char* name)
	{
		int err = 0;
		while(fd < 0 && err != ENOENT && err != ENXIO) {
			fd = open(name, O_RDONLY);
			if(fd < 0) err = errno;
		}
	}
}

namespace crandom
{
void init()
{
	if(fd < 0) try_file("/dev/urandom");
	if(fd < 0) try_file("/dev/random");
	if(fd < 0) throw std::runtime_error("Can't open /dev/urandom");
}

void generate(void* buffer, size_t buffersize)
{
	if(fd < 0)
		init();
	size_t out = 0;
	while(out < buffersize) {
		ssize_t r = read(fd, (char*)buffer + out, buffersize - out);
		if(r > 0) out += r;
	}
}
}

#endif

namespace crandom
{
uint64_t arch_get_tsc()
{
#ifdef ARCH_IS_I386
	uint32_t a, b;
	asm volatile("rdtsc" : "=a"(a), "=d"(b));
	return ((uint64_t)b << 32) | a;
#else
	return 0;
#endif
}

uint64_t arch_get_random()
{
#ifdef ARCH_IS_I386
	uint32_t r;
	asm volatile (".byte 0xb8, 0x01, 0x00, 0x00, 0x00, 0x0f, 0xa2, 0xf7, 0xc1, 0x00, 0x00, 0x00, 0x40, "
		"0x74, 0x05, 0x0f, 0xc7, 0xf0, 0xeb, 0x02, 0x31, 0xc0" : "=a"(r) : : "ebx", "ecx", "edx");
	return r;
#else
	return 0;
#endif
}
}
