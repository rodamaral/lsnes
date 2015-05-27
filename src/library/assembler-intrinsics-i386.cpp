#include "assembler-intrinsics-i386.hpp"
#include "serialization.hpp"
#include <stdexcept>

namespace assembler_intrinsics
{
	const I386::reg I386::reg_none(255);
	const I386::reg I386::reg_ax(0);
	const I386::reg I386::reg_bx(3);
	const I386::reg I386::reg_cx(1);
	const I386::reg I386::reg_dx(2);
	const I386::reg I386::reg_bp(5);
	const I386::reg I386::reg_sp(4);
	const I386::reg I386::reg_si(6);
	const I386::reg I386::reg_di(7);
	const I386::reg I386::reg_r8(8);
	const I386::reg I386::reg_r9(9);
	const I386::reg I386::reg_r10(10);
	const I386::reg I386::reg_r11(11);
	const I386::reg I386::reg_r12(12);
	const I386::reg I386::reg_r13(13);
	const I386::reg I386::reg_r14(14);
	const I386::reg I386::reg_r15(15);
	const I386::reg I386::reg_rip(254);

	const I386::reg i386_r0(0);
	const I386::reg i386_r1(1);
	const I386::reg i386_r2(2);
	const I386::reg i386_r3(3);
	const I386::reg i386_r4(4);
	const I386::reg i386_r5(5);
	const I386::reg i386_r6(6);
	const I386::reg i386_r7(7);

	I386::reg::reg(uint8_t _val)
	{
		val = _val;
	}

	I386::ref I386::reg::operator[](int32_t off) const
	{
		if(val < 16)
			return ref::reg_off(*this, off);
		else if(val == 254)
			return I386::ref::rip_off(off);
		else
			throw std::runtime_error("Bad register offset-base");
	}

	I386::sib_scale_intermediate I386::reg::operator*(uint8_t scale) const
	{
		return sib_scale_intermediate(*this, scale);
	}

	I386::ref I386::reg::operator[](const reg r) const
	{
		return ref::sib(*this, r, 1);
	}

	I386::ref I386::reg::operator[](sib_scale_intermediate r) const
	{
		return ref::sib(*this, r.index, r.scale);
	}

	I386::ref I386::reg::operator[](sib_scale_off_intermediate r) const
	{
		return ref::sib(*this, r.index, r.scale, r.offset);
	}

	I386::sib_scale_off_intermediate I386::reg::operator+(int32_t off) const
	{
		return sib_scale_off_intermediate(*this, 1, off);
	}

	bool I386::reg::hbit() { return (val & 8); }
	uint8_t I386::reg::lbits() { return val & 7; }
	uint8_t I386::reg::num() { return val & 15; }
	bool I386::reg::valid(bool amd64) { return (val < (amd64 ? 16 : 8)); }
	bool I386::reg::is_none() { return (val == 255); }

	I386::sib_scale_off_intermediate::sib_scale_off_intermediate(reg _idx, uint8_t _scale, int32_t _offset)
		: index(_idx), scale(_scale), offset(_offset)
	{
	}

	I386::sib_scale_intermediate::sib_scale_intermediate(reg _idx, uint8_t _scale)
		: index(_idx), scale(_scale)
	{
	}

	I386::sib_scale_off_intermediate I386::sib_scale_intermediate::operator+(int32_t off) const
	{
		return sib_scale_off_intermediate(index, scale, off);
	}


	void check_register(I386::reg r, bool amd64)
	{
		if(!r.valid(amd64))
			throw std::runtime_error("Illegal register");
	}

	void I386::label(assembler::label& l)
	{
		a._label(l);
	}

	I386::low::low(bool _need_amd64, uint8_t _rex_prefix, std::vector<uint8_t> _ref)
		: needs_amd64(_need_amd64), rex(_rex_prefix), mref(_ref)
	{
		if(rex) needs_amd64 = true;
	}
	bool I386::low::need_amd64() { return needs_amd64; }
	bool I386::low::has_rex() { return (rex != 0); }
	uint8_t I386::low::rex_prefix() { return rex; }
	std::vector<uint8_t> I386::low::bytes() { return mref; }

	void I386::low::emit_rex(assembler::assembler& a)
	{
		if(has_rex()) a(rex);
	}

	void I386::low::emit_bytes(assembler::assembler& a)
	{
		for(auto i : mref) a(i);
	}

	I386::ref::ref()
	{
	}

	I386::ref::ref(reg r)
	{
		if(!r.valid(true))
			throw std::runtime_error("Illegal register");
		needs_amd64 = r.hbit();
		rex = r.hbit() ? 1 : 0;
		mref.push_back(0xC0 + r.lbits());
	}

	I386::ref::ref(sib_scale_intermediate r)
	{
		*this = sib(reg_none, r.index, r.scale);
	}

	I386::ref::ref(sib_scale_off_intermediate r)
	{
		*this = sib(reg_none, r.index, r.scale, r.offset);
	}

	I386::ref I386::ref::reg_off(reg r, int32_t off)
	{
		if(!r.valid(true))
			throw std::runtime_error("Illegal register");
		bool need_off = (off != 0);
		bool need_loff = (off < -128 || off > 127);
		I386::ref x;
		x.needs_amd64 = r.hbit();
		x.rex = r.hbit() ? 1 : 0;
		if(r.lbits() == 5)
			need_off = true;	//EBP and R13 always need offset.
		uint8_t rtype = need_loff ? 2 : (need_off ? 1 : 0);
		if(r.lbits() == 4) {
			//SIB is required for these.
			x.mref.push_back(0x04 + rtype * 0x40);
			x.mref.push_back(0x24);
		} else {
			x.mref.push_back(r.lbits() + rtype * 0x40);
		}
		size_t bytes = x.mref.size();
		if(need_loff) {
			x.mref.resize(bytes + 4);
			serialization::s32l(&x.mref[bytes], off);
		} else if(need_off) {
			x.mref.resize(bytes + 1);
			serialization::s8l(&x.mref[bytes], off);
		}
		return x;
	}

	I386::ref I386::ref::rip_off(int32_t off)
	{
		I386::ref x;
		x.needs_amd64 = true;
		x.mref.resize(5);
		x.mref[0] = 0x05;
		serialization::s32l(&x.mref[1], off);
		x.rex = 0;
		return x;
	}

	I386::ref I386::ref::sib(reg base, reg index, uint8_t scale, int32_t off)
	{
		I386::ref x;
		if(!base.is_none() && !base.valid(true))
			throw std::runtime_error("Illegal base in SIB");
		if((!index.is_none() && !index.valid(true)) || index.num() == 4)
			throw std::runtime_error("Illegal index in SIB");
		uint8_t ss = 0;
		switch(scale) {
		case 1: ss = 0; break;
		case 2: ss = 1; break;
		case 4: ss = 2; break;
		case 8: ss = 3; break;
		default: throw std::runtime_error("Illegal scale in SIB");
		};

		bool need_off = (off != 0);
		bool need_loff = (off < -128 || off > 127);
		if(base.is_none()) {
			//Base is the offset.
			x.mref.push_back(0x04);				//SIB coming, no offset
			x.mref.push_back(ss * 64 + index.lbits() + 5);	//Base is "EBP".
			need_loff = true;
		} else {
			if(base.num() == 5)
				need_off = true;	//This always needs offset.
			uint8_t rtype = need_loff ? 2 : (need_off ? 1 : 0);
			x.mref.push_back(0x04 + rtype * 0x40);		//SIB coming.
			x.mref.push_back(ss * 64 + index.lbits() * 8 + base.lbits());
		}
		x.rex = 0;
		if(!base.is_none()) x.rex |= (base.hbit() ? 1 : 0);
		if(!index.is_none()) x.rex |= (index.hbit() ? 2 : 0);
		x.needs_amd64 = (x.rex != 0);
		size_t bytes = x.mref.size();
		if(need_loff) {
			x.mref.resize(bytes + 4);
			serialization::s32l(&x.mref[bytes], off);
		} else if(need_off) {
			x.mref.resize(bytes + 1);
			serialization::s8l(&x.mref[bytes], off);
		}
		return x;
	}

	I386::low I386::ref::operator()(reg r, bool set_size_flag, bool amd64)
	{
		check_register(r, amd64);
		auto c = mref;
		c[0] |= r.lbits() * 8;
		uint8_t _rex = rex;
		if(r.hbit()) _rex+=4;			//Set R
		if(set_size_flag && amd64) _rex+=8;	//Set S if needed.
		if(_rex) _rex+=0x40;			//Ret rex prefix bits.
		if((_rex || needs_amd64) && !amd64)
			throw std::runtime_error("Illegal memory reference for i386");
		return low(needs_amd64, _rex, c);
	}

	void I386::ref::emit(assembler::assembler& a, bool set_size_flag, bool amd64, reg r, uint8_t op1)
	{
		auto xref = (*this)(r, set_size_flag, amd64);
		xref.emit_rex(a);
		a(op1);
		xref.emit_bytes(a);
	}

	void I386::ref::emit(assembler::assembler& a, bool set_size_flag, bool amd64, reg r, uint8_t op1,
		uint8_t op2)
	{
		auto xref = (*this)(r, set_size_flag, amd64);
		xref.emit_rex(a);
		a(op1, op2);
		xref.emit_bytes(a);
	}

	I386::I386(assembler::assembler& _a, bool _amd64)
		: a(_a), amd64(_amd64)
	{
	}

	bool I386::is_amd64()
	{
		return amd64;
	}

	//Is i386?
	bool I386::is_i386()
	{
		return !amd64;
	}

	//Get word size.
	uint8_t I386::wordsize()
	{
		return amd64 ? 8 : 4;
	}

	//PUSH NWORD <reg>
	void I386::push_reg(reg r)
	{
		check_register(r, amd64);
		if(amd64 && r.hbit()) a(0x41);
		a(0x50 + r.lbits());
	}
	//POP NWORD <reg>
	void I386::pop_reg(reg r)
	{
		check_register(r, amd64);
		if(amd64 && r.hbit()) a(0x41);
		a(0x58 + r.lbits());
	}
	//MOV NWORD <reg>,<regmem>
	void I386::mov_reg_regmem(reg r, I386::ref mem)
	{
		mem.emit(a, true, amd64, r, 0x8B);
	}
	//XOR NWORD <reg>,<regmem>
	void I386::xor_reg_regmem(reg r, I386::ref mem)
	{
		mem.emit(a, true, amd64, r, 0x33);
	}

	//LEA <reg>,<mem>
	void I386::lea_reg_mem(reg r, I386::ref mem)
	{
		mem.emit(a, true, amd64, r, 0x8D);
	}

	//AND NWORD <regmem>, imm
	void I386::and_regmem_imm(I386::ref mem, int32_t imm)
	{
		mem.emit(a, true, amd64, i386_r4, 0x81);
		a(assembler::vle<int32_t>(imm));
	}

	//SHL NWORD <regmem>, imm
	void I386::shl_regmem_imm(I386::ref mem, uint8_t imm)
	{
		mem.emit(a, true, amd64, i386_r4, 0xC1);
		a(imm);
	}

	//SHL BYTE <regmem>, imm
	void I386::shl_regmem_imm8(ref mem, uint8_t imm)
	{
		mem.emit(a, true, amd64, i386_r4, 0xC0);
		a(imm);
	}

	//ADD NWORD <regmem>, imm
	void I386::add_regmem_imm(I386::ref mem, int32_t imm)
	{
		mem.emit(a, true, amd64, i386_r0, 0x81);
		a(assembler::vle<int32_t>(imm));
	}

	//ADD NWORD <reg>,<regmem>
	void I386::add_reg_regmem(reg r, I386::ref mem)
	{
		mem.emit(a, true, amd64, r, 0x03);
	}

	//MOV BYTE <regmem>, imm
	void I386::mov_regmem_imm8(I386::ref mem, uint8_t imm)
	{
		mem.emit(a, false, amd64, i386_r0, 0xC6);
		a(imm);
	}

	//MOV DWORD <regmem>, imm
	void I386::mov_regmem_imm32(I386::ref mem, uint32_t imm)
	{
		mem.emit(a, false, amd64, i386_r0, 0xC7);
		a(assembler::vle<uint32_t>(imm));
	}

	//MOV NWORD <reg>, imm.
	void I386::mov_reg_imm(reg r, size_t imm)
	{
		check_register(r, amd64);
		if(amd64) a(0x48 | (r.hbit() ? 1 : 0));
		a(0xB8 + r.lbits());
		a(assembler::vle<size_t>(imm));
	}

	//CMP NWORD <regmem>, imm
	void I386::cmp_regmem_imm(I386::ref mem, int32_t imm)
	{
		mem.emit(a, true, amd64, i386_r7, 0x81);
		a(assembler::vle<int32_t>(imm));
	}

	//MOV WORD <regmem>, <reg>
	void I386::mov_regmem_reg16(I386::ref mem, reg r)
	{
		a(0x66);
		mem.emit(a, false, amd64, r, 0x89);
	}

	//MOV BYTE <regmem>, <reg>
	void I386::mov_regmem_reg8(ref mem, reg r)
	{
		mem.emit(a, false, amd64, r, 0x88);
	}

	//MOV WORD <reg>, <regmem>
	void I386::mov_reg_regmem16(reg r, I386::ref mem)
	{
		a(0x66);
		mem.emit(a, false, amd64, r, 0x8B);
	}

	//MOV BYTE <reg>, <regmem>
	void I386::mov_reg_regmem8(reg r, ref mem)
	{
		mem.emit(a, false, amd64, r, 0x8A);
	}

	//OR BYTE <regmem>, imm
	void I386::or_regmem_imm8(I386::ref mem, uint8_t imm)
	{
		mem.emit(a, false, amd64, i386_r1, 0x80);
		a(imm);
	}

	//AND BYTE <regmem>, imm
	void I386::and_regmem_imm8(I386::ref mem, uint8_t imm)
	{
		mem.emit(a, false, amd64, i386_r4, 0x80);
		a(imm);
	}

	//INC NWORD <regmem>
	void I386::inc_regmem(I386::ref mem)
	{
		mem.emit(a, true, amd64, i386_r0, 0xFF);
	}

	//TEST BYTE <regmem>, imm
	void I386::test_regmem_imm8(I386::ref mem, uint8_t imm)
	{
		mem.emit(a, false, amd64, i386_r0, 0xF6);
		a(imm);
	}

	//CMP BYTE <regmem>, imm
	void I386::cmp_regmem_imm8(I386::ref mem, uint8_t imm)
	{
		mem.emit(a, false, amd64, i386_r7, 0x80);
		a(imm);
	}

	//MOV NWORD <reg>, <addr>.
	void I386::mov_reg_addr(reg r, assembler::label& l)
	{
		if(amd64) a(0x48 + (r.hbit() ? 1 : 0));
		a(0xB8 + r.lbits());
		address(l);
	}

	//NEG BYTE <regmem>
	void I386::neg_regmem8(ref mem)
	{
		mem.emit(a, false, amd64, i386_r3, 0xF6);
	}

	//ADD BYTE <regmem>, imm
	void I386::add_regmem_imm8(ref mem, uint8_t imm)
	{
		mem.emit(a, false, amd64, i386_r0, 0x80);
		a(imm);
	}

	//OR BYTE <regmem>, <reg>
	void I386::or_regmem_reg8(ref mem, reg r)
	{
		mem.emit(a, false, amd64, r, 0x08);
	}

	//CALL <addr>
	void I386::call_addr(assembler::label& l)
	{
		if(amd64) {
			call_regmem(reg_rip[2]);
			a(0xEB, 0x08);	//JMP +8
			address(l);
		} else {
			a(0xE8, assembler::relocation_tag(assembler::i386_reloc_rel32, l), assembler::pad_tag(4));
		}
	}

	//CALL <regmem>
	void I386::call_regmem(I386::ref mem)
	{
		mem.emit(a, false, amd64, i386_r2, 0xFF);
	}

	//JMP <regmem>
	void I386::jmp_regmem(I386::ref mem)
	{
		mem.emit(a, false, amd64, i386_r4, 0xFF);
	}

	//SETNZ <regmem>
	void I386::setnz_regmem(I386::ref mem)
	{
		mem.emit(a, false, amd64, i386_r0, 0x0F, 0x95);
	}

	//JNZ SHORT <label>
	void I386::jnz_short(assembler::label& l)
	{
		a(0x75, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
	}

	//JZ SHORT <label>
	void I386::jz_short(assembler::label& l)
	{
		a(0x74, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
	}

	//JMP SHORT <label>
	void I386::jmp_short(assembler::label& l)
	{
		a(0xEB, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
	}

	//JMP LONG <label>
	void I386::jmp_long(assembler::label& l)
	{
		a(0xE9, assembler::relocation_tag(assembler::i386_reloc_rel32, l), assembler::pad_tag(4));
	}

	//JZ LONG <label>
	void I386::jz_long(assembler::label& l)
	{
		a(0x0F, 0x84, assembler::relocation_tag(assembler::i386_reloc_rel32, l), assembler::pad_tag(4));
	}

	//JAE SHORT <label>
	void I386::jae_short(assembler::label& l)
	{
		a(0x73, assembler::relocation_tag(assembler::i386_reloc_rel8, l), 0x00);
	}

	//JAE LONG <label>
	void I386::jae_long(assembler::label& l)
	{
		a(0x0F, 0x83, assembler::relocation_tag(assembler::i386_reloc_rel32, l), assembler::pad_tag(4));
	}

	void I386::ret()
	{
		a(0xC3);
	}

	void I386::address(assembler::label& l)
	{
		a(assembler::relocation_tag((amd64 ? assembler::i386_reloc_abs64 : assembler::i386_reloc_abs32), l),
			assembler::pad_tag(wordsize()));
	}
}
