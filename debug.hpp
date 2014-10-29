#pragma once

void gambatte_set_debugbuf(uint8_t* dbuf, uint64_t addr, uint64_t size, uint8_t s, uint8_t c)
{
	if(addr >= size)
		return;
	dbuf[addr] |= (s & 7);
	dbuf[addr] &= ~(c & 7);
}

void gambatte_set_debugbuf_all(uint8_t* dbuf, uint64_t size, uint8_t s, uint8_t c)
{
	for(size_t i = 0; i < size; i++) {
		dbuf[i] |= ((s & 7) << 4);
		dbuf[i] &= ~((c & 7) << 4);
	}
}

void gambatte_set_cheat(uint8_t* dbuf, std::map<unsigned, uint8_t>& cbuf, uint64_t addr, uint64_t size, bool set,
	uint8_t value)
{
	if(addr >= size)
		return;
	if(set) {
		dbuf[addr] |= 8;
		cbuf[addr] = value;
	} else {
		dbuf[addr] &= ~8;
		cbuf.erase(addr);
	}
}

bool bsnes_trace_fn()
{
	if(trace_cpu_enable) {
		char buffer[1024];
		SNES::cpu.disassemble_opcode(buffer, SNES::cpu.regs.pc);
		cb_memory_trace(0, buffer, true);
	}
	return false;
}

bool bsnes_smp_trace_fn()
{
	if(trace_smp_enable) {
		nall::string _disasm = SNES::smp.disassemble_opcode(SNES::smp.regs.pc);
		std::string disasm(_disasm, _disasm.length());
		cb_memory_trace(1, disasm.c_str(), true);
	}
	return false;
}

bool bsnes_delayreset_fn()
{
	bsnes_trace_fn();	//Call this also.
	if(delayreset_cycles_run == delayreset_cycles_target || video_refresh_done)
		return true;
	delayreset_cycles_run++;
	return false;
}

void bsnes_update_trace_hook_state()
{
	if(forced_hook)
		return;
	if(!trace_cpu_enable)
		SNES::cpu.step_event = nall::function<bool()>();
	else
		SNES::cpu.step_event = bsnes_trace_fn;
	if(!trace_smp_enable)
		SNES::smp.step_event = nall::function<bool()>();
	else
		SNES::smp.step_event = bsnes_smp_trace_fn;
}
