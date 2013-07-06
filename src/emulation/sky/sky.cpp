#include "state.hpp"
#include "romimage.hpp"
#include "framebuffer.hpp"
#include "logic.hpp"
#include "demo.hpp"
#include "core/dispatch.hpp"
#include "core/audioapi.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "library/pixfmt-rgb32.hpp"

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
	struct framebuffer_info cover_fbinfo = {
		&_pixel_format_rgb32,		//Format.
		(char*)cover_fbmem,		//Memory.
		320, 200, 1280,			//Physical size.
		320, 200, 1280,			//Logical size.
		0, 0				//Offset.
	};

	port_controller X4 = {"(system)", "(system)", {
		{port_controller_button::TYPE_BUTTON, 'F', "framesync", true}
	}};
	port_controller X8 = {"sky", "sky", {
		{port_controller_button::TYPE_BUTTON, 'L', "left", true},
		{port_controller_button::TYPE_BUTTON, 'R', "right", true},
		{port_controller_button::TYPE_BUTTON, 'A', "up", true},
		{port_controller_button::TYPE_BUTTON, 'D', "down", true},
		{port_controller_button::TYPE_BUTTON, 'J', "A", true},
		{port_controller_button::TYPE_BUTTON, 'S', "start", true},
		{port_controller_button::TYPE_BUTTON, 's', "select", true},
		{port_controller_button::TYPE_NULL, '\0', "", true},
		{port_controller_button::TYPE_NULL, '\0', "", true},
	}};

	port_controller A8 = {"gamepad", "sky", {
		{port_controller_button::TYPE_NULL, '\0', "", false},
		{port_controller_button::TYPE_NULL, '\0', "", false},
		{port_controller_button::TYPE_BUTTON, 's', "select", false},
		{port_controller_button::TYPE_BUTTON, 'S', "start", false},
		{port_controller_button::TYPE_BUTTON, 'A', "up", false},
		{port_controller_button::TYPE_BUTTON, 'D', "down", false},
		{port_controller_button::TYPE_BUTTON, 'L', "left", false},
		{port_controller_button::TYPE_BUTTON, 'R', "right", false},
		{port_controller_button::TYPE_BUTTON, 'J', "A", false},
	}};
	port_controller B8 = {"gb", "sky", {
		{port_controller_button::TYPE_BUTTON, 'J', "A", false},
		{port_controller_button::TYPE_NULL, '\0', "", false},
		{port_controller_button::TYPE_BUTTON, 's', "select", false},
		{port_controller_button::TYPE_BUTTON, 'S', "start", false},
		{port_controller_button::TYPE_BUTTON, 'R', "right", false},
		{port_controller_button::TYPE_BUTTON, 'L', "left", false},
		{port_controller_button::TYPE_BUTTON, 'A', "up", false},
		{port_controller_button::TYPE_BUTTON, 'D', "down", false},
		{port_controller_button::TYPE_NULL, '\0', "", false},
	}};
	port_controller C8 = {"gba", "sky", {
		{port_controller_button::TYPE_BUTTON, 'J', "A", false},
		{port_controller_button::TYPE_NULL, '\0', "", false},
		{port_controller_button::TYPE_BUTTON, 's', "select", false},
		{port_controller_button::TYPE_BUTTON, 'S', "start", false},
		{port_controller_button::TYPE_BUTTON, 'R', "right", false},
		{port_controller_button::TYPE_BUTTON, 'L', "left", false},
		{port_controller_button::TYPE_BUTTON, 'A', "up", false},
		{port_controller_button::TYPE_BUTTON, 'D', "down", false},
		{port_controller_button::TYPE_NULL, '\0', "", false},
	}};

	port_controller_set X2 = {{&X4, &X8}};

	void port_write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x)
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

	short port_read(const unsigned char* buffer, unsigned idx, unsigned ctrl)
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
	size_t port_serialize(const unsigned char* buffer, char* textbuf)
	{
		size_t ptr = 0;
		short tmp;
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
	size_t port_deserialize(unsigned char* buffer, const char* textbuf)
	{
		memset(buffer, 0, 2);
		size_t ptr = 0;
		short tmp;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 1;
		skip_rest_of_field(textbuf, ptr, true);
		if(read_button_value(textbuf, ptr)) buffer[0] |= 2;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 4;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 8;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 16;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 32;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 64;
		if(read_button_value(textbuf, ptr)) buffer[0] |= 128;
		skip_rest_of_field(textbuf, ptr, false);
	};
	int port_legal(unsigned c)
	{
		if(c == 0) return true;
		return false;
	};
	unsigned port_used_indices(unsigned c)
	{
		if(c == 0) return 1;
		if(c == 1) return 9;
		return 0;
	};

	struct _psystem : public port_type
	{
		_psystem() : port_type("system", "system", 1,1)
		{
			write = port_write;
			read = port_read;
			serialize = port_serialize;
			deserialize = port_deserialize;
			legal = port_legal;
			used_indices = port_used_indices;
			controller_info = &X2;
		}
	} psystem;

	port_index_triple t(unsigned p, unsigned c, unsigned i, bool nl)
	{
		port_index_triple x;
		x.valid = true;
		x.port = p;
		x.controller = c;
		x.control = i;
		return x;
	}

	void controller_magic()
	{
		if(magic_flags & 1) {
			X2.controllers[1] = &A8;
			cstyle = 1;
		} else if(magic_flags & 2) {
			X2.controllers[1] = &B8;
			cstyle = 2;
		} else if(magic_flags & 4) {
			X2.controllers[1] = &C8;
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
				.extensions = "sky",
				.bios = NULL,
				.regions = {this},
				.images = {{"rom", "skyroads.zip", .mandatory = 1, .pass_mode = 1, .headersize = 0}},
				.settings = {},
				.core = this,
			}}),
			core_region({{
				.iname = "world",
				.hname = "World",
				.priority = 0,
				.handle = 0,
				.multi = false,
				.framemagic = {656250, 18227},
				.compatible_runs = {0}
			}}),
			core_sysregion("sky", *this, *this) {}

		std::string c_core_identifier() { return "Sky"; }
		bool c_set_region(core_region& region) { return (&region == this); }
		std::pair<uint32_t, uint32_t> c_video_rate() { return std::make_pair(656250, 18227); }
		std::pair<uint32_t, uint32_t> c_audio_rate() { return std::make_pair(48000, 1); }
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc) {
			std::map<std::string, std::vector<char>> r;
			std::vector<char> sram;
			sram.resize(32);
			memcpy(&sram[0], _gstate.sram, 32);
			r["sram"] = sram;
			return r;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc) {
			if(sram.count("sram") && sram["sram"].size() == 32)
				memcpy(_gstate.sram, &sram["sram"][0], 32);
			else
				memset(_gstate.sram, 0, 32);
		}
		void c_serialize(std::vector<char>& out) {
			auto wram = _gstate.as_ram();
			out.resize(wram.second);
			memcpy(&out[0], wram.first, wram.second);
		}
		void c_unserialize(const char* in, size_t insize) {
			auto wram = _gstate.as_ram();
			if(insize != wram.second)
				throw std::runtime_error("Save is of wrong size");
			memcpy(wram.first, in, wram.second);
			handle_loadstate(_gstate);
		}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t w, uint32_t h) {
			return std::make_pair(FB_WIDTH / w, FB_HEIGHT / h);
		}
		void c_install_handler() { hide(); }
		void c_uninstall_handler() {}
		void c_emulate() {
			static unsigned count[4];
			static unsigned tcount[4] = {5, 7, 8, 25};
			uint16_t x = 0;
			for(unsigned i = 0; i < 7; i++)
				if(ecore_callbacks->get_input(0, 1, iindexes[cstyle][i]))
					x |= (1 << i);
			pflag = true;
			simulate_frame(_gstate, x);
			uint32_t* fb = indirect_flag ? fadeffect_buffer : sky::framebuffer;
			framebuffer_info inf;
			inf.type = &_pixel_format_rgb32;
			inf.mem = const_cast<char*>(reinterpret_cast<const char*>(fb));
			inf.physwidth = FB_WIDTH;
			inf.physheight = FB_HEIGHT;
			inf.physstride = 4 * FB_WIDTH;
			inf.width = FB_WIDTH;
			inf.height = FB_HEIGHT;
			inf.stride = 4 * FB_WIDTH;
			inf.offset_x = 0;
			inf.offset_y = 0;

			framebuffer_raw ls(inf);
			ecore_callbacks->output_frame(ls, 656250, 18227);
			ecore_callbacks->timer_tick(18227, 656250);
			size_t samples = 1333;
			size_t extrasample = 0;
			for(unsigned i = 0; i < 4; i++) {
				count[i]++;
				if(count[i] == tcount[i]) {
					count[i] = 0;
					extrasample = extrasample ? 0 : 1;
				} else
					break;
			}
			samples += extrasample;
			int16_t sbuf[2668];
			fetch_sfx(_gstate, sbuf, samples);
			audioapi_submit_buffer(sbuf, samples, true, 48000);
			for(unsigned i = 0; i < samples; i++)
				information_dispatch::do_sample(sbuf[2 * i + 0], sbuf[2 * i + 1]);
		}
		void c_runtosave() {}
		bool c_get_pflag() { return pflag; }
		void c_set_pflag(bool _pflag) { pflag = _pflag; }
		framebuffer_raw& c_draw_cover() {
			static framebuffer_raw x(cover_fbinfo);
			return x;
		}
		std::string c_get_core_shortname() { return "sky"; }
		void c_pre_emulate_frame(controller_frame& cf) {}
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
				load_rom(filename);
			} catch(std::exception& e) {
				messages << e.what();
				return -1;
			}
			rom_boot_vector(_gstate);
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			controller_magic();
			controller_set r;
			r.ports.push_back(&psystem);
				r.portindex.indices.push_back(t(0, 0, 0, false));
			for(unsigned i = 0; i < 9; i++)
				r.portindex.indices.push_back(t(0, 1, i, true));
			r.portindex.logical_map.push_back(std::make_pair(0, 1));
			r.portindex.pcid_map.push_back(std::make_pair(0, 1));
			return r;
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map() { return std::make_pair(0, 0); }
		std::list<core_vma_info> c_vma_list()
		{
			std::list<core_vma_info> r;
			core_vma_info ram;
			ram.name = "RAM";
			ram.backing_ram = _gstate.as_ram().first;
			ram.size = _gstate.as_ram().second;
			ram.base = 0;
			ram.readonly = false;
			ram.endian = 0;
			ram.iospace_rw = NULL;
			r.push_back(ram);
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
	} sky_core;
}
