#ifndef _lua__simdvector__hpp__included__
#define _lua__simdvector__hpp__included__

#include <vector>
#include <string>
#include <cstdint>

struct lua_simdvector
{
	lua_simdvector(size_t size);
	std::vector<uint8_t> data;
};

template<typename T>
struct lua_simdvector_doverlay
{
	lua_simdvector_doverlay(lua_simdvector* v)
	{
		data = reinterpret_cast<T*>(&v->data[0]);
		elts = v->data.size() / sizeof(T);
	}
	T* data;
	size_t elts;
};

#endif