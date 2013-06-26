#include "core/emucore.hpp"

#include "core/command.hpp"
#include "core/memorymanip.hpp"
#include "core/moviedata.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"

#include <iostream>
#include <limits>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>

namespace
{
	struct translated_address
	{
		uint64_t rel_addr;
		uint64_t raw_addr;
		uint8_t* memory;
		uint64_t memory_size;
		bool not_writable;
		bool native_endian;
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write);
	};

	struct region
	{
		std::string name;
		uint64_t base;
		uint64_t size;
		uint8_t* memory;
		bool not_writable;
		bool native_endian;
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write);
	};

	std::vector<region> memory_regions;
	uint64_t linear_ram_size = 0;
	bool system_little_endian = true;

	uint8_t lsnes_mmio_iospace_handler(uint64_t offset, uint8_t data, bool write)
	{
		if(offset >= 0 && offset < 8 && !write) {
			//Frame counter.
			uint64_t x = get_movie().get_current_frame();
			return x >> (8 * (offset & 7));
		} else if(offset >= 8 && offset < 16 && !write) {
			//Movie length.
			uint64_t x = get_movie().get_frame_count();
			return x >> (8 * (offset & 7));
		} else if(offset >= 16 && offset < 24 && !write) {
			//Lag counter.
			uint64_t x = get_movie().get_lag_frames();
			return x >> (8 * (offset & 7));
		} else if(offset >= 24 && offset < 32 && !write) {
			//Rerecord counter.
			uint64_t x = rrdata::count();
			return x >> (8 * (offset & 7));
		} else
			return 0;
	}

	struct translated_address translate_address(uint64_t rawaddr) throw()
	{
		struct translated_address t;
		t.rel_addr = 0;
		t.raw_addr = 0;
		t.memory = NULL;
		t.memory_size = 0;
		t.not_writable = true;
		if((rawaddr >> 32) == 0xFFFFFFFFULL) {
			//lsnes MMIO.
			t.rel_addr = rawaddr & 0xFFFFFFFFULL;
			t.raw_addr = rawaddr;
			t.memory = NULL;
			t.memory_size = 0x100000000ULL;
			t.not_writable = false;
			t.native_endian = false;
			t.iospace_rw = lsnes_mmio_iospace_handler;
			return t;
		}
		for(auto i : memory_regions) {
			if(i.base > rawaddr || i.base + i.size <= rawaddr)
				continue;
			t.rel_addr = rawaddr - i.base;
			t.raw_addr = rawaddr;
			t.memory = i.memory;
			t.memory_size = i.size;
			t.not_writable = i.not_writable;
			t.native_endian = i.native_endian;
			t.iospace_rw = i.iospace_rw;
			break;
		}
		return t;
	}

	struct translated_address translate_address_linear_ram(uint64_t ramlinaddr) throw()
	{
		struct translated_address t;
		t.rel_addr = 0;
		t.raw_addr = 0;
		t.memory = NULL;
		t.memory_size = 0;
		t.not_writable = true;
		for(auto i : memory_regions) {
			if(i.not_writable || i.iospace_rw)
				continue;
			if(ramlinaddr >= i.size) {
				ramlinaddr -= i.size;
				continue;
			}
			t.rel_addr = ramlinaddr;
			t.raw_addr = i.base + ramlinaddr;
			t.memory = i.memory;
			t.memory_size = i.size;
			t.not_writable = i.not_writable;
			t.native_endian = i.native_endian;
			break;
		}
		return t;
	}

	uint64_t get_linear_ram_size() throw()
	{
		return linear_ram_size;
	}

	uint64_t create_region(const std::string& name, uint64_t base, uint64_t size,
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write)) throw(std::bad_alloc)
	{
		if(size == 0)
			return base;
		struct region r;
		r.name = name;
		r.base = base;
		r.memory = NULL;
		r.size = size;
		r.not_writable = false;
		r.native_endian = false;
		r.iospace_rw = iospace_rw;
		memory_regions.push_back(r);
		return base + size;
	}

	uint64_t create_region(const std::string& name, uint64_t base, uint8_t* memory, uint64_t size, bool readonly,
		bool native_endian = false) throw(std::bad_alloc)
	{
		if(size == 0)
			return base;
		struct region r;
		r.name = name;
		r.base = base;
		r.memory = memory;
		r.size = size;
		r.not_writable = readonly;
		r.native_endian = native_endian;
		r.iospace_rw = NULL;
		if(!readonly)
			linear_ram_size += size;
		memory_regions.push_back(r);
		return base + size;
	}

	uint8_t native_littleendian_convert(uint8_t x) throw()
	{
		return x;
	}

	uint16_t native_littleendian_convert(uint16_t x) throw()
	{
		if(!system_little_endian)
			return (((x >> 8) & 0xFF) | ((x << 8) & 0xFF00));
		else
			return x;
	}

	uint32_t native_littleendian_convert(uint32_t x) throw()
	{
		if(!system_little_endian)
			return (((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
				((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000));
		else
			return x;
	}

	uint64_t native_littleendian_convert(uint64_t x) throw()
	{
		if(!system_little_endian)
			return (((x >> 56) & 0xFF) | ((x >> 40) & 0xFF00) |
				((x >> 24) & 0xFF0000) | ((x >> 8) & 0xFF000000) |
				((x << 8) & 0xFF00000000ULL) | ((x << 24) & 0xFF0000000000ULL) |
				((x << 40) & 0xFF000000000000ULL) | ((x << 56) & 0xFF00000000000000ULL));
		else
			return x;
	}

	template<typename T>
	inline T memory_read_generic(uint64_t addr) throw()
	{
		struct translated_address laddr = translate_address(addr);
		T value = 0;
		if(laddr.iospace_rw) {
			for(size_t i = 0; i < sizeof(T); i++)
				if(laddr.rel_addr < laddr.memory_size)
					value |= laddr.iospace_rw(laddr.rel_addr++, 0, false) << (8 * i);
		} else {
			for(size_t i = 0; i < sizeof(T); i++)
				if(laddr.rel_addr < laddr.memory_size)
					value |= laddr.memory[laddr.rel_addr++] << (8 * i);
		}
		if(laddr.native_endian)
			value = native_littleendian_convert(value);
		return value;
	}

	template<typename T>
	inline bool memory_write_generic(uint64_t addr, T data) throw()
	{
		struct translated_address laddr = translate_address(addr);
		if(laddr.native_endian)
			data = native_littleendian_convert(data);
		if(laddr.rel_addr >= laddr.memory_size - (sizeof(T) - 1) || laddr.not_writable)
			return false;
		if(laddr.iospace_rw) {
			for(size_t i = 0; i < sizeof(T); i++)
				laddr.iospace_rw(laddr.rel_addr++, static_cast<uint8_t>(data >> (8 * i)), true);
		} else {
			for(size_t i = 0; i < sizeof(T); i++)
				laddr.memory[laddr.rel_addr++] = static_cast<uint8_t>(data >> (8 * i));
		}
		return true;
	}
}

void refresh_cart_mappings() throw(std::bad_alloc)
{
	linear_ram_size = 0;
	memory_regions.clear();
	if(!get_current_rom_info().first)
		return;
	auto vmalist = get_vma_list();
	for(auto i : vmalist) {
		if(i.iospace_rw)
			create_region(i.name, i.base, i.size, i.iospace_rw);
		else
			create_region(i.name, i.base, reinterpret_cast<uint8_t*>(i.backing_ram), i.size, i.readonly,
				i.native_endian);
	}
}

std::vector<struct memory_region> get_regions() throw(std::bad_alloc)
{
	std::vector<struct memory_region> out;
	for(auto i : memory_regions) {
		struct memory_region r;
		r.region_name = i.name;
		r.baseaddr = i.base;
		r.size = i.size;
		r.lastaddr = i.base + i.size - 1;
		r.readonly = i.not_writable;
		r.iospace = (i.iospace_rw != NULL);
		r.native_endian = i.native_endian;
		out.push_back(r);
	}
	return out;
}

uint8_t memory_read_byte(uint64_t addr) throw()
{
	return memory_read_generic<uint8_t>(addr);
}

void memory_read_bytes(uint64_t addr, uint64_t size, char* buffer) throw()
{
	struct translated_address laddr = translate_address(addr);
	size_t ssize = min(static_cast<uint64_t>(size), laddr.memory_size - laddr.rel_addr);
	if(ssize > 0) {
		if(laddr.iospace_rw) {
			for(size_t i = 0; i < ssize; i++)
				buffer[i] = laddr.iospace_rw(laddr.rel_addr++, 0, false);
		} else
			memcpy(buffer, laddr.memory + laddr.rel_addr, ssize);
	}
	memset(buffer + ssize, 0, size - ssize);
}

uint16_t memory_read_word(uint64_t addr) throw()
{
	return memory_read_generic<uint16_t>(addr);
}

uint32_t memory_read_dword(uint64_t addr) throw()
{
	return memory_read_generic<uint32_t>(addr);
}

uint64_t memory_read_qword(uint64_t addr) throw()
{
	return memory_read_generic<uint64_t>(addr);
}

//Byte write to address (false if failed).
bool memory_write_byte(uint64_t addr, uint8_t data) throw()
{
	return memory_write_generic<uint8_t>(addr, data);
}

void memory_write_bytes(uint64_t addr, uint64_t size, const char* buffer) throw()
{
	struct translated_address laddr = translate_address(addr);
	if(laddr.not_writable)
		return;
	size_t ssize = min(static_cast<uint64_t>(size), laddr.memory_size - laddr.rel_addr);
	if(ssize > 0) {
		if(laddr.iospace_rw) {
			for(size_t i = 0; i < ssize; i++)
				laddr.iospace_rw(laddr.rel_addr++, buffer[i], true);
		} else
			memcpy(laddr.memory + laddr.rel_addr, buffer, ssize);
	}
}

bool memory_write_word(uint64_t addr, uint16_t data) throw()
{
	return memory_write_generic<uint16_t>(addr, data);
}

bool memory_write_dword(uint64_t addr, uint32_t data) throw()
{
	return memory_write_generic<uint32_t>(addr, data);
}

bool memory_write_qword(uint64_t addr, uint64_t data) throw()
{
	return memory_write_generic<uint64_t>(addr, data);
}

memorysearch::memorysearch() throw(std::bad_alloc)
{
	reset();
}

void memorysearch::reset() throw(std::bad_alloc)
{
	uint64_t linearram = get_linear_ram_size();
	previous_content.resize(linearram);
	still_in.resize((linearram + 63) / 64);
	for(uint64_t i = 0; i < linearram / 64; i++)
		still_in[i] = 0xFFFFFFFFFFFFFFFFULL;
	if(linearram % 64)
		still_in[linearram / 64] = (1ULL << (linearram % 64)) - 1;
	uint64_t addr = 0;
	while(addr < linearram) {
		struct translated_address t = translate_address_linear_ram(addr);
		memcpy(&previous_content[addr], t.memory, t.memory_size);
		addr += t.memory_size;
	}
	candidates = linearram;
}

/**
 * \brief Native-value search function for trivial true function
 */
struct search_update
{
/**
 * \brief The underlying numeric type
 */
	typedef uint8_t value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(uint8_t oldv, uint8_t newv) const throw()
	{
		return true;
	}
};

/**
 * \brief Native-value search function for specific value
 */
template<typename T>
struct search_value
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Create new search object
 *
 * \param v The value to search for.
 */
	search_value(T v) throw()
	{
		val = v;
	}

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv == val);
	}

/**
 * \brief The value to look for
 */
	T val;
};

/**
 * \brief Native-value search function for specific difference
 */
template<typename T>
struct search_difference
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Create new search object
 *
 * \param v The value to search for.
 */
	search_difference(T v) throw()
	{
		val = v;
	}

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return ((newv - oldv) == val);
	}

/**
 * \brief The value to look for
 */
	T val;
};

/**
 * \brief Native-value search function for less-than function.
 */
template<typename T>
struct search_lt
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv < oldv);
	}
};

/**
 * \brief Native-value search function for less-or-equal-to function.
 */
template<typename T>
struct search_le
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv <= oldv);
	}
};

/**
 * \brief Native-value search function for equals function.
 */
template<typename T>
struct search_eq
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv == oldv);
	}
};

/**
 * \brief Native-value search function for not-equal function.
 */
template<typename T>
struct search_ne
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv != oldv);
	}
};

/**
 * \brief Native-value search function for greater-or-equal-to function.
 */
template<typename T>
struct search_ge
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv >= oldv);
	}
};

/**
 * \brief Native-value search function for greater-than function.
 */
template<typename T>
struct search_gt
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		return (newv > oldv);
	}
};

/**
 * \brief Native-value search function for sequence less-than function.
 */
template<typename T>
struct search_seqlt
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) != 0);
	}
};

/**
 * \brief Native-value search function for sequence less-or-equal function.
 */
template<typename T>
struct search_seqle
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) != 0) || (diff == 0);
	}
};

/**
 * \brief Native-value search function for sequence greater-or-equal function.
 */
template<typename T>
struct search_seqge
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) == 0);
	}
};

/**
 * \brief Native-value search function for sequence greater-than function.
 */
template<typename T>
struct search_seqgt
{
/**
 * \brief The underlying numeric type
 */
	typedef T value_type;

/**
 * \brief Condition function.
 * \param oldv The old value
 * \param newv The new value
 * \return True if new value satisfies condition, false otherwise.
 */
	bool operator()(T oldv, T newv) const throw()
	{
		T mask = (T)1 << (sizeof(T) * 8 - 1);
		T diff = newv - oldv;
		return ((diff & mask) == 0) && (diff != 0);
	}
};


/**
 * \brief Helper class to decode arguments to search functions
 *
 * This class acts as adapter between search conditions taking native numbers as parameters and the interface
 * expected for the search function.
 */
template<typename T>
struct search_value_helper
{
/**
 * \brief The underlying numeric type
 */
	typedef typename T::value_type value_type;

/**
 * \brief Constructor constructing condition object
 *
 * This constructor takes in condition object with the native-value interface and makes condition object with
 * interface used by search().
 *
 * \param v The condition object to wrap.
 */
	search_value_helper(const T& v)  throw()
		: val(v)
	{
	}

/**
 * \brief Condition function
 *
 * This function is search()-compatible condition function calling the underlying condition.
 */
	bool operator()(const uint8_t* newv, const uint8_t* oldv, uint64_t left, bool nativeendian) const throw()
	{
		if(left < sizeof(value_type))
			return false;
		value_type v1 = 0;
		value_type v2 = 0;
		if(nativeendian) {
			v1 = *reinterpret_cast<const value_type*>(oldv);
			v2 = *reinterpret_cast<const value_type*>(newv);
		} else
			for(size_t i = 0; i < sizeof(value_type); i++) {
				v1 |= static_cast<value_type>(oldv[i]) << (8 * i);
				v2 |= static_cast<value_type>(newv[i]) << (8 * i);
			}
		return val(v1, v2);
	}

/**
 * \brief The underlying condition.
 */
	const T& val;
};

void memorysearch::dq_range(uint64_t first, uint64_t last)
{
	struct translated_address t = translate_address_linear_ram(0);
	uint64_t switch_at = t.memory_size;
	uint64_t base = 0;
	uint64_t size = previous_content.size();
	for(uint64_t i = 0; i < size; i++) {
		if(still_in[i / 64] == 0) {
			i = (i + 64) >> 6 << 6;
			i--;
			continue;
		}
		//t.memory_size == 0 can happen if cart changes.
		while(i >= switch_at && t.memory_size > 0) {
			t = translate_address_linear_ram(switch_at);
			base = switch_at;
			switch_at += t.memory_size;
		}
		uint64_t addr = t.raw_addr + (i - base);
		if(t.memory_size == 0 || (addr >= first && addr <= last)) {
			if((still_in[i / 64] >> (i % 64)) & 1) {
				still_in[i / 64] &= ~(1ULL << (i % 64));
				candidates--;
			}
		}
	}
}


template<class T> void memorysearch::search(const T& obj) throw()
{
	search_value_helper<T> helper(obj);
	struct translated_address t = translate_address_linear_ram(0);
	uint64_t switch_at = t.memory_size;
	uint64_t base = 0;
	uint64_t size = previous_content.size();
	for(uint64_t i = 0; i < size; i++) {
		if(still_in[i / 64] == 0) {
			i = (i + 64) >> 6 << 6;
			i--;
			continue;
		}
		//t.memory_size == 0 can happen if cart changes.
		while(i >= switch_at && t.memory_size > 0) {
			t = translate_address_linear_ram(switch_at);
			base = switch_at;
			switch_at += t.memory_size;
		}
		if(t.memory_size == 0 || !helper(t.memory + i - base, &previous_content[i],
			t.memory_size - (i - base), t.native_endian)) {
			if((still_in[i / 64] >> (i % 64)) & 1) {
				still_in[i / 64] &= ~(1ULL << (i % 64));
				candidates--;
			}
		}
	}
	t = translate_address_linear_ram(0);
	base = 0;
	size = previous_content.size();
	while(base < size) {
		size_t m = t.memory_size;
		if(m > (size - base))
			m = size - base;
		memcpy(&previous_content[base], t.memory, m);
		base += t.memory_size;
		t = translate_address_linear_ram(base);
	}
}

void memorysearch::byte_value(uint8_t value) throw() { search(search_value<uint8_t>(value)); }
void memorysearch::byte_difference(uint8_t value) throw() { search(search_difference<uint8_t>(value)); }
void memorysearch::byte_slt() throw() { search(search_lt<int8_t>()); }
void memorysearch::byte_sle() throw() { search(search_le<int8_t>()); }
void memorysearch::byte_seq() throw() { search(search_eq<int8_t>()); }
void memorysearch::byte_sne() throw() { search(search_ne<int8_t>()); }
void memorysearch::byte_sge() throw() { search(search_ge<int8_t>()); }
void memorysearch::byte_sgt() throw() { search(search_gt<int8_t>()); }
void memorysearch::byte_ult() throw() { search(search_lt<uint8_t>()); }
void memorysearch::byte_ule() throw() { search(search_le<uint8_t>()); }
void memorysearch::byte_ueq() throw() { search(search_eq<uint8_t>()); }
void memorysearch::byte_une() throw() { search(search_ne<uint8_t>()); }
void memorysearch::byte_uge() throw() { search(search_ge<uint8_t>()); }
void memorysearch::byte_ugt() throw() { search(search_gt<uint8_t>()); }
void memorysearch::byte_seqlt() throw() { search(search_seqlt<uint8_t>()); }
void memorysearch::byte_seqle() throw() { search(search_seqle<uint8_t>()); }
void memorysearch::byte_seqge() throw() { search(search_seqge<uint8_t>()); }
void memorysearch::byte_seqgt() throw() { search(search_seqgt<uint8_t>()); }

void memorysearch::word_value(uint16_t value) throw() { search(search_value<uint16_t>(value)); }
void memorysearch::word_difference(uint16_t value) throw() { search(search_difference<uint16_t>(value)); }
void memorysearch::word_slt() throw() { search(search_lt<int16_t>()); }
void memorysearch::word_sle() throw() { search(search_le<int16_t>()); }
void memorysearch::word_seq() throw() { search(search_eq<int16_t>()); }
void memorysearch::word_sne() throw() { search(search_ne<int16_t>()); }
void memorysearch::word_sge() throw() { search(search_ge<int16_t>()); }
void memorysearch::word_sgt() throw() { search(search_gt<int16_t>()); }
void memorysearch::word_ult() throw() { search(search_lt<uint16_t>()); }
void memorysearch::word_ule() throw() { search(search_le<uint16_t>()); }
void memorysearch::word_ueq() throw() { search(search_eq<uint16_t>()); }
void memorysearch::word_une() throw() { search(search_ne<uint16_t>()); }
void memorysearch::word_uge() throw() { search(search_ge<uint16_t>()); }
void memorysearch::word_ugt() throw() { search(search_gt<uint16_t>()); }
void memorysearch::word_seqlt() throw() { search(search_seqlt<uint16_t>()); }
void memorysearch::word_seqle() throw() { search(search_seqle<uint16_t>()); }
void memorysearch::word_seqge() throw() { search(search_seqge<uint16_t>()); }
void memorysearch::word_seqgt() throw() { search(search_seqgt<uint16_t>()); }

void memorysearch::dword_value(uint32_t value) throw() { search(search_value<uint32_t>(value)); }
void memorysearch::dword_difference(uint32_t value) throw() { search(search_difference<uint32_t>(value)); }
void memorysearch::dword_slt() throw() { search(search_lt<int32_t>()); }
void memorysearch::dword_sle() throw() { search(search_le<int32_t>()); }
void memorysearch::dword_seq() throw() { search(search_eq<int32_t>()); }
void memorysearch::dword_sne() throw() { search(search_ne<int32_t>()); }
void memorysearch::dword_sge() throw() { search(search_ge<int32_t>()); }
void memorysearch::dword_sgt() throw() { search(search_gt<int32_t>()); }
void memorysearch::dword_ult() throw() { search(search_lt<uint32_t>()); }
void memorysearch::dword_ule() throw() { search(search_le<uint32_t>()); }
void memorysearch::dword_ueq() throw() { search(search_eq<uint32_t>()); }
void memorysearch::dword_une() throw() { search(search_ne<uint32_t>()); }
void memorysearch::dword_uge() throw() { search(search_ge<uint32_t>()); }
void memorysearch::dword_ugt() throw() { search(search_gt<uint32_t>()); }
void memorysearch::dword_seqlt() throw() { search(search_seqlt<uint32_t>()); }
void memorysearch::dword_seqle() throw() { search(search_seqle<uint32_t>()); }
void memorysearch::dword_seqge() throw() { search(search_seqge<uint32_t>()); }
void memorysearch::dword_seqgt() throw() { search(search_seqgt<uint32_t>()); }

void memorysearch::qword_value(uint64_t value) throw() { search(search_value<uint64_t>(value)); }
void memorysearch::qword_difference(uint64_t value) throw() { search(search_difference<uint64_t>(value)); }
void memorysearch::qword_slt() throw() { search(search_lt<int64_t>()); }
void memorysearch::qword_sle() throw() { search(search_le<int64_t>()); }
void memorysearch::qword_seq() throw() { search(search_eq<int64_t>()); }
void memorysearch::qword_sne() throw() { search(search_ne<int64_t>()); }
void memorysearch::qword_sge() throw() { search(search_ge<int64_t>()); }
void memorysearch::qword_sgt() throw() { search(search_gt<int64_t>()); }
void memorysearch::qword_ult() throw() { search(search_lt<uint64_t>()); }
void memorysearch::qword_ule() throw() { search(search_le<uint64_t>()); }
void memorysearch::qword_ueq() throw() { search(search_eq<uint64_t>()); }
void memorysearch::qword_une() throw() { search(search_ne<uint64_t>()); }
void memorysearch::qword_uge() throw() { search(search_ge<uint64_t>()); }
void memorysearch::qword_ugt() throw() { search(search_gt<uint64_t>()); }
void memorysearch::qword_seqlt() throw() { search(search_seqlt<uint64_t>()); }
void memorysearch::qword_seqle() throw() { search(search_seqle<uint64_t>()); }
void memorysearch::qword_seqge() throw() { search(search_seqge<uint64_t>()); }
void memorysearch::qword_seqgt() throw() { search(search_seqgt<uint64_t>()); }

void memorysearch::update() throw() { search(search_update()); }

uint64_t memorysearch::get_candidate_count() throw()
{
	return candidates;
}

std::list<uint64_t> memorysearch::get_candidates() throw(std::bad_alloc)
{
	struct translated_address t = translate_address_linear_ram(0);
	uint64_t switch_at = t.memory_size;
	uint64_t base = 0;
	uint64_t rbase = t.raw_addr;
	uint64_t size = previous_content.size();
	std::list<uint64_t> out;

	for(uint64_t i = 0; i < size; i++) {
		if(still_in[i / 64] == 0) {
			i = (i + 64) >> 6 << 6;
			i--;
			continue;
		}
		while(i >= switch_at && t.memory_size > 0) {
			t = translate_address_linear_ram(switch_at);
			base = switch_at;
			rbase = t.raw_addr - t.rel_addr;
			switch_at += t.memory_size;
		}
		if((still_in[i / 64] >> (i % 64)) & 1)
			out.push_back(i - base + rbase);
	}
	return out;
}

namespace
{
	memorysearch* isrch;

	std::string tokenize1(const std::string& command, const std::string& syntax);
	std::pair<std::string, std::string> tokenize2(const std::string& command, const std::string& syntax);
	std::pair<std::string, std::string> tokenize12(const std::string& command, const std::string& syntax);

	unsigned char hex(char ch)
	{
		switch(ch) {
		case '0':			return 0;
		case '1':			return 1;
		case '2':			return 2;
		case '3':			return 3;
		case '4':			return 4;
		case '5':			return 5;
		case '6':			return 6;
		case '7':			return 7;
		case '8':			return 8;
		case '9':			return 9;
		case 'a':	case 'A':	return 10;
		case 'b':	case 'B':	return 11;
		case 'c':	case 'C':	return 12;
		case 'd':	case 'D':	return 13;
		case 'e':	case 'E':	return 14;
		case 'f':	case 'F':	return 15;
		};
		throw std::runtime_error("Bad hex character");
	}

	class memorymanip_command : public command
	{
	public:
		memorymanip_command(const std::string& cmd) throw(std::bad_alloc)
			: command(cmd)
		{
			_command = cmd;
		}
		~memorymanip_command() throw() {}
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			regex_results t = regex("(([^ \t]+)([ \t]+([^ \t]+)([ \t]+([^ \t].*)?)?)?)?", args);
			firstword = t[2];
			secondword = t[4];
			has_tail = (t[6] != "");
			address_bad = true;
			value_bad = true;
			has_value = (secondword != "");
			try {
				if(t = regex("0x(.+)", firstword)) {
					if(t[1].length() > 16)
						throw 42;
					address = 0;
					for(unsigned i = 0; i < t[1].length(); i++)
						address = 16 * address + hex(t[1][i]);
				} else {
					address = parse_value<uint64_t>(firstword);
				}
				address_bad = false;
			} catch(...) {
			}
			try {
				if(t = regex("0x(.+)", secondword)) {
					if(t[1].length() > 16)
						throw 42;
					value = 0;
					for(unsigned i = 0; i < t[1].length(); i++)
						value = 16 * value + hex(t[1][i]);
				} else if(regex("-.*", secondword)) {
					value = static_cast<uint64_t>(parse_value<int64_t>(secondword));
				} else {
					value = parse_value<uint64_t>(secondword);
				}
				value_bad = false;
			} catch(...) {
			}
			invoke2();
		}
		virtual void invoke2() throw(std::bad_alloc, std::runtime_error) = 0;
		std::string firstword;
		std::string secondword;
		uint64_t address;
		uint64_t value;
		bool has_tail;
		bool address_bad;
		bool value_bad;
		bool has_value;
		std::string _command;
	};

	template<typename outer, typename inner, typename ret>
	class read_command : public memorymanip_command
	{
	public:
		read_command(const std::string& cmd, ret (*_rfn)(uint64_t addr)) throw(std::bad_alloc)
			: memorymanip_command(cmd)
		{
			rfn = _rfn;
		}
		~read_command() throw() {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(address_bad || has_value || has_tail)
				throw std::runtime_error("Syntax: " + _command + " <address>");
			{
				std::ostringstream x;
				x << "0x" << std::hex << address << " -> " << std::dec
					<< static_cast<outer>(static_cast<inner>(rfn(address)));
				messages << x.str() << std::endl;
			}
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Read memory"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " <address>\n"
				"Reads data from memory.\n";
		}

		ret (*rfn)(uint64_t addr);
	};

	template<typename arg, int64_t low, uint64_t high>
	class write_command : public memorymanip_command
	{
	public:
		write_command(const std::string& cmd, bool (*_wfn)(uint64_t addr, arg a)) throw(std::bad_alloc)
			: memorymanip_command(cmd)
		{
			wfn = _wfn;
		}
		~write_command() throw() {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(address_bad || value_bad || has_tail)
				throw std::runtime_error("Syntax: " + _command + " <address> <value>");
			int64_t value2 = static_cast<int64_t>(value);
			if(value2 < low || (value > high && value2 >= 0))
				throw std::runtime_error("Value to write out of range");
			wfn(address, value & high);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Write memory"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " <address> <value>\n"
				"Writes data to memory.\n";
		}
		bool (*wfn)(uint64_t addr, arg a);
	};

	class memorysearch_command : public memorymanip_command
	{
	public:
		memorysearch_command() throw(std::bad_alloc) : memorymanip_command("search-memory") {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(!isrch)
				isrch = new memorysearch();
			if(firstword == "sblt" && !has_value)
				isrch->byte_slt();
			else if(firstword == "sble" && !has_value)
				isrch->byte_sle();
			else if(firstword == "sbeq" && !has_value)
				isrch->byte_seq();
			else if(firstword == "sbne" && !has_value)
				isrch->byte_sne();
			else if(firstword == "sbge" && !has_value)
				isrch->byte_sge();
			else if(firstword == "sbgt" && !has_value)
				isrch->byte_sgt();
			else if(firstword == "ublt" && !has_value)
				isrch->byte_ult();
			else if(firstword == "uble" && !has_value)
				isrch->byte_ule();
			else if(firstword == "ubeq" && !has_value)
				isrch->byte_ueq();
			else if(firstword == "ubne" && !has_value)
				isrch->byte_une();
			else if(firstword == "ubge" && !has_value)
				isrch->byte_uge();
			else if(firstword == "ubgt" && !has_value)
				isrch->byte_ugt();
			else if(firstword == "bseqlt" && !has_value)
				isrch->byte_seqlt();
			else if(firstword == "bseqle" && !has_value)
				isrch->byte_seqle();
			else if(firstword == "bseqge" && !has_value)
				isrch->byte_seqge();
			else if(firstword == "bseqgt" && !has_value)
				isrch->byte_seqgt();
			else if(firstword == "b" && has_value) {
				if(static_cast<int64_t>(value) < -128 || value > 255)
					throw std::runtime_error("Value to compare out of range");
				isrch->byte_value(value & 0xFF);
			} else if(firstword == "bdiff" && has_value) {
				if(static_cast<int64_t>(value) < -128 || value > 255)
					throw std::runtime_error("Value to compare out of range");
				isrch->byte_difference(value & 0xFF);
			} else if(firstword == "swlt" && !has_value)
				isrch->word_slt();
			else if(firstword == "swle" && !has_value)
				isrch->word_sle();
			else if(firstword == "sweq" && !has_value)
				isrch->word_seq();
			else if(firstword == "swne" && !has_value)
				isrch->word_sne();
			else if(firstword == "swge" && !has_value)
				isrch->word_sge();
			else if(firstword == "swgt" && !has_value)
				isrch->word_sgt();
			else if(firstword == "uwlt" && !has_value)
				isrch->word_ult();
			else if(firstword == "uwle" && !has_value)
				isrch->word_ule();
			else if(firstword == "uweq" && !has_value)
				isrch->word_ueq();
			else if(firstword == "uwne" && !has_value)
				isrch->word_une();
			else if(firstword == "uwge" && !has_value)
				isrch->word_uge();
			else if(firstword == "uwgt" && !has_value)
				isrch->word_ugt();
			else if(firstword == "wseqlt" && !has_value)
				isrch->word_seqlt();
			else if(firstword == "wseqle" && !has_value)
				isrch->word_seqle();
			else if(firstword == "wseqge" && !has_value)
				isrch->word_seqge();
			else if(firstword == "wseqgt" && !has_value)
				isrch->word_seqgt();
			else if(firstword == "w" && has_value) {
				if(static_cast<int64_t>(value) < -32768 || value > 65535)
					throw std::runtime_error("Value to compare out of range");
				isrch->word_value(value & 0xFFFF);
			} else if(firstword == "wdiff" && has_value) {
				if(static_cast<int64_t>(value) < -32768 || value > 65535)
					throw std::runtime_error("Value to compare out of range");
				isrch->word_difference(value & 0xFFFF);
			} else if(firstword == "sdlt" && !has_value)
				isrch->dword_slt();
			else if(firstword == "sdle" && !has_value)
				isrch->dword_sle();
			else if(firstword == "sdeq" && !has_value)
				isrch->dword_seq();
			else if(firstword == "sdne" && !has_value)
				isrch->dword_sne();
			else if(firstword == "sdge" && !has_value)
				isrch->dword_sge();
			else if(firstword == "sdgt" && !has_value)
				isrch->dword_sgt();
			else if(firstword == "udlt" && !has_value)
				isrch->dword_ult();
			else if(firstword == "udle" && !has_value)
				isrch->dword_ule();
			else if(firstword == "udeq" && !has_value)
				isrch->dword_ueq();
			else if(firstword == "udne" && !has_value)
				isrch->dword_une();
			else if(firstword == "udge" && !has_value)
				isrch->dword_uge();
			else if(firstword == "udgt" && !has_value)
				isrch->dword_ugt();
			else if(firstword == "dseqlt" && !has_value)
				isrch->dword_seqlt();
			else if(firstword == "dseqle" && !has_value)
				isrch->dword_seqle();
			else if(firstword == "dseqge" && !has_value)
				isrch->dword_seqge();
			else if(firstword == "dseqgt" && !has_value)
				isrch->dword_seqgt();
			else if(firstword == "d" && has_value) {
				if(static_cast<int64_t>(value) < -2147483648LL || value > 4294967295ULL)
					throw std::runtime_error("Value to compare out of range");
				isrch->dword_value(value & 0xFFFFFFFFULL);
			} else if(firstword == "ddiff" && has_value) {
				if(static_cast<int64_t>(value) < -2147483648LL || value > 4294967295ULL)
					throw std::runtime_error("Value to compare out of range");
				isrch->dword_difference(value & 0xFFFFFFFFULL);
			} else if(firstword == "sqlt" && !has_value)
				isrch->qword_slt();
			else if(firstword == "sqle" && !has_value)
				isrch->qword_sle();
			else if(firstword == "sqeq" && !has_value)
				isrch->qword_seq();
			else if(firstword == "sqne" && !has_value)
				isrch->qword_sne();
			else if(firstword == "sqge" && !has_value)
				isrch->qword_sge();
			else if(firstword == "sqgt" && !has_value)
				isrch->qword_sgt();
			else if(firstword == "uqlt" && !has_value)
				isrch->qword_ult();
			else if(firstword == "uqle" && !has_value)
				isrch->qword_ule();
			else if(firstword == "uqeq" && !has_value)
				isrch->qword_ueq();
			else if(firstword == "uqne" && !has_value)
				isrch->qword_une();
			else if(firstword == "uqge" && !has_value)
				isrch->qword_uge();
			else if(firstword == "uqgt" && !has_value)
				isrch->qword_ugt();
			else if(firstword == "qseqlt" && !has_value)
				isrch->qword_seqlt();
			else if(firstword == "qseqle" && !has_value)
				isrch->qword_seqle();
			else if(firstword == "qseqge" && !has_value)
				isrch->qword_seqge();
			else if(firstword == "qseqgt" && !has_value)
				isrch->qword_seqgt();
			else if(firstword == "q" && has_value)
				isrch->qword_value(value);
			else if(firstword == "qdiff" && has_value)
				isrch->qword_difference(value);
			else if(firstword == "disqualify" && has_value)
				isrch->dq_range(value, value);
			else if(firstword == "disqualify_vma" && has_value) {
				auto r = translate_address(value);
				if(r.memory_size != 0)
					isrch->dq_range(r.raw_addr - r.rel_addr, r.raw_addr - r.rel_addr +
						r.memory_size - 1);
			} else if(firstword == "update" && !has_value)
				isrch->update();
			else if(firstword == "reset" && !has_value)
				isrch->reset();
			else if(firstword == "count" && !has_value)
				;
			else if(firstword == "print" && !has_value) {
				auto c = isrch->get_candidates();
				for(auto ci : c) {
					std::ostringstream x;
					x << "0x" << std::hex << std::setw(8) << std::setfill('0') << ci;
					messages << x.str() << std::endl;
				}
			} else
				throw std::runtime_error("Unknown memorysearch subcommand '" + firstword + "'");
			messages << isrch->get_candidate_count() << " candidates remain." << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Search memory addresses"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " {s,u}{b,w,d,q}{lt,le,eq,ne,ge,gt}\n"
				"Syntax: " + _command + " {b,w,d,q}seq{lt,le,ge,gt}\n"
				"Syntax: " + _command + " {b,w,d,q} <value>\n"
				"Syntax: " + _command + " {b,w,d,q}diff <value>\n"
				"Syntax: " + _command + " disqualify{,_vma} <address>\n"
				"Syntax: " + _command + " update\n"
				"Syntax: " + _command + " reset\n"
				"Syntax: " + _command + " count\n"
				"Syntax: " + _command + " print\n"
				"Searches addresses from memory.\n";
		}
	} memorysearch_o;

	read_command<uint64_t, uint8_t, uint8_t> ru1("read-byte", memory_read_byte);
	read_command<uint64_t, uint16_t, uint16_t> ru2("read-word", memory_read_word);
	read_command<uint64_t, uint32_t, uint32_t> ru4("read-dword", memory_read_dword);
	read_command<uint64_t, uint64_t, uint64_t> ru8("read-qword", memory_read_qword);
	read_command<int64_t, int8_t, uint8_t> rs1("read-sbyte", memory_read_byte);
	read_command<int64_t, int16_t, uint16_t> rs2("read-sword", memory_read_word);
	read_command<int64_t, int32_t, uint32_t> rs4("read-sdword", memory_read_dword);
	read_command<int64_t, int64_t, uint64_t> rs8("read-sqword", memory_read_qword);
	write_command<uint8_t, -128, 0xFF> w1("write-byte", memory_write_byte);
	write_command<uint16_t, -32768, 0xFFFF> w2("write-word", memory_write_word);
	write_command<uint32_t, -2147483648LL, 0xFFFFFFFFULL> w4("write-dword", memory_write_dword);
	write_command<uint64_t, -9223372036854775808LL, 0xFFFFFFFFFFFFFFFFULL> w8("write-qword", memory_write_qword);
}
