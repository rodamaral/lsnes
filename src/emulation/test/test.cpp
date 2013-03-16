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
#include "library/portfn.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"
#include "library/framebuffer.hpp"

namespace
{
	uint32_t cover_fbmem[480 * 432];
	bool pflag = false;

	//Framebuffer.
	struct framebuffer_info cover_fbinfo = {
		&_pixel_format_rgb32,		//Format.
		(char*)cover_fbmem,		//Memory.
		480, 432, 1920,			//Physical size.
		480, 432, 1920,			//Logical size.
		0, 0				//Offset.
	};

#include "ports.inc"
#include "slots.inc"
#include "regions.inc"

	core_setting_group test_settings;

	port_index_triple t(unsigned p, unsigned c, unsigned i, bool nl)
	{
		port_index_triple x;
		x.valid = true;
		x.port = p;
		x.controller = c;
		x.control = i;
		return x;
	}

	controller_set _controllerconfig(std::map<std::string, std::string>& settings)
	{
		std::map<std::string, std::string> _settings = settings;
		controller_set r;
		r.ports.push_back(&psystem);
		r.ports.push_back(&ptype1);
		r.ports.push_back(&ptype2);
		for(unsigned i = 0; i < 5; i++)
			r.portindex.indices.push_back(t(0, 0, i, false));
		for(unsigned i = 0; i < 21; i++)
			r.portindex.indices.push_back(t(1, 0, i, true));
		for(unsigned i = 0; i < 21; i++)
			r.portindex.indices.push_back(t(2, 0, i, true));
		r.portindex.logical_map.push_back(std::make_pair(1, 0));
		r.portindex.logical_map.push_back(std::make_pair(2, 0));
		r.portindex.pcid_map.push_back(std::make_pair(1, 0));
		r.portindex.pcid_map.push_back(std::make_pair(2, 0));
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

	extern core_core test_core;

	core_core_params _test_core = {
		.core_identifier = []() -> std::string { return "TEST"; },
		.set_region = [](core_region& region) -> bool { return (&region == &region_world); },
		.video_rate = []() -> std::pair<uint32_t, uint32_t> { return std::make_pair(60, 1); },
		.audio_rate = []() -> std::pair<uint32_t, uint32_t> { return std::make_pair(48000, 1); },
		.snes_rate = NULL,
		.save_sram = []() -> std::map<std::string, std::vector<char>> {
			std::map<std::string, std::vector<char>> s;
			return s;
		},
		.load_sram = [](std::map<std::string, std::vector<char>>& sram) -> void {},
		.serialize = [](std::vector<char>& out) -> void { out.clear(); },
		.unserialize = [](const char* in, size_t insize) -> void {},
		.get_region = []() -> core_region& { return region_world; },
		.power = []() -> void {},
		.unload_cartridge = []() -> void {},
		.get_scale_factors = [](uint32_t width, uint32_t height) -> std::pair<uint32_t, uint32_t> {
			return std::make_pair(max(512 / width, (uint32_t)1), max(448 / height, (uint32_t)1));
		},
		.install_handler = []() -> void  { test_core.hide(); },
		.uninstall_handler = []() -> void {},
		.emulate = []() -> void {
			pflag = false;
			redraw_screen();
			framebuffer_info inf;
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
			framebuffer_raw ls(inf);
			ecore_callbacks->output_frame(ls, 60,1);
		},
		.runtosave = []() -> void {},
		.get_pflag = []() -> bool { return pflag; },
		.set_pflag = [](bool _pflag) -> void { pflag = _pflag; },
		.request_reset = [](long delay, bool hard) -> void {},
		.port_types = port_types,
		.draw_cover = []() -> framebuffer_raw& {
			static framebuffer_raw x(cover_fbinfo);
			redraw_cover_fbinfo();
			return x;
		},
		.get_core_shortname = []() -> std::string { return "test"; },
		.pre_emulate_frame = [](controller_frame& cf) -> void {}
	};

	core_core test_core(_test_core);
	
	core_type_params  _type_test = {
		.iname = "test", .hname = "test", .id = 0, .reset_support = 0,
		.load_rom = [](core_romimage* img, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
			uint64_t rtc_subsec) -> int { return 0; },
		.controllerconfig = _controllerconfig, .extensions = "test", .bios = NULL, .regions = test_regions,
		.images = test_images, .settings = &test_settings, .core = &test_core,
		.get_bus_map = []() -> std::pair<uint64_t, uint64_t> { return std::make_pair(0, 0); }, 
		.vma_list = []() -> std::list<core_vma_info> { return std::list<core_vma_info>();},
		.srams = []() -> std::set<std::string> { return std::set<std::string>();}
	};

	core_type type_test(_type_test);
}
