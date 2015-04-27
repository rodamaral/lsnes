#include "state.hpp"
#include "romimage.hpp"
#include "framebuffer.hpp"
#include "instance.hpp"
#include "logic.hpp"
#include "demo.hpp"
#include "core/dispatch.hpp"
#include "core/audioapi.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "library/framebuffer-pixfmt-rgb32.hpp"

namespace sky
{
	bool pflag;
	int cstyle = 0;
	const unsigned iindexes[3][7] = {
		{0, 1, 2, 3, 4, 5, 6},
		{6, 7, 4, 5, 8, 3, 2},
		{5, 4, 6, 7, 0, 3, 2}
	};

	struct interface_device_reg sky_registers[] = {
		{NULL, NULL, NULL}
	};

	//Framebuffer.
	uint32_t cover_fbmem[320*200];
	struct framebuffer::info cover_fbinfo = {
		&framebuffer::pixfmt_rgb32,		//Format.
		(char*)cover_fbmem,		//Memory.
		320, 200, 1280,			//Physical size.
		320, 200, 1280,			//Logical size.
		0, 0				//Offset.
	};

	struct instance corei;

	portctrl::controller X4 = {"(system)", "(system)", {
		{portctrl::button::TYPE_BUTTON, 'F', "framesync", true}
	}};
	portctrl::controller X8 = {"sky", "sky", {
		{portctrl::button::TYPE_BUTTON, 'L', "left", true},
		{portctrl::button::TYPE_BUTTON, 'R', "right", true},
		{portctrl::button::TYPE_BUTTON, 'A', "up", true},
		{portctrl::button::TYPE_BUTTON, 'D', "down", true},
		{portctrl::button::TYPE_BUTTON, 'J', "A", true},
		{portctrl::button::TYPE_BUTTON, 'S', "start", true},
		{portctrl::button::TYPE_BUTTON, 's', "select", true},
		{portctrl::button::TYPE_NULL, '\0', "", true},
		{portctrl::button::TYPE_NULL, '\0', "", true},
	}};

	portctrl::controller A8 = {"gamepad", "sky", {
		{portctrl::button::TYPE_NULL, '\0', "", false},
		{portctrl::button::TYPE_NULL, '\0', "", false},
		{portctrl::button::TYPE_BUTTON, 's', "select", false},
		{portctrl::button::TYPE_BUTTON, 'S', "start", false},
		{portctrl::button::TYPE_BUTTON, 'A', "up", false},
		{portctrl::button::TYPE_BUTTON, 'D', "down", false},
		{portctrl::button::TYPE_BUTTON, 'L', "left", false},
		{portctrl::button::TYPE_BUTTON, 'R', "right", false},
		{portctrl::button::TYPE_BUTTON, 'J', "A", false},
	}};
	portctrl::controller B8 = {"gb", "sky", {
		{portctrl::button::TYPE_BUTTON, 'J', "A", false},
		{portctrl::button::TYPE_NULL, '\0', "", false},
		{portctrl::button::TYPE_BUTTON, 's', "select", false},
		{portctrl::button::TYPE_BUTTON, 'S', "start", false},
		{portctrl::button::TYPE_BUTTON, 'R', "right", false},
		{portctrl::button::TYPE_BUTTON, 'L', "left", false},
		{portctrl::button::TYPE_BUTTON, 'A', "up", false},
		{portctrl::button::TYPE_BUTTON, 'D', "down", false},
		{portctrl::button::TYPE_NULL, '\0', "", false},
	}};
	portctrl::controller C8 = {"gba", "sky", {
		{portctrl::button::TYPE_BUTTON, 'J', "A", false},
		{portctrl::button::TYPE_NULL, '\0', "", false},
		{portctrl::button::TYPE_BUTTON, 's', "select", false},
		{portctrl::button::TYPE_BUTTON, 'S', "start", false},
		{portctrl::button::TYPE_BUTTON, 'R', "right", false},
		{portctrl::button::TYPE_BUTTON, 'L', "left", false},
		{portctrl::button::TYPE_BUTTON, 'A', "up", false},
		{portctrl::button::TYPE_BUTTON, 'D', "down", false},
		{portctrl::button::TYPE_NULL, '\0', "", false},
	}};

	portctrl::controller_set X2 = {"sky", "sky", "sky", {X4, X8},{0}};

	void port_write(const portctrl::type* _this, unsigned char* buffer, unsigned idx, unsigned ctrl, short x)
	{
		switch(idx) {
		case 0:
			switch(ctrl) {
			case 0: if(x) buffer[0] |= 1; else buffer[0] &= ~1; break;
			};
			break;
		case 1:
			switch(256 * cstyle + ctrl) {
			case 0: if(x) buffer[0] |= 2; else buffer[0] &= ~2; break;
			case 1: if(x) buffer[0] |= 4; else buffer[0] &= ~4; break;
			case 2: if(x) buffer[0] |= 8; else buffer[0] &= ~8; break;
			case 3: if(x) buffer[0] |= 16; else buffer[0] &= ~16; break;
			case 4: if(x) buffer[0] |= 32; else buffer[0] &= ~32; break;
			case 5: if(x) buffer[0] |= 64; else buffer[0] &= ~64; break;
			case 6: if(x) buffer[0] |= 128; else buffer[0] &= ~128; break;

			case 258: if(x) buffer[0] |= 128; else buffer[0] &= ~128; break;
			case 259: if(x) buffer[0] |= 64; else buffer[0] &= ~64; break;
			case 260: if(x) buffer[0] |= 8; else buffer[0] &= ~8; break;
			case 261: if(x) buffer[0] |= 16; else buffer[0] &= ~16; break;
			case 262: if(x) buffer[0] |= 2; else buffer[0] &= ~2; break;
			case 263: if(x) buffer[0] |= 4; else buffer[0] &= ~4; break;
			case 264: if(x) buffer[0] |= 32; else buffer[0] &= ~32; break;

			case 512: if(x) buffer[0] |= 32; else buffer[0] &= ~32; break;
			case 514: if(x) buffer[0] |= 128; else buffer[0] &= ~128; break;
			case 515: if(x) buffer[0] |= 64; else buffer[0] &= ~64; break;
			case 516: if(x) buffer[0] |= 4; else buffer[0] &= ~4; break;
			case 517: if(x) buffer[0] |= 2; else buffer[0] &= ~2; break;
			case 518: if(x) buffer[0] |= 8; else buffer[0] &= ~8; break;
			case 519: if(x) buffer[0] |= 16; else buffer[0] &= ~16; break;
			};
		};
	}

	short port_read(const portctrl::type* _this, const unsigned char* buffer, unsigned idx, unsigned ctrl)
	{
		switch(idx) {
		case 0:
			switch(ctrl) {
			case 0: return (buffer[0] & 1) ? 1 : 0;
			}
			break;
		case 1:
			switch(256 * cstyle + ctrl) {
			case 0: return (buffer[0] & 2) ? 1 : 0;
			case 1: return (buffer[0] & 4) ? 1 : 0;
			case 2: return (buffer[0] & 8) ? 1 : 0;
			case 3: return (buffer[0] & 16) ? 1 : 0;
			case 4: return (buffer[0] & 32) ? 1 : 0;
			case 5: return (buffer[0] & 64) ? 1 : 0;
			case 6: return (buffer[0] & 128) ? 1 : 0;

			case 258: return (buffer[0] & 128) ? 1 : 0;
			case 259: return (buffer[0] & 64) ? 1 : 0;
			case 260: return (buffer[0] & 8) ? 1 : 0;
			case 261: return (buffer[0] & 16) ? 1 : 0;
			case 262: return (buffer[0] & 2) ? 1 : 0;
			case 263: return (buffer[0] & 4) ? 1 : 0;
			case 264: return (buffer[0] & 32) ? 1 : 0;

			case 512: return (buffer[0] & 32) ? 1 : 0;
			case 514: return (buffer[0] & 128) ? 1 : 0;
			case 515: return (buffer[0] & 64) ? 1 : 0;
			case 516: return (buffer[0] & 4) ? 1 : 0;
			case 517: return (buffer[0] & 2) ? 1 : 0;
			case 518: return (buffer[0] & 8) ? 1 : 0;
			case 519: return (buffer[0] & 16) ? 1 : 0;
			};
			break;
		};
		return 0;
	}
	size_t port_serialize(const portctrl::type* _this, const unsigned char* buffer, char* textbuf)
	{
		size_t ptr = 0;
		textbuf[ptr++] = (buffer[0] & 1) ? 'F' : '.';
		textbuf[ptr++] = '|';
		textbuf[ptr++] = (buffer[0] & 2) ? 'L' : '.';
		textbuf[ptr++] = (buffer[0] & 4) ? 'R' : '.';
		textbuf[ptr++] = (buffer[0] & 8) ? 'A' : '.';
		textbuf[ptr++] = (buffer[0] & 16) ? 'D' : '.';
		textbuf[ptr++] = (buffer[0] & 32) ? 'J' : '.';
		textbuf[ptr++] = (buffer[0] & 64) ? 'S' : '.';
		textbuf[ptr++] = (buffer[0] & 128) ? 's' : '.';
		textbuf[ptr] = '\0';
		return ptr;
	};
	size_t port_deserialize(const portctrl::type* _this, unsigned char* buffer, const char* textbuf)
	{
		memset(buffer, 0, 2);
		size_t ptr = 0;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 1;
		portctrl::skip_rest_of_field(textbuf, ptr, true);
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 2;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 4;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 8;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 16;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 32;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 64;
		if(portctrl::read_button_value(textbuf, ptr)) buffer[0] |= 128;
		portctrl::skip_rest_of_field(textbuf, ptr, false);
		return ptr;
	};

	struct _psystem : public portctrl::type
	{
		_psystem() : portctrl::type("system", "system", 1)
		{
			write = port_write;
			read = port_read;
			serialize = port_serialize;
			deserialize = port_deserialize;
			controller_info = &X2;
		}
	} psystem;

	portctrl::index_triple t(unsigned p, unsigned c, unsigned i, bool nl)
	{
		portctrl::index_triple x;
		x.valid = true;
		x.port = p;
		x.controller = c;
		x.control = i;
		return x;
	}

	void controller_magic()
	{
		if(magic_flags & 1) {
			X2.controllers[1] = A8;
			cstyle = 1;
		} else if(magic_flags & 2) {
			X2.controllers[1] = B8;
			cstyle = 2;
		} else if(magic_flags & 4) {
			X2.controllers[1] = C8;
			cstyle = 2;
		} else {
			cstyle = 0;
		}
	}

	struct _sky_core : public core_core, public core_type, public core_region, public core_sysregion
	{
		_sky_core()
			: core_core({&psystem},{}),
			core_type({{
				.iname = "sky",
				.hname = "Sky",
				.id = 3522,
				.sysname = "Sky",
				.bios = NULL,
				.regions = {this},
				.images = {{"rom", "skyroads.zip", .mandatory = 1, .pass_mode = 1, .headersize = 0,
					.extensions = "sky"}},
				.settings = {},
				.core = this,
			}}),
			core_region({{
				.iname = "world",
				.hname = "World",
				.priority = 0,
				.handle = 0,
				.multi = false,
				.framemagic = {18227, 656250},
				.compatible_runs = {0}
			}}),
			core_sysregion("sky", *this, *this) { hide(); }

		std::string c_core_identifier() const { return "Sky"; }
		bool c_set_region(core_region& region) { return (&region == this); }
		std::pair<uint32_t, uint32_t> c_video_rate() { return std::make_pair(656250, 18227); }
		double c_get_PAR() { return 5.0/6; }
		std::pair<uint32_t, uint32_t> c_audio_rate() { return std::make_pair(48000, 1); }
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc) {
			std::map<std::string, std::vector<char>> r;
			std::vector<char> sram;
			sram.resize(32);
			memcpy(&sram[0], corei.state.sram, 32);
			r["sram"] = sram;
			return r;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc) {
			if(sram.count("sram") && sram["sram"].size() == 32)
				memcpy(corei.state.sram, &sram["sram"][0], 32);
			else
				memset(corei.state.sram, 0, 32);
		}
		void c_serialize(std::vector<char>& out) {
			auto wram = corei.state.as_ram();
			out.resize(wram.second);
			memcpy(&out[0], wram.first, wram.second);
		}
		void c_unserialize(const char* in, size_t insize) {
			auto wram = corei.state.as_ram();
			if(insize != wram.second)
				throw std::runtime_error("Save is of wrong size");
			memcpy(wram.first, in, wram.second);
			handle_loadstate(corei);
		}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t w, uint32_t h) {
			return std::make_pair(FB_WIDTH / w, FB_HEIGHT / h);
		}
		void c_install_handler() {}
		void c_uninstall_handler() {}
		void c_emulate() {
			uint16_t x = 0;
			if(simulate_needs_input(corei)) {
				for(unsigned i = 0; i < 7; i++)
					if(ecore_callbacks->get_input(0, 1, iindexes[cstyle][i]))
						x |= (1 << i);
				pflag = true;
			}
			simulate_frame(corei, x);
			uint32_t* fb = corei.get_framebuffer();
			framebuffer::info inf;
			inf.type = &framebuffer::pixfmt_rgb32;
			inf.mem = reinterpret_cast<char*>(fb);
			inf.physwidth = FB_WIDTH;
			inf.physheight = FB_HEIGHT;
			inf.physstride = 4 * FB_WIDTH;
			inf.width = FB_WIDTH;
			inf.height = FB_HEIGHT;
			inf.stride = 4 * FB_WIDTH;
			inf.offset_x = 0;
			inf.offset_y = 0;

			framebuffer::raw ls(inf);
			ecore_callbacks->output_frame(ls, 656250, 18227);
			ecore_callbacks->timer_tick(18227, 656250);
			size_t samples = 1333;
			samples += corei.extrasamples();
			int16_t sbuf[2668];
			fetch_sfx(corei, sbuf, samples);
			CORE().audio->submit_buffer(sbuf, samples, true, 48000);
		}
		void c_runtosave() {}
		bool c_get_pflag() { return pflag; }
		void c_set_pflag(bool _pflag) { pflag = _pflag; }
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(cover_fbinfo);
			return x;
		}
		std::string c_get_core_shortname() const { return "sky"; }
		void c_pre_emulate_frame(portctrl::frame& cf) {}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p) {}
		const interface_device_reg* c_get_registers() { return sky_registers; }
		int t_load_rom(core_romimage* images, std::map<std::string, std::string>& settings,
			uint64_t rtc_sec, uint64_t rtc_subsec)
		{
			controller_magic();
			const unsigned char* _filename = images[0].data;
			size_t size = images[0].size;
			std::string filename(_filename, _filename + size);
			try {
				load_rom(corei, filename);
			} catch(std::exception& e) {
				messages << e.what();
				return -1;
			}
			//Clear the RAM.
			memset(corei.state.as_ram().first, 0, corei.state.as_ram().second);
			rom_boot_vector(corei);
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			controller_magic();
			controller_set r;
			r.ports.push_back(&psystem);
			r.logical_map.push_back(std::make_pair(0, 1));
			return r;
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map() { return std::make_pair(0, 0); }
		std::list<core_vma_info> c_vma_list()
		{
			std::list<core_vma_info> r;
			core_vma_info ram;
			ram.name = "RAM";
			ram.backing_ram = corei.state.as_ram().first;
			ram.size = 131072;
			ram.base = 0;
			ram.endian = 0;
			ram.volatile_flag = true;
			r.push_back(ram);
			core_vma_info wram;
			wram.name = "WRAM";
			wram.backing_ram = corei.state.as_ram().first + 131072;
			wram.size = corei.state.as_ram().second - 131072 - 32;
			wram.base = 131072;
			wram.endian = 0;
			wram.volatile_flag = true;
			r.push_back(wram);
			core_vma_info sram;
			sram.name = "SRAM";
			sram.backing_ram = corei.state.as_ram().first + corei.state.as_ram().second - 32;
			sram.size = 32;
			sram.base = corei.state.as_ram().second - 32;
			sram.endian = 0;
			sram.volatile_flag = false;
			r.push_back(sram);
			return r;
		}
		std::set<std::string> c_srams()
		{
			std::set<std::string> r;
			r.insert("sram");
			return r;
		}
		unsigned c_action_flags(unsigned id) { return 0; }
		int c_reset_action(bool hard) { return -1; }
		void c_set_debug_flags(uint64_t addr, unsigned int sflags, unsigned int cflags)
		{
		}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set)
		{
		}
		void c_debug_reset()
		{
		}
		std::vector<std::string> c_get_trace_cpus()
		{
			std::vector<std::string> r;
			return r;
		}
		void c_reset_to_load()
		{
			//Clear the RAM and jump to boot vector.
			memset(corei.state.as_ram().first, 0, corei.state.as_ram().second);
			rom_boot_vector(corei);
		}
	} sky_core;
}
