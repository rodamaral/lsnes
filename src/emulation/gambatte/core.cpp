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
	unsigned frame_overflow = 0;
	std::vector<unsigned char> romdata;
	uint32_t cover_fbmem[480 * 432];
	uint32_t primary_framebuffer[160*144];
	uint32_t accumulator_l = 0;
	uint32_t accumulator_r = 0;
	unsigned accumulator_s = 0;
	bool pflag = false;


	struct interface_device_reg gb_registers[] = {
		{"wrambank", []() -> uint64_t { return instance ? instance->getIoRam().first[0x170] & 0x07 : 0; },
			[](uint64_t v) {}},
		{NULL, NULL, NULL}
	};

	//Framebuffer.
	struct framebuffer_info cover_fbinfo = {
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

	void basic_init()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		instance = new gambatte::GB;
		instance->setInputGetter(&getinput);
		instance->set_walltime_fn(walltime_fn);
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
		rtc_fixed = false;
		romdata.resize(size);
		memcpy(&romdata[0], data, size);
		internal_rom = inttype;
		do_reset_flag = false;
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
			framebuffer_info inf;
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

			framebuffer_raw ls(inf);
			ecore_callbacks->output_frame(ls, 262144, 4389);
			audioapi_submit_buffer(soundbuf, emitted / 2, true, native_rate ? 2097152 : 32768);
		}
		void c_runtosave() {}
		bool c_get_pflag() { return pflag; }
		void c_set_pflag(bool _pflag) { pflag = _pflag; }
		framebuffer_raw& c_draw_cover() {
			static framebuffer_raw x(cover_fbinfo);
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
			return std::make_pair(0, 0);
		}
		std::list<core_vma_info> c_vma_list() { return get_VMAlist(); }
		std::set<std::string> c_srams() { return gambatte_srams(); }
	} gambatte_core;

	struct _type_dmg : public core_type, public core_sysregion
	{
		_type_dmg()
			: core_type({{
				.iname = "dmg",
				.hname = "Game Boy",
				.id = 1,
				.sysname = "Gameboy",
				.extensions = "gb;dmg",
				.bios = NULL,
				.regions = {&gambatte_core},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0}},
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
				.extensions = "gbc;cgb",
				.bios = NULL,
				.regions = {&gambatte_core},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0}},
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
				.extensions = "",
				.bios = NULL,
				.regions = {&gambatte_core},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0}},
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

	function_ptr_command<> cmp_save1(lsnes_cmd, "set-cmp-save", "", "\n", []() throw(std::bad_alloc,
		std::runtime_error) {
		if(!internal_rom)
			return;
		instance->saveState(cmp_save);
	});

	function_ptr_command<> cmp_save2(lsnes_cmd, "do-cmp-save", "", "\n", []() throw(std::bad_alloc,
		std::runtime_error) {
		std::vector<char> x;
		if(!internal_rom)
			return;
		instance->saveState(x, cmp_save);
	});
}
