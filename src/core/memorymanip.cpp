#include "core/command.hpp"
#include "core/memorymanip.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "interface/romtype.hpp"
#include "library/hex.hpp"
#include "library/string.hpp"
#include "library/int24.hpp"
#include "library/minmax.hpp"
#include "library/memorysearch.hpp"

#include <iostream>
#include <limits>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>

namespace
{
	uint8_t lsnes_mmio_iospace_read(uint64_t offset)
	{
		try {
			if(offset >= 0 && offset < 8) {
				//Frame counter.
				uint64_t x = CORE().mlogic->get_movie().get_current_frame();
				return x >> (8 * (offset & 7));
			} else if(offset >= 8 && offset < 16) {
				//Movie length.
				uint64_t x = CORE().mlogic->get_movie().get_frame_count();
				return x >> (8 * (offset & 7));
			} else if(offset >= 16 && offset < 24) {
				//Lag counter.
				uint64_t x = CORE().mlogic->get_movie().get_lag_frames();
				return x >> (8 * (offset & 7));
			} else if(offset >= 24 && offset < 32) {
				//Rerecord counter.
				uint64_t x = CORE().mlogic->get_rrdata().count();
				return x >> (8 * (offset & 7));
			} else
				return 0;
		} catch(...) {
			return 0;
		}
	}

	void lsnes_mmio_iospace_write(uint64_t offset, uint8_t data)
	{
		//Ignore.
	}

	class iospace_region : public memory_region
	{
	public:
		iospace_region(const std::string& _name, uint64_t _base, uint64_t _size, bool _special,
			uint8_t (*_read)(uint64_t offset), void (*_write)(uint64_t offset, uint8_t data))
		{
			name = _name;
			base = _base;
			size = _size;
			Xread = _read;
			Xwrite = _write;
			endian = -1;
			readonly = (_write == NULL);
			special = _special;
			direct_map = NULL;
		}
		~iospace_region() throw() {}
		void read(uint64_t offset, void* buffer, size_t rsize)
		{
			uint8_t* _buffer = reinterpret_cast<uint8_t*>(buffer);
			for(size_t i = 0; i < rsize; i++)
				_buffer[i] = Xread(offset + i);
		}
		bool write(uint64_t offset, const void* buffer, size_t rsize)
		{
			const uint8_t* _buffer = reinterpret_cast<const uint8_t*>(buffer);
			for(size_t i = 0; i < rsize; i++)
				Xwrite(offset + i, _buffer[i]);
			return offset + rsize <= size;
		}
		uint8_t (*Xread)(uint64_t offset);
		void (*Xwrite)(uint64_t offset, uint8_t data);
	};
}

cart_mappings_refresher::cart_mappings_refresher(memory_space& _mspace)
	: mspace(_mspace)
{
}

void cart_mappings_refresher::operator()() throw(std::bad_alloc)
{
	if(!our_rom.rtype)
		return;
	std::list<memory_region*> cur_regions = mspace.get_regions();
	std::list<memory_region*> regions;
	memory_region* tmp = NULL;
	auto vmalist = our_rom.rtype->vma_list();
	try {
		tmp = new iospace_region("LSNESMMIO", 0xFFFFFFFF00000000ULL, 32, true, lsnes_mmio_iospace_read,
			lsnes_mmio_iospace_write);
		regions.push_back(tmp);
		tmp = NULL;
		for(auto i : vmalist) {
			if(!i.backing_ram)
				tmp = new iospace_region(i.name, i.base, i.size, i.special, i.read, i.write);
			else
				tmp = new memory_region_direct(i.name, i.base, i.endian,
					reinterpret_cast<uint8_t*>(i.backing_ram), i.size, i.readonly);
			regions.push_back(tmp);
			tmp = NULL;
		}
		mspace.set_regions(regions);
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
	uint64_t parse_address(std::string addr)
	{
		if(regex_match("[0-9]+|0x[0-9A-Fa-f]+", addr)) {
			//Absolute in mapspace.
			return parse_value<uint64_t>(addr);
		} else if(regex_match("[^+]+\\+[0-9A-Fa-f]+", addr)) {
			//VMA-relative.
			regex_results r = regex("([^+]+)\\+([0-9A-Fa-f]+)", addr);
			std::string vma = r[1];
			std::string _offset = r[2];
			uint64_t offset = parse_value<uint64_t>("0x" + _offset);
			for(auto i : CORE().memory->get_regions())
				if(i->name == vma) {
					if(offset >= i->size)
						throw std::runtime_error("Offset out of range");
					return i->base + offset;
				}
			throw std::runtime_error("No such VMA");
		} else
			throw std::runtime_error("Unknown syntax");
	}

	std::string format_address(uint64_t addr)
	{
		for(auto i : CORE().memory->get_regions())
			if(i->base <= addr && i->base + i->size > addr) {
				//Hit.
				unsigned hcount = 1;
				uint64_t t = i->size;
				while(t > 0x10) { hcount++; t >>= 4; }
				return (stringfmt() << i->name << "+" << std::hex << std::setw(hcount)
					<< std::setfill('0')
					<< (addr - i->base)).str();
			}
		//Fallback.
		return hex::to(addr);
	}

	class memorymanip_command : public command::base
	{
	public:
		memorymanip_command(command::group& grp, const std::string& cmd) throw(std::bad_alloc)
			: command::base(grp, cmd, true)
		{
			_command = cmd;
		}
		~memorymanip_command() throw() {}
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			regex_results t = regex("(([^ \t]+)([ \t]+([^ \t]+)([ \t]+([^ \t].*)?)?)?)?", args);
			if(!t) {
				address_bad = true;
				return;
			}
			firstword = t[2];
			secondword = t[4];
			has_tail = (t[6] != "");
			address_bad = true;
			value_bad = true;
			has_value = (secondword != "");
			try {
				address = parse_address(firstword);
				address_bad = false;
			} catch(...) {
			}
			try {
				if(has_value) {
					valuef = parse_value<double>(secondword);
					has_valuef = true;
				}
			} catch(...) {
			}
			try {
				value = parse_value<uint64_t>(secondword);
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
		double valuef;
		bool has_tail;
		bool address_bad;
		bool value_bad;
		bool has_value;
		bool has_valuef;
		std::string _command;
	};

	template<typename ret, ret (memory_space::*_rfn)(uint64_t addr), bool hexd>
	class read_command : public memorymanip_command
	{
	public:
		read_command(command::group& grp, const std::string& cmd) throw(std::bad_alloc)
			: memorymanip_command(grp, cmd)
		{
		}
		~read_command() throw() {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(address_bad || has_value || has_tail)
				throw std::runtime_error("Syntax: " + _command + " <address>");
			{
				std::ostringstream x;
				if(hexd)
					x << format_address(address) << " -> "
						<< hex::to((CORE().memory->*_rfn)(address), true);
				else if(sizeof(ret) > 1)
					x << format_address(address) << " -> " << std::dec
						<< (CORE().memory->*_rfn)(address);
				else
					x << format_address(address) << " -> " << std::dec
						<< (int)(CORE().memory->*_rfn)(address);
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
		write_command(command::group& grp, const std::string& cmd)
			throw(std::bad_alloc)
			: memorymanip_command(grp, cmd)
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
			(CORE().memory->*_wfn)(address, value & high);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Write memory"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " <address> <value>\n"
				"Writes data to memory.\n";
		}
	};

	template<typename arg, bool (memory_space::*_wfn)(uint64_t addr, arg a)>
	class writef_command : public memorymanip_command
	{
	public:
		writef_command(command::group& grp, const std::string& cmd)
			throw(std::bad_alloc)
			: memorymanip_command(grp, cmd)
		{
		}
		~writef_command() throw() {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(address_bad || !has_valuef || has_tail)
				throw std::runtime_error("Syntax: " + _command + " <address> <value>");
			(CORE().memory->*_wfn)(address, valuef);
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Write memory"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + _command + " <address> <value>\n"
				"Writes data to memory.\n";
		}
	};

	command::byname_factory<read_command<uint8_t, &memory_space::read<uint8_t>, false>> ru1(lsnes_cmds,
		"read-byte");
	command::byname_factory<read_command<uint16_t, &memory_space::read<uint16_t>, false>> ru2(lsnes_cmds,
		"read-word");
	command::byname_factory<read_command<ss_uint24_t, &memory_space::read<ss_uint24_t>, false>> ru3(lsnes_cmds, 
		"read-hword");
	command::byname_factory<read_command<uint32_t, &memory_space::read<uint32_t>, false>> ru4(lsnes_cmds,
		"read-dword");
	command::byname_factory<read_command<uint64_t, &memory_space::read<uint64_t>, false>> ru8(lsnes_cmds,
		"read-qword");
	command::byname_factory<read_command<uint8_t, &memory_space::read<uint8_t>, true>> rh1(lsnes_cmds,
		"read-byte-hex");
	command::byname_factory<read_command<uint16_t, &memory_space::read<uint16_t>, true>> rh2(lsnes_cmds,
		"read-word-hex");
	command::byname_factory<read_command<ss_uint24_t, &memory_space::read<ss_uint24_t>, true>>
		rh3(lsnes_cmds, "read-hword-hex");
	command::byname_factory<read_command<uint32_t, &memory_space::read<uint32_t>, true>> rh4(lsnes_cmds,
		"read-dword-hex");
	command::byname_factory<read_command<uint64_t, &memory_space::read<uint64_t>, true>> rh8(lsnes_cmds,
		"read-qword-hex");
	command::byname_factory<read_command<int8_t, &memory_space::read<int8_t>, false>> rs1(lsnes_cmds,
		"read-sbyte");
	command::byname_factory<read_command<int16_t, &memory_space::read<int16_t>, false>> rs2(lsnes_cmds,
		"read-sword");
	command::byname_factory<read_command<ss_int24_t, &memory_space::read<ss_int24_t>, false>> rs3(lsnes_cmds,
		"read-shword");
	command::byname_factory<read_command<int32_t, &memory_space::read<int32_t>, false>> rs4(lsnes_cmds,
		"read-sdword");
	command::byname_factory<read_command<float, &memory_space::read<float>, false>> rf4(lsnes_cmds,
		"read-float");
	command::byname_factory<read_command<double, &memory_space::read<double>, false>> rf8(lsnes_cmds,
		"read-double");
	command::byname_factory<write_command<uint8_t, -128, 0xFF, &memory_space::write<uint8_t>>> w1(lsnes_cmds,
		"write-byte");
	command::byname_factory<write_command<uint16_t, -32768, 0xFFFF, &memory_space::write<uint16_t>>>
		w2(lsnes_cmds, "write-word");
	command::byname_factory<write_command<ss_uint24_t, -8388608, 0xFFFFFF, &memory_space::write<ss_uint24_t>>> 
		w3(lsnes_cmds, "write-hword");
	command::byname_factory<write_command<uint32_t, -2147483648LL, 0xFFFFFFFFULL, &memory_space::write<uint32_t>>>
		w4(lsnes_cmds, "write-dword");
	//Just straight writing the constant would cause a warning.
	command::byname_factory<write_command<uint64_t, -9223372036854775807LL-1, 0xFFFFFFFFFFFFFFFFULL,
		&memory_space::write<uint64_t>>> w8(lsnes_cmds, "write-qword");
	command::byname_factory<writef_command<float, &memory_space::write<float>>> wf4(lsnes_cmds, "write-float");
	command::byname_factory<writef_command<double, &memory_space::write<double>>> wf8(lsnes_cmds, "write-double");
}
