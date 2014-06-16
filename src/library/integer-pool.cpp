#include "integer-pool.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

namespace
{
	const unsigned special_level = 21;
	uint64_t level_base[] = {
		0ULL,
		1ULL,
		9ULL,
		73ULL,
		585ULL,
		4681ULL,
		37449ULL,
		299593ULL,
		2396745ULL,
		19173961ULL,
		153391689ULL,
		1227133513ULL,
		9817068105ULL,
		78536544841ULL,
		628292358729ULL,
		5026338869833ULL,
		40210710958665ULL,
		321685687669321ULL,
		2573485501354569ULL,
		20587884010836553ULL,
		164703072086692425ULL,
		1317624576693539401ULL,
		3623467585907233353ULL,		//Special: Fits 2^64 bits.
	};
	uint64_t level_size[] = {
		1ULL,
		8ULL,
		64ULL,
		512ULL,
		4096ULL,
		32768ULL,
		262144ULL,
		2097152ULL,
		16777216ULL,
		134217728ULL,
		1073741824ULL,
		8589934592ULL,
		68719476736ULL,
		549755813888ULL,
		4398046511104ULL,
		35184372088832ULL,
		281474976710656ULL,
		2251799813685248ULL,
		18014398509481984ULL,
		144115188075855872ULL,
		1152921504606846976ULL,
		2305843009213693952ULL,		//Special: 2^64 bits.
	};

	const unsigned levels = 22;
	unsigned level_from_size(uint64_t size)
	{
		for(unsigned i = 0; i < levels; i++) {
			if(size <= level_base[i + 1]) return i;
		}
		return levels;
	}

	unsigned lsbz(uint8_t b)
	{
		return ((b & 1) ? ((b & 2) ? ((b & 4) ? ((b & 8) ? ((b & 16) ? ((b & 32) ? ((b & 64) ?
			((b & 128) ? 8 : 7) : 6) : 5) : 4) : 3) : 2) : 1) : 0);
	}
}

integer_pool::integer_pool() throw()
{
	invalid = false;
	_bits2 = 0;
	bits = &_bits2;
}

uint64_t integer_pool::operator()() throw(std::bad_alloc)
{
	if(invalid) throw std::bad_alloc();
	//If the first byte is 0xFF, we got to expand the array.
	if(bits[0] == 0xFF) {
		unsigned level = level_from_size(_bits.size());  //If bits.size() == 0, this correctly returns 0.
		assert(level < special_level);
		std::vector<uint8_t> newbits;
		newbits.resize(level_base[level + 2]);
		memset(&newbits[0], 0, level_base[level + 2]);
		for(unsigned i = 0; i <= level; i++)
			memcpy(&newbits[level_base[i + 1]], &bits[level_base[i]], level_size[i]);
		newbits[0] = 1;
		std::swap(_bits, newbits);
		bits = &_bits[0];
	}
	//Find a free byte.
	unsigned level = level_from_size(_bits.size());  //If bits.size() == 0, this correctly returns 0.
	uint64_t pathbits = 0;
	for(unsigned i = 0; i < level; i++) {
		uint64_t byte = level_base[i] + pathbits;
		assert(bits[byte] < 255);
		unsigned lsb = lsbz(bits[byte]);
		pathbits = 8 * pathbits + lsb;
	}

	//Check if there are free integers in pool.
	if(pathbits > 0x1FFFFFFFFFFFFFFFULL) throw std::bad_alloc();

	//Reserve it, and propagate fullness downward if needed.
	uint64_t byte = level_base[level] + pathbits;
	assert(bits[byte] < 255);
	unsigned lsb = lsbz(bits[byte]);
	pathbits = 8 * pathbits + lsb;
	uint64_t bit = pathbits;
	bool carry = true;
	for(unsigned i = level; i <= level; i--) {
		if(carry) {
			byte = level_base[i] + (pathbits >> 3);
			bits[byte] |= (1 << (pathbits & 7));
			carry = (bits[byte] == 255);
			pathbits >>= 3;
		} else
			break;
	}
	return bit;
}

void integer_pool::operator()(uint64_t num) throw()
{
	if(invalid) return;
	unsigned level = level_from_size(_bits.size());  //If bits.size() == 0, this correctly returns 0.
	for(unsigned i = level; i <= level; i--) {
		uint64_t byte = level_base[i] + (num >> 3);
		bits[byte] &= ~(1 << (num & 7));
		num >>= 3;
	}
}

integer_pool::~integer_pool() throw()
{
	invalid = true;
}
