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

namespace
{
	uint32_t cover_fbmem[480 * 432];
	bool pflag = false;

	//Framebuffer.
	struct framebuffer::info cover_fbinfo = {
		&_pixel_format_rgb32,		//Format.
		(char*)cover_fbmem,		//Memory.
		480, 432, 1920,			//Physical size.
		480, 432, 1920,			//Logical size.
		0, 0				//Offset.
	};

	struct interface_device_reg test_registers[] = {
		{NULL, NULL, NULL}
	};

#include "ports.inc"

	controller_set test_controllerconfig(std::map<std::string, std::string>& settings)
	{
		std::map<std::string, std::string> _settings = settings;
		controller_set r;
		r.ports.push_back(&psystem);
		r.ports.push_back(&ptype1);
		r.ports.push_back(&ptype2);
		r.logical_map.push_back(std::make_pair(1, 0));
		r.logical_map.push_back(std::make_pair(2, 0));
		return r;
	}

	void redraw_cover_fbinfo()
	{
		for(size_t i = 0; i < sizeof(cover_fbmem) / sizeof(cover_fbmem[0]); i++)
			cover_fbmem[i] = 0x00000000;
		cover_render_string(cover_fbmem, 0, 0, "TEST MODE", 0xFFFFFF, 0x00000, 480, 432, 1920, 4);
	}

	void redraw_screen()
	{
		for(size_t i = 0; i < sizeof(cover_fbmem) / sizeof(cover_fbmem[0]); i++)
			cover_fbmem[i] = 0x00000000;
		{
			std::ostringstream str;
			unsigned k = 0;
			for(unsigned i = 0; i < 6; i++)
				str << ecore_callbacks->get_input(1, 0, i) << " ";
			for(unsigned i = 0; i < 15; i++)
				if(ecore_callbacks->get_input(1, 0, i + 6)) k |= (1 << i);
			str << std::hex << std::setw(4) << std::setfill('0') << k;
			cover_render_string(cover_fbmem, 0, 0, str.str(), 0xFFFFFF, 0x00000, 480, 432, 1920, 4);
		}
		{
			std::ostringstream str;
			unsigned k = 0;
			for(unsigned i = 0; i < 6; i++)
				str << ecore_callbacks->get_input(2, 0, i) << " ";
			for(unsigned i = 0; i < 15; i++)
				if(ecore_callbacks->get_input(2, 0, i + 6)) k |= (1 << i);
			str << std::hex << std::setw(4) << std::setfill('0') << k;
			cover_render_string(cover_fbmem, 0, 16, str.str(), 0xFFFFFF, 0x00000, 480, 432, 1920, 4);
		}
	}

	struct _test_core : public core_core, public core_type, public core_region, public core_sysregion
	{
		_test_core()
			: core_core({&psystem, &ptype1, &ptype2}, {
				{0, "xyzzy", "xyzzy", {
					{"Magic", "enum:[\"foo\",\"bar\",\"baz\",[\"qux\",\"zot\"]]"}
				}}
			}),
			core_type({{
				.iname = "test",
				.hname = "test",
				.id = 0,
				.sysname = "Test",
				.bios = NULL,
				.regions = {this},
				.images = {{"rom", "Cartridge ROM", 1, 0, 0, "test"}},
				.settings = {},
				.core = this,
			}}),
			core_region({{"world", "World", 0, 0, false, {1, 60}, {0}}}),
			core_sysregion("test", *this, *this) { hide(); }

		std::string c_core_identifier() { return "TEST"; }
		bool c_set_region(core_region& region) { return (&region == this); }
		std::pair<uint32_t, uint32_t> c_video_rate() { return std::make_pair(60, 1); }
		double c_get_PAR() { return 1.0; }
		std::pair<uint32_t, uint32_t> c_audio_rate() { return std::make_pair(48000, 1); }
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc) {
			std::map<std::string, std::vector<char>> s;
			return s;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc) {}
		void c_serialize(std::vector<char>& out) { out.clear(); }
		void c_unserialize(const char* in, size_t insize) {}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height) {
			return std::make_pair(max(512 / width, (uint32_t)1), max(448 / height, (uint32_t)1));
		}
		void  c_install_handler() {}
		void c_uninstall_handler() {}
		void c_emulate() {
			int16_t audio[800] = {0};
			pflag = false;
			redraw_screen();
			framebuffer::info inf;
			inf.type = &_pixel_format_rgb32;
			inf.mem = const_cast<char*>(reinterpret_cast<const char*>(cover_fbmem));
			inf.physwidth = 480;
			inf.physheight = 432;
			inf.physstride = 1920;
			inf.width = 480;
			inf.height = 432;
			inf.stride = 1920;
			inf.offset_x = 0;
			inf.offset_y = 0;
			framebuffer::raw ls(inf);
			ecore_callbacks->output_frame(ls, 60,1);
			audioapi_submit_buffer(audio, 800, false, 48000);
		}
		void c_runtosave() {}
		bool c_get_pflag() { return pflag; }
		void c_set_pflag(bool _pflag) { pflag = _pflag; }
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(cover_fbinfo);
			redraw_cover_fbinfo();
			return x;
		}
		std::string c_get_core_shortname() { return "test"; }
		void c_pre_emulate_frame(controller_frame& cf) {}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
		{
			if(id == 0)
				messages << "ID #0, choice: " << p[0].i << std::endl;
		}
		const interface_device_reg* c_get_registers() { return test_registers; }
		int t_load_rom(core_romimage* images, std::map<std::string, std::string>& settings,
			uint64_t rtc_sec, uint64_t rtc_subsec)
		{
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			return test_controllerconfig(settings);
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map() { return std::make_pair(0, 0); }
		std::list<core_vma_info> c_vma_list() { return std::list<core_vma_info>(); }
		std::set<std::string> c_srams() { return std::set<std::string>(); }
		unsigned c_action_flags(unsigned id) { return 1; }
		int c_reset_action(bool hard) { return -1; }
		void c_set_debug_flags(uint64_t addr, unsigned int sflags, unsigned int cflags) {}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set) {}
		void c_debug_reset() {}
		std::vector<std::string> c_get_trace_cpus()
		{
			return std::vector<std::string>();
		}
	} test_core;
}
