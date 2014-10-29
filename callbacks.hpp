#pragma once

void (*cb_message)(const char* msg, size_t length);
short (*cb_get_input)(unsigned port, unsigned index, unsigned control);
void (*cb_notify_action_update)();
void (*cb_timer_tick)(uint32_t increment, uint32_t per_second);
const char* (*cb_get_firmware_path)();
const char* (*cb_get_base_path)();
time_t (*cb_get_time)();
time_t (*cb_get_randomseed)();
void (*cb_memory_read)(uint64_t addr, uint64_t value);
void (*cb_memory_write)(uint64_t addr, uint64_t value);
void (*cb_memory_execute)(uint64_t addr, uint64_t cpunum);
void (*cb_memory_trace)(uint64_t proc, const char* str, int insn);
void (*cb_submit_sound)(const int16_t* samples, size_t count, int stereo, double rate);
void (*cb_notify_latch)(const char** params);
void (*cb_submit_frame)(struct lsnes_core_framebuffer_info* fb, uint32_t fps_n, uint32_t fps_d);
void* (*cb_add_disasm)(struct lsnes_core_disassembler* disasm);
void (*cb_remove_disasm)(void* handle);
int (*cb_render_text)(struct lsnes_core_fontrender_req* req);

void record_callbacks(lsnes_core_enumerate_cores& arg)
{
	cb_message = arg.message;
	cb_get_input = arg.get_input;
	cb_notify_action_update = arg.notify_action_update;
	cb_timer_tick = arg.timer_tick;
	cb_get_firmware_path = arg.get_firmware_path;
	cb_get_base_path = arg.get_base_path;
	cb_get_time = arg.get_time;
	cb_get_randomseed = arg.get_randomseed;
	cb_memory_read = arg.memory_read;
	cb_memory_write = arg.memory_write;
	cb_memory_execute = arg.memory_execute;
	cb_memory_trace = arg.memory_trace;
	cb_submit_sound = arg.submit_sound;
	cb_notify_latch = arg.notify_latch;
	cb_submit_frame = arg.submit_frame;
	if(arg.emu_flags1 >= 1) {
		cb_add_disasm = arg.add_disasm;
		cb_remove_disasm = arg.remove_disasm;
	}
	if(arg.emu_flags1 >= 2) {
		cb_render_text = arg.render_text;
	}
}
