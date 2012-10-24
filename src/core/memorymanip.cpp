#include "core/emucore.hpp"

#include "core/command.hpp"
#include "core/memorymanip.hpp"
#include "core/moviedata.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
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
	if(!get_current_rom_info().first)
		return;
	std::list<memory_region*> cur_regions = lsnes_memory.get_regions();
	std::list<memory_region*> regions;
	memory_region* tmp = NULL;
	auto vmalist = get_vma_list();
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
	memory_search* isrch;

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

	struct simple_mem_commands
	{
		simple_mem_commands()
		{
			cmds["sblt"] = &memory_search::byte_slt;
			cmds["sble"] = &memory_search::byte_sle;
			cmds["sbeq"] = &memory_search::byte_seq;
			cmds["sbne"] = &memory_search::byte_sne;
			cmds["sbge"] = &memory_search::byte_sge;
			cmds["sbgt"] = &memory_search::byte_sgt;
			cmds["ublt"] = &memory_search::byte_ult;
			cmds["uble"] = &memory_search::byte_ule;
			cmds["ubeq"] = &memory_search::byte_ueq;
			cmds["ubne"] = &memory_search::byte_une;
			cmds["ubge"] = &memory_search::byte_uge;
			cmds["ubgt"] = &memory_search::byte_ugt;
			cmds["bseqlt"] = &memory_search::byte_seqlt;
			cmds["bseqle"] = &memory_search::byte_seqle;
			cmds["bseqge"] = &memory_search::byte_seqge;
			cmds["bseqgt"] = &memory_search::byte_seqgt;
			cmds["swlt"] = &memory_search::word_slt;
			cmds["swle"] = &memory_search::word_sle;
			cmds["sweq"] = &memory_search::word_seq;
			cmds["swne"] = &memory_search::word_sne;
			cmds["swge"] = &memory_search::word_sge;
			cmds["swgt"] = &memory_search::word_sgt;
			cmds["uwlt"] = &memory_search::word_ult;
			cmds["uwle"] = &memory_search::word_ule;
			cmds["uweq"] = &memory_search::word_ueq;
			cmds["uwne"] = &memory_search::word_une;
			cmds["uwge"] = &memory_search::word_uge;
			cmds["uwgt"] = &memory_search::word_ugt;
			cmds["wseqlt"] = &memory_search::word_seqlt;
			cmds["wseqle"] = &memory_search::word_seqle;
			cmds["wseqge"] = &memory_search::word_seqge;
			cmds["wseqgt"] = &memory_search::word_seqgt;
			cmds["sdlt"] = &memory_search::dword_slt;
			cmds["sdle"] = &memory_search::dword_sle;
			cmds["sdeq"] = &memory_search::dword_seq;
			cmds["sdne"] = &memory_search::dword_sne;
			cmds["sdge"] = &memory_search::dword_sge;
			cmds["sdgt"] = &memory_search::dword_sgt;
			cmds["udlt"] = &memory_search::dword_ult;
			cmds["udle"] = &memory_search::dword_ule;
			cmds["udeq"] = &memory_search::dword_ueq;
			cmds["udne"] = &memory_search::dword_une;
			cmds["udge"] = &memory_search::dword_uge;
			cmds["udgt"] = &memory_search::dword_ugt;
			cmds["dseqlt"] = &memory_search::dword_seqlt;
			cmds["dseqle"] = &memory_search::dword_seqle;
			cmds["dseqge"] = &memory_search::dword_seqge;
			cmds["dseqgt"] = &memory_search::dword_seqgt;
			cmds["sqlt"] = &memory_search::qword_slt;
			cmds["sqle"] = &memory_search::qword_sle;
			cmds["sqeq"] = &memory_search::qword_seq;
			cmds["sqne"] = &memory_search::qword_sne;
			cmds["sqge"] = &memory_search::qword_sge;
			cmds["sqgt"] = &memory_search::qword_sgt;
			cmds["uqlt"] = &memory_search::qword_ult;
			cmds["uqle"] = &memory_search::qword_ule;
			cmds["uqeq"] = &memory_search::qword_ueq;
			cmds["uqne"] = &memory_search::qword_une;
			cmds["uqge"] = &memory_search::qword_uge;
			cmds["uqgt"] = &memory_search::qword_ugt;
			cmds["qseqlt"] = &memory_search::qword_seqlt;
			cmds["qseqle"] = &memory_search::qword_seqle;
			cmds["qseqge"] = &memory_search::qword_seqge;
			cmds["qseqgt"] = &memory_search::qword_seqgt;
		}
		std::map<std::string, void (memory_search::*)()> cmds;
	} simple_cmds;

	class memorysearch_command : public memorymanip_command
	{
	public:
		memorysearch_command() throw(std::bad_alloc) : memorymanip_command("search-memory") {}
		void invoke2() throw(std::bad_alloc, std::runtime_error)
		{
			if(!isrch)
				isrch = new memory_search(lsnes_memory);
			if(simple_cmds.cmds.count(firstword) && !has_value)
				(isrch->*(simple_cmds.cmds[firstword]))();
			else if(firstword == "b" && has_value) {
				if(static_cast<int64_t>(value) < -128 || value > 255)
					throw std::runtime_error("Value to compare out of range");
				isrch->byte_value(value & 0xFF);
			} else if(firstword == "bdiff" && has_value) {
				if(static_cast<int64_t>(value) < -128 || value > 255)
					throw std::runtime_error("Value to compare out of range");
				isrch->byte_difference(value & 0xFF);
			} else if(firstword == "w" && has_value) {
				if(static_cast<int64_t>(value) < -32768 || value > 65535)
					throw std::runtime_error("Value to compare out of range");
				isrch->word_value(value & 0xFFFF);
			} else if(firstword == "wdiff" && has_value) {
				if(static_cast<int64_t>(value) < -32768 || value > 65535)
					throw std::runtime_error("Value to compare out of range");
				isrch->word_difference(value & 0xFFFF);
			} else if(firstword == "d" && has_value) {
				if(static_cast<int64_t>(value) < -2147483648LL || value > 4294967295ULL)
					throw std::runtime_error("Value to compare out of range");
				isrch->dword_value(value & 0xFFFFFFFFULL);
			} else if(firstword == "ddiff" && has_value) {
				if(static_cast<int64_t>(value) < -2147483648LL || value > 4294967295ULL)
					throw std::runtime_error("Value to compare out of range");
				isrch->dword_difference(value & 0xFFFFFFFFULL);
			} else if(firstword == "q" && has_value)
				isrch->qword_value(value);
			else if(firstword == "qdiff" && has_value)
				isrch->qword_difference(value);
			else if(firstword == "disqualify" && has_value)
				isrch->dq_range(value, value);
			else if(firstword == "disqualify_vma" && has_value) {
				auto r = lsnes_memory.lookup(value);
				if(r.first)
					isrch->dq_range(r.first->base, r.first->last_address());
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
