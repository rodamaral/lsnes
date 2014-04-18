/***************************************************************************
 *   Copyright (C) 2013 by Ilari Liusvaara                                 *
 *                                                                         *
 * This program is free software: you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
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
#include "core/framebuffer.hpp"
#include "core/window.hpp"
#include "interface/callbacks.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "library/framebuffer-pixfmt-rgb15.hpp"
#include "library/string.hpp"
//#include "library/portfn.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"
#include "library/framebuffer.hpp"
#define __LIBRETRO__
#include "ameteor.hpp"
#include "ameteor/cartmem.hpp"

uint64_t get_utime();

namespace
{
	bool do_reset_flag = false;
	bool internal_rom = false;
	bool rtc_fixed;
	time_t rtc_fixed_val;
	std::vector<unsigned char> romdata;
	uint16_t cover_fbmem[720 * 480];
	bool pflag = false;
	int16_t soundbuf[65536];
	size_t soundbuf_fill;
	bool frame_happened;
	bool just_reset;

	struct interface_device_reg gba_registers[] = {
		{NULL, NULL, NULL}
	};

	//Framebuffer.
	struct framebuffer::info cover_fbinfo = {
		&framebuffer::pixfmt_rgb15,	//Format.
		(char*)cover_fbmem,		//Memory.
		720, 480, 1440,			//Physical size.
		720, 480, 1440,			//Logical size.
		0, 0				//Offset.
	};

#include "ports.inc"

	struct _output
	{
		void frame(const uint16_t* p)
		{
			AMeteor::Stop();
			framebuffer::info inf;
			inf.type = &framebuffer::pixfmt_rgb15;
			inf.mem = const_cast<char*>(reinterpret_cast<const char*>(p));
			inf.physwidth = 240;
			inf.physheight = 160;
			inf.physstride = 480;
			inf.width = 240;
			inf.height = 160;
			inf.stride = 480;
			inf.offset_x = 0;
			inf.offset_y = 0;

			framebuffer::raw ls(inf);
			ecore_callbacks->output_frame(ls, 262144, 4389);
			ecore_callbacks->timer_tick(4389, 262144);
			static uint32_t refreshes = 0;
			static uint64_t samples = 0;
			refreshes++;
			static double srate = 4194304.0/95.0;
			if(soundbuf_fill > 0) {
				samples += soundbuf_fill / 2;
				audioapi_submit_buffer(soundbuf, soundbuf_fill / 2, true, srate);
				soundbuf_fill = 0;
			}
			frame_happened = true;
		}
		void sample(const int16_t* s)
		{
			soundbuf[soundbuf_fill++] = s[0];
			soundbuf[soundbuf_fill++] = s[1];
		}
		void vblank()
		{
		}
	} output;

	std::string fmt_sram_size(uint32_t sram_size)
	{
		std::string mult = "";
		if(sram_size > 1024) {
			mult = "k";
			sram_size >>= 10;
		}
		if(sram_size > 1024) {
			mult = "M";
			sram_size >>= 10;
		}
		return (stringfmt() << sram_size << mult << "B").str();
	}

	void meteor_bus_write(uint64_t offset, uint8_t data)
	{
		uint8_t* m = AMeteor::_memory.GetRealAddress(offset);
		if(m) *m = data;
	}

	uint8_t meteor_bus_read(uint64_t offset)
	{
		uint8_t* m = AMeteor::_memory.GetRealAddress(offset);
		return m ? *m : 0xFF;
	}

	void avsync_hack()
	{
		//Hack: Send 223 samples.
		for(unsigned i = 0; i < 446; i++)
			soundbuf[soundbuf_fill++] = 0;
	}

	void meteor_poll_buttons()
	{
		uint16_t x = 0;
		for(unsigned i = 0; i < 10; i++)
			if(ecore_callbacks->get_input(0, 1, i))
				x |= (1 << i);
		AMeteor::_keypad.SetPadState(x ^ 0x3FF);	//Inverse polarity.
		pflag = true;
	}

	void basic_init()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		AMeteor::_memory.LoadCartInferred();
		AMeteor::_lcd.sig_vblank.connect(syg::mem_fun(output, &_output::vblank));
		AMeteor::_lcd.GetScreen().GetRenderer().SetFrameSlot(syg::mem_fun(output, &_output::frame));
		AMeteor::_sound.GetSpeaker().SetFrameSlot(syg::mem_fun(output, &_output::sample));
	}

	controller_set meteor_controllerconfig(std::map<std::string, std::string>& settings)
	{
		std::map<std::string, std::string> _settings = settings;
		controller_set r;
		r.ports.push_back(&psystem);
		r.logical_map.push_back(std::make_pair(0, 1));
		return r;
	}

	std::pair<uint64_t, uint64_t> meteor_bus_map()
	{
		return std::make_pair(0x100000000ULL, 0x100000000ULL);
	}

	void add_vma_mapped(std::list<core_vma_info>& l, const std::string& name, uint64_t base, uint8_t* ram,
		size_t ramsize, int endian, bool readonly)
	{
		core_vma_info v;
		v.name = name;
		v.base = base;
		v.size = ramsize;
		v.backing_ram = ram;
		v.endian = endian;
		v.readonly = readonly;
		v.read = NULL;
		v.write = NULL;
		l.push_back(v);
	}

	uint64_t get_vmabase(const std::string& name)
	{
		static std::map<std::string, uint64_t> unknown;
		uint64_t unknown_next = 0x200000000ULL;
		if(name == "brom") return 0x90000000ULL;
		if(name == "wbram") return 0;
		if(name == "wcram") return 0x40000ULL;
		if(name == "pram") return 0x100000ULL;
		if(name == "vram") return 0x110000ULL;
		if(name == "oam") return 0x120000ULL;
		if(name == "sram") return 0x10000000ULL;
		if(name == "rom") return 0x80000000ULL;
		//Unknown.
		if(!unknown.count(name)) {
			unknown[name] = unknown_next;
			unknown_next += 0x100000000ULL;
		}
		return unknown[name];
	}

	bool is_read_only_vma(const std::string& name)
	{
		if(name == "wbram") return false;
		if(name == "wcram") return false;
		if(name == "pram") return false;
		if(name == "vram") return false;
		if(name == "oam") return false;
		if(name == "sram") return false;
		//Dunno what this is.
		return true;
	}

	std::list<core_vma_info> get_VMAlist()
	{
		std::list<core_vma_info> vmas;
		if(!internal_rom)
			return vmas;
		auto mem_map = AMeteor::_memory.GetMemories();
		for(auto i : mem_map)
			add_vma_mapped(vmas, i.first, get_vmabase(i.first), i.second.first, i.second.second, -1,
				is_read_only_vma(i.first));
		//Bus mapping.
		core_vma_info bus;
		bus.name = "BUS";
		bus.base = 0x100000000ULL;
		bus.size = 0x100000000ULL;
		bus.backing_ram = NULL;
		bus.endian = -1;
		bus.readonly = false;
		bus.read = meteor_bus_read;
		bus.write = meteor_bus_write;
		vmas.push_back(bus);

		return vmas;
	}

	std::set<std::string> meteor_srams()
	{
		std::set<std::string> s;
		if(!internal_rom)
			return s;
		auto mem_map = AMeteor::_memory.GetMemories();
		if(mem_map.count("sram"))
			s.insert("sram");
		return s;
	}

	std::string get_cartridge_name()
	{
		std::ostringstream name;
		if(romdata.size() < 192)
			return "";	//Bad.
		for(unsigned i = 0; i < 12; i++) {
			if(romdata[0x0A0 + i])
				name << (char)romdata[0xA0 + i];
			else
				break;
		}
		name << "[AGB-";
		for(unsigned i = 0; i < 4; i++)
			name << (char)romdata[0xAC + i];
		name << "] (version " << (int)(unsigned char)romdata[0xBC];
		if((unsigned char)romdata[0x9C] == 0xA5)
			name << "-Debug";
		name << ")";
		return name.str();
	}

	void redraw_cover_fbinfo();

	struct _meteor_core : public core_core, public core_type, public core_region, public core_sysregion
	{
		_meteor_core()
			: core_core({&psystem}, {{0, "Soft reset", "reset", {}}}),
			core_type({{
				.iname = "agb",
				.hname = "Game Boy Advance",
				.id = 0,
				.sysname = "GBA",
				.bios = NULL,
				.regions = {this},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0, "gba;agb"}},
				.settings = {{"extbios", "Use GBA BIOS", "0", {
					{"0", "False", 0},
					{"1", "True", 1}
				}}},
				.core = this,
			}}),
			core_region({{"world", "World", 0, 0, false, {4389, 262144}, {0}}}),
			core_sysregion("magb", *this, *this) {}

		std::string c_core_identifier() { return "meteor 1.4.0"; }
		bool c_set_region(core_region& region) { return (&region == this); }
		std::pair<uint32_t, uint32_t> c_video_rate() { return std::make_pair(262144, 4389); }
		std::pair<uint32_t, uint32_t> c_audio_rate() { return std::make_pair(4194304, 95); }
		std::map<std::string, std::vector<char>>  c_save_sram() throw(std::bad_alloc) {
			std::map<std::string, std::vector<char>> s;
			if(!internal_rom)
				return s;
			std::vector<char> sram;
			uint32_t realsize = *(uint32_t*)(AMeteor::CartMemData + AMeteor::CartMem::MAX_SIZE);
			sram.resize(realsize);
			memcpy(&sram[0], AMeteor::CartMemData, realsize);
			s["sram"] = sram;
			return s;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc) {
			if(!internal_rom)
				return;
			if(!sram.count("sram")) {
				//Don't clear the SRAM descriptor.
				memset(AMeteor::CartMemData, 255, AMeteor::CartMem::MAX_SIZE);
				return;
			}
			std::vector<char>& s = sram["sram"];
			//Read the size from SRAM descriptor.
			uint32_t realsize = *(uint32_t*)(AMeteor::CartMemData + AMeteor::CartMem::MAX_SIZE);
			//Don't clear the SRAM descriptor.
			memset(AMeteor::CartMemData, 255, AMeteor::CartMem::MAX_SIZE);
			if(s.size() != realsize)
				messages << "Unexpected SRAM size, expected " << realsize << " got " << s.size()
					<< std::endl;
			memcpy(AMeteor::CartMemData, &s[0], min((size_t)realsize, s.size()));
		}
		void c_serialize(std::vector<char>& out) {
			if(!internal_rom)
				throw std::runtime_error("Can't save without ROM");
			std::ostringstream stream;
			AMeteor::SaveState(stream);
			std::string s = stream.str();
			out.resize(s.length());
			std::copy(s.begin(), s.end(), out.begin());
		}
		void c_unserialize(const char* in, size_t insize) {
			if(!internal_rom)
				throw std::runtime_error("Can't load without ROM");
			std::istringstream stream;
			stream.str(std::string((char*)in, insize));
			AMeteor::LoadState(stream);
			do_reset_flag = false;
		}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height) {
			return std::make_pair(max(720 / width, (uint32_t)1), max(480 / height, (uint32_t)1));
		}
		void  c_install_handler() { magic_flags |= 4; }
		void c_uninstall_handler() {}
		void c_emulate() {
			if(!internal_rom)
				return;
			int16_t reset = ecore_callbacks->get_input(0, 0, 1);
			if(reset) {
				AMeteor::Reset(AMeteor::UNIT_ALL & ~AMeteor::UNIT_MEMORY_BIOS &
					~AMeteor::UNIT_MEMORY_ROM);
				just_reset = true;
				messages << "GBA reset" << std::endl;
			}
			do_reset_flag = false;
			meteor_poll_buttons();
			frame_happened = false;
			if(just_reset) {
				avsync_hack();
				just_reset = false;
			}
			while(!frame_happened) {
				AMeteor::Run(281000);
				if(!frame_happened)
					std::cerr << "Warning: Run timeout" << std::endl;
			}
		}
		void c_runtosave() {}
		bool c_get_pflag() { return AMeteor::_io.GetPolled(); }
		void c_set_pflag(bool _pflag) { AMeteor::_io.SetPolled(_pflag); }
		void c_request_reset(long delay, bool hard) { do_reset_flag = true; }
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(cover_fbinfo);
			redraw_cover_fbinfo();
			return x;
		}
		std::string c_get_core_shortname() { return "meteor140"; }
		void c_pre_emulate_frame(controller_frame& cf) {
			cf.axis3(0, 0, 1, do_reset_flag ? 1 : 0);
		}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
		{
			if(id == 0)
				do_reset_flag = true;
		}
		unsigned int c_action_flags(unsigned id)
		{
			if(id == 0) return 1;
			return 0;
		}
		const interface_device_reg* c_get_registers() { return gba_registers; }
		int c_reset_action(bool hard) { return hard ? -1 : 0; }
		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
			uint64_t rtc_subsec) {
			uint32_t sram_size = 0;
			std::map<std::string, std::string> _settings = settings;
			get_settings().fill_defaults(_settings);
			basic_init();
			const char* markup = img[0].markup;
			if(!markup)
				markup = "";
			std::string _markup = markup;
			std::istringstream imarkup(markup);
			const unsigned char* data = img[0].data;
			size_t size = img[0].size;
			romdata.resize(size);
			memcpy(&romdata[0], data, size);
			std::string markup_line;

			while(std::getline(imarkup, markup_line)) {
				regex_results r;
				istrip_CR(markup_line);
				if(r = regex("sram_size=([0-9]+)", markup_line)) {
					try {
						sram_size = parse_value<uint32_t>(r[1]);
					} catch(...) {
					}
				} else if(r = regex("sram_size=([0-9]+)k", markup_line)) {
					try {
						sram_size = 1024 * parse_value<uint32_t>(r[1]);
					} catch(...) {
					}
				} else
					messages << "Unknown markup: " << markup_line << std::endl;
			}

			AMeteor::Reset(AMeteor::UNIT_ALL);
			if(_settings["extbios"] != "0") {
				//Load the BIOS.
				std::string bname = ecore_callbacks->get_firmware_path() + "/gbabios.bin";
				if(!AMeteor::_memory.LoadBios(bname.c_str())) {
					messages << "Can't load GBA BIOS" << std::endl;
					return -1;
				}
			}
			AMeteor::_memory.LoadRom((const uint8_t*)data, size);
			*(uint32_t*)(AMeteor::CartMemData + AMeteor::CartMem::MAX_SIZE) = sram_size;
			if(sram_size > 0)
				messages << "SRAM size: " << fmt_sram_size(sram_size) << std::endl;
			AMeteor::_memory.LoadCartInferred();
			internal_rom = true;
			do_reset_flag = false;
			just_reset = true;
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return meteor_controllerconfig(settings);
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map() { return meteor_bus_map(); }
		std::list<core_vma_info> c_vma_list() { return get_VMAlist(); }
		std::set<std::string> c_srams() { return meteor_srams(); }
		double c_get_PAR() { return 1.0; }
		void c_set_debug_flags(uint64_t addr, unsigned flags_set, unsigned flags_clear) {}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set) {}
		std::vector<std::string> c_get_trace_cpus() { return std::vector<std::string>(); }
		void c_debug_reset() {}
	} meteor_core;

	void redraw_cover_fbinfo()
	{
		for(size_t i = 0; i < sizeof(cover_fbmem) / sizeof(cover_fbmem[0]); i++)
			cover_fbmem[i] = 0x0000;
		std::string ident = meteor_core.c_core_identifier();
		cover_render_string(cover_fbmem, 0, 0, ident, 0xFFFF, 0x0000, 720, 480, 1440, 2);
		cover_render_string(cover_fbmem, 0, 16, "Internal ROM name: " + get_cartridge_name(), 0xFFFF, 0x0000,
			720, 480, 1440, 2);
		unsigned y = 32;
		for(auto i : cover_information()) {
			cover_render_string(cover_fbmem, 0, y, i, 0xFFFF, 0x0000, 720, 480, 1440, 2);
			y += 16;
		}
	}
}
