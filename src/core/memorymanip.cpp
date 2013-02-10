
#include "core/command.hpp"
#include "core/memorymanip.hpp"
#include "core/moviedata.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/memorysearch.hpp"

#include <iostream>
#include <limits>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>

memory_space lsnes_memory;

namespace
{
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

	class iospace_region : public memory_region
	{
	public:
		iospace_region(const std::string& _name, uint64_t _base, uint64_t _size,
			uint8_t (*_iospace_rw)(uint64_t offset, uint8_t data, bool write))
		{
			name = _name;
			base = _base;
			size = _size;
			iospace_rw = _iospace_rw;
			endian = -1;
			readonly = false;
			special = true;
			direct_map = NULL;
		}
		~iospace_region() throw() {}
		void read(uint64_t offset, void* buffer, size_t rsize)
		{
			uint8_t* _buffer = reinterpret_cast<uint8_t*>(buffer);
			for(size_t i = 0; i < rsize; i++)
				_buffer[i] = iospace_rw(offset + i, 0, false);
		}
		bool write(uint64_t offset, const void* buffer, size_t rsize)
		{
			const uint8_t* _buffer = reinterpret_cast<const uint8_t*>(buffer);
			for(size_t i = 0; i < rsize; i++)
				iospace_rw(offset + i, _buffer[i], true);
			return offset + rsize <= size;
		}
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write);
	};
}

void refresh_cart_mappings() throw(std::bad_alloc)
{
	if(!our_rom || !our_rom->rtype)
		return;
	std::list<memory_region*> cur_regions = lsnes_memory.get_regions();
	std::list<memory_region*> regions;
	memory_region* tmp = NULL;
	auto vmalist = our_rom->rtype->vma_list();
	try {
		tmp = new iospace_region("LSNESMMIO", 0xFFFFFFFF00000000ULL, 32, lsnes_mmio_iospace_handler);
		regions.push_back(tmp);
		tmp = NULL;
		for(auto i : vmalist) {
			if(i.iospace_rw)
				tmp = new iospace_region(i.name, i.base, i.size, i.iospace_rw);
			else
				tmp = new memory_region_direct(i.name, i.base, i.native_endian ? 0 : -1,
					reinterpret_cast<uint8_t*>(i.backing_ram), i.size, i.readonly);
			regions.push_back(tmp);
			tmp = NULL;
		}
		lsnes_memory.set_regions(regions);
	} catch(...) {
		if(tmp)
			delete tmp;
		for(auto i : regions)
			delete i;
		throw;
	}
	for(auto i : cur_regions)
		delete i;
}

namespace
{
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
			: command(lsnes_cmd, cmd)
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

	template<typename ret, ret (memory_space::*_rfn)(uint64_t addr)>
	class read_command : public memorymanip_command
	{
	public:
		read_command(const std::string& cmd) throw(std::bad_alloc)
			: memorymanip_command(cmd)
		{
		}
		~read_command() throw() {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(address_bad || has_value || has_tail)
				throw std::runtime_error("Syntax: " + _command + " <address>");
			{
				std::ostringstream x;
				x << "0x" << std::hex << address << " -> " << std::dec
					<< (lsnes_memory.*_rfn)(address);
				messages << x.str() << std::endl;
			}
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Read memory"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " <address>\n"
				"Reads data from memory.\n";
		}
	};

	template<typename arg, int64_t low, uint64_t high, bool (memory_space::*_wfn)(uint64_t addr, arg a)>
	class write_command : public memorymanip_command
	{
	public:
		write_command(const std::string& cmd)
			throw(std::bad_alloc)
			: memorymanip_command(cmd)
		{
		}
		~write_command() throw() {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(address_bad || value_bad || has_tail)
				throw std::runtime_error("Syntax: " + _command + " <address> <value>");
			int64_t value2 = static_cast<int64_t>(value);
			if(value2 < low || (value > high && value2 >= 0))
				throw std::runtime_error("Value to write out of range");
			(lsnes_memory.*_wfn)(address, value & high);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Write memory"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " <address> <value>\n"
				"Writes data to memory.\n";
		}
	};

	read_command<uint8_t, &memory_space::read<uint8_t>> ru1("read-byte");
	read_command<uint16_t, &memory_space::read<uint16_t>> ru2("read-word");
	read_command<uint32_t, &memory_space::read<uint32_t>> ru4("read-dword");
	read_command<uint64_t, &memory_space::read<uint64_t>> ru8("read-qword");
	read_command<int8_t, &memory_space::read<int8_t>> rs1("read-sbyte");
	read_command<int16_t, &memory_space::read<int16_t>> rs2("read-sword");
	read_command<int32_t, &memory_space::read<int32_t>> rs4("read-sdword");
	read_command<int64_t, &memory_space::read<int64_t>> rs8("read-sqword");
	write_command<uint8_t, -128, 0xFF, &memory_space::write<uint8_t>> w1("write-byte");
	write_command<uint16_t, -32768, 0xFFFF, &memory_space::write<uint16_t>> w2("write-word");
	write_command<uint32_t, -2147483648LL, 0xFFFFFFFFULL, &memory_space::write<uint32_t>> w4("write-dword");
	write_command<uint64_t, -9223372036854775808LL, 0xFFFFFFFFFFFFFFFFULL, &memory_space::write<uint64_t>>
		w8("write-qword");
}
