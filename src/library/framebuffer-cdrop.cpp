#include "framebuffer.hpp"
#include "arch-detect.hpp"
#include <iostream>

namespace framebuffer
{
namespace
{
	inline bool ssse3_available()
	{
		size_t res = 0;
#ifdef ARCH_IS_I386
		size_t page = 1;
		asm volatile(
			"cpuid\n"
			"\tshr $9,%0\n"
			"\tand $1,%0\n"
			: "=c"(res), "=a"(page) : "a"(page) : "%rbx", "%rdx");
#endif
		return res;
	}

	const char mask_drop4_8[]  __attribute__ ((aligned (16))) = {
		 0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13, 14, -1, -1, -1, -1,		//0 -> 0
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  4,		//1 -> 0
		 5,  6,  8,  9, 10, 12, 13, 14, -1, -1, -1, -1, -1, -1, -1, -1,		//1 -> 1
		-1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  4,  5,  6,  8,  9,		//2 -> 1
		10, 12, 13, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//2 -> 2
		-1, -1, -1, -1,  0,  1,  2,  4,  5,  6,  8,  9, 10, 12, 13, 14,		//3 -> 2
	};
	const char mask_drop4s_8[]  __attribute__ ((aligned (16))) = {
		 2,  1,  0,  6,  5,  4, 10,  9,  8, 14, 13, 12, -1, -1, -1, -1,		//0 -> 0
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  2,  1,  0,  6,		//1 -> 0
		 5,  4, 10,  9,  8, 14, 13, 12, -1, -1, -1, -1, -1, -1, -1, -1,		//1 -> 1
		-1, -1, -1, -1, -1, -1, -1, -1,  2,  1,  0,  6,  5,  4, 10,  9,		//2 -> 1
		 8, 14, 13, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//2 -> 2
		-1, -1, -1, -1,  2,  1,  0,  6,  5,  4, 10,  9,  8, 14, 13, 12,		//3 -> 2
	};
	const char mask_swap4_8[]  __attribute__ ((aligned (16))) = {
		 2,  1,  0,  3,  6,  5,  4,  7, 10,  9,  8, 11, 14, 13, 12, 15,		//0 -> 0
	};
	const char mask_drop4_16[]  __attribute__ ((aligned (16))) = {
		 0,  1,  2,  3,  4,  5,  8,  9, 10, 11, 12, 13, -1, -1, -1, -1,		//0 -> 0
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,		//1 -> 0
		 4,  5,  8,  9, 10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1,		//1 -> 1
		-1, -1, -1, -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  8,  9,		//2 -> 1
		10, 11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//2 -> 2
		-1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  8,  9, 10, 11, 12, 13,		//3 -> 2
	};
	const char mask_drop4s_16[]  __attribute__ ((aligned (16))) = {
		 4,  5,  2,  3,  0,  1, 12, 13, 10, 11,  8,  9, -1, -1, -1, -1,		//0 -> 0
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  4,  5,  2,  3,		//1 -> 0
		 0,  1, 12, 13, 10, 11,  8,  9, -1, -1, -1, -1, -1, -1, -1, -1,		//1 -> 1
		-1, -1, -1, -1, -1, -1, -1, -1,  4,  5,  2,  3,  0,  1, 12, 13,		//2 -> 1
		10, 11,  8,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//2 -> 2
		-1, -1, -1, -1,  4,  5,  2,  3,  0,  1, 12, 13, 10, 11,  8,  9,		//3 -> 2
	};
	const char mask_swap4_16[]  __attribute__ ((aligned (16))) = {
		 4,  5,  2,  3,  0,  1,  6,  7, 12, 13, 10, 11,  8,  9, 14, 15,		//0 -> 0
	};

#ifdef ARCH_IS_I386
	void ssse3_drop(uint8_t* dest, const uint8_t* src, size_t units, const char* masks)
	{
		size_t blocks = (units + 11) / 12;
		for(size_t i = 0; i < blocks; i++) {
			asm volatile(
				"MOVDQA 0(%1),%%xmm0\n"
				"\tMOVDQA 16(%1),%%xmm1\n"
				"\tMOVDQA 32(%1),%%xmm3\n"
				"\tMOVDQA 48(%1),%%xmm5\n"
				"\tMOVDQA %%xmm1,%%xmm2\n"
				"\tMOVDQA %%xmm3,%%xmm4\n"
				"\tPSHUFB 0(%2),%%xmm0\n"
				"\tPSHUFB 16(%2),%%xmm1\n"
				"\tPSHUFB 32(%2),%%xmm2\n"
				"\tPSHUFB 48(%2),%%xmm3\n"
				"\tPSHUFB 64(%2),%%xmm4\n"
				"\tPSHUFB 80(%2),%%xmm5\n"
				"\tPOR %%xmm0,%%xmm1\n"
				"\tPOR %%xmm2,%%xmm3\n"
				"\tPOR %%xmm4,%%xmm5\n"
				"\tMOVDQA %%xmm1,0(%0)\n"
				"\tMOVDQA %%xmm3,16(%0)\n"
				"\tMOVDQA %%xmm5,32(%0)\n"
			    : : "r" (dest), "r" (src), "r" (masks) : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
			dest += 48;
			src += 64;
		}
	}

	void ssse3_swap(uint8_t* dest, const uint8_t* src, size_t units, const char* masks)
	{
		size_t blocks = (units + 3) / 4;
		for(size_t i = 0; i < blocks; i++) {
			asm volatile(
				"MOVDQA (%1),%%xmm0\n"
				"\tPSHUFB (%2),%%xmm0\n"
				"\tMOVDQA %%xmm0,(%0)\n"
			    : : "r" (dest), "r" (src), "r" (masks) : "xmm0");
			dest += 16;
			src += 16;
		}
	}
#endif
}

void copy_drop4(uint8_t* dest, const uint32_t* src, size_t units)
{
#ifdef ARCH_IS_I386
	if(ssse3_available()) {
		ssse3_drop(dest, reinterpret_cast<const uint8_t*>(src), units, mask_drop4_8);
		return;
	}
#endif
	const uint8_t* _src = reinterpret_cast<const uint8_t*>(src);
	for(size_t i = 0; i < units; i++) {
		dest[3 * i + 0] = _src[4 * i + 0];
		dest[3 * i + 1] = _src[4 * i + 1];
		dest[3 * i + 2] = _src[4 * i + 2];
	}
}

void copy_drop4s(uint8_t* dest, const uint32_t* src, size_t units)
{
#ifdef ARCH_IS_I386
	if(ssse3_available()) {
		ssse3_drop(dest, reinterpret_cast<const uint8_t*>(src), units, mask_drop4s_8);
		return;
	}
#endif
	const uint8_t* _src = reinterpret_cast<const uint8_t*>(src);
	for(size_t i = 0; i < units; i++) {
		dest[3 * i + 0] = _src[4 * i + 2];
		dest[3 * i + 1] = _src[4 * i + 1];
		dest[3 * i + 2] = _src[4 * i + 0];
	}
}

void copy_swap4(uint8_t* dest, const uint32_t* src, size_t units)
{
#ifdef ARCH_IS_I386
	if(ssse3_available()) {
		ssse3_swap(dest, reinterpret_cast<const uint8_t*>(src), units, mask_swap4_8);
		return;
	}
#endif
	const uint8_t* _src = reinterpret_cast<const uint8_t*>(src);
	for(size_t i = 0; i < units; i++) {
		dest[4 * i + 0] = _src[4 * i + 2];
		dest[4 * i + 1] = _src[4 * i + 1];
		dest[4 * i + 2] = _src[4 * i + 0];
		dest[4 * i + 3] = _src[4 * i + 3];
	}
}

void copy_drop4(uint16_t* dest, const uint64_t* src, size_t units)
{
#ifdef ARCH_IS_I386
	if(ssse3_available()) {
		ssse3_drop(reinterpret_cast<uint8_t*>(dest), reinterpret_cast<const uint8_t*>(src), units,
			mask_drop4_16);
		return;
	}
#endif
	const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
	for(size_t i = 0; i < units; i++) {
		dest[3 * i + 0] = _src[4 * i + 0];
		dest[3 * i + 1] = _src[4 * i + 1];
		dest[3 * i + 2] = _src[4 * i + 2];
	}
}

void copy_drop4s(uint16_t* dest, const uint64_t* src, size_t units)
{
#ifdef ARCH_IS_I386
	if(ssse3_available()) {
		ssse3_drop(reinterpret_cast<uint8_t*>(dest), reinterpret_cast<const uint8_t*>(src), units,
			mask_drop4s_16);
		return;
	}
#endif
	const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
	for(size_t i = 0; i < units; i++) {
		dest[3 * i + 0] = _src[4 * i + 2];
		dest[3 * i + 1] = _src[4 * i + 1];
		dest[3 * i + 2] = _src[4 * i + 0];
	}
}

void copy_swap4(uint16_t* dest, const uint64_t* src, size_t units)
{
#ifdef ARCH_IS_I386
	if(ssse3_available()) {
		ssse3_swap(reinterpret_cast<uint8_t*>(dest), reinterpret_cast<const uint8_t*>(src), units,
			mask_swap4_16);
		return;
	}
#endif
	const uint16_t* _src = reinterpret_cast<const uint16_t*>(src);
	for(size_t i = 0; i < units; i++) {
		dest[4 * i + 0] = _src[4 * i + 2];
		dest[4 * i + 1] = _src[4 * i + 1];
		dest[4 * i + 2] = _src[4 * i + 0];
		dest[4 * i + 3] = _src[4 * i + 3];
	}
}
}
