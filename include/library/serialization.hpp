#ifndef _library__serialization__hpp__included__
#define _library__serialization__hpp__included__

#include <cstdlib>

template<typename T1, typename T2, size_t ssize, bool be>
void _write_common(unsigned char* target, T1 value)
{
	for(size_t i = 0; i < ssize; i++)
		if(be)
			target[i] = static_cast<T2>(value) >> 8 * (ssize - i - 1);
		else
			target[i] = static_cast<T2>(value) >> 8 * i;
}

template<typename T1, typename T2, size_t ssize, bool be>
T1 _read_common(const unsigned char* source)
{
	T2 value = 0;
	for(size_t i = 0; i < ssize; i++)
		if(be)
			value |= static_cast<T2>(source[i]) << 8 * (ssize - i - 1);
		else
			value |= static_cast<T2>(source[i]) << 8 * i;
	return static_cast<T1>(value);
}

#define write8sbe(t, v)  _write_common<  int8_t,  uint8_t, 1,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write8sle(t, v)  _write_common<  int8_t,  uint8_t, 1, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write8ube(t, v)  _write_common< uint8_t,  uint8_t, 1,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write8ule(t, v)  _write_common< uint8_t,  uint8_t, 1, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write16sbe(t, v) _write_common< int16_t, uint16_t, 2,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write16sle(t, v) _write_common< int16_t, uint16_t, 2, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write16ube(t, v) _write_common<uint16_t, uint16_t, 2,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write16ule(t, v) _write_common<uint16_t, uint16_t, 2, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write32sbe(t, v) _write_common< int32_t, uint32_t, 4,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write32sle(t, v) _write_common< int32_t, uint32_t, 4, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write32ube(t, v) _write_common<uint32_t, uint32_t, 4,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write32ule(t, v) _write_common<uint32_t, uint32_t, 4, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write64sbe(t, v) _write_common< int64_t, uint64_t, 8,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write64sle(t, v) _write_common< int64_t, uint64_t, 8, false>(reinterpret_cast<unsigned char*>(t), (v))
#define write64ube(t, v) _write_common<uint64_t, uint64_t, 8,  true>(reinterpret_cast<unsigned char*>(t), (v))
#define write64ule(t, v) _write_common<uint64_t, uint64_t, 8, false>(reinterpret_cast<unsigned char*>(t), (v))
#define read8sbe(t)  _read_common<  int8_t,  uint8_t, 1,  true>(reinterpret_cast<const unsigned char*>(t))
#define read8sle(t)  _read_common<  int8_t,  uint8_t, 1, false>(reinterpret_cast<const unsigned char*>(t))
#define read8ube(t)  _read_common< uint8_t,  uint8_t, 1,  true>(reinterpret_cast<const unsigned char*>(t))
#define read8ule(t)  _read_common< uint8_t,  uint8_t, 1, false>(reinterpret_cast<const unsigned char*>(t))
#define read16sbe(t) _read_common< int16_t, uint16_t, 2,  true>(reinterpret_cast<const unsigned char*>(t))
#define read16sle(t) _read_common< int16_t, uint16_t, 2, false>(reinterpret_cast<const unsigned char*>(t))
#define read16ube(t) _read_common<uint16_t, uint16_t, 2,  true>(reinterpret_cast<const unsigned char*>(t))
#define read16ule(t) _read_common<uint16_t, uint16_t, 2, false>(reinterpret_cast<const unsigned char*>(t))
#define read32sbe(t) _read_common< int32_t, uint32_t, 4,  true>(reinterpret_cast<const unsigned char*>(t))
#define read32sle(t) _read_common< int32_t, uint32_t, 4, false>(reinterpret_cast<const unsigned char*>(t))
#define read32ube(t) _read_common<uint32_t, uint32_t, 4,  true>(reinterpret_cast<const unsigned char*>(t))
#define read32ule(t) _read_common<uint32_t, uint32_t, 4, false>(reinterpret_cast<const unsigned char*>(t))
#define read64sbe(t) _read_common< int64_t, uint64_t, 8,  true>(reinterpret_cast<const unsigned char*>(t))
#define read64sle(t) _read_common< int64_t, uint64_t, 8, false>(reinterpret_cast<const unsigned char*>(t))
#define read64ube(t) _read_common<uint64_t, uint64_t, 8,  true>(reinterpret_cast<const unsigned char*>(t))
#define read64ule(t) _read_common<uint64_t, uint64_t, 8, false>(reinterpret_cast<const unsigned char*>(t))

#endif
