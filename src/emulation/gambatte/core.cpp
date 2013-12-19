/***************************************************************************
 *   Copyright (C) 2012-2013 by Ilari Liusvaara                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License version 2 for more details.                *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   version 2 along with this program; if not, write to the               *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "lsnes.hpp"
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "disassemble-gb.hpp"
#include "core/audioapi.hpp"
#include "core/misc.hpp"
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/settings.hpp"
#include "core/framebuffer.hpp"
#include "core/window.hpp"
#include "interface/callbacks.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "library/pixfmt-rgb32.hpp"
#include "library/string.hpp"
#include "library/controller-data.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"
#include "library/framebuffer.hpp"
#define HAVE_CSTDINT
#include "libgambatte/include/gambatte.h"

#define SAMPLES_PER_FRAME 35112

namespace
{
	setting_var<setting_var_model_bool<setting_yes_no>> output_native(lsnes_vset, "gambatte-native-sound",
		"Gambatte‣Sound Output at native rate", false);

	bool do_reset_flag = false;
	core_type* internal_rom = NULL;
	bool rtc_fixed;
	time_t rtc_fixed_val;
	gambatte::GB* instance;
	bool reallocate_debug = false;
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
	gambatte::debugbuffer debugbuf;
	size_t cur_romsize;
	size_t cur_ramsize;
#endif
	unsigned frame_overflow = 0;
	std::vector<unsigned char> romdata;
	uint32_t cover_fbmem[480 * 432];
	uint32_t primary_framebuffer[160*144];
	uint32_t accumulator_l = 0;
	uint32_t accumulator_r = 0;
	unsigned accumulator_s = 0;
	bool pflag = false;
	bool disable_breakpoints = false;
	bool palette_colors_default[3] = {true, true, true};
	uint32_t palette_colors[12];

	struct interface_device_reg gb_registers[] = {
		{"wrambank", []() -> uint64_t { return instance ? instance->getIoRam().first[0x170] & 0x07 : 0; },
			[](uint64_t v) {}},
		{"cyclecount", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_CYCLECOUNTER); },
			[](uint64_t v) {}},
		{"pc", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_PC); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_PC, v); }},
		{"sp", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_SP); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_SP, v); }},
		{"hf1", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_HF1); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_HF1, v); }},
		{"hf2", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_HF2); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_HF2, v); }},
		{"zf", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_ZF); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_ZF, v); }},
		{"cf", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_CF); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_CF, v); }},
		{"a", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_A); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_A, v); }},
		{"b", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_B); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_B, v); }},
		{"c", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_C); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_C, v); }},
		{"d", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_D); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_D, v); }},
		{"e", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_E); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_E, v); }},
		{"f", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_F); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_F, v); }},
		{"h", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_H); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_H, v); }},
		{"l", []() -> uint64_t { return instance->get_cpureg(gambatte::GB::REG_L); },
			[](uint64_t v) { instance->set_cpureg(gambatte::GB::REG_L, v); }},
		{NULL, NULL, NULL}
	};

	//Framebuffer.
	struct framebuffer::info cover_fbinfo = {
		&_pixel_format_rgb32,		//Format.
		(char*)cover_fbmem,		//Memory.
		480, 432, 1920,			//Physical size.
		480, 432, 1920,			//Logical size.
		0, 0				//Offset.
	};

#include "ports.inc"

	time_t walltime_fn()
	{
		if(rtc_fixed)
			return rtc_fixed_val;
		if(ecore_callbacks)
			return ecore_callbacks->get_time();
		else
			return time(0);
	}

	class myinput : public gambatte::InputGetter
	{
	public:
		unsigned operator()()
		{
			unsigned v = 0;
			for(unsigned i = 0; i < 8; i++) {
				if(ecore_callbacks->get_input(0, 1, i))
					v |= (1 << i);
			}
			pflag = true;
			return v;
		};
	} getinput;

	uint64_t get_address(unsigned clazz, unsigned offset)
	{
		switch(clazz) {
		case 0: return 0x1000000 + offset; //BUS.
		case 1: return offset; //WRAM.
		case 2: return 0x18000 + offset; //IOAMHRAM
		case 3: return 0x80000000 + offset; //ROM
		case 4: return 0x20000 + offset;  //SRAM.
		}
		return 0xFFFFFFFFFFFFFFFF;
	}

	void gambatte_read_handler(unsigned clazz, unsigned offset, uint8_t value, bool exec)
	{
		if(disable_breakpoints) return;
		uint64_t _addr = get_address(clazz, offset);
		if(_addr != 0xFFFFFFFFFFFFFFFFULL) {
			if(exec)
				ecore_callbacks->memory_execute(_addr, 0);
			else
				ecore_callbacks->memory_read(_addr, value);
		}
	}

	void gambatte_write_handler(unsigned clazz, unsigned offset, uint8_t value)
	{
		if(disable_breakpoints) return;
		uint64_t _addr = get_address(clazz, offset);
		if(_addr != 0xFFFFFFFFFFFFFFFFULL)
			ecore_callbacks->memory_write(_addr, value);
	}

	int get_hl(gambatte::GB* instance)
	{
		return instance->get_cpureg(gambatte::GB::REG_H) * 256 +
			instance->get_cpureg(gambatte::GB::REG_L);
	}

	int get_bc(gambatte::GB* instance)
	{
		return instance->get_cpureg(gambatte::GB::REG_B) * 256 +
			instance->get_cpureg(gambatte::GB::REG_C);
	}

	int get_de(gambatte::GB* instance)
	{
		return instance->get_cpureg(gambatte::GB::REG_D) * 256 +
			instance->get_cpureg(gambatte::GB::REG_E);
	}

	std::string hex4(uint16_t v)
	{
		return (stringfmt() << std::setw(4) << std::setfill('0') << std::hex << v).str();
	}

	std::string hex2(uint8_t v)
	{
		return (stringfmt() << std::setw(2) << std::setfill('0') << std::hex << (int)v).str();
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

	void gambatte_trace_handler(uint16_t _pc)
	{
		std::ostringstream s;
		int addr = -1;
		uint16_t opcode;
		uint32_t pc = _pc;
		uint16_t offset = 0;
		std::function<uint8_t()> fetch = [pc, &offset, &s]() -> uint8_t {
			unsigned addr = pc + offset++;
			uint8_t v;
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
			disable_breakpoints = true;
			v = instance->bus_read(addr);
			disable_breakpoints = true;
#endif
			s << hex2(v);
			return v;
		};
		s << hex4(pc) << " ";
		auto d = disassemble_gb_opcode(pc, fetch, addr, opcode);
		while(s.str().length() < 12) s << " ";
		s << d;
		switch(memclass[opcode >> 8]) {
		case 1: addr = get_bc(instance); break;
		case 2: addr = get_de(instance); break;
		case 3: addr = get_hl(instance); break;
		case 4: addr = 0xFF00 + instance->get_cpureg(gambatte::GB::REG_C); break;
		case 5: if(opcode & 7)  addr = get_hl(instance); break;
		}
		while(s.str().length() < 28) s << " ";
		if(addr >= 0) {
			s << "[" << std::setw(4) << std::setfill('0') << std::hex << addr << "] ";
		} else
			s << "       ";

		s << "A:" << hex2(instance->get_cpureg(gambatte::GB::REG_A));
		s << " B:" << hex2(instance->get_cpureg(gambatte::GB::REG_B));
		s << " C:" << hex2(instance->get_cpureg(gambatte::GB::REG_C));
		s << " D:" << hex2(instance->get_cpureg(gambatte::GB::REG_D));
		s << " E:" << hex2(instance->get_cpureg(gambatte::GB::REG_E));
		s << " H:" << hex2(instance->get_cpureg(gambatte::GB::REG_H));
		s << " L:" << hex2(instance->get_cpureg(gambatte::GB::REG_L));
		s << " PC:" << hex4(instance->get_cpureg(gambatte::GB::REG_PC));
		s << " SP:" << hex4(instance->get_cpureg(gambatte::GB::REG_SP));
		s << " F:" 
			<< (instance->get_cpureg(gambatte::GB::REG_CF) ? "C" : "-")
			<< (instance->get_cpureg(gambatte::GB::REG_ZF) ? "-" : "Z")
			<< (instance->get_cpureg(gambatte::GB::REG_HF1) ? "1" : "-")
			<< (instance->get_cpureg(gambatte::GB::REG_HF2) ? "2" : "-");
		std::string _s = s.str();
		ecore_callbacks->memory_trace(0, _s.c_str());
	}

	void basic_init()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		instance = new gambatte::GB;
		instance->setInputGetter(&getinput);
		instance->set_walltime_fn(walltime_fn);
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
		uint8_t* tmp = new uint8_t[98816];
		memset(tmp, 0, 98816);
		debugbuf.wram = tmp;
		debugbuf.bus = tmp + 32768;
		debugbuf.ioamhram = tmp + 98304;
		debugbuf.read = gambatte_read_handler;
		debugbuf.write = gambatte_write_handler;
		debugbuf.trace = gambatte_trace_handler;
		debugbuf.trace_cpu = false;
		instance->set_debug_buffer(debugbuf);
#endif
	}

	int load_rom_common(core_romimage* img, unsigned flags, uint64_t rtc_sec, uint64_t rtc_subsec,
		core_type* inttype)
	{
		basic_init();
		const char* markup = img[0].markup;
		int flags2 = 0;
		if(markup) {
			flags2 = atoi(markup);
			flags2 &= 4;
		}
		flags |= flags2;
		const unsigned char* data = img[0].data;
		size_t size = img[0].size;

		//Reset it really.
		instance->~GB();
		memset(instance, 0, sizeof(gambatte::GB));
		new(instance) gambatte::GB;
		instance->setInputGetter(&getinput);
		instance->set_walltime_fn(walltime_fn);
		memset(primary_framebuffer, 0, sizeof(primary_framebuffer));
		frame_overflow = 0;

		rtc_fixed = true;
		rtc_fixed_val = rtc_sec;
		instance->load(data, size, flags);
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
		size_t sramsize = instance->getSaveRam().second;
		size_t romsize = size;
		if(reallocate_debug || cur_ramsize != sramsize || cur_romsize != romsize) {
			if(debugbuf.cart) delete[] debugbuf.cart;
			if(debugbuf.sram) delete[] debugbuf.sram;
			debugbuf.cart = NULL;
			debugbuf.sram = NULL;
			if(sramsize) debugbuf.sram = new uint8_t[(sramsize + 4095) >> 12 << 12];
			if(romsize) debugbuf.cart = new uint8_t[(romsize + 4095) >> 12 << 12];
			if(sramsize) memset(debugbuf.sram, 0, (sramsize + 4095) >> 12 << 12);
			if(romsize) memset(debugbuf.cart, 0, (romsize + 4095) >> 12 << 12);
			memset(debugbuf.wram, 0, 32768);
			memset(debugbuf.ioamhram, 0, 512);
			debugbuf.wramcheat.clear();
			debugbuf.sramcheat.clear();
			debugbuf.cartcheat.clear();
			debugbuf.trace_cpu = false;
			reallocate_debug = false;
			cur_ramsize = sramsize;
			cur_romsize = romsize;
		}
		instance->set_debug_buffer(debugbuf);
#endif
		rtc_fixed = false;
		romdata.resize(size);
		memcpy(&romdata[0], data, size);
		internal_rom = inttype;
		do_reset_flag = false;

		for(unsigned i = 0; i < 12; i++)
			if(!palette_colors_default[i >> 2])
				instance->setDmgPaletteColor(i >> 2, i & 3, palette_colors[i]);

		return 1;
	}

	controller_set gambatte_controllerconfig(std::map<std::string, std::string>& settings)
	{
		std::map<std::string, std::string> _settings = settings;
		controller_set r;
		r.ports.push_back(&psystem);
		r.logical_map.push_back(std::make_pair(0, 1));
		return r;
	}

#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
	uint8_t gambatte_bus_iospace_rw(uint64_t offset, uint8_t data, bool write)
	{
		uint8_t val = 0;
		disable_breakpoints = true;
		if(write)
			instance->bus_write(offset, data);
		else
			val = instance->bus_read(offset);
		disable_breakpoints = false;
		return val;
	}
#endif

	std::list<core_vma_info> get_VMAlist()
	{
		std::list<core_vma_info> vmas;
		if(!internal_rom)
			return vmas;
		core_vma_info sram;
		core_vma_info wram;
		core_vma_info vram;
		core_vma_info ioamhram;
		core_vma_info rom;
		core_vma_info bus;

		auto g = instance->getSaveRam();
		sram.name = "SRAM";
		sram.base = 0x20000;
		sram.size = g.second;
		sram.backing_ram = g.first;
		sram.endian = -1;

		auto g2 = instance->getWorkRam();
		wram.name = "WRAM";
		wram.base = 0;
		wram.size = g2.second;
		wram.backing_ram = g2.first;
		wram.endian = -1;

		auto g3 = instance->getVideoRam();
		vram.name = "VRAM";
		vram.base = 0x10000;
		vram.size = g3.second;
		vram.backing_ram = g3.first;
		vram.endian = -1;

		auto g4 = instance->getIoRam();
		ioamhram.name = "IOAMHRAM";
		ioamhram.base = 0x18000;
		ioamhram.size = g4.second;
		ioamhram.backing_ram = g4.first;
		ioamhram.endian = -1;

		rom.name = "ROM";
		rom.base = 0x80000000;
		rom.size = romdata.size();
		rom.backing_ram = (void*)&romdata[0];
		rom.endian = -1;
		rom.readonly = true;

		if(sram.size)
			vmas.push_back(sram);
		vmas.push_back(wram);
		vmas.push_back(rom);
		vmas.push_back(vram);
		vmas.push_back(ioamhram);
#ifdef BSNES_SUPPORTS_ADV_BREAKPOINTS
		bus.name = "BUS";
		bus.base = 0x1000000;
		bus.size = 0x10000;
		bus.iospace_rw = gambatte_bus_iospace_rw;
		bus.endian = -1;
		vmas.push_back(bus);
#endif
		return vmas;
	}

	std::set<std::string> gambatte_srams()
	{
		std::set<std::string> s;
		if(!internal_rom)
			return s;
		auto g = instance->getSaveRam();
		if(g.second)
			s.insert("main");
		s.insert("rtc");
		return s;
	}

	std::string get_cartridge_name()
	{
		std::ostringstream name;
		if(romdata.size() < 0x200)
			return "";	//Bad.
		for(unsigned i = 0; i < 16; i++) {
			if(romdata[0x134 + i])
				name << (char)romdata[0x134 + i];
			else
				break;
		}
		return name.str();
	}

	void redraw_cover_fbinfo();

	struct _gambatte_core : public core_core, public core_region
	{
		_gambatte_core()
			: core_core({&psystem}, {
				{0, "Soft reset", "reset", {}},
				{1, "Change BG palette", "bgpalette", {
					{"Color 0","string:[0-9A-Fa-f]{6}"},
					{"Color 1","string:[0-9A-Fa-f]{6}"},
					{"Color 2","string:[0-9A-Fa-f]{6}"},
					{"Color 3","string:[0-9A-Fa-f]{6}"}
				}},{2, "Change SP1 palette", "sp1palette", {
					{"Color 0","string:[0-9A-Fa-f]{6}"},
					{"Color 1","string:[0-9A-Fa-f]{6}"},
					{"Color 2","string:[0-9A-Fa-f]{6}"},
					{"Color 3","string:[0-9A-Fa-f]{6}"}
				}}, {3, "Change SP2 palette", "sp2palette", {
					{"Color 0","string:[0-9A-Fa-f]{6}"},
					{"Color 1","string:[0-9A-Fa-f]{6}"},
					{"Color 2","string:[0-9A-Fa-f]{6}"},
					{"Color 3","string:[0-9A-Fa-f]{6}"}
				}}
			}),
			core_region({{"world", "World", 0, 0, false, {4389, 262144}, {0}}}) {}

		std::string c_core_identifier() { return "libgambatte "+gambatte::GB::version(); }
		bool c_set_region(core_region& region) { return (&region == this); }
		std::pair<uint32_t, uint32_t> c_video_rate() { return std::make_pair(262144, 4389); }
		double c_get_PAR() { return 1.0; }
		std::pair<uint32_t, uint32_t> c_audio_rate() {
			if(output_native)
				return std::make_pair(2097152, 1);
			else
				return std::make_pair(32768, 1);
		}
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc) {
			std::map<std::string, std::vector<char>> s;
			if(!internal_rom)
				return s;
			auto g = instance->getSaveRam();
			s["main"].resize(g.second);
			memcpy(&s["main"][0], g.first, g.second);
			s["rtc"].resize(8);
			time_t timebase = instance->getRtcBase();
			for(size_t i = 0; i < 8; i++)
				s["rtc"][i] = ((unsigned long long)timebase >> (8 * i));
			return s;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc) {
			if(!internal_rom)
				return;
			std::vector<char> x = sram.count("main") ? sram["main"] : std::vector<char>();
			std::vector<char> x2 = sram.count("rtc") ? sram["rtc"] : std::vector<char>();
			auto g = instance->getSaveRam();
			if(x.size()) {
				if(x.size() != g.second)
					messages << "WARNING: SRAM 'main': Loaded " << x.size()
						<< " bytes, but the SRAM is " << g.second << "." << std::endl;
				memcpy(g.first, &x[0], min(x.size(), g.second));
			}
			if(x2.size()) {
				time_t timebase = 0;
				for(size_t i = 0; i < 8 && i < x2.size(); i++)
					timebase |= (unsigned long long)(unsigned char)x2[i] << (8 * i);
				instance->setRtcBase(timebase);
			}
		}
		void c_serialize(std::vector<char>& out) {
			if(!internal_rom)
				throw std::runtime_error("Can't save without ROM");
			instance->saveState(out);
			size_t osize = out.size();
			out.resize(osize + 4 * sizeof(primary_framebuffer) / sizeof(primary_framebuffer[0]));
			for(size_t i = 0; i < sizeof(primary_framebuffer) / sizeof(primary_framebuffer[0]); i++)
				write32ube(&out[osize + 4 * i], primary_framebuffer[i]);
			out.push_back(frame_overflow >> 8);
			out.push_back(frame_overflow);
		}
		void c_unserialize(const char* in, size_t insize) {
			if(!internal_rom)
				throw std::runtime_error("Can't load without ROM");
			size_t foffset = insize - 2 - 4 * sizeof(primary_framebuffer) /
				sizeof(primary_framebuffer[0]);
			std::vector<char> tmp;
			tmp.resize(foffset);
			memcpy(&tmp[0], in, foffset);
			instance->loadState(tmp);
			for(size_t i = 0; i < sizeof(primary_framebuffer) / sizeof(primary_framebuffer[0]); i++)
				primary_framebuffer[i] = read32ube(&in[foffset + 4 * i]);

			unsigned x1 = (unsigned char)in[insize - 2];
			unsigned x2 = (unsigned char)in[insize - 1];
			frame_overflow = x1 * 256 + x2;
			do_reset_flag = false;
		}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height) {
			return std::make_pair(max(512 / width, (uint32_t)1), max(448 / height, (uint32_t)1));
		}
		void  c_install_handler() { magic_flags |= 2; }
		void c_uninstall_handler() {}
		void c_emulate() {
			if(!internal_rom)
				return;
			bool native_rate = output_native;
			int16_t reset = ecore_callbacks->get_input(0, 0, 1);
			if(reset) {
				instance->reset();
				messages << "GB(C) reset" << std::endl;
			}
			do_reset_flag = false;

			uint32_t samplebuffer[SAMPLES_PER_FRAME + 2064];
			int16_t soundbuf[2 * (SAMPLES_PER_FRAME + 2064)];
			size_t emitted = 0;
			while(true) {
				unsigned samples_emitted = SAMPLES_PER_FRAME - frame_overflow;
				long ret = instance->runFor(primary_framebuffer, 160, samplebuffer, samples_emitted);
				if(native_rate)
					for(unsigned i = 0; i < samples_emitted; i++) {
						soundbuf[emitted++] = (int16_t)(samplebuffer[i]);
						soundbuf[emitted++] = (int16_t)(samplebuffer[i] >> 16);
					}
				else
					for(unsigned i = 0; i < samples_emitted; i++) {
						uint32_t l = (int32_t)(int16_t)(samplebuffer[i]) + 32768;
						uint32_t r = (int32_t)(int16_t)(samplebuffer[i] >> 16) + 32768;
						accumulator_l += l;
						accumulator_r += r;
						accumulator_s++;
						if((accumulator_s & 63) == 0) {
							int16_t l2 = (accumulator_l >> 6) - 32768;
							int16_t r2 = (accumulator_r >> 6) - 32768;
							soundbuf[emitted++] = l2;
							soundbuf[emitted++] = r2;
							accumulator_l = accumulator_r = 0;
							accumulator_s = 0;
						}
					}
				ecore_callbacks->timer_tick(samples_emitted, 2097152);
				frame_overflow += samples_emitted;
				if(frame_overflow >= SAMPLES_PER_FRAME) {
					frame_overflow -= SAMPLES_PER_FRAME;
					break;
				}
			}
			framebuffer::info inf;
			inf.type = &_pixel_format_rgb32;
			inf.mem = const_cast<char*>(reinterpret_cast<const char*>(primary_framebuffer));
			inf.physwidth = 160;
			inf.physheight = 144;
			inf.physstride = 640;
			inf.width = 160;
			inf.height = 144;
			inf.stride = 640;
			inf.offset_x = 0;
			inf.offset_y = 0;

			framebuffer::raw ls(inf);
			ecore_callbacks->output_frame(ls, 262144, 4389);
			audioapi_submit_buffer(soundbuf, emitted / 2, true, native_rate ? 2097152 : 32768);
		}
		void c_runtosave() {}
		bool c_get_pflag() { return pflag; }
		void c_set_pflag(bool _pflag) { pflag = _pflag; }
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(cover_fbinfo);
			redraw_cover_fbinfo();
			return x;
		}
		std::string c_get_core_shortname() { return "gambatte"+gambatte::GB::version(); }
		void c_pre_emulate_frame(controller_frame& cf) {
			cf.axis3(0, 0, 1, do_reset_flag ? 1 : 0);
		}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
		{
			uint32_t a, b, c, d;
			switch(id) {
			case 0:		//Soft reset.
				do_reset_flag = true;
				break;
			case 1:		//Change DMG BG palette.
			case 2:		//Change DMG SP1 palette.
			case 3:		//Change DMG SP2 palette.
				a = strtoul(p[0].s.c_str(), NULL, 16);
				b = strtoul(p[1].s.c_str(), NULL, 16);
				c = strtoul(p[2].s.c_str(), NULL, 16);
				d = strtoul(p[3].s.c_str(), NULL, 16);
				palette_colors[4 * (id - 1) + 0] = a;
				palette_colors[4 * (id - 1) + 1] = b;
				palette_colors[4 * (id - 1) + 2] = c;
				palette_colors[4 * (id - 1) + 3] = d;
				palette_colors_default[id - 1] = false;
				if(instance) {
					instance->setDmgPaletteColor(id - 1, 0, a);
					instance->setDmgPaletteColor(id - 1, 1, b);
					instance->setDmgPaletteColor(id - 1, 2, c);
					instance->setDmgPaletteColor(id - 1, 3, d);
				}
			}
		}
		const interface_device_reg* c_get_registers() { return gb_registers; }
		unsigned c_action_flags(unsigned id) { return (id < 4) ? 1 : 0; }
		int c_reset_action(bool hard) { return hard ? -1 : 0; }
		std::pair<uint64_t, uint64_t> c_get_bus_map()
		{
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
			return std::make_pair(0x1000000, 0x10000);
#else
			return std::make_pair(0, 0);
#endif
		}
		std::list<core_vma_info> c_vma_list() { return get_VMAlist(); }
		std::set<std::string> c_srams() { return gambatte_srams(); }
		void c_set_debug_flags(uint64_t addr, unsigned int sflags, unsigned int cflags)
		{
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
			if(addr == 0 && sflags & 8) debugbuf.trace_cpu = true;
			if(addr == 0 && cflags & 8) debugbuf.trace_cpu = false;
			if(addr >= 0 && addr < 32768) {
				debugbuf.wram[addr] |= (sflags & 7);
				debugbuf.wram[addr] &= ~(cflags & 7);
			} else if(addr >= 0x20000 && addr < 0x20000 + instance->getSaveRam().second) {
				debugbuf.sram[addr - 0x20000] |= (sflags & 7);
				debugbuf.sram[addr - 0x20000] &= ~(cflags & 7);
			} else if(addr >= 0x18000 && addr < 0x18200) {
				debugbuf.ioamhram[addr - 0x18000] |= (sflags & 7);
				debugbuf.ioamhram[addr - 0x18000] &= ~(cflags & 7);
			} else if(addr >= 0x80000000 && addr < 0x80000000 + romdata.size()) {
				debugbuf.cart[addr - 0x80000000] |= (sflags & 7);
				debugbuf.cart[addr - 0x80000000] &= ~(cflags & 7);
			} else if(addr >= 0x1000000 && addr < 0x1010000) {
				debugbuf.bus[addr - 0x1000000] |= (sflags & 7);
				debugbuf.bus[addr - 0x1000000] &= ~(cflags & 7);
			} else if(addr == 0xFFFFFFFFFFFFFFFFULL) {
				//Set/Clear every known debug.
				for(unsigned i = 0; i < 32768; i++) {
					debugbuf.wram[i] |= ((sflags & 7) << 4);
					debugbuf.wram[i] &= ~((cflags & 7) << 4);
				}
				for(unsigned i = 0; i < 65536; i++) {
					debugbuf.bus[i] |= ((sflags & 7) << 4);
					debugbuf.bus[i] &= ~((cflags & 7) << 4);
				}
				for(unsigned i = 0; i < 512; i++) {
					debugbuf.ioamhram[i] |= ((sflags & 7) << 4);
					debugbuf.ioamhram[i] &= ~((cflags & 7) << 4);
				}
				for(unsigned i = 0; i < instance->getSaveRam().second; i++) {
					debugbuf.sram[i] |= ((sflags & 7) << 4);
					debugbuf.sram[i] &= ~((cflags & 7) << 4);
				}
				for(unsigned i = 0; i < romdata.size(); i++) {
					debugbuf.cart[i] |= ((sflags & 7) << 4);
					debugbuf.cart[i] &= ~((cflags & 7) << 4);
				}
			}
#endif
		}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set)
		{
#ifdef GAMBATTE_SUPPORTS_ADV_DEBUG
			if(addr >= 0 && addr < 32768) {
				if(set) {
					debugbuf.wram[addr] |= 8;
					debugbuf.wramcheat[addr] = value;
				} else {
					debugbuf.wram[addr] &= ~8;
					debugbuf.wramcheat.erase(addr);
				}
			} else if(addr >= 0x20000 && addr < 0x20000 + instance->getSaveRam().second) {
				auto addr2 = addr - 0x20000;
				if(set) {
					debugbuf.sram[addr2] |= 8;
					debugbuf.sramcheat[addr2] = value;
				} else {
					debugbuf.sram[addr2] &= ~8;
					debugbuf.sramcheat.erase(addr2);
				}
			} else if(addr >= 0x80000000 && addr < 0x80000000 + romdata.size()) {
				auto addr2 = addr - 0x80000000;
				if(set) {
					debugbuf.cart[addr2] |= 8;
					debugbuf.cartcheat[addr2] = value;
				} else {
					debugbuf.cart[addr2] &= ~8;
					debugbuf.cartcheat.erase(addr2);
				}
			}
#endif
		}
		void c_debug_reset()
		{
			//Next load will reset trace.
			reallocate_debug = true;
			palette_colors_default[0] = true;
			palette_colors_default[1] = true;
			palette_colors_default[2] = true;
		}
		std::vector<std::string> c_get_trace_cpus()
		{
			std::vector<std::string> r;
			r.push_back("cpu");
			return r;
		}
	} gambatte_core;

	struct _type_dmg : public core_type, public core_sysregion
	{
		_type_dmg()
			: core_type({{
				.iname = "dmg",
				.hname = "Game Boy",
				.id = 1,
				.sysname = "Gameboy",
				.bios = NULL,
				.regions = {&gambatte_core},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0, "gb;dmg"}},
				.settings = {},
				.core = &gambatte_core,
			}}),
			core_sysregion("gdmg", *this, gambatte_core) {}

		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom_common(img, gambatte::GB::FORCE_DMG, secs, subsecs, this);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return gambatte_controllerconfig(settings);
		}
	} type_dmg;

	struct _type_gbc : public core_type, public core_sysregion
	{
		_type_gbc()
			: core_type({{
				.iname = "gbc",
				.hname = "Game Boy Color",
				.id = 0,
				.sysname = "Gameboy",
				.bios = NULL,
				.regions = {&gambatte_core},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0, "gbc;cgb"}},
				.settings = {},
				.core = &gambatte_core,
			}}),
			core_sysregion("ggbc", *this, gambatte_core) {}

		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom_common(img, 0, secs, subsecs, this);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return gambatte_controllerconfig(settings);
		}
	} type_gbc;

	struct _type_gbca : public core_type, public core_sysregion
	{
		_type_gbca()
			: core_type({{
				.iname = "gbc_gba",
				.hname = "Game Boy Color (GBA)",
				.id = 2,
				.sysname = "Gameboy",
				.bios = NULL,
				.regions = {&gambatte_core},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0, ""}},
				.settings = {},
				.core = &gambatte_core,
			}}),
			core_sysregion("ggbca", *this, gambatte_core) {}

		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return load_rom_common(img, gambatte::GB::GBA_CGB, secs, subsecs, this);
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return gambatte_controllerconfig(settings);
		}
	} type_gbca;

	void redraw_cover_fbinfo()
	{
		for(size_t i = 0; i < sizeof(cover_fbmem) / sizeof(cover_fbmem[0]); i++)
			cover_fbmem[i] = 0x00000000;
		std::string ident = gambatte_core.get_core_identifier();
		cover_render_string(cover_fbmem, 0, 0, ident, 0xFFFFFF, 0x00000, 480, 432, 1920, 4);
		cover_render_string(cover_fbmem, 0, 16, "Internal ROM name: " + get_cartridge_name(),
			0xFFFFFF, 0x00000, 480, 432, 1920, 4);
		unsigned y = 32;
		for(auto i : cover_information()) {
			cover_render_string(cover_fbmem, 0, y, i, 0xFFFFFF, 0x000000, 480, 432, 1920, 4);
			y += 16;
		}
	}

	std::vector<char> cmp_save;

	command::fnptr<> cmp_save1(lsnes_cmd, "set-cmp-save", "", "\n", []() throw(std::bad_alloc,
		std::runtime_error) {
		if(!internal_rom)
			return;
		instance->saveState(cmp_save);
	});

	command::fnptr<> cmp_save2(lsnes_cmd, "do-cmp-save", "", "\n", []() throw(std::bad_alloc,
		std::runtime_error) {
		std::vector<char> x;
		if(!internal_rom)
			return;
		instance->saveState(x, cmp_save);
	});

	struct oninit {
		oninit()
		{
			register_sysregion_mapping("gdmg", "GB");
			register_sysregion_mapping("ggbc", "GBC");
			register_sysregion_mapping("ggbca", "GBC");
		}
	} _oninit;
}
