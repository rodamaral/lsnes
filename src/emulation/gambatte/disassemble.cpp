#include "disassemble-gb.hpp"
#include "interface/disassembler.hpp"
#include "library/string.hpp"
#include "library/hex.hpp"
#include <functional>
#include <sstream>
#include <iomanip>

namespace
{
	std::string signdec(uint8_t v, bool psign = false);
	std::string signdec(uint8_t v, bool psign)
	{
		if(v < 128) return (stringfmt() << (psign ? "+" : "") << (int)v).str();
		return (stringfmt() << (int)v - 256).str();
	}

	uint16_t jraddr(uint16_t pc, uint8_t lt)
	{
		return pc + lt + 2 - ((lt < 128) ? 0 : 256);
	}

	const char* instructions[] = {
		"NOP",        "LD BC, %w", "LD (BC),A",  "INC BC", "INC B",    "DEC B",    "LD B,%b",    "RCLA",
		"LD (%W),SP", "ADD HL,BC", "LD A,(BC)",  "DEC BC", "INC C",    "DEC C",    "LD C,%b",    "RRCA",
		"STOP",       "LD DE, %w", "LD (DE),A",  "INC DE", "INC D",    "DEC D",    "LD D,%b",    "RLA",
		"JR %R",      "ADD HL,DE", "LD A,(DE)",  "DEC DE", "INC E",    "DEC E",    "LD E,%b",    "RRA",
		"JR NZ,%R",   "LD HL, %w", "LD (HL+),A", "INC HL", "INC H",    "DEC H",    "LD H,%b",    "DAA",
		"JR Z,%R",    "ADD HL,HL", "LD A,(HL+)", "DEC HL", "INC L",    "DEC L",    "LD L,%b",    "CPL",
		"JR NC,%R",   "LD SP, %w", "LD (HL-),A", "INC SP", "INC (HL)", "DEC (HL)", "LD (HL),%b", "SCF",
		"JR Z,%R",    "ADD HL,SP", "LD A,(HL-)", "DEC SP", "INC A",    "DEC A",    "LD A,%b",    "CCF",

		"LD B,B", "LD B,C", "LD B,D", "LD B,E", "LD B,H", "LD B,L", "LD B,(HL)", "LD B,A",
		"LD C,B", "LD C,C", "LD C,D", "LD C,E", "LD C,H", "LD C,L", "LD C,(HL)", "LD C,A",
		"LD D,B", "LD D,C", "LD D,D", "LD D,E", "LD D,H", "LD D,L", "LD D,(HL)", "LD D,A",
		"LD E,B", "LD E,C", "LD E,D", "LD E,E", "LD E,H", "LD E,L", "LD E,(HL)", "LD E,A",
		"LD H,B", "LD H,C", "LD H,D", "LD H,E", "LD H,H", "LD H,L", "LD H,(HL)", "LD H,A",
		"LD L,B", "LD L,C", "LD L,D", "LD L,E", "LD L,H", "LD L,L", "LD L,(HL)", "LD L,A",
		"LD (HL),B", "LD (HL),C", "LD (HL),D", "LD (HL),E", "LD (HL),H", "LD (HL),L", "HALT", "LD (HL),A",
		"LD A,B", "LD A,C", "LD A,D", "LD A,E", "LD A,H", "LD A,L", "LD A,(HL)", "LD A,A",

		"ADD B", "ADD C", "ADD D", "ADD E", "ADD H", "ADD L", "ADD (HL)", "ADD A",
		"ADC B", "ADC C", "ADC D", "ADC E", "ADC H", "ADC L", "ADC (HL)", "ADC A",
		"SUB B", "SUB C", "SUB D", "SUB E", "SUB H", "SUB L", "SUB (HL)", "SUB A",
		"SBC B", "SBC C", "SBC D", "SBC E", "SBC H", "SBC L", "SBC (HL)", "SBC A",
		"AND B", "AND C", "AND D", "AND E", "AND H", "AND L", "AND (HL)", "AND A",
		"XOR B", "XOR C", "XOR D", "XOR E", "XOR H", "XOR L", "XOR (HL)", "XOR A",
		"OR B",  "OR C",  "OR D",  "OR E",  "OR H",  "OR L",  "OR (HL)",  "OR A",
		"CP B",  "CP C",  "CP D",  "CP E",  "CP H",  "CP L",  "CP (HL)",  "CP A",

		"RET NZ", "POP BC", "JP NZ,%w", "JP %w", "CALL NZ,%w", "PUSH BC", "ADD %b", "RST 0x00",
		"RET Z", "RET", "JP Z,%w", "", "CALL Z,%w", "CALL %w", "ADC %b", "RST 0x08",
		"RET NC", "POP DE", "JP NC,%w", "INVALID_D3", "CALL NC,%w", "PUSH DE", "SUB %b", "RST 0x10",
		"RET C", "RETI", "JP C,%w", "INVALID_DB", "CALL C,%w", "INVALID_DD", "SBC %b", "RST 0x18",
		"LDH (%B),A", "POP HL", "LD (C),A", "INVALID_E3", "INVALID_E4", "PUSH HL", "AND %b", "RST 0x20",
		"ADD SP,%s", "JP (HL)", "LD (%W),A", "INVALID_EB", "INVALID_EC", "INVALID_ED", "XOR %b", "RST 0x28",
		"LDH A, (%B)", "POP AF", "LD A,(C)", "DI", "INVALID_F4", "PUSH AF", "OR %b", "RST 0x30",
		"LD HL,SP%S", "LD SP,HL", "LD A,(%W)", "EI", "INVALID_FC", "INVALID_FD", "CP %b", "RST 0x38",
	};
}

std::string disassemble_gb_opcode(uint16_t pc, std::function<uint8_t()> fetch, int& addr, uint16_t& opc)
{
	std::function<uint16_t()> fetch2 = [&fetch]() -> uint16_t {
		uint8_t lt, ht;
		lt = fetch();
		ht = fetch();
		return 256 * ht + lt;
	};
	uint8_t opcode = fetch();
	opc = (uint16_t)opcode << 8;
	if(opcode == 0xCB) {
		//Bit ops.
		static const char* regs[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
		opcode = fetch();
		opc |= opcode;
		uint8_t block = opcode >> 6;
		uint8_t bit = (opcode >> 3) & 7;
		uint8_t reg = opcode & 7;
		switch(block) {
		case 0:
			switch(bit) {
			case 0: return (stringfmt() << "RLC " << regs[reg]).str();
			case 1: return (stringfmt() << "RRC " << regs[reg]).str();
			case 2: return (stringfmt() << "RL " << regs[reg]).str();
			case 3: return (stringfmt() << "RR " << regs[reg]).str();
			case 4: return (stringfmt() << "SLA " << regs[reg]).str();
			case 5: return (stringfmt() << "SRA " << regs[reg]).str();
			case 6: return (stringfmt() << "SWAP " << regs[reg]).str();
			case 7: return (stringfmt() << "SRL " << regs[reg]).str();
			};
		case 1: return (stringfmt() << "BIT " << (int)bit << "," << regs[reg]).str();
		case 2: return (stringfmt() << "RES " << (int)bit << "," << regs[reg]).str();
		case 3: return (stringfmt() << "SET " << (int)bit << "," << regs[reg]).str();
		};
	}
	std::ostringstream o;
	const char* ins = instructions[opcode];
	uint16_t tmp;
	for(size_t i = 0; ins[i]; i++) {
		if(ins[i] != '%')
			o << ins[i];
		else {
			switch(ins[i + 1]) {
			case 'b':
				o << "0x" << hex::to8(fetch());
				break;
			case 'B':
				o << "0x" << hex::to8(tmp = fetch());
				addr = 0xFF00 + tmp;
				break;
			case 'w':
				o << "0x" << hex::to16(fetch2());
				break;
			case 'W':
				o << "0x" << hex::to16(tmp = fetch2());
				addr = tmp;
				break;
			case 'R':
				o << signdec(tmp = fetch(), false);
				addr = jraddr(pc, tmp);
				break;
			case 's':
				o << signdec(tmp = fetch(), false);
				break;
			case 'S':
				o << signdec(tmp = fetch(), true);
				break;
			}
			i++;
		}
	}
	return o.str();
}

namespace
{
	struct gb_disassembler : public disassembler
	{
		gb_disassembler() : disassembler("gb") {}
		std::string disassemble(uint64_t base, std::function<unsigned char()> fetchpc)
		{
			int dummy;
			uint16_t dummy2;
			return disassemble_gb_opcode(base, fetchpc, dummy, dummy2);
		}
	} gb_disasm;
}
