#include "memoryspace.hpp"
#include "minmax.hpp"
#include "serialization.hpp"
#include "int24.hpp"
#include "string.hpp"
#include <algorithm>

namespace
{
	template<typename T, bool linear> inline T internal_read(memory_space& m, uint64_t addr)
	{
		std::pair<memory_space::region*, uint64_t> g;
		if(linear)
			g = m.lookup_linear(addr);
		else
			g = m.lookup(addr);
		if(!g.first || g.second + sizeof(T) > g.first->size)
			return 0;
		if(g.first->direct_map)
			return serialization::read_endian<T>(g.first->direct_map + g.second, g.first->endian);
		else {
			T buf;
			g.first->read(g.second, &buf, sizeof(T));
			return serialization::read_endian<T>(&buf, g.first->endian);
		}
	}

	template<typename T, bool linear> inline bool internal_write(memory_space& m, uint64_t addr, T value)
	{
		std::pair<memory_space::region*, uint64_t> g;
		if(linear)
			g = m.lookup_linear(addr);
		else
			g = m.lookup(addr);
		if(!g.first || g.first->readonly || g.second + sizeof(T) > g.first->size)
			return false;
		if(g.first->direct_map)
			serialization::write_endian(g.first->direct_map + g.second, value, g.first->endian);
		else {
			T buf;
			serialization::write_endian(&buf, value, g.first->endian);
			g.first->write(g.second, &buf, sizeof(T));
		}
		return true;
	}

	void read_range_r(memory_space::region& r, uint64_t offset, void* buffer, size_t bsize)
	{
		if(r.direct_map) {
			if(offset >= r.size) {
				memset(buffer, 0, bsize);
				return;
			}
			uint64_t maxcopy = min(static_cast<uint64_t>(bsize), r.size - offset);
			memcpy(buffer, r.direct_map + offset, maxcopy);
			if(maxcopy < bsize)
				memset(reinterpret_cast<char*>(buffer) + maxcopy, 0, bsize - maxcopy);
		} else
			r.read(offset, buffer, bsize);
	}

	bool write_range_r(memory_space::region& r, uint64_t offset, const void* buffer, size_t bsize)
	{
		if(r.readonly)
			return false;
		if(r.direct_map) {
			if(offset >= r.size)
				return false;
			uint64_t maxcopy = min(static_cast<uint64_t>(bsize), r.size - offset);
			memcpy(r.direct_map + offset, buffer, maxcopy);
			return true;
		} else
			return r.write(offset, buffer, bsize);
	}
}

memory_space::region::~region() throw()
{
}

void memory_space::region::read(uint64_t offset, void* buffer, size_t tsize)
{
	if(!direct_map || offset >= size) {
		memset(buffer, 0, tsize);
		return;
	}
	uint64_t maxcopy = min(static_cast<uint64_t>(tsize), size - offset);
	memcpy(buffer, direct_map + offset, maxcopy);
	if(maxcopy < tsize)
		memset(reinterpret_cast<char*>(buffer) + maxcopy, 0, tsize - maxcopy);
}

bool memory_space::region::write(uint64_t offset, const void* buffer, size_t tsize)
{
	if(!direct_map || readonly || offset >= size)
		return false;
	uint64_t maxcopy = min(static_cast<uint64_t>(tsize), size - offset);
	memcpy(direct_map + offset, buffer, maxcopy);
	return true;
}

std::pair<memory_space::region*, uint64_t> memory_space::lookup(uint64_t address)
{
	threads::alock m(mlock);
	size_t lb = 0;
	size_t ub = u_regions.size();
	while(lb < ub) {
		size_t mb = (lb + ub) / 2;
		if(u_regions[mb]->base > address) {
			ub = mb;
			continue;
		}
		if(u_regions[mb]->last_address() < address) {
			lb = mb + 1;
			continue;
		}
		return std::make_pair(u_regions[mb], address - u_regions[mb]->base);
	}
	return std::make_pair(reinterpret_cast<region*>(NULL), 0);
}

std::pair<memory_space::region*, uint64_t> memory_space::lookup_linear(uint64_t linear)
{
	threads::alock m(mlock);
	if(linear >= linear_size)
		return std::make_pair(reinterpret_cast<region*>(NULL), 0);
	size_t lb = 0;
	size_t ub = linear_bases.size() - 1;
	while(lb < ub) {
		size_t mb = (lb + ub) / 2;
		if(linear_bases[mb] > linear) {
			ub = mb;
			continue;
		}
		if(linear_bases[mb + 1] <= linear) {
			lb = mb + 1;
			continue;
		}
		return std::make_pair(u_lregions[mb], linear - linear_bases[mb]);
	}
	return std::make_pair(reinterpret_cast<region*>(NULL), 0);
}

void memory_space::read_all_linear_memory(uint8_t* buffer)
{
	auto g = lookup_linear(0);
	size_t off = 0;
	while(g.first) {
		read_range_r(*g.first, g.second, buffer + off, g.first->size);
		off += g.first->size;
		g = lookup_linear(off);
	}
}

#define MSR memory_space::read
#define MSW memory_space::write
#define MSRL memory_space::read_linear
#define MSWL memory_space::write_linear

template<> int8_t MSR (uint64_t address) { return internal_read<int8_t, false>(*this, address); }
template<> uint8_t MSR (uint64_t address) { return internal_read<uint8_t, false>(*this, address); }
template<> int16_t MSR (uint64_t address) { return internal_read<int16_t, false>(*this, address); }
template<> uint16_t MSR (uint64_t address) { return internal_read<uint16_t, false>(*this, address); }
template<> ss_int24_t MSR (uint64_t address) { return internal_read<ss_int24_t, false>(*this, address); }
template<> ss_uint24_t MSR (uint64_t address) { return internal_read<ss_uint24_t, false>(*this, address); }
template<> int32_t MSR (uint64_t address) { return internal_read<int32_t, false>(*this, address); }
template<> uint32_t MSR (uint64_t address) { return internal_read<uint32_t, false>(*this, address); }
template<> int64_t MSR (uint64_t address) { return internal_read<int64_t, false>(*this, address); }
template<> uint64_t MSR (uint64_t address) { return internal_read<uint64_t, false>(*this, address); }
template<> float MSR (uint64_t address) { return internal_read<float, false>(*this, address); }
template<> double MSR (uint64_t address) { return internal_read<double, false>(*this, address); }
template<> bool MSW (uint64_t a, int8_t v) { return internal_write<int8_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint8_t v) { return internal_write<uint8_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, int16_t v) { return internal_write<int16_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint16_t v) { return internal_write<uint16_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, ss_int24_t v) { return internal_write<ss_int24_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, ss_uint24_t v) { return internal_write<ss_uint24_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, int32_t v) { return internal_write<int32_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint32_t v) { return internal_write<uint32_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, int64_t v) { return internal_write<int64_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint64_t v) { return internal_write<uint64_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, float v) { return internal_write<float, false>(*this, a, v); }
template<> bool MSW (uint64_t a, double v) { return internal_write<double, false>(*this, a, v); }
template<> int8_t MSRL (uint64_t address) { return internal_read<int8_t, true>(*this, address); }
template<> uint8_t MSRL (uint64_t address) { return internal_read<uint8_t, true>(*this, address); }
template<> int16_t MSRL (uint64_t address) { return internal_read<int16_t, true>(*this, address); }
template<> uint16_t MSRL (uint64_t address) { return internal_read<uint16_t, true>(*this, address); }
template<> ss_int24_t MSRL (uint64_t address) { return internal_read<ss_int24_t, true>(*this, address); }
template<> ss_uint24_t MSRL (uint64_t address) { return internal_read<ss_uint24_t, true>(*this, address); }
template<> int32_t MSRL (uint64_t address) { return internal_read<int32_t, true>(*this, address); }
template<> uint32_t MSRL (uint64_t address) { return internal_read<uint32_t, true>(*this, address); }
template<> int64_t MSRL (uint64_t address) { return internal_read<int64_t, true>(*this, address); }
template<> uint64_t MSRL (uint64_t address) { return internal_read<uint64_t, true>(*this, address); }
template<> float MSRL (uint64_t address) { return internal_read<float, true>(*this, address); }
template<> double MSRL (uint64_t address) { return internal_read<double, true>(*this, address); }
template<> bool MSWL (uint64_t a, int8_t v) { return internal_write<int8_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint8_t v) { return internal_write<uint8_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, int16_t v) { return internal_write<int16_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint16_t v) { return internal_write<uint16_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, ss_int24_t v) { return internal_write<ss_int24_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, ss_uint24_t v) { return internal_write<ss_uint24_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, int32_t v) { return internal_write<int32_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint32_t v) { return internal_write<uint32_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, int64_t v) { return internal_write<int64_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint64_t v) { return internal_write<uint64_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, float v) { return internal_write<float, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, double v) { return internal_write<double, true>(*this, a, v); }

void memory_space::read_range(uint64_t address, void* buffer, size_t bsize)
{
	auto g = lookup(address);
	if(!g.first) {
		memset(buffer, 0, bsize);
		return;
	}
	read_range_r(*g.first, g.second, buffer, bsize);
}

bool memory_space::write_range(uint64_t address, const void* buffer, size_t bsize)
{
	auto g = lookup(address);
	if(!g.first)
		return false;
	return write_range_r(*g.first, g.second, buffer, bsize);
}

void memory_space::read_range_linear(uint64_t address, void* buffer, size_t bsize)
{
	auto g = lookup_linear(address);
	if(!g.first) {
		memset(buffer, 0, bsize);
		return;
	}
	read_range_r(*g.first, g.second, buffer, bsize);
}

bool memory_space::write_range_linear(uint64_t address, const void* buffer, size_t bsize)
{
	auto g = lookup_linear(address);
	if(!g.first)
		return false;
	return write_range_r(*g.first, g.second, buffer, bsize);
}

memory_space::region* memory_space::lookup_n(size_t n)
{
	threads::alock m(mlock);
	if(n >= u_regions.size())
		return NULL;
	return u_regions[n];
}


std::list<memory_space::region*> memory_space::get_regions()
{
	threads::alock m(mlock);
	std::list<region*> r;
	for(auto i : u_regions)
		r.push_back(i);
	return r;
}

char* memory_space::get_physical_mapping(uint64_t base, uint64_t size)
{
	uint64_t last = base + size - 1;
	if(last < base)
		return NULL;	//Warps around.
	auto g1 = lookup(base);
	auto g2 = lookup(last);
	if(g1.first != g2.first)
		return NULL;	//Not the same VMA.
	if(!g1.first || !g1.first->direct_map)
		return NULL;	//Not mapped.
	//OK.
	return reinterpret_cast<char*>(g1.first->direct_map + g1.second);
}

void memory_space::set_regions(const std::list<memory_space::region*>& regions)
{
	threads::alock m(mlock);
	std::vector<region*> n_regions;
	std::vector<region*> n_lregions;
	std::vector<uint64_t> n_linear_bases;
	//Calculate array sizes.
	n_regions.resize(regions.size());
	size_t linear_c = 0;
	for(auto i : regions)
		if(!i->readonly && !i->special)
			linear_c++;
	n_lregions.resize(linear_c);
	n_linear_bases.resize(linear_c + 1);

	//Fill the main array (it must be sorted!).
	size_t i = 0;
	for(auto j : regions)
		n_regions[i++] = j;
	std::sort(n_regions.begin(), n_regions.end(),
		[](region* a, region* b) -> bool { return a->base < b->base; });

	//Fill linear address arrays from the main array.
	i = 0;
	uint64_t base = 0;
	for(auto j : n_regions) {
		if(j->readonly || j->special)
			continue;
		n_lregions[i] = j;
		n_linear_bases[i] = base;
		base = base + j->size;
		i++;
	}
	n_linear_bases[i] = base;

	std::swap(u_regions, n_regions);
	std::swap(u_lregions, n_lregions);
	std::swap(linear_bases, n_linear_bases);
	linear_size = base;
}

int memory_space::_get_system_endian()
{
	if(sysendian)
		return sysendian;
	uint16_t magic = 258;
	return (*reinterpret_cast<uint8_t*>(&magic) == 1) ? 1 : -1;
}

int memory_space::sysendian = 0;

std::string memory_space::address_to_textual(uint64_t addr)
{
	threads::alock m(mlock);
	for(auto i : u_regions) {
		if(addr >= i->base && addr <= i->last_address()) {
			return (stringfmt() << i->name << "+" << std::hex << (addr - i->base)).str();
		}
	}
	return (stringfmt() << std::hex << addr).str();
}

memory_space::region_direct::region_direct(const std::string& _name, uint64_t _base, int _endian,
	unsigned char* _memory, size_t _size, bool _readonly)
{
	name = _name;
	base = _base;
	endian = _endian;
	direct_map = _memory;
	size = _size;
	readonly = _readonly;
	special = false;
}

memory_space::region_direct::~region_direct() throw() {}

namespace
{
	const static uint64_t p63 = 0x8000000000000000ULL;
	const static uint64_t p64m1 = 0xFFFFFFFFFFFFFFFFULL;

	void block_bounds(uint64_t base, uint64_t size, uint64_t& low, uint64_t& high)
	{
		if(base + size >= base) {
			//No warparound.
			low = min(low, base);
			high = max(high, base + size - 1);
		} else if(base + size == 0) {
			//Just barely avoids warparound.
			low = min(low, base);
			high = 0xFFFFFFFFFFFFFFFFULL;
		} else {
			//Fully warps around.
			low = 0;
			high = 0xFFFFFFFFFFFFFFFFULL;
		}
	}

	//Stride < 2^63.
	//rows and stride is nonzero.
	std::pair<uint64_t, uint64_t> base_bounds(uint64_t base, uint64_t rows, uint64_t stride)
	{
		uint64_t space = p64m1 - base;
		if(space / stride < rows - 1)
			//Approximate a bit.
			return std::make_pair(0, p64m1);
		return std::make_pair(base, base + (rows - 1) * stride);
	}
}

std::pair<uint64_t, uint64_t> memoryspace_row_bounds(uint64_t base, uint64_t size, uint64_t rows,
	uint64_t stride)
{
	uint64_t low = p64m1;
	uint64_t high = 0;
	if(size && rows) {
		uint64_t lb, hb;
		if(stride == 0) {
			//Case I: Stride is 0.
			//Just one block is accessed.
			lb = base;
			hb = base;
			block_bounds(base, size, low, high);
		} else if(stride == p63) {
			//Case II: Stride is 2^63.
			//If there are multiple blocks, There are 2 accessed blocks, [base, base+size) and
			//[base+X, base+size+X), where X=2^63.
			lb = base;
			hb = (rows > 1) ? (base + p63) : base;
		} else if(stride > p63) {
			//Case III: Stride is negative.
			//Flip the problem around to get stride that is positive.
			auto g = base_bounds(p64m1 - base, rows, ~stride + 1);
			lb = p64m1 - g.first;
			hb = p64m1 - g.second;
		} else {
			//Case IV: Stride is positive.
			auto g = base_bounds(base, rows, stride);
			lb = g.first;
			hb = g.second;
		}
		block_bounds(lb, size, low, high);
		block_bounds(hb, size, low, high);
	}
	return std::make_pair(low, high);
}

bool memoryspace_row_limited(uint64_t base, uint64_t size, uint64_t rows, uint64_t stride, uint64_t limit)
{
	auto g = memoryspace_row_bounds(base, size, rows, stride);
	if(g.first > g.second)
		return true;
	return (g.second < limit);
}
