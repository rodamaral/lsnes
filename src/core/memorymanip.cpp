#include "core/command.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "interface/core.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"

#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace
{
	struct translated_address
	{
		uint64_t rel_addr;		//Address relative to start of VMA.
		uint64_t raw_addr;		//Raw address.
		uint64_t mem_size;		//Memory size.
		vma_structure* vma;		//VMA referred to.
		bool read_only;			//RO flag.
		bool native_endian;		//Is in native endian.
	};

	std::vector<vma_structure*> memory_regions;
	uint64_t linear_ram_size = 0;
	bool system_little_endian = true;

	struct translated_address translate_address(uint64_t rawaddr) throw()
	{
		struct translated_address t;
		t.rel_addr = 0;
		t.raw_addr = 0;
		t.mem_size = 0;
		t.vma = NULL;
		t.read_only = true;
		t.native_endian = true;
		for(auto i : memory_regions) {
			if(i->get_base() > rawaddr || i->get_base() + i->get_size() <= rawaddr)
				continue;
			t.rel_addr = rawaddr - i->get_base();
			t.raw_addr = rawaddr;
			t.vma = i;
			t.mem_size = i->get_size();
			t.read_only = i->is_readonly();
			t.native_endian = (!system_little_endian && i->get_endian() == vma_structure::E_BIG) ||
				(system_little_endian && i->get_endian() == vma_structure::E_LITTLE) ||
				i->get_endian() == vma_structure::E_HOST;
			break;
		}
		return t;
	}

	struct translated_address translate_address_linear_ram(uint64_t ramlinaddr) throw()
	{
		struct translated_address t;
		t.rel_addr = 0;
		t.raw_addr = 0;
		t.mem_size = 0;
		t.vma = NULL;
		t.read_only = true;
		t.native_endian = true;
		for(auto i : memory_regions) {
			if(i->is_readonly())
				continue;
			if(ramlinaddr >= i->get_size()) {
				ramlinaddr -= i->get_size();
				continue;
			}
			t.rel_addr = ramlinaddr;
			t.raw_addr = i->get_base() + ramlinaddr;
			t.vma = i;
			t.mem_size = i->get_size();
			t.read_only = i->is_readonly();
			t.native_endian = (!system_little_endian && i->get_endian() == vma_structure::E_BIG) ||
				(system_little_endian && i->get_endian() == vma_structure::E_LITTLE) ||
				i->get_endian() == vma_structure::E_HOST;
			break;
		}
		return t;
	}

	uint64_t get_linear_ram_size() throw()
	{
		return linear_ram_size;
	}

	void copy_regions_from_core()
	{
		uint64_t new_linsize = 0;
		std::vector<vma_structure*> new_memory_regions;
		for(size_t i = 0; i < emucore_vma_slots(); i++) {
			vma_structure* j = emucore_vma_slot(i);
			new_memory_regions.push_back(j);
			if(!j->is_readonly())
				new_linsize += j->get_size();
		}
		std::swap(memory_regions, new_memory_regions);
		linear_ram_size = new_linsize;
	}

	template<typename T> T endian_convert(T x) throw();

	template<> uint8_t endian_convert(uint8_t x) throw()
	{
		return x;
	}

	template<> uint16_t endian_convert(uint16_t x) throw()
	{
		return (((x >> 8) & 0xFF) | ((x << 8) & 0xFF00));
	}

	template<> uint32_t endian_convert(uint32_t x) throw()
	{
		return (((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
			((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000));
	}

	template<> uint64_t endian_convert(uint64_t x) throw()
	{
		return (((x >> 56) & 0xFF) | ((x >> 40) & 0xFF00) |
			((x >> 24) & 0xFF0000) | ((x >> 8) & 0xFF000000) |
			((x << 8) & 0xFF00000000ULL) | ((x << 24) & 0xFF0000000000ULL) |
			((x << 40) & 0xFF000000000000ULL) | ((x << 56) & 0xFF00000000000000ULL));
	}

	template<> int8_t endian_convert(int8_t x) throw() { return endian_convert(static_cast<uint8_t>(x)); }
	template<> int16_t endian_convert(int16_t x) throw() { return endian_convert(static_cast<uint16_t>(x)); }
	template<> int32_t endian_convert(int32_t x) throw() { return endian_convert(static_cast<uint32_t>(x)); }
	template<> int64_t endian_convert(int64_t x) throw() { return endian_convert(static_cast<uint64_t>(x)); }

	template<typename T> T memory_read(uint64_t addr) throw()
	{
		struct translated_address laddr = translate_address(addr);
		T value = 0;
		laddr.vma->copy_from_core(laddr.rel_addr, reinterpret_cast<char*>(&value), sizeof(T));
		if(!laddr.native_endian)
			value = endian_convert(value);
		return value;
	}

	//Byte write to address (false if failed).
	template<typename T> bool memory_write(uint64_t addr, T data) throw()
	{
		struct translated_address laddr = translate_address(addr);
		if(laddr.rel_addr > laddr.mem_size - sizeof(T) || laddr.read_only)
			return false;
		if(!laddr.native_endian)
			data = endian_convert(data);
		laddr.vma->copy_to_core(laddr.rel_addr, reinterpret_cast<char*>(&data), sizeof(T));
		return true;
	}
}

void refresh_cart_mappings() throw(std::bad_alloc)
{
	uint16_t x = 258;
	system_little_endian = (*reinterpret_cast<char*>(&x) == 2);
	emucore_refresh_cart();
	copy_regions_from_core();
}

std::vector<vma_structure*> get_regions() throw(std::bad_alloc)
{
	std::vector<vma_structure*> out;
	for(auto i : memory_regions)
		out.push_back(i);
	return out;
}

uint8_t memory_read_byte(uint64_t addr) throw() { return memory_read<uint8_t>(addr); }
uint16_t memory_read_word(uint64_t addr) throw() { return memory_read<uint16_t>(addr); }
uint32_t memory_read_dword(uint64_t addr) throw() { return memory_read<uint32_t>(addr); }
uint64_t memory_read_qword(uint64_t addr) throw() { return memory_read<uint64_t>(addr); }
bool memory_write_byte(uint64_t addr, uint8_t data) throw() { return memory_write(addr, data); }
bool memory_write_word(uint64_t addr, uint16_t data) throw() { return memory_write(addr, data); }
bool memory_write_dword(uint64_t addr, uint32_t data) throw() { return memory_write(addr, data); }
bool memory_write_qword(uint64_t addr, uint64_t data) throw() { return memory_write(addr, data); }

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
		t.vma->copy_from_core(0, reinterpret_cast<char*>(&previous_content[addr]), t.mem_size);
		addr += t.mem_size;
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
		value_type v1 = *reinterpret_cast<const value_type*>(oldv);
		value_type v2 = *reinterpret_cast<const value_type*>(newv);
		if(!nativeendian) {
			v1 = endian_convert<value_type>(v1);
			v2 = endian_convert<value_type>(v2);
		}
		return val(v1, v2);
	}

/**
 * \brief The underlying condition.
 */
	const T& val;
};

#define BUFFER_LOWATER_MARK 16
#define BUFFER_HIWATER_MARK 4096

template<class T> void memorysearch::search(const T& obj) throw()
{
	search_value_helper<T> helper(obj);
	struct translated_address t = translate_address_linear_ram(0);
	uint64_t switch_at = t.mem_size;
	uint64_t base = 0;
	uint64_t size = previous_content.size();
	uint8_t buffer[BUFFER_HIWATER_MARK];
	size_t bufferfill = 0;
	size_t bufferused = 0;
	for(uint64_t i = 0; i < size; i++) {
		if(still_in[i / 64] == 0) {
			uint64_t old_i = i;
			i = (i + 64) >> 6 << 6;
			size_t delta_i = i - old_i;
			bufferused += delta_i;
			if(bufferused > bufferfill)
				bufferused = bufferfill = 0;
			i--;
			continue;
		}
		//t.mem_size == 0 can happen if cart changes.
		while(i >= switch_at && t.mem_size > 0) {
			t = translate_address_linear_ram(switch_at);
			base = switch_at;
			switch_at += t.mem_size;
			bufferfill = 0;
			bufferused = 0;
		}
		if(bufferfill - bufferused < BUFFER_LOWATER_MARK && i - bufferused + bufferfill < switch_at) {
			//Right now the buffer covers the region from [i - bufferused, i - bufferused + bufferfill).
			if(bufferused < bufferfill)
				memmove(buffer, buffer + bufferused, bufferfill - bufferused);
			bufferfill -= bufferused;
			bufferused = 0;
			//Right now the buffer covers the region from [i, i + bufferfill).
			size_t csize = min(static_cast<uint64_t>(BUFFER_HIWATER_MARK), t.mem_size - (i - base)) -
				bufferfill;
			t.vma->copy_from_core(i - base + bufferfill, reinterpret_cast<char*>(buffer + bufferfill),
				csize);
			bufferfill += csize;
		}
		if(t.mem_size == 0 || !helper(buffer + bufferused, &previous_content[i], t.mem_size - (i - base),
			t.native_endian)) {
			if((still_in[i / 64] >> (i % 64)) & 1) {
				still_in[i / 64] &= ~(1ULL << (i % 64));
				candidates--;
			}
		}
		if(t.mem_size > 0)
			bufferused++;
	}
	t = translate_address_linear_ram(0);
	base = 0;
	size = previous_content.size();
	while(base < size) {
		size_t m = t.mem_size;
		if(m > (size - base))
			m = size - base;
		t.vma->copy_from_core(0, reinterpret_cast<char*>(&previous_content[base]), m);
		base += t.mem_size;
		t = translate_address_linear_ram(base);
	}
}

void memorysearch::byte_value(uint8_t value) throw() { search(search_value<uint8_t>(value)); }
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

void memorysearch::word_value(uint16_t value) throw() { search(search_value<uint16_t>(value)); }
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

void memorysearch::dword_value(uint32_t value) throw() { search(search_value<uint32_t>(value)); }
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

void memorysearch::qword_value(uint64_t value) throw() { search(search_value<uint64_t>(value)); }
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

void memorysearch::update() throw() { search(search_update()); }

uint64_t memorysearch::get_candidate_count() throw()
{
	return candidates;
}

std::list<uint64_t> memorysearch::get_candidates() throw(std::bad_alloc)
{
	struct translated_address t = translate_address_linear_ram(0);
	uint64_t switch_at = t.mem_size;
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
		while(i >= switch_at && t.mem_size > 0) {
			t = translate_address_linear_ram(switch_at);
			base = switch_at;
			rbase = t.raw_addr - t.rel_addr;
			switch_at += t.mem_size;
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
			else if(firstword == "b" && has_value) {
				if(static_cast<int64_t>(value) < -128 || value > 255)
					throw std::runtime_error("Value to compare out of range");
				isrch->byte_value(value & 0xFF);
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
			else if(firstword == "w" && has_value) {
				if(static_cast<int64_t>(value) < -32768 || value > 65535)
					throw std::runtime_error("Value to compare out of range");
				isrch->word_value(value & 0xFFFF);
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
			else if(firstword == "d" && has_value) {
				if(static_cast<int64_t>(value) < -2147483648LL || value > 4294967295ULL)
					throw std::runtime_error("Value to compare out of range");
				isrch->dword_value(value & 0xFFFFFFFFULL);
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
			else if(firstword == "q" && has_value)
				isrch->qword_value(value);
			else if(firstword == "update" && !has_value)
				isrch->update();
			else if(firstword == "reset" && !has_value)
				isrch->reset();
			else if(firstword == "count" && !has_value)
				;
			else if(firstword == "print" && !has_value) {
				auto c = isrch->get_candidates();
				for(auto ci : c) {
					std::ostringstream x;
					x << "0x" << std::hex << std::setw(16) << std::setfill('0') << ci;
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
				"Syntax: " + _command + " {b,w,d,q} <value>\n"
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
	read_command<uint64_t, int8_t, uint8_t> rs1("read-sbyte", memory_read_byte);
	read_command<uint64_t, int16_t, uint16_t> rs2("read-sword", memory_read_word);
	read_command<uint64_t, int32_t, uint32_t> rs4("read-sdword", memory_read_dword);
	read_command<uint64_t, int64_t, uint64_t> rs8("read-sqword", memory_read_qword);
	write_command<uint8_t, -128, 0xFF> w1("write-byte", memory_write_byte);
	write_command<uint16_t, -32768, 0xFFFF> w2("write-word", memory_write_word);
	write_command<uint32_t, -2147483648LL, 0xFFFFFFFFULL> w4("write-dword", memory_write_dword);
	write_command<uint64_t, -9223372036854775808LL, 0xFFFFFFFFFFFFFFFFULL> w8("write-qword", memory_write_qword);
}
