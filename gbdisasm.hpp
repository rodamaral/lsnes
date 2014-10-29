#pragma once


template<typename T> std::string tohex(T data, bool prefix) throw(std::bad_alloc)
{
	return (stringfmt() << (prefix ? "0x" : "") << std::hex << std::setfill('0') << std::setw(2 * sizeof(T))
		<< (uint64_t)data).str();
}

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

const char* gb_instructions[] = {
	"NOP",        "LD BC, %w", "LD (BC),A",  "INC BC", "INC B",    "DEC B",    "LD B,%b",    "RCLA",
	"LD (%W),SP", "ADD HL,BC", "LD A,(BC)",  "DEC BC", "INC C",    "DEC C",    "LD C,%b",    "RRCA",
	"STOP",       "LD DE, %w", "LD (DE),A",  "INC DE", "INC D",    "DEC D",    "LD D,%b",    "RLA",
	"JR %R",      "ADD HL,DE", "LD A,(DE)",  "DEC DE", "INC E",    "DEC E",    "LD E,%b",    "RRA",
	"JR NZ,%R",   "LD HL, %w", "LD (HL+),A", "INC HL", "INC H",    "DEC H",    "LD H,%b",    "DAA",
	"JR Z,%R",    "ADD HL,HL", "LD A,(HL+)", "DEC HL", "INC L",    "DEC L",    "LD L,%b",    "CPL",
	"JR NC,%R",   "LD SP, %w", "LD (HL-),A", "INC SP", "INC (HL)", "DEC (HL)", "LD (HL),%b", "SCF",
	"JR C,%R",    "ADD HL,SP", "LD A,(HL-)", "DEC SP", "INC A",    "DEC A",    "LD A,%b",    "CCF",

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
	const char* ins = gb_instructions[opcode];
	uint16_t tmp;
	for(size_t i = 0; ins[i]; i++) {
		if(ins[i] != '%')
			o << ins[i];
		else {
			switch(ins[i + 1]) {
			case 'b':
				o << tohex<uint8_t>(fetch(), true);
				break;
			case 'B':
				o << tohex<uint8_t>(tmp = fetch(), true);
				addr = 0xFF00 + tmp;
				break;
			case 'w':
				o << tohex<uint16_t>(fetch2(), true);
				break;
			case 'W':
				o << tohex<uint16_t>(tmp = fetch2(), true);
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

int get_hl(gambatte::GB* instance)
{
	return instance->get_cpureg(gambatte::GB::REG_H) * 256 + instance->get_cpureg(gambatte::GB::REG_L);
}

int get_bc(gambatte::GB* instance)
{
	return instance->get_cpureg(gambatte::GB::REG_B) * 256 + instance->get_cpureg(gambatte::GB::REG_C);
}

int get_de(gambatte::GB* instance)
{
	return instance->get_cpureg(gambatte::GB::REG_D) * 256 + instance->get_cpureg(gambatte::GB::REG_E);
}

//0 => None or already done.
//1 => BC
//2 => DE
//3 => HL
//4 => 0xFF00 + C.
//5 => Bitops
int memclass[] = {
//      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,  //0
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,  //1
	0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0,  //2
	0, 0, 3, 0, 3, 3, 3, 0, 0, 0, 3, 0, 0, 0, 0, 0,  //3
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //4
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //5
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //6
	3, 3, 3, 3, 3, 3, 0, 3, 0, 0, 0, 0, 0, 0, 3, 0,  //7
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //8
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //9
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //A
	0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0,  //B
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0,  //C
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //D
	0, 0, 4, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0,  //E
	0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //F.
};

const char* hexch = "0123456789abcdef";
inline void buffer_h8(char*& ptr, uint8_t v)
{
	*(ptr++) = hexch[v >> 4];
	*(ptr++) = hexch[v & 15];
}

inline void buffer_h16(char*& ptr, uint16_t v)
{
	*(ptr++) = hexch[v >> 12];
	*(ptr++) = hexch[(v >> 8) & 15];
	*(ptr++) = hexch[(v >> 4) & 15];
	*(ptr++) = hexch[v & 15];
}

inline void buffer_str(char*& ptr, const char* str)
{
	while(*str)
		*(ptr++) = *(str++);
}

void gambatte_debug_trace(uint16_t _pc)
{
	static char buffer[512];
	char* buffer_ptr = buffer;
	int addr = -1;
	uint16_t opcode;
	uint32_t pc = _pc;
	uint16_t offset = 0;
	std::function<uint8_t()> fetch = [pc, &offset, &buffer_ptr]() -> uint8_t {
		unsigned addr = pc + offset++;
		uint8_t v;
		disable_breakpoints = true;
		v = gb_instance->bus_read(addr);
		disable_breakpoints = false;
		buffer_h8(buffer_ptr, v);
		return v;
	};
	buffer_h16(buffer_ptr, pc);
	*(buffer_ptr++) = ' ';
	auto d = disassemble_gb_opcode(pc, fetch, addr, opcode);
	while(buffer_ptr < buffer + 12)
		*(buffer_ptr++) = ' ';
	buffer_str(buffer_ptr, d.c_str());
	switch(memclass[opcode >> 8]) {
	case 1: addr = get_bc(gb_instance); break;
	case 2: addr = get_de(gb_instance); break;
	case 3: addr = get_hl(gb_instance); break;
	case 4: addr = 0xFF00 + gb_instance->get_cpureg(gambatte::GB::REG_C); break;
	case 5: if((opcode & 7) == 6)  addr = get_hl(gb_instance); break;
	}
	while(buffer_ptr < buffer + 28)
		*(buffer_ptr++) = ' ';
	if(addr >= 0) {
		buffer_str(buffer_ptr, "[");
		buffer_h16(buffer_ptr, addr);
		buffer_str(buffer_ptr, "]");
	} else
		buffer_str(buffer_ptr, "      ");

	buffer_str(buffer_ptr, "A:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_A));
	buffer_str(buffer_ptr, " B:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_B));
	buffer_str(buffer_ptr, " C:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_C));
	buffer_str(buffer_ptr, " D:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_D));
	buffer_str(buffer_ptr, " E:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_E));
	buffer_str(buffer_ptr, " H:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_H));
	buffer_str(buffer_ptr, " L:");
	buffer_h8(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_L));
	buffer_str(buffer_ptr, " SP:");
	buffer_h16(buffer_ptr, gb_instance->get_cpureg(gambatte::GB::REG_SP));
	buffer_str(buffer_ptr, " F:");
	*(buffer_ptr++) = gb_instance->get_cpureg(gambatte::GB::REG_CF) ? 'C' : '-';
	*(buffer_ptr++) = gb_instance->get_cpureg(gambatte::GB::REG_ZF) ? '-' : 'Z';
	*(buffer_ptr++) = gb_instance->get_cpureg(gambatte::GB::REG_HF1) ? '1' : '-';
	*(buffer_ptr++) = gb_instance->get_cpureg(gambatte::GB::REG_HF2) ? '2' : '-';
	*(buffer_ptr++) = '\0';
	cb_memory_trace(2, buffer, true);
}

const char* disasm_gb(uint64_t base, unsigned char(*fetch)(void* ctx), void* ctx)
{
	static char out[1024];
	void* _ctx = ctx;
	unsigned char(*_fetch)(void* ctx) = fetch;
	int dummy;
	uint16_t dummy2;
	std::string _out = disassemble_gb_opcode(base, [_fetch, _ctx]() { return _fetch(_ctx); }, dummy, dummy2);
	strcpy(out, _out.c_str());
	return out;
}

void* handle_disasm_gb;

void gb_add_disasms()
{
	if(!cb_add_disasm)
		return;
	lsnes_core_disassembler d;
	d.name = "c-gb";
	d.fn = disasm_gb;
	if(!handle_disasm_gb)
		handle_disasm_gb = cb_add_disasm(&d);
}

void gb_remove_disasms()
{
	if(!cb_remove_disasm)
		return;
	if(handle_disasm_gb)
		cb_remove_disasm(handle_disasm_gb);
}