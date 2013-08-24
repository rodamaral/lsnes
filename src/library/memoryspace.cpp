#include "memoryspace.hpp"
#include "minmax.hpp"
#include <algorithm>

namespace
{
	template<typename T, bool linear> inline T internal_read(memory_space& m, uint64_t addr)
	{
		const int system_endian = memory_space::get_system_endian();
		std::pair<memory_region*, uint64_t> g;
		if(linear)
			g = m.lookup_linear(addr);
		else
			g = m.lookup(addr);
		if(!g.first || g.second + sizeof(T) > g.first->size)
			return 0;
		if((!g.first->endian || g.first->endian == system_endian) && memory_space::can_read_unaligned()) {
			//Native endian.
			T buf;
			if(g.first->direct_map)
				return *reinterpret_cast<T*>(g.first->direct_map + g.second);
			else
				g.first->read(g.second, &buf, sizeof(T));
			return buf;
		} else {
			//Can't read directly.
			unsigned char buf[sizeof(T)];
			if(g.first->direct_map)
				memcpy(buf, g.first->direct_map + g.second, sizeof(T));
			else
				g.first->read(g.second, buf, sizeof(T));
			if(g.first->endian && g.first->endian != system_endian) {
				//Needs byteswap.
				for(size_t i = 0; i < sizeof(T) / 2; i++)
					std::swap(buf[i], buf[sizeof(T) - i - 1]);
			}
			return *reinterpret_cast<T*>(buf);
		}
	}

	template<typename T, bool linear> inline bool internal_write(memory_space& m, uint64_t addr, T value)
	{
		const int system_endian = memory_space::get_system_endian();
		std::pair<memory_region*, uint64_t> g;
		if(linear)
			g = m.lookup_linear(addr);
		else
			g = m.lookup(addr);
		if(!g.first || g.first->readonly || g.second + sizeof(T) > g.first->size)
			return false;
		if((!g.first->endian || g.first->endian == system_endian) && memory_space::can_read_unaligned()) {
			//Native endian.
			if(g.first->direct_map) {
				*reinterpret_cast<T*>(g.first->direct_map + g.second) = value;
				return true;
			} else
				g.first->write(g.second, &value, sizeof(T));
		} else {
			//The opposite endian (little).
			unsigned char buf[sizeof(T)];
			*reinterpret_cast<T*>(buf) = value;
			if(g.first->endian && g.first->endian != system_endian) {
				//Needs byteswap.
				for(size_t i = 0; i < sizeof(T) / 2; i++)
					std::swap(buf[i], buf[sizeof(T) - i - 1]);
			}
			if(g.first->direct_map)
				memcpy(g.first->direct_map + g.second, buf, sizeof(T));
			else
				return g.first->write(g.second, buf, sizeof(T));
		}
		return true;
	}

	void read_range_r(memory_region& r, uint64_t offset, void* buffer, size_t bsize)
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

	bool write_range_r(memory_region& r, uint64_t offset, const void* buffer, size_t bsize)
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

memory_region::~memory_region() throw()
{
}

void memory_region::read(uint64_t offset, void* buffer, size_t tsize)
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

bool memory_region::write(uint64_t offset, const void* buffer, size_t tsize)
{
	if(!direct_map || readonly || offset >= size)
		return false;
	uint64_t maxcopy = min(static_cast<uint64_t>(tsize), size - offset);
	memcpy(direct_map + offset, buffer, maxcopy);
	return true;
}

std::pair<memory_region*, uint64_t> memory_space::lookup(uint64_t address)
{
	umutex_class m(mutex);
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
	return std::make_pair(reinterpret_cast<memory_region*>(NULL), 0);
}

std::pair<memory_region*, uint64_t> memory_space::lookup_linear(uint64_t linear)
{
	umutex_class m(mutex);
	if(linear >= linear_size)
		return std::make_pair(reinterpret_cast<memory_region*>(NULL), 0);
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
	return std::make_pair(reinterpret_cast<memory_region*>(NULL), 0);
}

#define MSR memory_space::read
#define MSW memory_space::write
#define MSRL memory_space::read_linear
#define MSWL memory_space::write_linear

template<> int8_t MSR (uint64_t address) { return internal_read<int8_t, false>(*this, address); }
template<> uint8_t MSR (uint64_t address) { return internal_read<uint8_t, false>(*this, address); }
template<> int16_t MSR (uint64_t address) { return internal_read<int16_t, false>(*this, address); }
template<> uint16_t MSR (uint64_t address) { return internal_read<uint16_t, false>(*this, address); }
template<> int32_t MSR (uint64_t address) { return internal_read<int32_t, false>(*this, address); }
template<> uint32_t MSR (uint64_t address) { return internal_read<uint32_t, false>(*this, address); }
template<> int64_t MSR (uint64_t address) { return internal_read<int64_t, false>(*this, address); }
template<> uint64_t MSR (uint64_t address) { return internal_read<uint64_t, false>(*this, address); }
template<> bool MSW (uint64_t a, int8_t v) { return internal_write<int8_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint8_t v) { return internal_write<uint8_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, int16_t v) { return internal_write<int16_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint16_t v) { return internal_write<uint16_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, int32_t v) { return internal_write<int32_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint32_t v) { return internal_write<uint32_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, int64_t v) { return internal_write<int64_t, false>(*this, a, v); }
template<> bool MSW (uint64_t a, uint64_t v) { return internal_write<uint64_t, false>(*this, a, v); }
template<> int8_t MSRL (uint64_t address) { return internal_read<int8_t, true>(*this, address); }
template<> uint8_t MSRL (uint64_t address) { return internal_read<uint8_t, true>(*this, address); }
template<> int16_t MSRL (uint64_t address) { return internal_read<int16_t, true>(*this, address); }
template<> uint16_t MSRL (uint64_t address) { return internal_read<uint16_t, true>(*this, address); }
template<> int32_t MSRL (uint64_t address) { return internal_read<int32_t, true>(*this, address); }
template<> uint32_t MSRL (uint64_t address) { return internal_read<uint32_t, true>(*this, address); }
template<> int64_t MSRL (uint64_t address) { return internal_read<int64_t, true>(*this, address); }
template<> uint64_t MSRL (uint64_t address) { return internal_read<uint64_t, true>(*this, address); }
template<> bool MSWL (uint64_t a, int8_t v) { return internal_write<int8_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint8_t v) { return internal_write<uint8_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, int16_t v) { return internal_write<int16_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint16_t v) { return internal_write<uint16_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, int32_t v) { return internal_write<int32_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint32_t v) { return internal_write<uint32_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, int64_t v) { return internal_write<int64_t, true>(*this, a, v); }
template<> bool MSWL (uint64_t a, uint64_t v) { return internal_write<uint64_t, true>(*this, a, v); }

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

memory_region* memory_space::lookup_n(size_t n)
{
	umutex_class m(mutex);
	if(n >= u_regions.size())
		return NULL;
	return u_regions[n];
}


std::list<memory_region*> memory_space::get_regions()
{
	umutex_class m(mutex);
	std::list<memory_region*> r;
	for(auto i : u_regions)
		r.push_back(i);
	return r;
}

void memory_space::set_regions(const std::list<memory_region*>& regions)
{
	umutex_class m(mutex);
	std::vector<memory_region*> n_regions;
	std::vector<memory_region*> n_lregions;
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
		[](memory_region* a, memory_region* b) -> bool { return a->base < b->base; });

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

memory_region_direct::memory_region_direct(const std::string& _name, uint64_t _base, int _endian,
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

memory_region_direct::~memory_region_direct() throw() {}
