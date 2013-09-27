#include "memorysearch.hpp"
#include "eatarg.hpp"
#include "minmax.hpp"
#include "serialization.hpp"
#include "int24.hpp"
#include <iostream>

memory_search::memory_search(memory_space& space) throw(std::bad_alloc)
	: mspace(space)
{
	candidates = 0;
}


struct search_update
{
	typedef uint8_t value_type;
	bool operator()(uint8_t oldv, uint8_t newv) const throw() { return true; }
};

template<typename T>
struct search_value
{
	typedef T value_type;
	search_value(T v) throw() { val = v; }
	bool operator()(T oldv, T newv) const throw() { return (newv == val); }
	T val;
};

template<typename T>
struct search_difference
{
	typedef T value_type;
	search_difference(T v) throw() { val = v; }
	bool operator()(T oldv, T newv) const throw() { return ((newv - oldv) == val); }
	T val;
};

template<typename T>
struct search_lt
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw() { return (newv < oldv); }
};

template<typename T>
struct search_le
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw() { return (newv <= oldv); }
};

template<typename T>
struct search_eq
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw() { return (newv == oldv); }
};

template<typename T>
struct search_ne
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw() { return (newv != oldv); }
};

template<typename T>
struct search_ge
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw() { return (newv >= oldv); }
};

template<typename T>
struct search_gt
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw() { return (newv > oldv); }
};

template<typename T>
struct search_seqlt
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) != (T)0);
	}
};

template<typename T>
struct search_seqle
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) != (T)0) || (diff == (T)0);
	}
};

template<typename T>
struct search_seqge
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) == (T)0);
	}
};

template<typename T>
struct search_seqgt
{
	typedef T value_type;
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) == (T)0) && (diff != (T)0);
	}
};


template<typename T>
struct search_value_helper
{
	typedef typename T::value_type value_type;
	search_value_helper(const T& v)  throw()
		: val(v)
	{
	}
	bool operator()(const uint8_t* newv, const uint8_t* oldv, uint64_t left, int endian) const throw()
	{
		if(left < sizeof(value_type))
			return false;
		value_type v1 = read_of_endian<value_type>(oldv, endian);
		value_type v2 = read_of_endian<value_type>(newv, endian);
		return val(v1, v2);
	}
	const T& val;
};

namespace
{
	void dq_all_after(uint64_t* still_in, uint64_t& candidates, uint64_t size, uint64_t after)
	{
		for(uint64_t i = after; i < size; i++) {
			if((still_in[i / 64] >> (i % 64)) & 1) {
				still_in[i / 64] &= ~(1ULL << (i % 64));
				candidates--;
			}
		}
	}

	inline void dq_entry(uint64_t* still_in, uint64_t& candidates, uint64_t i)
	{
		if((still_in[i / 64] >> (i % 64)) & 1) {
			still_in[i / 64] &= ~(1ULL << (i % 64));
			candidates--;
		}
	}

	inline uint64_t next_multiple_of_64(uint64_t i)
	{
		return (i + 64) >> 6 << 6;
	}

	template<typename T>
	void search_block_mapped(uint64_t* still_in, uint64_t& candidates, memory_region& region, uint64_t rbase,
		uint64_t ibase, T& helper, std::vector<uint8_t>& previous_content)
	{
		if(ibase >= previous_content.size())
			return;
		unsigned char* mem = region.direct_map;
		uint64_t rsize = region.size;
		int endian = region.endian;
		uint64_t i = ibase;
		uint64_t switch_at = ibase + rsize - rbase;	//The smallest i not in this region.
		rsize = min(rsize, previous_content.size() - ibase);
		for(unsigned j = rbase; j < rsize; i++, j++) {
			//Advance blocks of 64 addresses if none of the addresses match.
			while(still_in[i / 64] == 0 && next_multiple_of_64(i) <= switch_at) {
				uint64_t old_i = i;
				i = next_multiple_of_64(i);
				j += i - old_i;
			}
			//This might match. Check it.
			if(!helper(mem + j, &previous_content[i], rsize - j, endian))
				dq_entry(still_in, candidates, i);
		}
	}

	template<typename T>
	void search_block_read(uint64_t* still_in, uint64_t& candidates, memory_region& region, uint64_t rbase,
		uint64_t ibase, T& helper, std::vector<uint8_t>& previous_content)
	{
		if(ibase >= previous_content.size())
			return;
		uint64_t rsize = region.size;
		int endian = region.endian;
		uint64_t i = ibase;
		uint64_t switch_at = ibase + rsize - rbase;	//The smallest i not in this region.

		//The buffer.
		const size_t buffer_capacity = 4096;
		unsigned char buffer[buffer_capacity];
		uint64_t buffer_offset = 0;		//The offset in buffer.
		uint64_t buffer_remaining = 0;		//Number of bytes remaining in buffer.
		uint64_t buffer_soffset = rbase;	//The offset buffer start corresponds to.
		uint64_t buffer_eoffset = rbase;	//The first offset not in buffer.

		rsize = min(rsize, previous_content.size() - ibase);
		for(unsigned j = rbase; j < rsize; i++, j++) {
			//Fill the buffer again if it has gotten low enough.
			if(buffer_remaining < 256 && buffer_eoffset != rsize) {
				if(buffer_remaining)
					memmove(buffer, buffer + buffer_offset, buffer_remaining);
				buffer_offset = 0;
				size_t fill_amount = min((uint64_t)buffer_capacity, rsize - buffer_eoffset);
				region.read(buffer_eoffset, buffer + buffer_remaining, fill_amount);
				buffer_eoffset += fill_amount;
				buffer_remaining += fill_amount;
			}
			//Advance blocks of 64 addresses if none of the addresses match.
			while(still_in[i / 64] == 0 && next_multiple_of_64(i) <= switch_at) {
				uint64_t old_i = i;
				i = next_multiple_of_64(i);
				uint64_t advance = i - old_i;
				j += advance;
				buffer_offset += advance;
				buffer_remaining -= advance;
				buffer_soffset += advance;
			}
			//This might match. Check it.
			if(!helper(buffer + buffer_offset, &previous_content[i], buffer_remaining, endian))
				dq_entry(still_in, candidates, i);
			buffer_offset++;
			buffer_remaining--;
			buffer_soffset++;
		}
	}

	void copy_block_mapped(uint8_t* old, memory_region& region, uint64_t rbase, uint64_t maxr)
	{
		memcpy(old, region.direct_map + rbase, min(region.size - rbase, maxr));
	}

	void copy_block_read(uint8_t* old, memory_region& region, uint64_t rbase, uint64_t maxr)
	{
		region.read(rbase, old, min(region.size - rbase, maxr));
	}

	void dq_block(uint64_t* still_in, uint64_t& candidates, memory_region& region, uint64_t rbase, uint64_t ibase,
		uint64_t first, uint64_t last, uint64_t ramsize)
	{
		if(ibase >= ramsize)
			return;
		if(last < region.base + rbase || first >= region.base + region.size)
			return;		//Nothing to DQ in this block.
		first = max(first, region.base + rbase) - region.base + rbase;
		last = last - region.base - rbase;
		last = min(last, ramsize - ibase);
		uint64_t ilast = ibase + last;
		for(uint64_t i = ibase + first; i <= ilast; i++) {
			while(still_in[i / 64] == 0 && next_multiple_of_64(i) <= ilast) {
				i = next_multiple_of_64(i);
				continue;
			}
			dq_entry(still_in, candidates, i);
		}
	}

	void candidates_block(std::list<uint64_t>& out, memory_region& region, uint64_t rbase, uint64_t ibase,
		uint64_t* still_in, uint64_t ramsize)
	{
		if(ibase >= ramsize)
			return;
		uint64_t rsize = region.size;
		uint64_t i = ibase;
		uint64_t switch_at = ibase + rsize - rbase;	//The smallest i not in this region.
		rsize = min(rsize, ramsize - ibase);
		for(unsigned j = rbase; j < rsize; i++, j++) {
			//Advance blocks of 64 addresses if none of the addresses match.
			while(still_in[i / 64] == 0 && next_multiple_of_64(i) <= switch_at) {
				uint64_t old_i = i;
				i = next_multiple_of_64(i);
				j += i - old_i;
			}
			if((still_in[i / 64] >> (i % 64)) & 1)
				out.push_back(region.base + j);
		}
	}
}

void memory_search::dq_range(uint64_t first, uint64_t last)
{
	auto t = mspace.lookup_linear(0);
	if(!t.first)
		return;
	uint64_t i = 0;
	uint64_t size = previous_content.size();
	while(true) {
		//Switch blocks.
		t = mspace.lookup_linear(i);
		if(!t.first) {
			//DQ all rest.
			dq_all_after(&still_in[0], candidates, size, i);
			break;
		}
		dq_block(&still_in[0], candidates, *t.first, t.second, i, first, last, previous_content.size());
		i += t.first->size - t.second;
	}
}

template<class T> void memory_search::search(const T& obj) throw()
{
	search_value_helper<T> helper(obj);
	auto t = mspace.lookup_linear(0);
	if(!t.first)
		return;
	uint64_t size = previous_content.size();
	uint64_t i = 0;
	while(true) {
		//Switch blocks.
		t = mspace.lookup_linear(i);
		if(!t.first) {
			//DQ all rest.
			dq_all_after(&still_in[0], candidates, size, i);
			break;
		}
		if(t.first->direct_map) {
			search_block_mapped(&still_in[0], candidates, *t.first, t.second, i, helper, previous_content);
			copy_block_mapped(&previous_content[i], *t.first, t.second,
				  max((uint64_t)previous_content.size(), i) - i);
		} else {
			search_block_read(&still_in[0], candidates, *t.first, t.second, i, helper, previous_content);
			copy_block_read(&previous_content[i], *t.first, t.second,
				  max((uint64_t)previous_content.size(), i) - i);
		}
		i += t.first->size - t.second;
	}
}

template<typename T> void memory_search::s_value(T value) throw() { search(search_value<T>(value)); }
template<typename T> void memory_search::s_difference(T value) throw() { search(search_difference<T>(value)); }
template<typename T> void memory_search::s_lt() throw() { search(search_lt<T>()); }
template<typename T> void memory_search::s_le() throw() { search(search_le<T>()); }
template<typename T> void memory_search::s_eq() throw() { search(search_eq<T>()); }
template<typename T> void memory_search::s_ne() throw() { search(search_ne<T>()); }
template<typename T> void memory_search::s_ge() throw() { search(search_ge<T>()); }
template<typename T> void memory_search::s_gt() throw() { search(search_gt<T>()); }
template<typename T> void memory_search::s_seqlt() throw() { search(search_seqlt<T>()); }
template<typename T> void memory_search::s_seqle() throw() { search(search_seqle<T>()); }
template<typename T> void memory_search::s_seqge() throw() { search(search_seqge<T>()); }
template<typename T> void memory_search::s_seqgt() throw() { search(search_seqgt<T>()); }

template<typename T> void memorysearch_pull_type(memory_search& s)
{
	T val;
	eat_argument(&memory_search::s_value<T>);
	eat_argument(&memory_search::s_difference<T>);
	eat_argument(&memory_search::s_lt<T>);
	eat_argument(&memory_search::s_le<T>);
	eat_argument(&memory_search::s_eq<T>);
	eat_argument(&memory_search::s_ne<T>);
	eat_argument(&memory_search::s_ge<T>);
	eat_argument(&memory_search::s_gt<T>);
	eat_argument(&memory_search::s_seqlt<T>);
	eat_argument(&memory_search::s_seqle<T>);
	eat_argument(&memory_search::s_seqge<T>);
	eat_argument(&memory_search::s_seqgt<T>);
}

template<typename T> void memorysearch_pull_type2(memory_search& s)
{
	T val;
	eat_argument(&memory_search::s_value<T>);
	eat_argument(&memory_search::s_difference<T>);
	eat_argument(&memory_search::s_lt<T>);
	eat_argument(&memory_search::s_le<T>);
	eat_argument(&memory_search::s_eq<T>);
	eat_argument(&memory_search::s_ne<T>);
	eat_argument(&memory_search::s_ge<T>);
	eat_argument(&memory_search::s_gt<T>);
}

void memorysearch_pull_all(memory_search& s)
{
	memorysearch_pull_type<int8_t>(s);
	memorysearch_pull_type<uint8_t>(s);
	memorysearch_pull_type<int16_t>(s);
	memorysearch_pull_type<uint16_t>(s);
	memorysearch_pull_type<ss_int24_t>(s);
	memorysearch_pull_type<ss_uint24_t>(s);
	memorysearch_pull_type<int32_t>(s);
	memorysearch_pull_type<uint32_t>(s);
	memorysearch_pull_type<int64_t>(s);
	memorysearch_pull_type<uint64_t>(s);
	memorysearch_pull_type2<float>(s);
	memorysearch_pull_type2<double>(s);
}

void memory_search::update() throw() { search(search_update()); }

uint64_t memory_search::get_candidate_count() throw()
{
	return candidates;
}

std::list<uint64_t> memory_search::get_candidates() throw(std::bad_alloc)
{
	std::list<uint64_t> out;
	auto t = mspace.lookup_linear(0);
	if(!t.first)
		return out;
	uint64_t i = 0;
	while(true) {
		//Switch blocks.
		t = mspace.lookup_linear(i);
		if(!t.first)
			return out;
		candidates_block(out, *t.first, t.second, i, &still_in[0], previous_content.size());
		i += t.first->size - t.second;
	}
	return out;
}

void memory_search::reset() throw(std::bad_alloc)
{
	uint64_t linearram = mspace.get_linear_size();
	previous_content.resize(linearram);
	still_in.resize((linearram + 63) / 64);
	for(uint64_t i = 0; i < linearram / 64; i++)
		still_in[i] = 0xFFFFFFFFFFFFFFFFULL;
	if(linearram % 64)
		still_in[linearram / 64] = (1ULL << (linearram % 64)) - 1;
	candidates = linearram;

	auto t = mspace.lookup_linear(0);
	if(!t.first)
		return;
	uint64_t i = 0;
	while(true) {
		//Switch blocks.
		t = mspace.lookup_linear(i);
		if(!t.first)
			break;
		if(t.first->direct_map)
			copy_block_mapped(&previous_content[i], *t.first, t.second,
				max((uint64_t)previous_content.size(), i) - i);
		else
			copy_block_read(&previous_content[i], *t.first, t.second,
				max((uint64_t)previous_content.size(), i) - i);
		i += t.first->size - t.second;
	}
}
