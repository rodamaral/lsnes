#ifndef _library__serialization__hpp__included__
#define _library__serialization__hpp__included__

#include <cstdint>
#include <cstdlib>
#include <map>

namespace serialization
{
template<size_t n> struct unsigned_of {};
template<> struct unsigned_of<1> { typedef uint8_t t; };
template<> struct unsigned_of<2> { typedef uint16_t t; };
template<> struct unsigned_of<4> { typedef uint32_t t; };
template<> struct unsigned_of<8> { typedef uint64_t t; };

template<typename T1, bool be>
void write_common(uint8_t* target, T1 value)
{
	for(size_t i = 0; i < sizeof(T1); i++)
		if(be)
			target[i] = static_cast<typename unsigned_of<sizeof(T1)>::t>(value) >> 8 * (sizeof(T1) - i - 1);
		else
			target[i] = static_cast<typename unsigned_of<sizeof(T1)>::t>(value) >> 8 * i;
}

template<typename T1, bool be>
T1 read_common(const uint8_t* source)
{
	typename unsigned_of<sizeof(T1)>::t value = 0;
	for(size_t i = 0; i < sizeof(T1); i++)
		if(be)
			value |= static_cast<typename unsigned_of<sizeof(T1)>::t>(source[i]) << 8 * (sizeof(T1) - i - 1);
		else
			value |= static_cast<typename unsigned_of<sizeof(T1)>::t>(source[i]) << 8 * i;
	return static_cast<T1>(value);
}

inline void s8b(void* t, int8_t v)
{
	write_common< int8_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s8l(void* t, int8_t v)
{
	write_common< int8_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u8b(void* t, uint8_t v)
{
	write_common< uint8_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u8l(void* t, uint8_t v)
{
	write_common< uint8_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s16b(void* t, int16_t v)
{
	write_common< int16_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s16l(void* t, int16_t v)
{
	write_common< int16_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u16b(void* t, uint16_t v)
{
	write_common<uint16_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u16l(void* t, uint16_t v)
{
	write_common<uint16_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s32b(void* t, int32_t v)
{
	write_common< int32_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s32l(void* t, int32_t v)
{
	write_common< int32_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u32b(void* t, uint32_t v)
{
	write_common<uint32_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u32l(void* t, uint32_t v)
{
	write_common<uint32_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s64b(void* t, int64_t v)
{
	write_common< int64_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void s64l(void* t, int64_t v)
{
	write_common< int64_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u64b(void* t, uint64_t v)
{
	write_common<uint64_t, true>(reinterpret_cast<uint8_t*>(t), v);
}
inline void u64l(void* t, uint64_t v)
{
	write_common<uint64_t, false>(reinterpret_cast<uint8_t*>(t), v);
}
inline int8_t s8b(const void* t)
{
	return read_common< int8_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline int8_t s8l(const void* t)
{
	return read_common< int8_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline uint8_t u8b(const void* t)
{
	return read_common< uint8_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline uint8_t u8l(const void* t)
{
	return read_common< uint8_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline int16_t s16b(const void* t)
{
	return read_common< int16_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline int16_t s16l(const void* t)
{
	return read_common< int16_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline uint16_t u16b(const void* t)
{
	return read_common<uint16_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline uint16_t u16l(const void* t)
{
	return read_common<uint16_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline int32_t s32b(const void* t)
{
	return read_common< int32_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline int32_t s32l(const void* t)
{
	return read_common< int32_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline uint32_t u32b(const void* t)
{
	return read_common<uint32_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline uint32_t u32l(const void* t)
{
	return read_common<uint32_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline int64_t s64b(const void* t)
{
	return read_common< int64_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline int64_t s64l(const void* t)
{
	return read_common< int64_t, false>(reinterpret_cast<const uint8_t*>(t));
}
inline uint64_t u64b(const void* t)
{
	return read_common<uint64_t, true>(reinterpret_cast<const uint8_t*>(t));
}
inline uint64_t u64l(const void* t)
{
	return read_common<uint64_t, false>(reinterpret_cast<const uint8_t*>(t));
}

template<typename T> void swap_endian(T& value)
{
	char* _value = reinterpret_cast<char*>(&value);
	for(size_t i = 0; i < sizeof(T)/2; i++)
		std::swap(_value[i], _value[sizeof(T)-i-1]);
}

template<typename T> void swap_endian(T& value, int endian)
{
	short _magic = 258;
	char magic = *reinterpret_cast<char*>(&_magic);
	bool do_swap = (endian == -1 && magic == 1) || (endian == 1 && magic == 2);
	if(do_swap)
		swap_endian(value);
}

template<typename T> T read_endian(const void* value, int endian)
{
	T val;
	memcpy(&val, value, sizeof(T));
	swap_endian(val, endian);
	return val;
}

template<typename T> void write_endian(void* value, const T& val, int endian)
{
	memcpy(value, &val, sizeof(T));
	swap_endian(*reinterpret_cast<T*>(value), endian);
}
}

#endif
