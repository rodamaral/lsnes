#include "portctrl-data.hpp"
#include "portctrl-parse-asmgen.hpp"
#include "assembler-intrinsics-i386.hpp"

using namespace assembler_intrinsics;

namespace portctrl
{
namespace codegen
{
int calc_mask_bit(uint8_t mask)
{
	switch(mask) {
	case 0x01:	return 0;
	case 0x02:	return 1;
	case 0x04:	return 2;
	case 0x08:	return 3;
	case 0x10:	return 4;
	case 0x20:	return 5;
	case 0x40:	return 6;
	case 0x80:	return 7;
	default:	return -1;
	}
}

template<> void emit_serialize_prologue(I386& a, assembler::label_list& labels)
{
	//Variable assignments:
	//SI: State block.
	//DX: Output buffer.
	//AX: Write position counter.

	//On amd64, the ABI places the parameters of serialize() into those registers, but on i386, the parameters
	//are passed on stack. So load the parameters into correct registers. Also, save the registers beforehand.
	if(a.is_i386()) {
		a.push_reg(I386::reg_si);
		a.push_reg(I386::reg_dx);
		a.mov_reg_regmem(I386::reg_si, I386::reg_sp[16]);
		a.mov_reg_regmem(I386::reg_dx, I386::reg_sp[20]);
	}
	//Set write position to 0.
	a.xor_reg_regmem(I386::reg_ax, I386::reg_ax);
}

template<> void emit_serialize_button(I386& a, assembler::label_list& labels, int32_t offset, uint8_t mask,
	uint8_t ch)
{
	//Test the bit with specified mask on specified position. If set, set CL to 1, otherwise to 0.
	a.test_regmem_imm8(I386::reg_si[offset], mask);
	a.setnz_regmem(I386::reg_cx);
	//Negate CL. This causes CL to become 0xFF or 0x00.
	a.neg_regmem8(I386::reg_cx);
	//AND CL with ch-'.' and add '.'. This results in ch if CL was 0xFF and '.' if CL was 0x00.
	a.and_regmem_imm8(I386::reg_cx, ch - '.');
	a.add_regmem_imm8(I386::reg_cx, '.');
	//Write the byte to write position and advance write position.
	a.mov_regmem_reg8(I386::reg_dx[I386::reg_ax], I386::reg_cx);
	a.inc_regmem(I386::reg_ax);
}

template<> void emit_serialize_axis(I386& a, assembler::label_list& labels, int32_t offset)
{
	//Get reference to write_axis_value() routine.
	assembler::label& axisprint = labels.external((void*)write_axis_value);

	//Save the state registers.
	if(a.is_i386()) a.push_reg(I386::reg_di);
	a.push_reg(I386::reg_si);
	a.push_reg(I386::reg_dx);
	a.push_reg(I386::reg_ax);
	//Load the axis value from controller state (i386/amd64 is LE, so endianess is right).
	a.mov_reg_regmem16(I386::reg_si, I386::reg_si[offset]);
	//The high bits of SI are junk. Mask those off. This will become the second parameter of axis_print().
	a.and_regmem_imm(I386::reg_si, 0xFFFF);
	//Set first parameter of axis_print() to current write position.
	a.lea_reg_mem(I386::reg_di, I386::reg_dx[I386::reg_ax]);
	//If on i386, push the parameters into stack, as parameters are passed there.
	if(a.is_i386()) a.push_reg(I386::reg_si);
	if(a.is_i386()) a.push_reg(I386::reg_di);
	//Call the axis_print() routine.
	a.call_addr(axisprint);
	//If on i386, clean up the passed parameters.
	if(a.is_i386()) a.add_regmem_imm(I386::reg_sp, 8);
	//Restore the state. We restore ax to cx, so we preserve the return value of axis_print().
	a.pop_reg(I386::reg_cx);
	a.pop_reg(I386::reg_dx);
	a.pop_reg(I386::reg_si);
	if(a.is_i386()) a.pop_reg(I386::reg_di);
	//Add the old pointer to return value of axis_print() and make that the new pointer. Addition is commutative,
	//so new = old + delta and new = delta + old are the same.
	a.add_reg_regmem(I386::reg_ax, I386::reg_cx);
}

template<> void emit_serialize_pipe(I386& a, assembler::label_list& labels)
{
	//Write '|' to write position and adavance write position.
	a.mov_regmem_imm8(I386::reg_dx[I386::reg_ax], '|');
	a.inc_regmem(I386::reg_ax);
}

template<> void emit_serialize_epilogue(I386& a, assembler::label_list& labels)
{
	//If on i386, restore the saved registers.
	if(a.is_i386()) a.pop_reg(I386::reg_dx);
	if(a.is_i386()) a.pop_reg(I386::reg_si);
	//Exit routine. The pointer in AX is correct for return value.
	a.ret();
}

template<> void emit_deserialize_prologue(I386& a, assembler::label_list& labels)
{
	//Variable assignments:
	//SI: State block.
	//DX: Input buffer.
	//AX: read position counter.

	//On amd64, the ABI places the parameters of deserialize() into those registers, but on i386, the parameters
	//are passed on stack. So load the parameters into correct registers. Also, save the registers beforehand.
	if(a.is_i386()) {
		a.push_reg(I386::reg_si);
		a.push_reg(I386::reg_dx);
		a.mov_reg_regmem(I386::reg_si, I386::reg_sp[16]);
		a.mov_reg_regmem(I386::reg_dx, I386::reg_sp[20]);
	}
	//Set read position to 0.
	a.xor_reg_regmem(I386::reg_ax, I386::reg_ax);
}

template<> void emit_deserialize_clear_storage(I386& a, assembler::label_list& labels, int32_t size)
{
	int32_t i;
	//Write four zero bytes at once if there is space for so.
	for(i = 0; i < size - 3; i += 4)
		a.mov_regmem_imm32(I386::reg_si[i], 0);
	//Write the rest one byte at a time.
	for(; i < size; i++)
		a.mov_regmem_imm8(I386::reg_si[i], 0);
}

template<> void emit_deserialize_button(I386& a, assembler::label_list& labels, int32_t offset,
	uint8_t mask, assembler::label& next_pipe, assembler::label& end_deserialize)
{
	assembler::label& clear_button = labels;

	//Load the next input byte into CL.
	a.mov_reg_regmem8(I386::reg_cx, I386::reg_dx[I386::reg_ax]);
	//Check for special values '|', '\r', '\n', '\0'.
	a.cmp_regmem_imm8(I386::reg_cx, '|');
	a.jz_long(next_pipe);
	a.cmp_regmem_imm8(I386::reg_cx, '\r');
	a.jz_long(end_deserialize);
	a.cmp_regmem_imm8(I386::reg_cx, '\n');
	a.jz_long(end_deserialize);
	a.cmp_regmem_imm8(I386::reg_cx, '\0');
	a.jz_long(end_deserialize);
	//Check for released values ' ' and '.'. If found, skip setting the bit.
	a.cmp_regmem_imm8(I386::reg_cx, ' ');
	a.jz_short(clear_button);
	a.cmp_regmem_imm8(I386::reg_cx, '.');
	a.jz_short(clear_button);
	//Pressed. Set the button bit.
	a.or_regmem_imm8(I386::reg_si[offset], mask);
	a.label(clear_button);
	//Advance the read pointer.
	a.inc_regmem(I386::reg_ax);
}

template<> void emit_deserialize_axis(I386& a, assembler::label_list& labels, int32_t offset)
{
	//Get reference to read_axis_value() routine.
	assembler::label& axisread = labels.external((void*)read_axis_value);

	//Save status registers.
	if(a.is_i386()) a.push_reg(I386::reg_di);
	a.push_reg(I386::reg_si);
	a.push_reg(I386::reg_dx);
	a.push_reg(I386::reg_ax);
	//Set first parameter of read_axis_value() to start of serialization input buffer.
	//Set the second parameter to be reference to saved read counter on stack. On i386, push the parameters
	//to proper places.
	a.mov_reg_regmem(I386::reg_di, I386::reg_dx);
	a.mov_reg_regmem(I386::reg_si, I386::reg_sp);
	if(a.is_i386()) a.push_reg(I386::reg_si);
	if(a.is_i386()) a.push_reg(I386::reg_di);
	//Call read_axis_value().
	a.call_addr(axisread);
	//Clean up the paraemters from stack (i386 only).
	if(a.is_i386()) a.add_regmem_imm(I386::reg_sp, 8);
	//Temporarily put the return value into CX.
	a.mov_reg_regmem(I386::reg_cx, I386::reg_ax);
	//Restore the status registers. Note that AX (read position) has been modified by reference.
	a.pop_reg(I386::reg_ax);
	a.pop_reg(I386::reg_dx);
	a.pop_reg(I386::reg_si);
	if(a.is_i386()) a.pop_reg(I386::reg_di);
	//Write the axis value into controller state.
	a.mov_regmem_reg16(I386::reg_si[offset], I386::reg_cx);
}

template<> void emit_deserialize_skip_until_pipe(I386& a, assembler::label_list& labels, assembler::label& next_pipe, assembler::label& deserialize_end)
{
	assembler::label& loop = labels;
	//While...
	a.label(loop);
	//Load next character.
	a.mov_reg_regmem8(I386::reg_cx, I386::reg_dx[I386::reg_ax]);
	//If it is '|', handle as pipe.
	a.cmp_regmem_imm8(I386::reg_cx, '|');
	a.jz_long(next_pipe);
	//If it is '\r', \n' or '\0', handle as end of input.
	a.cmp_regmem_imm8(I386::reg_cx, '\r');
	a.jz_long(deserialize_end);
	a.cmp_regmem_imm8(I386::reg_cx, '\n');
	a.jz_long(deserialize_end);
	a.cmp_regmem_imm8(I386::reg_cx, '\0');
	a.jz_long(deserialize_end);
	//Not interesting: Advance read position and go back to loop.
	a.inc_regmem(I386::reg_ax);
	a.jmp_short(loop);
}

template<> void emit_deserialize_skip(I386& a, assembler::label_list& labels)
{
	//Increment the read position to skip the byte.
	a.inc_regmem(I386::reg_ax);
}

template<> void emit_deserialize_special_blank(I386& a, assembler::label_list& labels)
{
	//AX is the pending return value. Set that.
	a.mov_reg_imm(I386::reg_ax, DESERIALIZE_SPECIAL_BLANK);
}

template<> void emit_deserialize_epilogue(I386& a, assembler::label_list& labels)
{
	//Restore the saved registers on i386.
	if(a.is_i386()) a.pop_reg(I386::reg_dx);
	if(a.is_i386()) a.pop_reg(I386::reg_si);
	//Exit routine. The pointer in AX is correct for return value (changed to DESERIALIZE_SPECIAL_BLANK if
	//needed).
	a.ret();
}

template<> void emit_read_prologue(I386& a, assembler::label_list& labels)
{
	//Variable assignments:
	//SI: State block.
	//DX: Controller number
	//CX: Button index.
	//AX: Pending return value.

	//Save registers if i386 and load the parameters from stack into variables.
	if(a.is_i386()) {
		a.push_reg(I386::reg_si);
		a.push_reg(I386::reg_dx);
		a.push_reg(I386::reg_cx);
		a.mov_reg_regmem(I386::reg_si, I386::reg_sp[20]);
		a.mov_reg_regmem(I386::reg_dx, I386::reg_sp[24]);
		a.mov_reg_regmem(I386::reg_cx, I386::reg_sp[28]);
	}
	//Zero out the pending return value.
	a.xor_reg_regmem(I386::reg_ax, I386::reg_ax);
}

template<> void emit_read_epilogue(I386& a, assembler::label_list& labels)
{
	//Restore the saved registers on i386.
	if(a.is_i386()) a.pop_reg(I386::reg_cx);
	if(a.is_i386()) a.pop_reg(I386::reg_dx);
	if(a.is_i386()) a.pop_reg(I386::reg_si);
	//Exit routine. AX is the pending return.
	a.ret();
}

template<> void emit_read_dispatch(I386& a, assembler::label_list& labels,
	unsigned controllers, unsigned ilog2controls, assembler::label& end)
{
	//Get reference to table after dispatch code.
	assembler::label& table = labels;
	//Is the controller number out of range? If yes, jump to epilogue (pending return register is 0).
	a.cmp_regmem_imm(I386::reg_dx, controllers);
	a.jae_long(end);
	//Is the control number out of range? If yes, jump to epilogue (pending return register is 0).
	a.cmp_regmem_imm(I386::reg_cx, 1 << ilog2controls);
	a.jae_long(end);
	//The numbers are in range. Compute 1-D index as controller * (2^ilog2controls) + control into DX.
	a.shl_regmem_imm(I386::reg_dx, ilog2controls);
	a.add_reg_regmem(I386::reg_dx, I386::reg_cx);
	//Load base address of jump table to CX.
	a.mov_reg_addr(I386::reg_cx, table);
	//Jump to entry in position DX in table (doing words to bytes conversion).
	a.jmp_regmem(I386::reg_cx[I386::reg_dx * a.wordsize()]);
	//The table comes after this routine.
	a.label(table);
}

template<> assembler::label& emit_read_label(I386& a, assembler::label_list& labels)
{
	//Get reference to whatever code is called.
	assembler::label& l = labels;
	//Write address entry.
	a.address(l);
	//Return the newly created reference.
	return l;
}

template<> void emit_read_label_bad(I386& a, assembler::label_list& labels, assembler::label& b)
{
	//Write address entry for specified label.
	a.address(b);
}

template<> void emit_read_button(I386& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset, uint8_t mask)
{
	//Declare the label jumping into this code.
	a.label(l);
	//The pending return is 0, so we can just set low byte of it to 1 if button bit is set.
	a.test_regmem_imm8(I386::reg_si[offset], mask);
	a.setnz_regmem(I386::reg_ax);	//Really AL.
	//Jump to epilogue.
	a.jmp_long(end);
}

template<> void emit_read_axis(I386& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset)
{
	//Declare the label jumping into this code.
	a.label(l);
	//Just read the axis value. i386/amd64 is LE, so endianess is correct.
	a.mov_reg_regmem16(I386::reg_ax, I386::reg_si[offset]);
	//Jump to epilogue.
	a.jmp_long(end);
}

template<> void emit_write_prologue(I386& a, assembler::label_list& labels)
{
	//Variable assignments:
	//SI: State block.
	//DX: Controller number
	//CX: Button index.
	//AX: Value to write.

	//Save registers if i386 and load the parameters from stack into variables.
	if(a.is_i386()) {
		a.push_reg(I386::reg_si);
		a.push_reg(I386::reg_dx);
		a.push_reg(I386::reg_cx);
		a.push_reg(I386::reg_ax);
		a.mov_reg_regmem(I386::reg_si, I386::reg_sp[24]);
		a.mov_reg_regmem(I386::reg_dx, I386::reg_sp[28]);
		a.mov_reg_regmem(I386::reg_cx, I386::reg_sp[32]);
		a.mov_reg_regmem(I386::reg_ax, I386::reg_sp[36]);
	} else {
		//On amd64, the fifth parameter is in r8. Copy it to ax to be consistent with i386 case.
		a.mov_reg_regmem(I386::reg_ax, I386::reg_r8);
	}
}

template<> void emit_write_epilogue(I386& a, assembler::label_list& labels)
{
	//Restore saved registers on i386.
	if(a.is_i386()) a.pop_reg(I386::reg_ax);
	if(a.is_i386()) a.pop_reg(I386::reg_cx);
	if(a.is_i386()) a.pop_reg(I386::reg_dx);
	if(a.is_i386()) a.pop_reg(I386::reg_si);
	//Return. This function has no return value.
	a.ret();
}

template<> void emit_write_button(I386& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset, uint8_t mask)
{
	//See if mask can be done as shift.
	int mask_bit = calc_mask_bit(mask);
	//Label jumping to this code.
	a.label(l);
	//Is the write value nonzero? If not, set AL to 1 (otherwise set to 0).
	//We don't need write value after this, so freely trash it.
	a.cmp_regmem_imm(I386::reg_ax, 0);
	a.setnz_regmem(I386::reg_ax);
	//Set AL to be mask if write value was nonzero.
	if(mask_bit >= 0) {
		//Shift the 0 or 1 from LSB to correct place.
		a.shl_regmem_imm8(I386::reg_ax, mask_bit);
	} else {
		//Negate the number to 0xFF or 0x00 and then mask the bits not in mask, giving either mask or 0.
		a.neg_regmem8(I386::reg_ax);
		a.and_regmem_imm8(I386::reg_ax, mask);
	}
	//Clear the bits in mask.
	a.and_regmem_imm8(I386::reg_si[offset], ~mask);
	//If needed, set the bits back.
	a.or_regmem_reg8(I386::reg_si[offset], I386::reg_ax);
	//Jump to epilogue.
	a.jmp_long(end);
}

template<> void emit_write_axis(I386& a, assembler::label_list& labels, assembler::label& l,
	assembler::label& end, int32_t offset)
{
	//Label jumping to this code.
	a.label(l);
	//Write the write value to axis value. i386/amd64 is LE, so endianess is correct.
	a.mov_regmem_reg16(I386::reg_si[offset], I386::reg_ax);
	//Jump to epilogue.
	a.jmp_long(end);
}
}
}
