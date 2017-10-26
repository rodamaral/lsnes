#include "interface/disassembler.hpp"
#include <functional>
#include <stdexcept>
#include <iostream>

namespace
{
	template<typename T, bool be> T fetch_generic(std::function<unsigned char()> fetchpc)
	{
		size_t b = sizeof(T);
		T res = 0;
		for(size_t i = 0; i < b; i++) {
			size_t bit = 8 * (be ? (b - i - 1) : i);
			res |= (static_cast<T>(fetchpc()) << bit);
		}
		return res;
	}
}

disassembler::disassembler(const std::string& _name)
{
	disasms()[name = _name] = this;
}

disassembler::~disassembler()
{
	disasms().erase(name);
}

disassembler& disassembler::byname(const std::string& name)
{
	if(disasms().count(name))
		return *disasms()[name];
	throw std::runtime_error("No such disassembler known");
}

std::set<std::string> disassembler::list()
{
	std::set<std::string> r;
	for(auto& i : disasms())
		r.insert(i.first);
	return r;
}

std::map<std::string, disassembler*>& disassembler::disasms()
{
	static std::map<std::string, disassembler*> x;
	return x;
}

template<> int8_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int8_t, false>(fetchpc);
}

template<> uint8_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint8_t, false>(fetchpc);
}

template<> int16_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int16_t, false>(fetchpc);
}

template<> uint16_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint16_t, false>(fetchpc);
}

template<> int32_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int32_t, false>(fetchpc);
}

template<> uint32_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint32_t, false>(fetchpc);
}

template<> int64_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int64_t, false>(fetchpc);
}

template<> uint64_t disassembler::fetch_le(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint64_t, false>(fetchpc);
}

template<> int8_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int8_t, true>(fetchpc);
}

template<> uint8_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint8_t, true>(fetchpc);
}

template<> int16_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int16_t, true>(fetchpc);
}

template<> uint16_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint16_t, true>(fetchpc);
}

template<> int32_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int32_t, true>(fetchpc);
}

template<> uint32_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint32_t, true>(fetchpc);
}

template<> int64_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<int64_t, true>(fetchpc);
}

template<> uint64_t disassembler::fetch_be(std::function<unsigned char()> fetchpc)
{
	return fetch_generic<uint64_t, true>(fetchpc);
}
