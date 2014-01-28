/*************************************************************************
 * Copyright (C) 2011-2013 by Ilari Liusvaara                            *
 *                                                                       *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/
#include "lsnes.hpp"
#include <sstream>
#include <map>
#include <string>
#include <cctype>
#include <vector>
#include <fstream>
#include <climits>
#include "core/audioapi.hpp"
#include "core/misc.hpp"
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "interface/setting.hpp"
#include "interface/callbacks.hpp"
#include "library/framebuffer-pixfmt-lrgb.hpp"
#include "library/hex.hpp"
#include "library/string.hpp"
#include "library/controller-data.hpp"
#include "library/framebuffer.hpp"
#include "library/lua-base.hpp"
#include "lua/internal.hpp"
#ifdef BSNES_HAS_DEBUGGER
#define DEBUGGER
#endif
#include <snes/snes.hpp>
#include <gameboy/gameboy.hpp>
#include LIBSNES_INCLUDE_FILE

#define DURATION_NTSC_FRAME 357366
#define DURATION_NTSC_FIELD 357368
#define DURATION_PAL_FRAME 425568
#define DURATION_PAL_FIELD 425568
#define ROM_TYPE_NONE 0
#define ROM_TYPE_SNES 1
#define ROM_TYPE_BSX 2
#define ROM_TYPE_BSXSLOTTED 3
#define ROM_TYPE_SUFAMITURBO 4
#define ROM_TYPE_SGB 5

#define ADDR_KIND_ALL -1
#define ADDR_KIND_NONE -2

namespace
{
	bool p1disable = false;
	bool do_hreset_flag = false;
	long do_reset_flag = -1;
	bool support_hreset = false;
	bool support_dreset = false;
	bool save_every_frame = false;
	bool have_saved_this_frame = false;
	int16_t blanksound[1070] = {0};
	int16_t soundbuf[8192] = {0};
	size_t soundbuf_fill = 0;
	bool last_hires = false;
	bool last_interlace = false;
	bool last_PAL = false;
	bool disable_breakpoints = false;
	uint64_t trace_counter;
	bool trace_cpu_enable;
	bool trace_smp_enable;
	SNES::Interface* old;
	bool stepping_into_save;
	bool video_refresh_done;
	bool forced_hook = false;
	std::map<int16_t, std::pair<uint64_t, uint64_t>> ptrmap;
	uint32_t cover_fbmem[512 * 448];
	//Delay reset.
	unsigned long long delayreset_cycles_run;
	unsigned long long delayreset_cycles_target;

	//Framebuffer.
	struct framebuffer::info cover_fbinfo = {
		&framebuffer::pixfmt_lrgb,		//Format.
		(char*)cover_fbmem,		//Memory.
		512, 448, 2048,			//Physical size.
		512, 448, 2048,			//Logical size.
		0, 0				//Offset.
	};

	struct interface_device_reg snes_registers[] = {
		{"pbpc", []() -> uint64_t { return SNES::cpu.regs.pc; }, [](uint64_t v) { SNES::cpu.regs.pc = v; }},
		{"pb", []() -> uint64_t { return SNES::cpu.regs.pc >> 16; },
			[](uint64_t v) { SNES::cpu.regs.pc = (v << 16) | (SNES::cpu.regs.pc & 0xFFFF); }},
		{"pc", []() -> uint64_t { return SNES::cpu.regs.pc & 0xFFFF; },
			[](uint64_t v) { SNES::cpu.regs.pc = (v & 0xFFFF) | (SNES::cpu.regs.pc & ~0xFFFF); }},
		{"r0", []() -> uint64_t { return SNES::cpu.regs.r[0]; }, [](uint64_t v) { SNES::cpu.regs.r[0] = v; }},
		{"r1", []() -> uint64_t { return SNES::cpu.regs.r[1]; }, [](uint64_t v) { SNES::cpu.regs.r[1] = v; }},
		{"r2", []() -> uint64_t { return SNES::cpu.regs.r[2]; }, [](uint64_t v) { SNES::cpu.regs.r[2] = v; }},
		{"r3", []() -> uint64_t { return SNES::cpu.regs.r[3]; }, [](uint64_t v) { SNES::cpu.regs.r[3] = v; }},
		{"r4", []() -> uint64_t { return SNES::cpu.regs.r[4]; }, [](uint64_t v) { SNES::cpu.regs.r[4] = v; }},
		{"r5", []() -> uint64_t { return SNES::cpu.regs.r[5]; }, [](uint64_t v) { SNES::cpu.regs.r[5] = v; }},
		{"a", []() -> uint64_t { return SNES::cpu.regs.a; }, [](uint64_t v) { SNES::cpu.regs.a = v; }},
		{"x", []() -> uint64_t { return SNES::cpu.regs.x; }, [](uint64_t v) { SNES::cpu.regs.x = v; }},
		{"y", []() -> uint64_t { return SNES::cpu.regs.y; }, [](uint64_t v) { SNES::cpu.regs.y = v; }},
		{"z", []() -> uint64_t { return SNES::cpu.regs.z; }, [](uint64_t v) { SNES::cpu.regs.z = v; }},
		{"s", []() -> uint64_t { return SNES::cpu.regs.s; }, [](uint64_t v) { SNES::cpu.regs.s = v; }},
		{"d", []() -> uint64_t { return SNES::cpu.regs.d; }, [](uint64_t v) { SNES::cpu.regs.d = v; }},
		{"db", []() -> uint64_t { return SNES::cpu.regs.db; }, [](uint64_t v) { SNES::cpu.regs.db = v; }},
		{"p", []() -> uint64_t { return SNES::cpu.regs.p; }, [](uint64_t v) { SNES::cpu.regs.p = v; }},
		{"e", []() -> uint64_t { return SNES::cpu.regs.e; }, [](uint64_t v) { SNES::cpu.regs.e = v; }},
		{"irq", []() -> uint64_t { return SNES::cpu.regs.irq; }, [](uint64_t v) { SNES::cpu.regs.irq = v; }},
		{"wai", []() -> uint64_t { return SNES::cpu.regs.wai; }, [](uint64_t v) { SNES::cpu.regs.wai = v; }},
		{"mdr", []() -> uint64_t { return SNES::cpu.regs.mdr; }, [](uint64_t v) { SNES::cpu.regs.mdr = v; }},
		{"vector", []() -> uint64_t { return SNES::cpu.regs.vector; },
			[](uint64_t v) { SNES::cpu.regs.vector = v; }},
		{"aa", []() -> uint64_t { return SNES::cpu.aa; }, [](uint64_t v) { SNES::cpu.aa = v; }},
		{"rd", []() -> uint64_t { return SNES::cpu.rd; }, [](uint64_t v) { SNES::cpu.rd = v; }},
		{"sp", []() -> uint64_t { return SNES::cpu.sp; }, [](uint64_t v) { SNES::cpu.sp = v; }},
		{"dp", []() -> uint64_t { return SNES::cpu.dp; }, [](uint64_t v) { SNES::cpu.dp = v; }},
		{"p_n", []() -> uint64_t { return SNES::cpu.regs.p.n; }, [](uint64_t v) { SNES::cpu.regs.p.n = v; },
			true},
		{"p_v", []() -> uint64_t { return SNES::cpu.regs.p.v; }, [](uint64_t v) { SNES::cpu.regs.p.v = v; },
			true},
		{"p_m", []() -> uint64_t { return SNES::cpu.regs.p.m; }, [](uint64_t v) { SNES::cpu.regs.p.m = v; },
			true},
		{"p_x", []() -> uint64_t { return SNES::cpu.regs.p.x; }, [](uint64_t v) { SNES::cpu.regs.p.x = v; },
			true},
		{"p_d", []() -> uint64_t { return SNES::cpu.regs.p.d; }, [](uint64_t v) { SNES::cpu.regs.p.d = v; },
			true},
		{"p_i", []() -> uint64_t { return SNES::cpu.regs.p.i; }, [](uint64_t v) { SNES::cpu.regs.p.i = v; },
			true},
		{"p_z", []() -> uint64_t { return SNES::cpu.regs.p.z; }, [](uint64_t v) { SNES::cpu.regs.p.z = v; },
			true},
		{"p_c", []() -> uint64_t { return SNES::cpu.regs.p.c; }, [](uint64_t v) { SNES::cpu.regs.p.c = v; },
			true},
#ifdef BSNES_IS_COMPAT
		{"ppu_display_disabled", []() -> uint64_t { return SNES::ppu.regs.display_disabled; },
			[](uint64_t v) { SNES::ppu.regs.display_disabled = v; }, true},
		{"ppu_oam_priority", []() -> uint64_t { return SNES::ppu.regs.oam_priority; },
			[](uint64_t v) { SNES::ppu.regs.oam_priority = v; }, true},
		{"ppu_bg_tilesize[0]", []() -> uint64_t { return SNES::ppu.regs.bg_tilesize[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tilesize[0] = v; }, true},
		{"ppu_bg_tilesize[1]", []() -> uint64_t { return SNES::ppu.regs.bg_tilesize[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tilesize[1] = v; }, true},
		{"ppu_bg_tilesize[2]", []() -> uint64_t { return SNES::ppu.regs.bg_tilesize[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tilesize[2] = v; }, true},
		{"ppu_bg_tilesize[3]", []() -> uint64_t { return SNES::ppu.regs.bg_tilesize[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tilesize[3] = v; }, true},
		{"ppu_bg3_priority", []() -> uint64_t { return SNES::ppu.regs.bg3_priority; },
			[](uint64_t v) { SNES::ppu.regs.bg3_priority = v; }, true},
		{"ppu_mosaic_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.mosaic_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.mosaic_enabled[0] = v; }, true},
		{"ppu_mosaic_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.mosaic_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.mosaic_enabled[1] = v; }, true},
		{"ppu_mosaic_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.mosaic_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.mosaic_enabled[2] = v; }, true},
		{"ppu_mosaic_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.mosaic_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.mosaic_enabled[3] = v; }, true},
		{"ppu_vram_incmode", []() -> uint64_t { return SNES::ppu.regs.vram_incmode; },
			[](uint64_t v) { SNES::ppu.regs.vram_incmode = v; }, true},
		{"ppu_mode7_vflip", []() -> uint64_t { return SNES::ppu.regs.mode7_vflip; },
			[](uint64_t v) { SNES::ppu.regs.mode7_vflip = v; }, true},
		{"ppu_mode7_hflip", []() -> uint64_t { return SNES::ppu.regs.mode7_hflip; },
			[](uint64_t v) { SNES::ppu.regs.mode7_hflip = v; }, true},
		{"ppu_window1_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.window1_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.window1_enabled[0] = v; }, true},
		{"ppu_window1_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.window1_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.window1_enabled[1] = v; }, true},
		{"ppu_window1_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.window1_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.window1_enabled[2] = v; }, true},
		{"ppu_window1_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.window1_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.window1_enabled[3] = v; }, true},
		{"ppu_window1_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.window1_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.window1_enabled[4] = v; }, true},
		{"ppu_window1_enabled[5]", []() -> uint64_t { return SNES::ppu.regs.window1_enabled[5]; },
			[](uint64_t v) { SNES::ppu.regs.window1_enabled[5] = v; }, true},
		{"ppu_window1_invert[0]", []() -> uint64_t { return SNES::ppu.regs.window1_invert[0]; },
			[](uint64_t v) { SNES::ppu.regs.window1_invert[0] = v; }, true},
		{"ppu_window1_invert[1]", []() -> uint64_t { return SNES::ppu.regs.window1_invert[1]; },
			[](uint64_t v) { SNES::ppu.regs.window1_invert[1] = v; }, true},
		{"ppu_window1_invert[2]", []() -> uint64_t { return SNES::ppu.regs.window1_invert[2]; },
			[](uint64_t v) { SNES::ppu.regs.window1_invert[2] = v; }, true},
		{"ppu_window1_invert[3]", []() -> uint64_t { return SNES::ppu.regs.window1_invert[3]; },
			[](uint64_t v) { SNES::ppu.regs.window1_invert[3] = v; }, true},
		{"ppu_window1_invert[4]", []() -> uint64_t { return SNES::ppu.regs.window1_invert[4]; },
			[](uint64_t v) { SNES::ppu.regs.window1_invert[4] = v; }, true},
		{"ppu_window1_invert[5]", []() -> uint64_t { return SNES::ppu.regs.window1_invert[5]; },
			[](uint64_t v) { SNES::ppu.regs.window1_invert[5] = v; }, true},
		{"ppu_window2_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.window2_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.window2_enabled[0] = v; }, true},
		{"ppu_window2_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.window2_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.window2_enabled[1] = v; }, true},
		{"ppu_window2_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.window2_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.window2_enabled[2] = v; }, true},
		{"ppu_window2_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.window2_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.window2_enabled[3] = v; }, true},
		{"ppu_window2_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.window2_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.window2_enabled[4] = v; }, true},
		{"ppu_window2_enabled[5]", []() -> uint64_t { return SNES::ppu.regs.window2_enabled[5]; },
			[](uint64_t v) { SNES::ppu.regs.window2_enabled[5] = v; }, true},
		{"ppu_window2_invert[0]", []() -> uint64_t { return SNES::ppu.regs.window2_invert[0]; },
			[](uint64_t v) { SNES::ppu.regs.window2_invert[0] = v; }, true},
		{"ppu_window2_invert[1]", []() -> uint64_t { return SNES::ppu.regs.window2_invert[1]; },
			[](uint64_t v) { SNES::ppu.regs.window2_invert[1] = v; }, true},
		{"ppu_window2_invert[2]", []() -> uint64_t { return SNES::ppu.regs.window2_invert[2]; },
			[](uint64_t v) { SNES::ppu.regs.window2_invert[2] = v; }, true},
		{"ppu_window2_invert[3]", []() -> uint64_t { return SNES::ppu.regs.window2_invert[3]; },
			[](uint64_t v) { SNES::ppu.regs.window2_invert[3] = v; }, true},
		{"ppu_window2_invert[4]", []() -> uint64_t { return SNES::ppu.regs.window2_invert[4]; },
			[](uint64_t v) { SNES::ppu.regs.window2_invert[4] = v; }, true},
		{"ppu_window2_invert[5]", []() -> uint64_t { return SNES::ppu.regs.window2_invert[5]; },
			[](uint64_t v) { SNES::ppu.regs.window2_invert[5] = v; }, true},
		{"ppu_bg_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.bg_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_enabled[0] = v; }, true},
		{"ppu_bg_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.bg_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_enabled[1] = v; }, true},
		{"ppu_bg_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.bg_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_enabled[2] = v; }, true},
		{"ppu_bg_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.bg_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_enabled[3] = v; }, true},
		{"ppu_bg_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.bg_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.bg_enabled[4] = v; }, true},
		{"ppu_bgsub_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.bgsub_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.bgsub_enabled[0] = v; }, true},
		{"ppu_bgsub_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.bgsub_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.bgsub_enabled[1] = v; }, true},
		{"ppu_bgsub_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.bgsub_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.bgsub_enabled[2] = v; }, true},
		{"ppu_bgsub_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.bgsub_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.bgsub_enabled[3] = v; }, true},
		{"ppu_bgsub_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.bgsub_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.bgsub_enabled[4] = v; }, true},
		{"ppu_window_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.window_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.window_enabled[0] = v; }, true},
		{"ppu_window_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.window_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.window_enabled[1] = v; }, true},
		{"ppu_window_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.window_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.window_enabled[2] = v; }, true},
		{"ppu_window_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.window_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.window_enabled[3] = v; }, true},
		{"ppu_window_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.window_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.window_enabled[4] = v; }, true},
		{"ppu_sub_window_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.sub_window_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.sub_window_enabled[0] = v; }, true},
		{"ppu_sub_window_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.sub_window_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.sub_window_enabled[1] = v; }, true},
		{"ppu_sub_window_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.sub_window_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.sub_window_enabled[2] = v; }, true},
		{"ppu_sub_window_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.sub_window_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.sub_window_enabled[3] = v; }, true},
		{"ppu_sub_window_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.sub_window_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.sub_window_enabled[4] = v; }, true},
		{"ppu_addsub_mode", []() -> uint64_t { return SNES::ppu.regs.addsub_mode; },
			[](uint64_t v) { SNES::ppu.regs.addsub_mode = v; }, true},
		{"ppu_direct_color", []() -> uint64_t { return SNES::ppu.regs.direct_color; },
			[](uint64_t v) { SNES::ppu.regs.direct_color = v; }, true},
		{"ppu_color_mode", []() -> uint64_t { return SNES::ppu.regs.color_mode; },
			[](uint64_t v) { SNES::ppu.regs.color_mode = v; }, true},
		{"ppu_color_halve", []() -> uint64_t { return SNES::ppu.regs.color_halve; },
			[](uint64_t v) { SNES::ppu.regs.color_halve = v; }, true},
		{"ppu_color_enabled[0]", []() -> uint64_t { return SNES::ppu.regs.color_enabled[0]; },
			[](uint64_t v) { SNES::ppu.regs.color_enabled[0] = v; }, true},
		{"ppu_color_enabled[1]", []() -> uint64_t { return SNES::ppu.regs.color_enabled[1]; },
			[](uint64_t v) { SNES::ppu.regs.color_enabled[1] = v; }, true},
		{"ppu_color_enabled[2]", []() -> uint64_t { return SNES::ppu.regs.color_enabled[2]; },
			[](uint64_t v) { SNES::ppu.regs.color_enabled[2] = v; }, true},
		{"ppu_color_enabled[3]", []() -> uint64_t { return SNES::ppu.regs.color_enabled[3]; },
			[](uint64_t v) { SNES::ppu.regs.color_enabled[3] = v; }, true},
		{"ppu_color_enabled[4]", []() -> uint64_t { return SNES::ppu.regs.color_enabled[4]; },
			[](uint64_t v) { SNES::ppu.regs.color_enabled[4] = v; }, true},
		{"ppu_color_enabled[5]", []() -> uint64_t { return SNES::ppu.regs.color_enabled[5]; },
			[](uint64_t v) { SNES::ppu.regs.color_enabled[5] = v; }, true},
		{"ppu_mode7_extbg", []() -> uint64_t { return SNES::ppu.regs.mode7_extbg; },
			[](uint64_t v) { SNES::ppu.regs.mode7_extbg = v; }, true},
		{"ppu_pseudo_hires", []() -> uint64_t { return SNES::ppu.regs.pseudo_hires; },
			[](uint64_t v) { SNES::ppu.regs.pseudo_hires = v; }, true},
		{"ppu_overscan", []() -> uint64_t { return SNES::ppu.regs.overscan; },
			[](uint64_t v) { SNES::ppu.regs.overscan = v; }, true},
		{"ppu_oam_interlace", []() -> uint64_t { return SNES::ppu.regs.oam_interlace; },
			[](uint64_t v) { SNES::ppu.regs.oam_interlace = v; }, true},
		{"ppu_interlace", []() -> uint64_t { return SNES::ppu.regs.interlace; },
			[](uint64_t v) { SNES::ppu.regs.interlace = v; }, true},
		{"ppu_latch_hcounter", []() -> uint64_t { return SNES::ppu.regs.latch_hcounter; },
			[](uint64_t v) { SNES::ppu.regs.latch_hcounter = v; }, true},
		{"ppu_latch_vcounter", []() -> uint64_t { return SNES::ppu.regs.latch_vcounter; },
			[](uint64_t v) { SNES::ppu.regs.latch_vcounter = v; }, true},
		{"ppu_counters_latched", []() -> uint64_t { return SNES::ppu.regs.counters_latched; },
			[](uint64_t v) { SNES::ppu.regs.counters_latched = v; }, true},
		{"ppu_time_over", []() -> uint64_t { return SNES::ppu.regs.time_over; },
			[](uint64_t v) { SNES::ppu.regs.time_over = v; }, true},
		{"ppu_range_over", []() -> uint64_t { return SNES::ppu.regs.range_over; },
			[](uint64_t v) { SNES::ppu.regs.range_over = v; }, true},
		{"ppu_ppu1_mdr", []() -> uint64_t { return SNES::ppu.regs.ppu1_mdr; },
			[](uint64_t v) { SNES::ppu.regs.ppu1_mdr = v; }},
		{"ppu_ppu2_mdr", []() -> uint64_t { return SNES::ppu.regs.ppu2_mdr; },
			[](uint64_t v) { SNES::ppu.regs.ppu2_mdr = v; }},
		{"ppu_bg_y[0]", []() -> uint64_t { return SNES::ppu.regs.bg_y[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_y[0] = v; }},
		{"ppu_bg_y[1]", []() -> uint64_t { return SNES::ppu.regs.bg_y[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_y[1] = v; }},
		{"ppu_bg_y[2]", []() -> uint64_t { return SNES::ppu.regs.bg_y[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_y[2] = v; }},
		{"ppu_bg_y[3]", []() -> uint64_t { return SNES::ppu.regs.bg_y[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_y[3] = v; }},
		{"ppu_ioamaddr", []() -> uint64_t { return SNES::ppu.regs.ioamaddr; },
			[](uint64_t v) { SNES::ppu.regs.ioamaddr = v; }},
		{"ppu_icgramaddr", []() -> uint64_t { return SNES::ppu.regs.icgramaddr; },
			[](uint64_t v) { SNES::ppu.regs.icgramaddr = v; }},
		{"ppu_display_brightness", []() -> uint64_t { return SNES::ppu.regs.display_brightness; },
			[](uint64_t v) { SNES::ppu.regs.display_brightness = v; }},
		{"ppu_oam_basesize", []() -> uint64_t { return SNES::ppu.regs.oam_basesize; },
			[](uint64_t v) { SNES::ppu.regs.oam_basesize = v; }},
		{"ppu_oam_nameselect", []() -> uint64_t { return SNES::ppu.regs.oam_nameselect; },
			[](uint64_t v) { SNES::ppu.regs.oam_nameselect = v; }},
		{"ppu_oam_tdaddr", []() -> uint64_t { return SNES::ppu.regs.oam_tdaddr; },
			[](uint64_t v) { SNES::ppu.regs.oam_tdaddr = v; }},
		{"ppu_oam_baseaddr", []() -> uint64_t { return SNES::ppu.regs.oam_baseaddr; },
			[](uint64_t v) { SNES::ppu.regs.oam_baseaddr = v; }},
		{"ppu_oam_addr", []() -> uint64_t { return SNES::ppu.regs.oam_addr; },
			[](uint64_t v) { SNES::ppu.regs.oam_addr = v; }},
		{"ppu_oam_firstsprite", []() -> uint64_t { return SNES::ppu.regs.oam_firstsprite; },
			[](uint64_t v) { SNES::ppu.regs.oam_firstsprite = v; }},
		{"ppu_oam_latchdata", []() -> uint64_t { return SNES::ppu.regs.oam_latchdata; },
			[](uint64_t v) { SNES::ppu.regs.oam_latchdata = v; }},
		{"ppu_bg_mode", []() -> uint64_t { return SNES::ppu.regs.bg_mode; },
			[](uint64_t v) { SNES::ppu.regs.bg_mode = v; }},
		{"ppu_mosaic_size", []() -> uint64_t { return SNES::ppu.regs.mosaic_size; },
			[](uint64_t v) { SNES::ppu.regs.mosaic_size = v; }},
		{"ppu_mosaic_countdown", []() -> uint64_t { return SNES::ppu.regs.mosaic_countdown; },
			[](uint64_t v) { SNES::ppu.regs.mosaic_countdown = v; }},
		{"ppu_bg_scaddr[0]", []() -> uint64_t { return SNES::ppu.regs.bg_scaddr[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scaddr[0] = v; }},
		{"ppu_bg_scaddr[1]", []() -> uint64_t { return SNES::ppu.regs.bg_scaddr[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scaddr[1] = v; }},
		{"ppu_bg_scaddr[2]", []() -> uint64_t { return SNES::ppu.regs.bg_scaddr[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scaddr[2] = v; }},
		{"ppu_bg_scaddr[3]", []() -> uint64_t { return SNES::ppu.regs.bg_scaddr[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scaddr[3] = v; }},
		{"ppu_bg_scsize[0]", []() -> uint64_t { return SNES::ppu.regs.bg_scsize[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scsize[0] = v; }},
		{"ppu_bg_scsize[1]", []() -> uint64_t { return SNES::ppu.regs.bg_scsize[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scsize[1] = v; }},
		{"ppu_bg_scsize[2]", []() -> uint64_t { return SNES::ppu.regs.bg_scsize[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scsize[2] = v; }},
		{"ppu_bg_scsize[3]", []() -> uint64_t { return SNES::ppu.regs.bg_scsize[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_scsize[3] = v; }},
		{"ppu_bg_tdaddr[0]", []() -> uint64_t { return SNES::ppu.regs.bg_tdaddr[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tdaddr[0] = v; }},
		{"ppu_bg_tdaddr[1]", []() -> uint64_t { return SNES::ppu.regs.bg_tdaddr[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tdaddr[1] = v; }},
		{"ppu_bg_tdaddr[2]", []() -> uint64_t { return SNES::ppu.regs.bg_tdaddr[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tdaddr[2] = v; }},
		{"ppu_bg_tdaddr[3]", []() -> uint64_t { return SNES::ppu.regs.bg_tdaddr[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_tdaddr[3] = v; }},
		{"ppu_bg_ofslatch", []() -> uint64_t { return SNES::ppu.regs.bg_ofslatch; },
			[](uint64_t v) { SNES::ppu.regs.bg_ofslatch = v; }},
		{"ppu_m7_hofs", []() -> uint64_t { return SNES::ppu.regs.m7_hofs; },
			[](uint64_t v) { SNES::ppu.regs.m7_hofs = v; }},
		{"ppu_m7_vofs", []() -> uint64_t { return SNES::ppu.regs.m7_vofs; },
			[](uint64_t v) { SNES::ppu.regs.m7_vofs = v; }},
		{"ppu_bg_hofs[0]", []() -> uint64_t { return SNES::ppu.regs.bg_hofs[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_hofs[0] = v; }},
		{"ppu_bg_hofs[1]", []() -> uint64_t { return SNES::ppu.regs.bg_hofs[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_hofs[1] = v; }},
		{"ppu_bg_hofs[2]", []() -> uint64_t { return SNES::ppu.regs.bg_hofs[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_hofs[2] = v; }},
		{"ppu_bg_hofs[3]", []() -> uint64_t { return SNES::ppu.regs.bg_hofs[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_hofs[3] = v; }},
		{"ppu_bg_vofs[0]", []() -> uint64_t { return SNES::ppu.regs.bg_vofs[0]; },
			[](uint64_t v) { SNES::ppu.regs.bg_vofs[0] = v; }},
		{"ppu_bg_vofs[1]", []() -> uint64_t { return SNES::ppu.regs.bg_vofs[1]; },
			[](uint64_t v) { SNES::ppu.regs.bg_vofs[1] = v; }},
		{"ppu_bg_vofs[2]", []() -> uint64_t { return SNES::ppu.regs.bg_vofs[2]; },
			[](uint64_t v) { SNES::ppu.regs.bg_vofs[2] = v; }},
		{"ppu_bg_vofs[3]", []() -> uint64_t { return SNES::ppu.regs.bg_vofs[3]; },
			[](uint64_t v) { SNES::ppu.regs.bg_vofs[3] = v; }},
		{"ppu_vram_mapping", []() -> uint64_t { return SNES::ppu.regs.vram_mapping; },
			[](uint64_t v) { SNES::ppu.regs.vram_mapping = v; }},
		{"ppu_vram_incsize", []() -> uint64_t { return SNES::ppu.regs.vram_incsize; },
			[](uint64_t v) { SNES::ppu.regs.vram_incsize = v; }},
		{"ppu_vram_addr", []() -> uint64_t { return SNES::ppu.regs.vram_addr; },
			[](uint64_t v) { SNES::ppu.regs.vram_addr = v; }},
		{"ppu_mode7_repeat", []() -> uint64_t { return SNES::ppu.regs.mode7_repeat; },
			[](uint64_t v) { SNES::ppu.regs.mode7_repeat = v; }},
		{"ppu_m7_latch", []() -> uint64_t { return SNES::ppu.regs.m7_latch; },
			[](uint64_t v) { SNES::ppu.regs.m7_latch = v; }},
		{"ppu_m7a", []() -> uint64_t { return SNES::ppu.regs.m7a; },
			[](uint64_t v) { SNES::ppu.regs.m7a = v; }},
		{"ppu_m7b", []() -> uint64_t { return SNES::ppu.regs.m7b; },
			[](uint64_t v) { SNES::ppu.regs.m7b = v; }},
		{"ppu_m7c", []() -> uint64_t { return SNES::ppu.regs.m7c; },
			[](uint64_t v) { SNES::ppu.regs.m7c = v; }},
		{"ppu_m7d", []() -> uint64_t { return SNES::ppu.regs.m7d; },
			[](uint64_t v) { SNES::ppu.regs.m7d = v; }},
		{"ppu_m7x", []() -> uint64_t { return SNES::ppu.regs.m7x; },
			[](uint64_t v) { SNES::ppu.regs.m7x = v; }},
		{"ppu_m7y", []() -> uint64_t { return SNES::ppu.regs.m7y; },
			[](uint64_t v) { SNES::ppu.regs.m7y = v; }},
		{"ppu_cgram_addr", []() -> uint64_t { return SNES::ppu.regs.cgram_addr; },
			[](uint64_t v) { SNES::ppu.regs.cgram_addr = v; }},
		{"ppu_cgram_latchdata", []() -> uint64_t { return SNES::ppu.regs.cgram_latchdata; },
			[](uint64_t v) { SNES::ppu.regs.cgram_latchdata = v; }},
		{"ppu_window1_left", []() -> uint64_t { return SNES::ppu.regs.window1_left; },
			[](uint64_t v) { SNES::ppu.regs.window1_left = v; }},
		{"ppu_window1_right", []() -> uint64_t { return SNES::ppu.regs.window1_right; },
			[](uint64_t v) { SNES::ppu.regs.window1_right = v; }},
		{"ppu_window2_left", []() -> uint64_t { return SNES::ppu.regs.window2_left; },
			[](uint64_t v) { SNES::ppu.regs.window2_left = v; }},
		{"ppu_window2_right", []() -> uint64_t { return SNES::ppu.regs.window2_right; },
			[](uint64_t v) { SNES::ppu.regs.window2_right = v; }},
		{"ppu_window_mask[0]", []() -> uint64_t { return SNES::ppu.regs.window_mask[0]; },
			[](uint64_t v) { SNES::ppu.regs.window_mask[0] = v; }},
		{"ppu_window_mask[1]", []() -> uint64_t { return SNES::ppu.regs.window_mask[1]; },
			[](uint64_t v) { SNES::ppu.regs.window_mask[1] = v; }},
		{"ppu_window_mask[2]", []() -> uint64_t { return SNES::ppu.regs.window_mask[2]; },
			[](uint64_t v) { SNES::ppu.regs.window_mask[2] = v; }},
		{"ppu_window_mask[3]", []() -> uint64_t { return SNES::ppu.regs.window_mask[3]; },
			[](uint64_t v) { SNES::ppu.regs.window_mask[3] = v; }},
		{"ppu_window_mask[4]", []() -> uint64_t { return SNES::ppu.regs.window_mask[4]; },
			[](uint64_t v) { SNES::ppu.regs.window_mask[4] = v; }},
		{"ppu_window_mask[5]", []() -> uint64_t { return SNES::ppu.regs.window_mask[5]; },
			[](uint64_t v) { SNES::ppu.regs.window_mask[5] = v; }},
		{"ppu_color_mask", []() -> uint64_t { return SNES::ppu.regs.color_mask; },
			[](uint64_t v) { SNES::ppu.regs.color_mask = v; }},
		{"ppu_colorsub_mask", []() -> uint64_t { return SNES::ppu.regs.colorsub_mask; },
			[](uint64_t v) { SNES::ppu.regs.colorsub_mask = v; }},
		{"ppu_color_r", []() -> uint64_t { return SNES::ppu.regs.color_r; },
			[](uint64_t v) { SNES::ppu.regs.color_r = v; }},
		{"ppu_color_g", []() -> uint64_t { return SNES::ppu.regs.color_g; },
			[](uint64_t v) { SNES::ppu.regs.color_g = v; }},
		{"ppu_color_b", []() -> uint64_t { return SNES::ppu.regs.color_b; },
			[](uint64_t v) { SNES::ppu.regs.color_b = v; }},
		{"ppu_color_rgb", []() -> uint64_t { return SNES::ppu.regs.color_rgb; },
			[](uint64_t v) { SNES::ppu.regs.color_rgb = v; }},
		{"ppu_scanlines", []() -> uint64_t { return SNES::ppu.regs.scanlines; },
			[](uint64_t v) { SNES::ppu.regs.scanlines = v; }},
		{"ppu_hcounter", []() -> uint64_t { return SNES::ppu.regs.hcounter; },
			[](uint64_t v) { SNES::ppu.regs.hcounter = v; }},
		{"ppu_vcounter", []() -> uint64_t { return SNES::ppu.regs.vcounter; },
			[](uint64_t v) { SNES::ppu.regs.vcounter = v; }},
		{"ppu_vram_readbuffer", []() -> uint64_t { return SNES::ppu.regs.vram_readbuffer; },
			[](uint64_t v) { SNES::ppu.regs.vram_readbuffer = v; }},
		{"ppu_oam_itemcount", []() -> uint64_t { return SNES::ppu.regs.oam_itemcount; },
			[](uint64_t v) { SNES::ppu.regs.oam_itemcount = v; }},
		{"ppu_oam_tilecount", []() -> uint64_t { return SNES::ppu.regs.oam_tilecount; },
			[](uint64_t v) { SNES::ppu.regs.oam_tilecount = v; }},
#endif
		//TODO: SMP registers, DSP registers, chip registers.
		{NULL, NULL, NULL}
	};

#include "ports.inc"

	core_region region_auto{{"autodetect", "Autodetect", 1, 0, true, {178683, 10738636}, {0,1,2}}};
	core_region region_pal{{"pal", "PAL", 0, 2, false, {6448, 322445}, {2}}};
	core_region region_ntsc{{"ntsc", "NTSC", 0, 1, false, {178683, 10738636}, {1}}};

	std::vector<core_setting_value_param> boolean_values = {{"0", "False", 0}, {"1", "True", 1}};
	core_setting_group bsnes_settings = {
		{"port1", "Port 1 Type", "gamepad", {
			{"none", "None", 0},
			{"gamepad", "Gamepad", 1},
			{"gamepad16", "Gamepad (16-button)", 2},
			{"ygamepad16", "Y-cabled gamepad (16-button)", 9},
			{"multitap", "Multitap", 3},
			{"multitap16", "Multitap (16-button)", 4},
			{"mouse", "Mouse", 5}
		}},
		{"port2", "Port 2 Type", "none", {
			{"none", "None", 0},
			{"gamepad", "Gamepad", 1},
			{"gamepad16", "Gamepad (16-button)", 2},
			{"ygamepad16", "Y-cabled gamepad (16-button)", 9},
			{"multitap", "Multitap", 3},
			{"multitap16", "Multitap (16-button)", 4},
			{"mouse", "Mouse", 5},
			{"superscope", "Super Scope", 8},
			{"justifier", "Justifier", 6},
			{"justifiers", "2 Justifiers", 7}
		}},
		{"hardreset", "Support hard resets", "0", boolean_values},
		{"saveevery", "Emulate saving each frame", "0", boolean_values},
		{"radominit", "Random initial state", "0", boolean_values},
		{"compact", "Don't support delayed resets", "0", boolean_values},
#ifdef BSNES_SUPPORTS_ALT_TIMINGS
		{"alttimings", "Alternate poll timings", "0", boolean_values},
#endif
	};

	////////////////// PORTS COMMON ///////////////////
	port_type* index_to_ptype[] = {
		&none, &gamepad, &gamepad16, &multitap, &multitap16, &mouse, &justifier, &justifiers, &superscope,
		&ygamepad16
	};
	unsigned index_to_bsnes_type[] = {
		SNES_DEVICE_NONE, SNES_DEVICE_JOYPAD, SNES_DEVICE_JOYPAD, SNES_DEVICE_MULTITAP, SNES_DEVICE_MULTITAP,
		SNES_DEVICE_MOUSE, SNES_DEVICE_JUSTIFIER, SNES_DEVICE_JUSTIFIERS, SNES_DEVICE_SUPER_SCOPE,
		SNES_DEVICE_JOYPAD
	};

	bool port_is_ycable[2];

	void snesdbg_on_break();
	void snesdbg_on_trace();
	std::pair<int, uint64_t> recognize_address(uint64_t addr);

	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	};

	void basic_init()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		static my_interfaced i;
		SNES::interface = &i;
	}

	core_type* internal_rom = NULL;

	template<bool(*T)(const char*,const unsigned char*, unsigned)>
	bool load_rom_X1(core_romimage* img)
	{
		return T(img[0].markup, img[0].data, img[0].size);
	}

	template<bool(*T)(const char*,const unsigned char*, unsigned, const char*,const unsigned char*, unsigned)>
	bool load_rom_X2(core_romimage* img)
	{
		return T(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size);
	}

	template<bool(*T)(const char*,const unsigned char*, unsigned, const char*,const unsigned char*, unsigned,
		const char*,const unsigned char*, unsigned)>
	bool load_rom_X3(core_romimage* img)
	{
		return T(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size,
			img[2].markup, img[2].data, img[2].size);
	}


	int load_rom(core_type* ctype, core_romimage* img, std::map<std::string, std::string>& settings,
		uint64_t secs, uint64_t subsecs, bool(*fun)(core_romimage*))
	{
		std::map<std::string, std::string> _settings = settings;
		bsnes_settings.fill_defaults(_settings);
		signed type1 = bsnes_settings.ivalue_to_index(_settings, "port1");
		signed type2 = bsnes_settings.ivalue_to_index(_settings, "port2");
		signed hreset = bsnes_settings.ivalue_to_index(_settings, "hardreset");
		signed compact = bsnes_settings.ivalue_to_index(_settings, "compact");
		signed esave = bsnes_settings.ivalue_to_index(_settings, "saveevery");
		signed irandom = bsnes_settings.ivalue_to_index(_settings, "radominit");
#ifdef BSNES_SUPPORTS_ALT_TIMINGS
		signed ialttimings = bsnes_settings.ivalue_to_index(_settings, "alttimings");
#endif

		basic_init();
		snes_term();
		snes_unload_cartridge();
		SNES::config.random = (irandom != 0);
		save_every_frame = (esave != 0);
		support_hreset = (hreset != 0 || compact != 0);
		support_dreset = (compact == 0);
		SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;
#ifdef BSNES_SUPPORTS_ALT_TIMINGS
		SNES::config.cpu.alt_poll_timings = (ialttimings != 0);
#endif
		bool r = fun(img);
		if(r) {
			internal_rom = ctype;
			snes_set_controller_port_device(false, index_to_bsnes_type[type1]);
			snes_set_controller_port_device(true, index_to_bsnes_type[type2]);
			port_is_ycable[0] = (type1 == 9);
			port_is_ycable[1] = (type2 == 9);
			have_saved_this_frame = false;
			do_reset_flag = -1;
			if(ecore_callbacks)
				ecore_callbacks->action_state_updated();
		}
		return r ? 0 : -1;
	}

	controller_set bsnes_controllerconfig(std::map<std::string, std::string>& settings)
	{
		std::map<std::string, std::string> _settings = settings;
		bsnes_settings.fill_defaults(_settings);
		signed type1 = bsnes_settings.ivalue_to_index(_settings, "port1");
		signed type2 = bsnes_settings.ivalue_to_index(_settings, "port2");
		signed hreset = bsnes_settings.ivalue_to_index(_settings, "hardreset");
		signed compact = bsnes_settings.ivalue_to_index(_settings, "compact");
		controller_set r;
		if(compact)
			r.ports.push_back(&psystem_compact);
		else if(hreset)
			r.ports.push_back(&psystem_hreset);
		else
			r.ports.push_back(&psystem);
		r.ports.push_back(index_to_ptype[type1]);
		r.ports.push_back(index_to_ptype[type2]);
		unsigned p1controllers = r.ports[1]->controller_info->controllers.size();
		unsigned p2controllers = r.ports[2]->controller_info->controllers.size();
		r.logical_map.resize(p1controllers + p2controllers);
		if(p1controllers == 4) {
			r.logical_map[0] = std::make_pair(1, 0);
			for(size_t j = 0; j < p2controllers; j++)
				r.logical_map[j + 1] = std::make_pair(2U, j);
			for(size_t j = 1; j < p1controllers; j++)
				r.logical_map[j + p2controllers] = std::make_pair(1U, j);
		} else {
			for(size_t j = 0; j < p1controllers; j++)
				r.logical_map[j] = std::make_pair(1, j);
			for(size_t j = 0; j < p2controllers; j++)
				r.logical_map[j + p1controllers] = std::make_pair(2U, j);
		}
		return r;
	}

	class my_interface : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			const char* _hint = hint;
			std::string _hint2 = _hint;
			std::string fwp = ecore_callbacks->get_firmware_path();
			regex_results r;
			std::string msubase = ecore_callbacks->get_base_path();
			if(regex_match(".*\\.sfc", msubase))
				msubase = msubase.substr(0, msubase.length() - 4);

			if(_hint2 == "msu1.rom" || _hint2 == ".msu") {
				//MSU-1 main ROM.
				std::string x = msubase + ".msu";
				messages << "MSU main data file: " << x << std::endl;
				return x.c_str();
			}
			if(r = regex("(track)?(-([0-9])+\\.pcm)", _hint2)) {
				//MSU track.
				std::string x = msubase + r[2];
				messages << "MSU track " << r[3] << "': " << x << std::endl;
				return x.c_str();
			}
			std::string finalpath = fwp + "/" + _hint2;
			return finalpath.c_str();
		}

		time_t currentTime()
		{
			return ecore_callbacks->get_time();
		}

		time_t randomSeed()
		{
			return ecore_callbacks->get_randomseed();
		}

		void notifyLatched()
		{
			std::list<std::string> dummy;
			ecore_callbacks->notify_latch(dummy);
		}

		void videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan);

		void audioSample(int16_t l_sample, int16_t r_sample)
		{
			uint16_t _l = l_sample;
			uint16_t _r = r_sample;
			soundbuf[soundbuf_fill++] = l_sample;
			soundbuf[soundbuf_fill++] = r_sample;
			//The SMP emits a sample every 768 ticks of its clock. Use this in order to keep track of
			//time.
			ecore_callbacks->timer_tick(768, SNES::system.apu_frequency());
		}

		int16_t inputPoll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
		{
			if(port_is_ycable[port ? 1 : 0]) {
				int16_t bit0 = ecore_callbacks->get_input(port ? 2 : 1, 0, id);
				int16_t bit1 = ecore_callbacks->get_input(port ? 2 : 1, 1, id);
				return bit0 + 2 * bit1;
			}
			int16_t offset = 0;
			//The superscope/justifier handling is nuts.
			if(port && SNES::input.port2) {
				SNES::SuperScope* ss = dynamic_cast<SNES::SuperScope*>(SNES::input.port2);
				SNES::Justifier* js = dynamic_cast<SNES::Justifier*>(SNES::input.port2);
				if(ss && index == 0) {
					if(id == 0)
						offset = ss->x;
					if(id == 1)
						offset = ss->y;
				}
				if(js && index == 0) {
					if(id == 0)
						offset = js->player1.x;
					if(id == 1)
						offset = js->player1.y;
				}
				if(js && js->chained && index == 1) {
					if(id == 0)
						offset = js->player2.x;
					if(id == 1)
						offset = js->player2.y;
				}
			}
			return ecore_callbacks->get_input(port ? 2 : 1, index, id) - offset;
		}
	} my_interface_obj;

	bool trace_fn()
	{
#ifdef BSNES_HAS_DEBUGGER
		if(trace_counter && !--trace_counter) {
			//Trace counter did transition 1->0. Call the hook.
			snesdbg_on_trace();
		}
		if(trace_cpu_enable) {
			char buffer[1024];
			SNES::cpu.disassemble_opcode(buffer, SNES::cpu.regs.pc);
			ecore_callbacks->memory_trace(0, buffer);
		}
		return false;
#endif
	}
	bool smp_trace_fn()
	{
#ifdef BSNES_HAS_DEBUGGER
		if(trace_smp_enable) {
			nall::string _disasm = SNES::smp.disassemble_opcode(SNES::smp.regs.pc);
			std::string disasm(_disasm, _disasm.length());
			ecore_callbacks->memory_trace(1, disasm.c_str());
		}
		return false;
#endif
	}
	bool delayreset_fn()
	{
		trace_fn();	//Call this also.
		if(delayreset_cycles_run == delayreset_cycles_target || video_refresh_done)
			return true;
		delayreset_cycles_run++;
		return false;
	}


	bool trace_enabled()
	{
		return (trace_counter || !!trace_cpu_enable);
	}

	void update_trace_hook_state()
	{
		if(forced_hook)
			return;
#ifdef BSNES_HAS_DEBUGGER
		if(!trace_enabled())
			SNES::cpu.step_event = nall::function<bool()>();
		else
			SNES::cpu.step_event = trace_fn;
		if(!trace_smp_enable)
			SNES::smp.step_event = nall::function<bool()>();
		else
			SNES::smp.step_event = smp_trace_fn;
#endif
	}

	std::string sram_name(const nall::string& _id, SNES::Cartridge::Slot slotname)
	{
		std::string id(_id, _id.length());
		//Fixup name change by bsnes v087...
		if(id == "bsx.ram")
			id = ".bss";
		if(id == "bsx.psram")
			id = ".bsp";
		if(id == "program.rtc")
			id = ".rtc";
		if(id == "upd96050.ram")
			id = ".dsp";
		if(id == "program.ram")
			id = ".srm";
		if(slotname == SNES::Cartridge::Slot::SufamiTurboA)
			return "slota." + id.substr(1);
		if(slotname == SNES::Cartridge::Slot::SufamiTurboB)
			return "slotb." + id.substr(1);
		return id.substr(1);
	}

	uint8_t snes_bus_iospace_rw(uint64_t offset, uint8_t data, bool write)
	{
		uint8_t val = 0;
		disable_breakpoints = true;
		if(write)
			SNES::bus.write(offset, data);
		else
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
			val = SNES::bus.read(offset, false);
#else
			val = SNES::bus.read(offset);
#endif
		disable_breakpoints = false;
		return val;
	}

	uint8_t ptrtable_iospace_rw(uint64_t offset, uint8_t data, bool write)
	{
		uint16_t entry = offset >> 4;
		if(!ptrmap.count(entry))
			return 0;
		uint64_t val = ((offset & 15) < 8) ? ptrmap[entry].first : ptrmap[entry].second;
		uint8_t byte = offset & 7;
		//These things are always little-endian.
		return (val >> (8 * byte));
	}

	void create_region(std::list<core_vma_info>& inf, const std::string& name, uint64_t base, uint64_t size,
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write)) throw(std::bad_alloc)
	{
		if(size == 0)
			return;
		core_vma_info i;
		i.name = name;
		i.base = base;
		i.size = size;
		i.endian = -1;
		i.iospace_rw = iospace_rw;
		inf.push_back(i);
	}

	void create_region(std::list<core_vma_info>& inf, const std::string& name, uint64_t base, uint8_t* memory,
		uint64_t size, bool readonly, bool native_endian = false) throw(std::bad_alloc)
	{
		if(size == 0)
			return;
		core_vma_info i;
		i.name = name;
		i.base = base;
		i.size = size;
		i.backing_ram = memory;
		i.readonly = readonly;
		i.endian = native_endian ? 0 : -1;
		i.volatile_flag = true;
		//SRAMs aren't volatile.
		for(unsigned j = 0; j < SNES::cartridge.nvram.size(); j++) {
			SNES::Cartridge::NonVolatileRAM& r = SNES::cartridge.nvram[j];
			if(r.data == memory)
				i.volatile_flag = false;
		}
		inf.push_back(i);
	}

	void create_region(std::list<core_vma_info>& inf, const std::string& name, uint64_t base,
		SNES::MappedRAM& memory, bool readonly, bool native_endian = false) throw(std::bad_alloc)
	{
		create_region(inf, name, base, memory.data(), memory.size(), readonly, native_endian);
	}

	void map_internal(std::list<core_vma_info>& inf, const std::string& name, uint16_t index, void* memory,
		size_t memsize)
	{
		ptrmap[index] = std::make_pair(reinterpret_cast<uint64_t>(memory), static_cast<uint64_t>(memsize));
		create_region(inf, name, 0x101000000 + index * 0x1000000, reinterpret_cast<uint8_t*>(memory),
			memsize, true, true);
	}

	std::list<core_vma_info> get_VMAlist();
	std::set<std::string> bsnes_srams()
	{
		std::set<std::string> r;
		if(!internal_rom)
			return r;
		for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
			SNES::Cartridge::NonVolatileRAM& s = SNES::cartridge.nvram[i];
			r.insert(sram_name(s.id, s.slot));
		}
		return r;
	}

	uint64_t translate_class_address(uint8_t clazz, unsigned offset)
	{
		switch(clazz) {
		case 1:		//ROM.
			return 0x80000000 + offset;
		case 2:		//SRAM.
			return 0x10000000 + offset;
		case 3:		//WRAM
			return 0x007E0000 + offset;
		case 8:		//SufamiTurboA ROM.
			return 0x90000000 + offset;
		case 9:		//SufamiTurboB ROM.
			return 0xA0000000 + offset;
		case 10:	//SufamiTurboA RAM.
			return 0x20000000 + offset;
		case 11:	//SufamiTurboB RAM.
			return 0x30000000 + offset;
		case 12:	//BSX flash.
			return 0x90000000 + offset;
		default:	//Other, including bus.
			return 0xFFFFFFFFFFFFFFFFULL;
		}
	}

	void bsnes_debug_read(uint8_t clazz, unsigned offset, unsigned addr, uint8_t val, bool exec)
	{
		if(disable_breakpoints) return;
		uint64_t _addr = translate_class_address(clazz, offset);
		if(_addr != 0xFFFFFFFFFFFFFFFFULL) {
			if(exec)
				ecore_callbacks->memory_execute(_addr, 0);
			else
				ecore_callbacks->memory_read(_addr, val);
		}
		if(exec)
			ecore_callbacks->memory_execute(0x1000000 + _addr, 0);
		else
			ecore_callbacks->memory_read(0x1000000 + _addr, val);
	}

	void bsnes_debug_write(uint8_t clazz, unsigned offset, unsigned addr, uint8_t val)
	{
		if(disable_breakpoints) return;
		uint64_t _addr = translate_class_address(clazz, offset);
		if(_addr != 0xFFFFFFFFFFFFFFFFULL)
			ecore_callbacks->memory_write(_addr, val);
		ecore_callbacks->memory_write(0x1000000 + _addr, val);
	}

	void redraw_cover_fbinfo();

	struct _bsnes_core : public core_core
	{
		_bsnes_core() : core_core({&gamepad, &gamepad16, &justifier, &justifiers, &mouse, &multitap,
			&multitap16, &none, &superscope, &psystem, &psystem_hreset, &psystem_compact}, {
				{0, "Soft reset", "reset", {}},
				{1, "Hard reset", "hardreset", {}},
#ifdef BSNES_HAS_DEBUGGER
				{2, "Delayed soft reset", "delayreset", {
					{"Delay","int:0,99999999"}
				}},
				{3, "Delayed hard reset", "delayhardreset", {
					{"Delay","int:0,99999999"}
				}},
#endif
#ifdef BSNES_IS_COMPAT
				{4, "Layers‣BG1 Priority 0", "bg1pri0", {{"", "toggle"}}},
				{5, "Layers‣BG1 Priority 1", "bg1pri1", {{"", "toggle"}}},
				{8, "Layers‣BG2 Priority 0", "bg2pri0", {{"", "toggle"}}},
				{9, "Layers‣BG2 Priority 1", "bg2pri1", {{"", "toggle"}}},
				{12, "Layers‣BG3 Priority 0", "bg3pri0", {{"", "toggle"}}},
				{13, "Layers‣BG3 Priority 1", "bg3pri1", {{"", "toggle"}}},
				{16, "Layers‣BG4 Priority 0", "bg4pri0", {{"", "toggle"}}},
				{17, "Layers‣BG4 Priority 1", "bg4pri1", {{"", "toggle"}}},
				{20, "Layers‣Sprite Priority 0", "oampri0", {{"", "toggle"}}},
				{21, "Layers‣Sprite Priority 1", "oampri1", {{"", "toggle"}}},
				{22, "Layers‣Sprite Priority 2", "oampri2", {{"", "toggle"}}},
				{23, "Layers‣Sprite Priority 3", "oampri3", {{"", "toggle"}}},
#endif
			}) {}

		std::string c_core_identifier() {
			return (stringfmt() << snes_library_id() << " (" << SNES::Info::Profile << " core)").str();
		}
		bool c_set_region(core_region& region) {
			if(&region == &region_auto)
				SNES::config.region = SNES::System::Region::Autodetect;
			else if(&region == &region_ntsc)
				SNES::config.region = SNES::System::Region::NTSC;
			else if(&region == &region_pal)
				SNES::config.region = SNES::System::Region::PAL;
			else
				return false;
			return true;
		}
		std::pair<uint32_t, uint32_t> c_video_rate() {
			if(!internal_rom)
				return std::make_pair(60, 1);
			uint32_t div;
			if(SNES::system.region() == SNES::System::Region::PAL)
				div = last_interlace ? DURATION_PAL_FIELD : DURATION_PAL_FRAME;
			else
				div = last_interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
			return std::make_pair(SNES::system.cpu_frequency(), div);
		}
		double c_get_PAR() {
			double base = (SNES::system.region() == SNES::System::Region::PAL) ? 1.25 : 1.146;
			return base;
		}
		std::pair<uint32_t, uint32_t> c_audio_rate() {
			if(!internal_rom)
				return std::make_pair(64081, 2);
			return std::make_pair(SNES::system.apu_frequency(), static_cast<uint32_t>(768));
		}
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc) {
			std::map<std::string, std::vector<char>> out;
			if(!internal_rom)
				return out;
			for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
				SNES::Cartridge::NonVolatileRAM& r = SNES::cartridge.nvram[i];
				std::string savename = sram_name(r.id, r.slot);
				std::vector<char> x;
				x.resize(r.size);
				memcpy(&x[0], r.data, r.size);
				out[savename] = x;
			}
			return out;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc) {
			std::set<std::string> used;
			if(!internal_rom) {
				for(auto i : sram)
					messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge."
						<< std::endl;
				return;
			}
			if(sram.empty())
				return;
			for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
				SNES::Cartridge::NonVolatileRAM& r = SNES::cartridge.nvram[i];
				std::string savename = sram_name(r.id, r.slot);
				if(sram.count(savename)) {
					std::vector<char>& x = sram[savename];
					if(r.size != x.size())
						messages << "WARNING: SRAM '" << savename << "': Loaded " << x.size()
							<< " bytes, but the SRAM is " << r.size << "." << std::endl;
					memcpy(r.data, &x[0], (r.size < x.size()) ? r.size : x.size());
					used.insert(savename);
				} else
					messages << "WARNING: SRAM '" << savename << ": No data." << std::endl;
			}
			for(auto i : sram)
				if(!used.count(i.first))
					messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge."
						<< std::endl;
		}
		void c_serialize(std::vector<char>& out) {
			if(!internal_rom)
				throw std::runtime_error("No ROM loaded");
			serializer s = SNES::system.serialize();
			out.resize(s.size());
			memcpy(&out[0], s.data(), s.size());
		}
		void c_unserialize(const char* in, size_t insize) {
			if(!internal_rom)
				throw std::runtime_error("No ROM loaded");
			serializer s(reinterpret_cast<const uint8_t*>(in), insize);
			if(!SNES::system.unserialize(s))
				throw std::runtime_error("SNES core rejected savestate");
			have_saved_this_frame = true;
			do_reset_flag = -1;
		}
		core_region& c_get_region() {
			return (SNES::system.region() == SNES::System::Region::PAL) ? region_pal : region_ntsc;
		}
		void c_power() {
			if(internal_rom) snes_power();
		}
		void c_unload_cartridge() {
			if(!internal_rom) return;
			snes_term();
			snes_unload_cartridge();
			internal_rom = NULL;
		}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height) {
			return std::make_pair((width < 400) ? 2 : 1, (height < 400) ? 2 : 1);
		}
		void c_install_handler() {
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
			SNES::bus.debug_read = bsnes_debug_read;
			SNES::bus.debug_write = bsnes_debug_write;
#endif
			basic_init();
			old = SNES::interface;
			SNES::interface = &my_interface_obj;
			SNES::system.init();
			magic_flags |= 1;
		}
		void c_uninstall_handler() { SNES::interface = old; }
		void c_emulate() {
			if(!internal_rom)
				return;
			bool was_delay_reset = false;
			int16_t reset = ecore_callbacks->get_input(0, 0, 1);
			int16_t hreset = 0;
			if(support_hreset)
				hreset = ecore_callbacks->get_input(0, 0, 4);
			if(reset) {
				long hi = ecore_callbacks->get_input(0, 0, 2);
				long lo = ecore_callbacks->get_input(0, 0, 3);
				long delay = 10000 * hi + lo;
				if(delay > 0) {
					was_delay_reset = true;
#ifdef BSNES_HAS_DEBUGGER
					messages << "Executing delayed reset... This can take some time!"
						<< std::endl;
					video_refresh_done = false;
					delayreset_cycles_run = 0;
					delayreset_cycles_target = delay;
					forced_hook = true;
					SNES::cpu.step_event = delayreset_fn;
again:
					SNES::system.run();
					if(SNES::scheduler.exit_reason() == SNES::Scheduler::ExitReason::DebuggerEvent
						&& SNES::debugger.break_event ==
						SNES::Debugger::BreakEvent::BreakpointHit) {
						snesdbg_on_break();
						goto again;
					}
					SNES::cpu.step_event = nall::function<bool()>();
					forced_hook = false;
					update_trace_hook_state();
					if(video_refresh_done) {
						//Force the reset here.
						do_reset_flag = -1;
						messages << "SNES reset (forced at " << delayreset_cycles_run << ")"
							<< std::endl;
						if(hreset)
							SNES::system.power();
						else
							SNES::system.reset();
						return;
					}
					if(hreset)
						SNES::system.power();
					else
						SNES::system.reset();
					messages << "SNES reset (delayed " << delayreset_cycles_run << ")"
						<< std::endl;
#else
					messages << "Delayresets not supported on this bsnes version "
						"(needs v084 or v085)" << std::endl;
					if(hreset)
						SNES::system.power();
					else
						SNES::system.reset();
#endif
				} else if(delay == 0) {
					if(hreset)
						SNES::system.power();
					else
						SNES::system.reset();
					messages << "SNES reset" << std::endl;
				}
			}
			do_reset_flag = -1;

			if(!have_saved_this_frame && save_every_frame && !was_delay_reset)
				SNES::system.runtosave();
#ifdef BSNES_HAS_DEBUGGER
			if(trace_enabled())
				SNES::cpu.step_event = trace_fn;
#endif
again2:
			SNES::system.run();
			if(SNES::scheduler.exit_reason() == SNES::Scheduler::ExitReason::DebuggerEvent &&
				SNES::debugger.break_event == SNES::Debugger::BreakEvent::BreakpointHit) {
				snesdbg_on_break();
				goto again2;
			}
#ifdef BSNES_HAS_DEBUGGER
			SNES::cpu.step_event = nall::function<bool()>();
#endif
			have_saved_this_frame = false;
		}
		void c_runtosave() {
			if(!internal_rom)
				return;
			stepping_into_save = true;
			SNES::system.runtosave();
			have_saved_this_frame = true;
			stepping_into_save = false;
		}
		bool c_get_pflag() { return SNES::cpu.controller_flag; }
		void c_set_pflag(bool pflag) { SNES::cpu.controller_flag = pflag; }
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(cover_fbinfo);
			redraw_cover_fbinfo();
			return x;
		}
		std::string c_get_core_shortname()
		{
#ifdef BSNES_IS_COMPAT
			return (stringfmt() << "bsnes" << BSNES_VERSION << "c").str();
#else
			return (stringfmt() << "bsnes" << BSNES_VERSION << "a").str();
#endif
		}
		void c_pre_emulate_frame(controller_frame& cf)
		{
			cf.axis3(0, 0, 1, (do_reset_flag >= 0) ? 1 : 0);
			if(support_hreset)
				cf.axis3(0, 0, 4, do_hreset_flag ? 1 : 0);
			if(do_reset_flag >= 0) {
				cf.axis3(0, 0, 2, do_reset_flag / 10000);
				cf.axis3(0, 0, 3, do_reset_flag % 10000);
			} else {
				cf.axis3(0, 0, 2, 0);
				cf.axis3(0, 0, 3, 0);
			}
		}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
		{
			switch(id) {
			case 0:		//Soft reset.
				do_reset_flag = 0;
				do_hreset_flag = false;
				break;
			case 1:		//Hard reset.
				do_reset_flag = 0;
				do_hreset_flag = true;
				break;
			case 2:		//Delayed soft reset.
				do_reset_flag = p[0].i;
				do_hreset_flag = false;
				break;
			case 3:		//Delayed hard reset.
				do_reset_flag = p[0].i;
				do_hreset_flag = true;
				break;
			}
#ifdef BSNES_IS_COMPAT
			if(id >= 4 && id <= 23) {
				unsigned y = (id - 4) / 4;
				SNES::ppu.layer_enabled[y][id % 4] = !SNES::ppu.layer_enabled[y][id % 4];
				ecore_callbacks->action_state_updated();
			}
#endif
		}
		const interface_device_reg* c_get_registers() { return snes_registers; }
		unsigned c_action_flags(unsigned id)
		{
			if((id == 2 || id == 3) && !support_dreset)
				return 0;
			if(id == 0 || id == 2)
				return 1;
			if(id == 1 || id == 3)
				return support_hreset ? 1 : 0;
#ifdef BSNES_IS_COMPAT
			if(id >= 4 && id <= 23) {
				unsigned y = (id - 4) / 4;
				return SNES::ppu.layer_enabled[y][id % 4] ? 3 : 1;
			}
#endif
		}
		int c_reset_action(bool hard)
		{
			return hard ? (support_hreset ? 1 : -1) : 0;
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map()
		{
			return std::make_pair(0x1000000, 0x1000000);
		}
		std::list<core_vma_info> c_vma_list() { return get_VMAlist(); }
		std::set<std::string> c_srams() { return bsnes_srams(); }
		std::pair<unsigned, unsigned> c_lightgun_scale() {
			return std::make_pair(256, last_PAL ? 239 : 224);
		}
		void c_set_debug_flags(uint64_t addr, unsigned int sflags, unsigned int cflags)
		{
			if(addr == 0) {
				if(sflags & 8) trace_cpu_enable = true;
				if(cflags & 8) trace_cpu_enable = false;
				update_trace_hook_state();
			}
			if(addr == 1) {
				if(sflags & 8) trace_smp_enable = true;
				if(cflags & 8) trace_smp_enable = false;
				update_trace_hook_state();
			}
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
			auto _addr = recognize_address(addr);
			if(_addr.first == ADDR_KIND_ALL)
				SNES::bus.debugFlags(sflags & 7, cflags & 7);
			else if(_addr.first != ADDR_KIND_NONE && ((sflags | cflags) & 7))
				SNES::bus.debugFlags(sflags & 7, cflags & 7, _addr.first, _addr.second);
#endif
		}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set)
		{
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
			bool s = false;
			auto _addr = recognize_address(addr);
			if(_addr.first == ADDR_KIND_NONE || _addr.first == ADDR_KIND_ALL)
				return;
			unsigned x = 0;
			while(x < 0x1000000) {
				x = SNES::bus.enumerateMirrors(_addr.first, _addr.second, x);
				if(x < 0x1000000) {
					if(set) {
						for(size_t i = 0; i < SNES::cheat.size(); i++) {
							if(SNES::cheat[i].addr == x) {
								SNES::cheat[i].data = value;
								s = true;
								break;
							}
						}
						if(!s) SNES::cheat.append({x, (uint8_t)value, true});
					} else
						for(size_t i = 0; i < SNES::cheat.size(); i++) {
							if(SNES::cheat[i].addr == x) {
								SNES::cheat.remove(i);
								break;
							}
						}
				}
				x++;
			}
			SNES::cheat.synchronize();
#endif
		}
		void c_debug_reset()
		{
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
			SNES::bus.clearDebugFlags();
			SNES::cheat.reset();
#endif
			trace_cpu_enable = false;
			trace_smp_enable = false;
			update_trace_hook_state();
		}
		std::vector<std::string> c_get_trace_cpus()
		{
			std::vector<std::string> r;
			r.push_back("cpu");
			r.push_back("smp");
			//TODO: Trace various chips.
			return r;
		}
	} bsnes_core;

	struct _type_snes : public core_type
	{
		_type_snes()
			: core_type({{
				.iname = "snes",
				.hname = "SNES",
				.id = 0,
				.sysname = "SNES",
				.bios = NULL,
				.regions = {&region_auto, &region_ntsc, &region_pal},
				.images = {{"rom", "Cartridge ROM", 1, 0, 512,
					"sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh"}},
				.settings = bsnes_settings,
				.core = &bsnes_core,
			}}) {}

		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom(this, img, settings, secs, subsecs,
				load_rom_X1<snes_load_cartridge_normal>);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return bsnes_controllerconfig(settings);
		}
	} type_snes;
	core_sysregion snes_pal("snes_pal", type_snes, region_pal);
	core_sysregion snes_ntsc("snes_ntsc", type_snes, region_ntsc);

	struct _type_bsx : public core_type, public core_sysregion
	{
		_type_bsx()
			: core_type({{
				.iname = "bsx",
				.hname = "BS-X (non-slotted)",
				.id = 1,
				.sysname = "BS-X",
				.bios = "bsx.sfc",
				.regions = {&region_ntsc},
				.images = {{"rom", "BS-X BIOS", 1, 0, 512,
					"sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh"},
					{"bsx", "BS-X Flash", 2, 0, 512, "bs"}},
				.settings = bsnes_settings,
				.core = &bsnes_core,
			}}),
			core_sysregion("bsx", *this, region_ntsc) {}

		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom(this, img, settings, secs, subsecs,
				load_rom_X2<snes_load_cartridge_bsx>);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return bsnes_controllerconfig(settings);
		}
	} type_bsx;

	struct _type_bsxslotted : public core_type, public core_sysregion
	{
		_type_bsxslotted()
			: core_type({{
				.iname = "bsxslotted",
				.hname = "BS-X (slotted)",
				.id = 2,
				.sysname = "BS-X",
				.bios = "bsxslotted.sfc",
				.regions = {&region_ntsc},
				.images = {{"rom", "BS-X BIOS", 1, 0, 512,
					"sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh"},
					{"bsx", "BS-X Flash", 2, 0, 512, "bss"}},
				.settings = bsnes_settings,
				.core = &bsnes_core,
			}}),
			core_sysregion("bsxslotted", *this, region_ntsc) {}
		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom(this, img, settings, secs, subsecs,
				load_rom_X2<snes_load_cartridge_bsx_slotted>);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return bsnes_controllerconfig(settings);
		}
	} type_bsxslotted;

	struct _type_sufamiturbo : public core_type, public core_sysregion
	{
		_type_sufamiturbo()
			: core_type({{
				.iname = "sufamiturbo",
				.hname = "Sufami Turbo",
				.id = 3,
				.sysname = "SufamiTurbo",
				.bios = "sufamiturbo.sfc",
				.regions = {&region_ntsc},
				.images = {
					{"rom", "ST BIOS", 1, 0, 512, "sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh"},
					{"slot-a", "ST SLOT A ROM", 2, 0, 512, "st"},
					{"slot-b", "ST SLOT B ROM", 2, 0, 512, "st"}
				},
				.settings = bsnes_settings,
				.core = &bsnes_core,
			}}),
			core_sysregion("sufamiturbo", *this, region_ntsc) {}
		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom(this, img, settings, secs, subsecs,
				load_rom_X3<snes_load_cartridge_sufami_turbo>);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return bsnes_controllerconfig(settings);
		}
	} type_sufamiturbo;

	struct _type_sgb : public core_type
	{
		_type_sgb()
			: core_type({{
				.iname = "sgb",
				.hname = "Super Game Boy",
				.id = 4,
				.sysname = "SGB",
				.bios = "sgb.sfc",
				.regions = {&region_auto, &region_ntsc, &region_pal},
				.images = {{"rom", "SGB BIOS", 1, 0, 512,
					"sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh"},
					{"dmg", "DMG ROM", 2, 0, 512, "gb;dmg;sgb"}},
				.settings = bsnes_settings,
				.core = &bsnes_core,
			}}) {}
		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom(this, img, settings, secs, subsecs,
				load_rom_X2<snes_load_cartridge_super_game_boy>);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return bsnes_controllerconfig(settings);
		}
	} type_sgb;
	core_sysregion sgb_pal("sgb_pal", type_sgb, region_pal);
	core_sysregion sgb_ntsc("sgb_ntsc", type_sgb, region_ntsc);

	void redraw_cover_fbinfo()
	{
		for(size_t i = 0; i < sizeof(cover_fbmem) / sizeof(cover_fbmem[0]); i++)
			cover_fbmem[i] = 0;
		std::string ident = bsnes_core.get_core_identifier();
		cover_render_string(cover_fbmem, 0, 0, ident, 0x7FFFF, 0x00000, 512, 448, 2048, 4);
		std::ostringstream name;
		name << "Internal ROM name: ";
		disable_breakpoints = true;
		for(unsigned i = 0; i < 21; i++) {
			unsigned busaddr = 0x00FFC0 + i;
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
			unsigned char ch = SNES::bus.read(busaddr, false);
#else
			unsigned char ch = SNES::bus.read(busaddr);
#endif
			if(ch < 32 || ch > 126)
				name << "<" << hex::to8(ch) << ">";
			else
				name << ch;
		}
		disable_breakpoints = false;
		cover_render_string(cover_fbmem, 0, 16, name.str(), 0x7FFFF, 0x00000, 512, 448, 2048, 4);
		unsigned y = 32;
		for(auto i : cover_information()) {
			cover_render_string(cover_fbmem, 0, y, i, 0x7FFFF, 0x00000, 512, 448, 2048, 4);
			y += 16;
		}
#ifdef BSNES_SUPPORTS_ALT_TIMINGS
		if(SNES::config.cpu.alt_poll_timings) {
			cover_render_string(cover_fbmem, 0, y, "Alternate timings enabled.", 0x7FFFF, 0x00000,
				512, 448, 2048, 4);
			y += 16;
		}
#endif
	}

	void my_interface::videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan)
	{
		last_hires = hires;
		last_interlace = interlace;
		bool region = (SNES::system.region() == SNES::System::Region::PAL);
		last_PAL = region;
		if(stepping_into_save)
			messages << "Got video refresh in runtosave, expect desyncs!" << std::endl;
		video_refresh_done = true;
		uint32_t fps_n, fps_d;
		auto fps = bsnes_core.get_video_rate();
		fps_n = fps.first;
		fps_d = fps.second;
		uint32_t g = gcd(fps_n, fps_d);
		fps_n /= g;
		fps_d /= g;

		framebuffer::info inf;
		inf.type = &framebuffer::pixfmt_lrgb;
		inf.mem = const_cast<char*>(reinterpret_cast<const char*>(data));
		inf.physwidth = 512;
		inf.physheight = 512;
		inf.physstride = 2048;
		inf.width = hires ? 512 : 256;
		inf.height = (region ? 239 : 224) * (interlace ? 2 : 1);
		inf.stride = interlace ? 2048 : 4096;
		inf.offset_x = 0;
		inf.offset_y = (region ? (overscan ? 9 : 1) : (overscan ? 16 : 9)) * 2;
		framebuffer::raw ls(inf);

		ecore_callbacks->output_frame(ls, fps_n, fps_d);
		if(soundbuf_fill > 0) {
			auto freq = SNES::system.apu_frequency();
			audioapi_submit_buffer(soundbuf, soundbuf_fill / 2, true, freq / 768.0);
			soundbuf_fill = 0;
		}
	}

	std::list<core_vma_info> get_VMAlist()
	{
		std::list<core_vma_info> ret;
		if(!internal_rom)
			return ret;
		create_region(ret, "WRAM", 0x007E0000, SNES::cpu.wram, 131072, false);
		create_region(ret, "APURAM", 0x00000000, SNES::smp.apuram, 65536, false);
		create_region(ret, "VRAM", 0x00010000, SNES::ppu.vram, 65536, false);
		create_region(ret, "OAM", 0x00020000, SNES::ppu.oam, 544, false);
		create_region(ret, "CGRAM", 0x00021000, SNES::ppu.cgram, 512, false);
		if(SNES::cartridge.has_srtc()) create_region(ret, "RTC", 0x00022000, SNES::srtc.rtc, 20, false);
		if(SNES::cartridge.has_spc7110rtc()) create_region(ret, "RTC", 0x00022000, SNES::spc7110.rtc, 20,
			false);
		if(SNES::cartridge.has_necdsp()) {
			create_region(ret, "DSPRAM", 0x00023000, reinterpret_cast<uint8_t*>(SNES::necdsp.dataRAM),
				4096, false, true);
			create_region(ret, "DSPPROM", 0xF0000000, reinterpret_cast<uint8_t*>(SNES::necdsp.programROM),
				65536, true, true);
			create_region(ret, "DSPDROM", 0xF0010000, reinterpret_cast<uint8_t*>(SNES::necdsp.dataROM),
				4096, true, true);
		}
		create_region(ret, "SRAM", 0x10000000, SNES::cartridge.ram, false);
		create_region(ret, "ROM", 0x80000000, SNES::cartridge.rom, true);
		create_region(ret, "BUS", 0x1000000, 0x1000000, snes_bus_iospace_rw);
		create_region(ret, "PTRTABLE", 0x100000000, 0x100000, ptrtable_iospace_rw);
		map_internal(ret, "CPU_STATE", 0, &SNES::cpu, sizeof(SNES::cpu));
		map_internal(ret, "PPU_STATE", 1, &SNES::ppu, sizeof(SNES::ppu));
		map_internal(ret, "SMP_STATE", 2, &SNES::smp, sizeof(SNES::smp));
		map_internal(ret, "DSP_STATE", 3, &SNES::dsp, sizeof(SNES::dsp));
		if(internal_rom == &type_bsx || internal_rom == &type_bsxslotted) {
			create_region(ret, "BSXFLASH", 0x90000000, SNES::bsxflash.memory, true);
			create_region(ret, "BSX_RAM", 0x20000000, SNES::bsxcartridge.sram, false);
			create_region(ret, "BSX_PRAM", 0x30000000, SNES::bsxcartridge.psram, false);
		}
		if(internal_rom == &type_sufamiturbo) {
			create_region(ret, "SLOTA_ROM", 0x90000000, SNES::sufamiturbo.slotA.rom, true);
			create_region(ret, "SLOTB_ROM", 0xA0000000, SNES::sufamiturbo.slotB.rom, true);
			create_region(ret, "SLOTA_RAM", 0x20000000, SNES::sufamiturbo.slotA.ram, false);
			create_region(ret, "SLOTB_RAM", 0x30000000, SNES::sufamiturbo.slotB.ram, false);
		}
		if(internal_rom == &type_sgb) {
			map_internal(ret, "GBCPU_STATE", 4, &GameBoy::cpu, sizeof(GameBoy::cpu));
			create_region(ret, "GBROM", 0x90000000, GameBoy::cartridge.romdata,
				GameBoy::cartridge.romsize, true);
			create_region(ret, "GBRAM", 0x20000000, GameBoy::cartridge.ramdata,
				GameBoy::cartridge.ramsize, false);
			create_region(ret, "GBWRAM", 0x00030000, GameBoy::cpu.wram, 32768, false);
			create_region(ret, "GBHRAM", 0x00038000, GameBoy::cpu.hram, 128, true);
		}
		return ret;
	}

	std::pair<int, uint64_t> recognize_address(uint64_t addr)
	{
		if(addr == 0xFFFFFFFFFFFFFFFFULL)
			return std::make_pair(ADDR_KIND_ALL, 0);
		if(addr >= 0x80000000 && addr <= 0x8FFFFFFF) //Rom.
			return std::make_pair(1, addr - 0x80000000);
		if(addr >= 0x10000000 && addr <= 0x1FFFFFFF) //SRAM.
			return std::make_pair(2, addr - 0x10000000);
		if(addr >= 0x007E0000 && addr <= 0x007FFFFF) //WRAM.
			return std::make_pair(3, addr - 0x007E0000);
		if(internal_rom == &type_sufamiturbo) {
			if(addr >= 0x90000000 && addr <= 0x9FFFFFFF) //SufamiTurboA Rom.
				return std::make_pair(8, addr - 0x90000000);
			if(addr >= 0xA0000000 && addr <= 0xAFFFFFFF) //SufamiTurboB Rom.
				return std::make_pair(9, addr - 0x90000000);
			if(addr >= 0x20000000 && addr <= 0x2FFFFFFF) //SufamiTurboA Ram.
				return std::make_pair(10, addr - 0x20000000);
			if(addr >= 0x20000000 && addr <= 0x3FFFFFFF) //SufamiTurboB Ram.
				return std::make_pair(11, addr - 0x30000000);
		}
		if(internal_rom == &type_bsx || internal_rom == &type_bsxslotted) {
			if(addr >= 0x90000000 && addr <= 0x9FFFFFFF) //BSX flash.
				return std::make_pair(12, addr - 0x90000000);
		}
		if(addr >= 0x01000000 && addr <= 0x01FFFFFF) //BUS.
			return std::make_pair(255, addr - 0x01000000);
		return std::make_pair(ADDR_KIND_NONE, 0);
	}

	command::fnptr<command::arg_filename> dump_core(lsnes_cmd, "dump-core", "No description available",
		"No description available\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			std::vector<char> out;
			bsnes_core.serialize(out);
			std::ofstream x(args, std::ios_base::out | std::ios_base::binary);
			x.write(&out[0], out.size());
		});

#ifdef BSNES_HAS_DEBUGGER
	lua::state* snes_debug_cb_keys[SNES::Debugger::Breakpoints];
	lua::state* snes_debug_cb_trace;

	void snesdbg_execute_callback(lua::state*& cb, signed r)
	{
		if(!cb)
			return;
		cb->pushlightuserdata(&cb);
		cb->gettable(LUA_REGISTRYINDEX);
		cb->pushnumber(r);
		if(cb->type(-2) == LUA_TFUNCTION) {
			int s = cb->pcall(1, 0, 0);
			if(s)
				cb->pop(1);
		} else {
			messages << "Can't execute debug callback" << std::endl;
			cb->pop(2);
		}
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
	}

	void snesdbg_on_break()
	{
		signed r = SNES::debugger.breakpoint_hit;
		snesdbg_execute_callback(snes_debug_cb_keys[r], r);
	}

	void snesdbg_on_trace()
	{
		snesdbg_execute_callback(snes_debug_cb_trace, -1);
	}

	void snesdbg_set_callback(lua::state& L, lua::state*& cb)
	{
		cb = &L.get_master();
		L.pushlightuserdata(&cb);
		L.pushvalue(-2);
		L.settable(LUA_REGISTRYINDEX);
	}

	bool snesdbg_get_bp_enabled(lua::state& L)
	{
		bool r;
		L.getfield(-1, "addr");
		r = (L.type(-1) == LUA_TNUMBER);
		L.pop(1);
		return r;
	}

	uint32_t snesdbg_get_bp_addr(lua::state& L)
	{
		uint32_t r = 0;
		L.getfield(-1, "addr");
		if(L.type(-1) == LUA_TNUMBER)
			r = static_cast<uint32_t>(L.tonumber(-1));
		L.pop(1);
		return r;
	}

	uint32_t snesdbg_get_bp_data(lua::state& L)
	{
		signed r = -1;
		L.getfield(-1, "data");
		if(L.type(-1) == LUA_TNUMBER)
			r = static_cast<signed>(L.tonumber(-1));
		L.pop(1);
		return r;
	}

	SNES::Debugger::Breakpoint::Mode snesdbg_get_bp_mode(lua::state& L)
	{
		SNES::Debugger::Breakpoint::Mode r = SNES::Debugger::Breakpoint::Mode::Exec;
		L.getfield(-1, "mode");
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "e"))
			r = SNES::Debugger::Breakpoint::Mode::Exec;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "x"))
			r = SNES::Debugger::Breakpoint::Mode::Exec;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "exec"))
			r = SNES::Debugger::Breakpoint::Mode::Exec;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "r"))
			r = SNES::Debugger::Breakpoint::Mode::Read;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "read"))
			r = SNES::Debugger::Breakpoint::Mode::Read;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "w"))
			r = SNES::Debugger::Breakpoint::Mode::Write;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "write"))
			r = SNES::Debugger::Breakpoint::Mode::Write;
		L.pop(1);
		return r;
	}

	SNES::Debugger::Breakpoint::Source snesdbg_get_bp_source(lua::state& L)
	{
		SNES::Debugger::Breakpoint::Source r = SNES::Debugger::Breakpoint::Source::CPUBus;
		L.getfield(-1, "source");
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "cpubus"))
			r = SNES::Debugger::Breakpoint::Source::CPUBus;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "apuram"))
			r = SNES::Debugger::Breakpoint::Source::APURAM;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "vram"))
			r = SNES::Debugger::Breakpoint::Source::VRAM;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "oam"))
			r = SNES::Debugger::Breakpoint::Source::OAM;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "cgram"))
			r = SNES::Debugger::Breakpoint::Source::CGRAM;
		L.pop(1);
		return r;
	}

	void snesdbg_get_bp_callback(lua::state& L)
	{
		L.getfield(-1, "callback");
	}

	lua::fnptr2 lua_memory_setdebug(lua_func_misc, "memory.setdebug", [](lua::state& L, lua::parameters& P)
		-> int {
		unsigned r;
		int ltbl;

		P(r);

		if(r >= SNES::Debugger::Breakpoints)
			throw std::runtime_error("Bad breakpoint number");
		if(P.is_novalue()) {
			//Clear breakpoint.
			SNES::debugger.breakpoint[r].enabled = false;
			return 0;
		} else if(P.is_table()) {
			P(P.table(ltbl));
			L.pushvalue(ltbl);
			auto& x = SNES::debugger.breakpoint[r];
			x.enabled = snesdbg_get_bp_enabled(L);
			x.addr = snesdbg_get_bp_addr(L);
			x.data = snesdbg_get_bp_data(L);
			x.mode = snesdbg_get_bp_mode(L);
			x.source = snesdbg_get_bp_source(L);
			snesdbg_get_bp_callback(L);
			snesdbg_set_callback(L, snes_debug_cb_keys[r]);
			L.pop(2);
			return 0;
		} else
			P.expected("table or nil");
	});

	lua::fnptr2 lua_memory_setstep(lua_func_misc, "memory.setstep", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t r;
		int lfn = 2;

		P(r);
		if(P.is_function() || P.is_novalue()) lfn = P.skip();

		L.pushvalue(lfn);
		snesdbg_set_callback(L, snes_debug_cb_trace);
		trace_counter = r;
		update_trace_hook_state();
		L.pop(1);
		return 0;
	});

	lua::fnptr2 lua_memory_settrace(lua_func_misc, "memory.settrace", [](lua::state& L, lua::parameters& P)
		-> int {
		std::string r;

		P(r);

		lsnes_cmd.invoke("tracelog cpu " + r);
	});

	command::fnptr<const std::string&> start_trace(lsnes_cmd, "set-trace", "No description available",
		"No description available\n",
		[](const std::string& r) throw(std::bad_alloc, std::runtime_error) {
			lsnes_cmd.invoke("tracelog cpu " + r);
		});

#ifdef BSNES_IS_COMPAT
	lua::fnptr2 lua_layerenabled(lua_func_misc, "snes.enablelayer", [](lua::state& L, lua::parameters& P) -> int {
		unsigned layer, priority;
		bool enabled;

		P(layer, priority, enabled);

		SNES::ppu.layer_enable(layer, priority, enabled);
		return 0;
	});
#endif

	lua::fnptr2 lua_smpdiasm(lua_func_misc, "snes.smpdisasm", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t addr;

		P(addr);

		nall::string _disasm = SNES::smp.disassemble_opcode(addr);
		std::string disasm(_disasm, _disasm.length());
		L.pushlstring(disasm);
		return 1;
	});
#else
	void snesdbg_on_break() {}
	void snesdbg_on_trace() {}
#endif

	struct oninit {
		oninit()
		{
			register_sysregion_mapping("snes_pal", "SNES");
			register_sysregion_mapping("snes_ntsc", "SNES");
			register_sysregion_mapping("bsx", "SNES");
			register_sysregion_mapping("bsxslotted", "SNES");
			register_sysregion_mapping("sufamiturbo", "SNES");
			register_sysregion_mapping("sgb_ntsc", "SGB");
			register_sysregion_mapping("sgb_pal", "SGB");
		}
	} _oninit;
}
