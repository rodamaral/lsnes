#ifndef _library__assembler_intrinsics_i386__hpp__included__
#define _library__assembler_intrinsics_i386__hpp__included__

#include "assembler.hpp"
#include <cstdint>
#include <cstdlib>

namespace assembler_intrinsics
{
	struct I386
	{
		struct ref;
		struct sib_scale_off_intermediate;
		struct sib_scale_intermediate;
		struct reg
		{
			explicit reg(uint8_t _val);
			ref operator[](int32_t off) const;
			sib_scale_intermediate operator*(uint8_t scale) const;
			ref operator[](const reg r) const;
			ref operator[](sib_scale_intermediate r) const;
			ref operator[](sib_scale_off_intermediate r) const;
			sib_scale_off_intermediate operator+(int32_t off) const;
			bool hbit();
			uint8_t lbits();
			uint8_t num();
			bool valid(bool amd64);
			bool is_none();
		private:
			friend class ref;
			friend class sib_scale_intermediate;
			friend class sib_scale_off_intermediate;
			uint8_t val;
		};
		const static reg reg_none;
		const static reg reg_ax;
		const static reg reg_bx;
		const static reg reg_cx;
		const static reg reg_dx;
		const static reg reg_bp;	//Don't use in 8-bit mode.
		const static reg reg_sp;	//Don't use in 8-bit mode.
		const static reg reg_si;	//Don't use in 8-bit mode.
		const static reg reg_di;	//Don't use in 8-bit mode.
		const static reg reg_r8;
		const static reg reg_r9;
		const static reg reg_r10;
		const static reg reg_r11;
		const static reg reg_r12;
		const static reg reg_r13;
		const static reg reg_r14;
		const static reg reg_r15;
		const static reg reg_rip;

		struct sib_scale_off_intermediate
		{
			sib_scale_off_intermediate(reg idx, uint8_t scale, int32_t offset);
		private:
			friend class reg;
			friend class ref;
			reg index;
			uint8_t scale;
			int32_t offset;
		};
		struct sib_scale_intermediate
		{
			sib_scale_intermediate(reg idx, uint8_t scale);
			sib_scale_off_intermediate operator+(int32_t off) const;
		private:
			friend class reg;
			friend class ref;
			reg index;
			uint8_t scale;
		};
		struct low
		{
			low(bool _need_amd64, uint8_t _rex_prefix, std::vector<uint8_t> _ref);
			bool need_amd64();
			bool has_rex();
			uint8_t rex_prefix();
			std::vector<uint8_t> bytes();
			void emit_rex(assembler::assembler& a);
			void emit_bytes(assembler::assembler& a);
		private:
			bool needs_amd64;
			uint8_t rex;
			std::vector<uint8_t> mref;
		};
		struct ref
		{
			ref();
			ref(reg r);
			ref(sib_scale_intermediate r);
			ref(sib_scale_off_intermediate r);
			low operator()(reg r, bool set_size_flag, bool amd64);
			void emit(assembler::assembler& a, bool set_size_flag, bool amd64, reg r, uint8_t op1);
			void emit(assembler::assembler& a, bool set_size_flag, bool amd64, reg r, uint8_t op1,
				uint8_t op2);
		private:
			friend class sib_scale_intermediate;
			friend class sib_scale_off_intermediate;
			friend class reg;
			static ref reg_off(reg r, int32_t off = 0);
			static ref rip_off(int32_t off = 0);
			static ref sib(reg base, reg index, uint8_t scale = 1, int32_t off = 0);
			bool needs_amd64;
			uint8_t rex;
			std::vector<uint8_t> mref;
		};
		I386(assembler::assembler& _a, bool _amd64);
		//Is amd64?
		bool is_amd64();
		//Is i386?
		bool is_i386();
		//Get word size.
		uint8_t wordsize();
		//Label:
		void label(assembler::label& l);
		//PUSH NWORD <reg>
		void push_reg(reg r);
		//POP NWORD <reg>
		void pop_reg(reg r);
		//MOV NWORD <reg>,<regmem>
		void mov_reg_regmem(reg r, ref mem);
		//LEA <reg>,<mem>
		void lea_reg_mem(reg r, ref mem);
		//XOR NWORD <reg>,<regmem>
		void xor_reg_regmem(reg r, ref mem);
		//MOV BYTE <regmem>, imm
		void mov_regmem_imm8(ref mem, uint8_t imm);
		//ADD BYTE <regmem>, imm
		void add_regmem_imm8(ref mem, uint8_t imm);
		//MOV DWORD <regmem>, imm
		void mov_regmem_imm32(ref mem, uint32_t imm);
		//MOV WORD <regmem>, <reg>
		void mov_regmem_reg16(ref mem, reg r);
		//MOV WORD <reg>, <regmem>
		void mov_reg_regmem16(reg r, ref mem);
		//MOV BYTE <reg>, <regmem>
		void mov_reg_regmem8(reg r, ref mem);
		//MOV BYTE <regmem>, <reg>
		void mov_regmem_reg8(ref mem, reg r);
		//OR BYTE <regmem>, imm
		void or_regmem_imm8(ref mem, uint8_t imm);
		//AND BYTE <regmem>, imm
		void and_regmem_imm8(ref mem, uint8_t imm);
		//TEST BYTE <regmem>, imm
		void test_regmem_imm8(ref mem, uint8_t imm);
		//CMP BYTE <regmem>, imm
		void cmp_regmem_imm8(ref mem, uint8_t imm);
		//CMP NWORD <regmem>, imm
		void cmp_regmem_imm(ref mem, int32_t imm);
		//SHL NWORD <regmem>, imm
		void shl_regmem_imm(ref mem, uint8_t imm);
		//SHL BYTE <regmem>, imm
		void shl_regmem_imm8(ref mem, uint8_t imm);
		//AND NWORD <regmem>, imm
		void and_regmem_imm(ref mem, int32_t imm);
		//ADD NWORD <regmem>, imm
		void add_regmem_imm(ref mem, int32_t imm);
		//ADD NWORD <reg>,<regmem>
		void add_reg_regmem(reg r, ref mem);
		//INC NWORD <regmem>
		void inc_regmem(ref mem);
		//MOV NWORD <reg>, <addr>.
		void mov_reg_addr(reg r, assembler::label& l);
		//MOV NWORD <reg>, imm.
		void mov_reg_imm(reg r, size_t imm);
		//NEG BYTE <regmem>
		void neg_regmem8(ref mem);
		//OR BYTE <regmem>, <reg>
		void or_regmem_reg8(ref mem, reg r);
		//CALL <addr>
		void call_addr(assembler::label& l);
		//CALL <regmem>
		void call_regmem(ref mem);
		//SETNZ <regmem>
		void setnz_regmem(ref mem);
		//JMP <regmem>
		void jmp_regmem(ref mem);
		//JNZ SHORT <label>
		void jnz_short(assembler::label& l);
		//JZ SHORT <label>
		void jz_short(assembler::label& l);
		//JZ SHORT <label>
		void jz_long(assembler::label& l);
		//JMP SHORT <label>
		void jmp_short(assembler::label& l);
		//JMP LONG <label>
		void jmp_long(assembler::label& l);
		//JAE SHORT <label>
		void jae_short(assembler::label& l);
		//JAE LONG <label>
		void jae_long(assembler::label& l);
		//RET
		void ret();
		//Write address constant.
		void address(assembler::label& l);
	private:
		assembler::assembler& a;
		bool amd64;
	};
}

#endif
