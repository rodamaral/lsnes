#include "core/command.hpp"
#include "interface/disassembler.hpp"
#include "library/bintohex.hpp"
#include "library/minmax.hpp"
#include "core/memorymanip.hpp"
#include "core/window.hpp"
#include "library/string.hpp"
#include <iomanip>
#include <fstream>
#include <iostream>

namespace
{
	struct dres
	{
		uint64_t addr;
		uint64_t len;
		std::string disasm;
	};

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

	uint64_t parse_hexordec(const std::string& k)
	{
		regex_results t;
		uint64_t address = 0;
		if(t = regex("0x(.+)", k)) {
			if(t[1].length() > 16)
				throw 42;
			address = 0;
			for(unsigned i = 0; i < t[1].length(); i++)
				address = 16 * address + hex(t[1][i]);
		} else {
			address = parse_value<uint64_t>(k);
		}
		return address;
	}

	function_ptr_command<const std::string&> disassemble(lsnes_cmd, "disassemble", "Disassemble code",
		"Syntax: disassemble <kind> <addr> [<count>] [to <filename>]\nDisassemble code\n",
		[](const std::string& t) throw(std::bad_alloc, std::runtime_error) {
		regex_results r = regex("([^ \t]+)[ \t]+([0-9]+|0x[0-9A-Fa-f]+)([ \t]+([0-9]+))?"
			"([ \t]+to[ \t]+(.+))?", t);
		if(!r) {
			messages << "Syntax: disassemble <kind> <addr> [<count>] [to <filename>]" << std::endl;
			return;
		}
		std::string kind = r[1];
		uint64_t addr = parse_hexordec(r[2]);
		uint64_t count = 1;
		if(r[4] != "")
			count = parse_value<uint64_t>(r[4]);
		std::string file;
		if(r[6] != "")
			file = r[6];
		std::list<dres> result;
		disassembler* d;
		try {
			d = &disassembler::byname(kind);
		} catch(std::exception& e) {
			messages << "Can't find such disassembler" << std::endl;
			return;
		}
		uint64_t laddr = addr;
		uint64_t longest = 0;
		for(uint64_t i = 1; i <= count; i++) {
			uint64_t bytes = 0;
			dres x;
			x.addr = laddr;
			x.disasm = d->disassemble(laddr, [&bytes, laddr]() -> unsigned char {
				return lsnes_memory.read<uint8_t>(laddr + bytes++);
			});
			x.len = bytes;
			result.push_back(x);
			longest = max(longest, bytes);
			laddr += bytes;
		}
		std::ostream* strm = &messages.getstream();
		if(file != "") {
			strm = new std::ofstream(file);
			if(!*strm) {
				messages << "Can't open output file" << std::endl;
				return;
			}
		}
		for(auto i : result) {
			std::vector<unsigned char> tmp;
			tmp.resize(i.len);
			lsnes_memory.read_range(i.addr, &tmp[0], i.len);
			std::string l = (stringfmt() << std::setw(16) << std::setfill('0') << std::hex
				<< i.addr).str() + " " + binary_to_hex(&tmp[0], i.len) + " "
					+ i.disasm;
			(*strm) << l << std::endl;
		}
		if(file != "")
			delete strm;
	});
}
