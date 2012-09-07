#ifdef CORETYPE_BSNES
#include "lsnes.hpp"
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include "core/misc.hpp"
#include "core/emucore.hpp"
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/window.hpp"
#include "library/pixfmt-lrgb.hpp"
#include "library/string.hpp"
#include "library/framebuffer.hpp"
#include <snes/snes.hpp>
#include <gameboy/gameboy.hpp>
#ifdef BSNES_V087
#include <target-libsnes/libsnes.hpp>
#else
#include <ui-libsnes/libsnes.hpp>
#endif

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

/**
 * Logical button IDs.
 */
#define LOGICAL_BUTTON_LEFT 0
#define LOGICAL_BUTTON_RIGHT 1
#define LOGICAL_BUTTON_UP 2
#define LOGICAL_BUTTON_DOWN 3
#define LOGICAL_BUTTON_A 4
#define LOGICAL_BUTTON_B 5
#define LOGICAL_BUTTON_X 6
#define LOGICAL_BUTTON_Y 7
#define LOGICAL_BUTTON_L 8
#define LOGICAL_BUTTON_R 9
#define LOGICAL_BUTTON_SELECT 10
#define LOGICAL_BUTTON_START 11
#define LOGICAL_BUTTON_TRIGGER 12
#define LOGICAL_BUTTON_CURSOR 13
#define LOGICAL_BUTTON_TURBO 14
#define LOGICAL_BUTTON_PAUSE 15

const char* button_symbols = "BYsSudlrAXLRTSTCUP";

namespace
{
	uint32_t norom_frame[512 * 448];

	void init_norom_frame()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		for(size_t i = 0; i < 512 * 448; i++)
			norom_frame[i] = 0x7C21F;
	}

	int regions_compatible(unsigned rom, unsigned run)
	{
		return (!rom || rom == run);
	}

	unsigned header_fn(size_t r)
	{
		if((r % 1024) == 512)
			return 512;
		else
			return 0;
	}

	core_type* internal_rom = NULL;
	extern core_type type_snes;
	extern core_type type_bsx;
	extern core_type type_bsxslotted;
	extern core_type type_sufamiturbo;
	extern core_type type_sgb;

	int load_rom_snes(core_romimage* img, uint64_t secs, uint64_t subsecs)
	{
		snes_term();
		snes_unload_cartridge();
		bool r = snes_load_cartridge_normal(img[0].markup, img[0].data, img[0].size);
		if(r)
			internal_rom = &type_snes;
		return r ? 0 : -1;
	}

	int load_rom_bsx(core_romimage* img, uint64_t secs, uint64_t subsecs)
	{
		snes_term();
		snes_unload_cartridge();
		bool r = snes_load_cartridge_bsx(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size);
		if(r)
			internal_rom = &type_bsx;
		return r ? 0 : -1;
	}

	int load_rom_bsxslotted(core_romimage* img, uint64_t secs, uint64_t subsecs)
	{
		snes_term();
		snes_unload_cartridge();
		bool r = snes_load_cartridge_bsx_slotted(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size);
		if(r)
			internal_rom = &type_bsxslotted;
		return r ? 0 : -1;
	}

	int load_rom_sgb(core_romimage* img, uint64_t secs, uint64_t subsecs)
	{
		snes_term();
		snes_unload_cartridge();
		bool r = snes_load_cartridge_super_game_boy(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size);
		if(r)
			internal_rom = &type_sgb;
		return r ? 0 : -1;
	}

	int load_rom_sufamiturbo(core_romimage* img, uint64_t secs, uint64_t subsecs)
	{
		snes_term();
		snes_unload_cartridge();
		bool r = snes_load_cartridge_sufami_turbo(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size, img[2].markup, img[2].data, img[2].size);
		if(r)
			internal_rom = &type_sufamiturbo;
		return r ? 0 : -1;
	}

	uint64_t ntsc_magic[4] = {178683, 10738636, 16639264, 596096};
	uint64_t pal_magic[4] = {6448, 322445, 19997208, 266440};

	core_region region_auto("autodetect", "Autodetect", 1, 0, true, ntsc_magic, regions_compatible);
	core_region region_ntsc("ntsc", "NTSC", 0, 1, true, ntsc_magic, regions_compatible);
	core_region region_pal("pal", "PAL", 0, 2, true, pal_magic, regions_compatible);
	core_romimage_info image_snescart("rom", "Cartridge ROM", 1, header_fn);
	core_romimage_info image_bsxbios("rom", "BS-X BIOS", 1, header_fn);
	core_romimage_info image_bsxflash("bsx", "BS-X Flash", 2, header_fn);
	core_romimage_info image_bsxsflash("bsx", "BS-X Flash", 2, header_fn);
	core_romimage_info image_sgbbios("rom", "SGB BIOS", 1, header_fn);
	core_romimage_info image_dmg("dmg", "DMG ROM", 2, header_fn);
	core_romimage_info image_stbios("rom", "ST BIOS", 1, header_fn);
	core_romimage_info image_stslota("slot-a", "ST Slot A ROM", 2, header_fn);
	core_romimage_info image_stslotb("slot-b", "ST Slot B ROM", 2, header_fn);
	core_type type_snes("snes", "SNES", 0, load_rom_snes, "sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh");
	core_type type_bsx("bsx", "BS-X (non-slotted)", 1, load_rom_bsx, "");
	core_type type_bsxslotted("bsxslotted", "BS-X (slotted)", 2, load_rom_bsxslotted, "");
	core_type type_sufamiturbo("sufamiturbo", "Sufami Turbo", 3, load_rom_sufamiturbo, "");
	core_type type_sgb("sgb", "Super Game Boy", 4, load_rom_sgb, "");
	core_type_region_bind bind_A(type_snes, region_auto);
	core_type_region_bind bind_B(type_snes, region_ntsc);
	core_type_region_bind bind_C(type_snes, region_pal);
	core_type_region_bind bind_D(type_bsx, region_ntsc);
	core_type_region_bind bind_E(type_bsxslotted, region_ntsc);
	core_type_region_bind bind_F(type_sufamiturbo, region_ntsc);
	core_type_region_bind bind_G(type_sgb, region_auto);
	core_type_region_bind bind_H(type_sgb, region_ntsc);
	core_type_region_bind bind_I(type_sgb, region_pal);
	core_type_image_bind bind_J(type_snes, image_snescart, 0);
	core_type_image_bind bind_K(type_bsx, image_bsxbios, 0);
	core_type_image_bind bind_L(type_bsx, image_bsxflash, 1);
	core_type_image_bind bind_M(type_bsxslotted, image_bsxbios, 0);
	core_type_image_bind bind_N(type_bsxslotted, image_bsxsflash, 1);
	core_type_image_bind bind_O(type_sufamiturbo, image_stbios, 0);
	core_type_image_bind bind_P(type_sufamiturbo, image_stslota, 1);
	core_type_image_bind bind_Q(type_sufamiturbo, image_stslotb, 2);
	core_type_image_bind bind_R(type_sgb, image_sgbbios, 0);
	core_type_image_bind bind_S(type_sgb, image_dmg, 1);
	core_sysregion sr1("snes_ntsc", type_snes, region_ntsc);
	core_sysregion sr2("snes_pal", type_snes, region_pal);
	core_sysregion sr3("bsx", type_bsx, region_ntsc);
	core_sysregion sr4("bsxslotted", type_bsxslotted, region_ntsc);
	core_sysregion sr5("sufamiturbo", type_sufamiturbo, region_ntsc);
	core_sysregion sr6("sgb_ntsc", type_sgb, region_ntsc);
	core_sysregion sr7("sgb_pal", type_sgb, region_pal);

	bool last_hires = false;
	bool last_interlace = false;
	bool stepping_into_save;
	bool video_refresh_done;
	//Delay reset.
	unsigned long long delayreset_cycles_run;
	unsigned long long delayreset_cycles_target;
	
	bool p1disable = false;
	std::map<int16_t, std::pair<uint64_t, uint64_t>> ptrmap;

	const char* buttonnames[] = {
		"left", "right", "up", "down", "A", "B", "X", "Y", "L", "R", "select", "start", "trigger",
		"cursor", "turbo", "pause"
	};

	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	};

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
		if(write)
			SNES::bus.write(offset, data);
		else
			return SNES::bus.read(offset);
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

	void create_region(std::list<vma_info>& inf, const std::string& name, uint64_t base, uint64_t size,
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write)) throw(std::bad_alloc)
	{
		if(size == 0)
			return;
		vma_info i;
		i.name = name;
		i.base = base;
		i.size = size;
		i.readonly = false;
		i.native_endian = false;
		i.iospace_rw = iospace_rw;
		inf.push_back(i);
	}

	void create_region(std::list<vma_info>& inf, const std::string& name, uint64_t base, uint8_t* memory,
		uint64_t size, bool readonly, bool native_endian = false) throw(std::bad_alloc)
	{
		if(size == 0)
			return;
		vma_info i;
		i.name = name;
		i.base = base;
		i.size = size;
		i.backing_ram = memory;
		i.readonly = readonly;
		i.native_endian = native_endian;
		i.iospace_rw = NULL;
		inf.push_back(i);
	}

	void create_region(std::list<vma_info>& inf, const std::string& name, uint64_t base,
		SNES::MappedRAM& memory, bool readonly, bool native_endian = false) throw(std::bad_alloc)
	{
		create_region(inf, name, base, memory.data(), memory.size(), readonly, native_endian);
	}

	void map_internal(std::list<vma_info>& inf, const std::string& name, uint16_t index, void* memory,
		size_t memsize)
	{
		ptrmap[index] = std::make_pair(reinterpret_cast<uint64_t>(memory), static_cast<uint64_t>(memsize));
		create_region(inf, name, 0x101000000 + index * 0x1000000, reinterpret_cast<uint8_t*>(memory),
			memsize, true, true);
	}

	bool delayreset_fn()
	{
		if(delayreset_cycles_run == delayreset_cycles_target || video_refresh_done)
			return true;
		delayreset_cycles_run++;
		return false;
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

		void videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan)
		{
			last_hires = hires;
			last_interlace = interlace;
			bool region = (&core_get_region() == &region_pal);
			if(stepping_into_save)
				messages << "Got video refresh in runtosave, expect desyncs!" << std::endl;
			video_refresh_done = true;
			uint32_t fps_n, fps_d;
			auto fps = get_video_rate();
			fps_n = fps.first;
			fps_d = fps.second;
			uint32_t g = gcd(fps_n, fps_d);
			fps_n /= g;
			fps_d /= g;

			framebuffer_info inf;
			inf.type = &_pixel_format_lrgb;
			inf.mem = const_cast<char*>(reinterpret_cast<const char*>(data));
			inf.physwidth = 512;
			inf.physheight = 512;
			inf.physstride = 2048;
			inf.width = hires ? 512 : 256;
			inf.height = (region ? 239 : 224) * (interlace ? 2 : 1);
			inf.stride = interlace ? 2048 : 4096;
			inf.offset_x = 0;
			inf.offset_y = (region ? (overscan ? 9 : 1) : (overscan ? 16 : 9)) * 2;

			framebuffer_raw ls(inf);
			ecore_callbacks->output_frame(ls, fps_n, fps_d);
			information_dispatch::do_raw_frame(data, hires, interlace, overscan, region ?
				VIDEO_REGION_PAL : VIDEO_REGION_NTSC);
		}

		void audioSample(int16_t l_sample, int16_t r_sample)
		{
			uint16_t _l = l_sample;
			uint16_t _r = r_sample;
			platform::audio_sample(_l + 32768, _r + 32768);
			information_dispatch::do_sample(l_sample, r_sample);
			//The SMP emits a sample every 768 ticks of its clock. Use this in order to keep track of
			//time.
			auto hz = get_audio_rate();
			ecore_callbacks->timer_tick(hz.second, hz.first);
		}

		int16_t inputPoll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
		{
			return ecore_callbacks->get_input(port ? 1 : 0, index, id);
		}
	};

	void set_core_controller_generic(unsigned port, unsigned id, bool p2only)
	{
		if(port > 1)
			return;
		if(port == 1)
			snes_set_controller_port_device(true, id);
		if(port == 0) {
			snes_set_controller_port_device(false, p2only ? SNES_DEVICE_NONE : id);
			p1disable = p2only;
		}
	}

	void set_core_controller_none(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_NONE, false);
	}

	void set_core_controller_gamepad(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_JOYPAD, false);
	}

	void set_core_controller_mouse(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_MOUSE, false);
	}

	void set_core_controller_multitap(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_MULTITAP, false);
	}

	void set_core_controller_superscope(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_SUPER_SCOPE, true);
	}

	void set_core_controller_justifier(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_JUSTIFIER, true);
	}

	void set_core_controller_justifiers(unsigned port) throw()
	{
		set_core_controller_generic(port, SNES_DEVICE_JUSTIFIERS, true);
	}

	int get_button_id_none(unsigned controller, unsigned lbid) throw()
	{
		return -1;
	}

	int get_button_id_gamepad(unsigned controller, unsigned lbid) throw()
	{
		if(controller > 0)
			return -1;
		switch(lbid) {
		case LOGICAL_BUTTON_LEFT:	return SNES_DEVICE_ID_JOYPAD_LEFT;
		case LOGICAL_BUTTON_RIGHT:	return SNES_DEVICE_ID_JOYPAD_RIGHT;
		case LOGICAL_BUTTON_UP:		return SNES_DEVICE_ID_JOYPAD_UP;
		case LOGICAL_BUTTON_DOWN:	return SNES_DEVICE_ID_JOYPAD_DOWN;
		case LOGICAL_BUTTON_A:		return SNES_DEVICE_ID_JOYPAD_A;
		case LOGICAL_BUTTON_B:		return SNES_DEVICE_ID_JOYPAD_B;
		case LOGICAL_BUTTON_X:		return SNES_DEVICE_ID_JOYPAD_X;
		case LOGICAL_BUTTON_Y:		return SNES_DEVICE_ID_JOYPAD_Y;
		case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_JOYPAD_L;
		case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_JOYPAD_R;
		case LOGICAL_BUTTON_SELECT:	return SNES_DEVICE_ID_JOYPAD_SELECT;
		case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JOYPAD_START;
		default:			return -1;
		}
	}

	int get_button_id_mouse(unsigned controller, unsigned lbid) throw()
	{
		if(controller > 0)
			return -1;
		switch(lbid) {
		case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_MOUSE_LEFT;
		case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_MOUSE_RIGHT;
		default:			return -1;
		}
	}

	int get_button_id_multitap(unsigned controller, unsigned lbid) throw()
	{
		if(controller > 3)
			return -1;
		switch(lbid) {
		case LOGICAL_BUTTON_LEFT:	return SNES_DEVICE_ID_JOYPAD_LEFT;
		case LOGICAL_BUTTON_RIGHT:	return SNES_DEVICE_ID_JOYPAD_RIGHT;
		case LOGICAL_BUTTON_UP:		return SNES_DEVICE_ID_JOYPAD_UP;
		case LOGICAL_BUTTON_DOWN:	return SNES_DEVICE_ID_JOYPAD_DOWN;
		case LOGICAL_BUTTON_A:		return SNES_DEVICE_ID_JOYPAD_A;
		case LOGICAL_BUTTON_B:		return SNES_DEVICE_ID_JOYPAD_B;
		case LOGICAL_BUTTON_X:		return SNES_DEVICE_ID_JOYPAD_X;
		case LOGICAL_BUTTON_Y:		return SNES_DEVICE_ID_JOYPAD_Y;
		case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_JOYPAD_L;
		case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_JOYPAD_R;
		case LOGICAL_BUTTON_SELECT:	return SNES_DEVICE_ID_JOYPAD_SELECT;
		case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JOYPAD_START;
		default:			return -1;
		}
	}

	int get_button_id_superscope(unsigned controller, unsigned lbid) throw()
	{
		if(controller > 0)
			return -1;
		switch(lbid) {
		case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER;
		case LOGICAL_BUTTON_CURSOR:	return SNES_DEVICE_ID_SUPER_SCOPE_CURSOR;
		case LOGICAL_BUTTON_TURBO:	return SNES_DEVICE_ID_SUPER_SCOPE_TURBO;
		case LOGICAL_BUTTON_PAUSE:	return SNES_DEVICE_ID_SUPER_SCOPE_PAUSE;
		default:			return -1;
		}
	}

	int get_button_id_justifier(unsigned controller, unsigned lbid) throw()
	{
		if(controller > 0)
			return -1;
		switch(lbid) {
		case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JUSTIFIER_START;
		case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_JUSTIFIER_TRIGGER;
		default:			return -1;
		}
	}

	int get_button_id_justifiers(unsigned controller, unsigned lbid) throw()
	{
		if(controller > 1)
			return -1;
		switch(lbid) {
		case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JUSTIFIER_START;
		case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_JUSTIFIER_TRIGGER;
		default:			return -1;
		}
	}

	struct porttype_gamepad : public porttype_info
	{
		porttype_gamepad() : porttype_info("gamepad", "Gamepad", 1, generic_port_size<1, 0, 12>())
		{
			write = generic_port_write<1, 0, 12>;
			read = generic_port_read<1, 0, 12>;
			display = generic_port_display<1, 0, 12, 0>;
			serialize = generic_port_serialize<1, 0, 12, 0>;
			deserialize = generic_port_deserialize<1, 0, 12>;
			legal = generic_port_legal<3>;
			deviceflags = generic_port_deviceflags<1, 1>;
			ctrlname = "gamepad";
			controllers = 1;
			set_core_controller = set_core_controller_gamepad;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_gamepad(controller, lbid);
		}
	} gamepad;

	struct porttype_justifier : public porttype_info
	{
		porttype_justifier() : porttype_info("justifier", "Justifier", 5, generic_port_size<1, 2, 2>())
		{
			write = generic_port_write<1, 2, 2>;
			read = generic_port_read<1, 2, 2>;
			display = generic_port_display<1, 2, 2, 12>;
			serialize = generic_port_serialize<1, 2, 2, 12>;
			deserialize = generic_port_deserialize<1, 2, 2>;
			legal = generic_port_legal<2>;
			deviceflags = generic_port_deviceflags<1, 3>;
			ctrlname = "justifier";
			controllers = 1;
			set_core_controller = set_core_controller_justifier;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_justifier(controller, lbid);
		}
	} justifier;

	struct porttype_justifiers : public porttype_info
	{
		porttype_justifiers() : porttype_info("justifiers", "2 Justifiers", 6, generic_port_size<2, 2, 2>())
		{
			write = generic_port_write<2, 2, 2>;
			read = generic_port_read<2, 2, 2>;
			display = generic_port_display<2, 2, 2, 0>;
			serialize = generic_port_serialize<2, 2, 2, 12>;
			deserialize = generic_port_deserialize<2, 2, 2>;
			legal = generic_port_legal<2>;
			deviceflags = generic_port_deviceflags<2, 3>;
			ctrlname = "justifier";
			controllers = 2;
			set_core_controller = set_core_controller_justifiers;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_justifiers(controller, lbid);
		}
	} justifiers;

	struct porttype_mouse : public porttype_info
	{
		porttype_mouse() : porttype_info("mouse", "Mouse", 3, generic_port_size<1, 2, 2>())
		{
			write = generic_port_write<1, 2, 2>;
			read = generic_port_read<1, 2, 2>;
			display = generic_port_display<1, 2, 2, 0>;
			serialize = generic_port_serialize<1, 2, 2, 12>;
			deserialize = generic_port_deserialize<1, 2, 2>;
			legal = generic_port_legal<3>;
			deviceflags = generic_port_deviceflags<1, 5>;
			ctrlname = "mouse";
			controllers = 1;
			set_core_controller = set_core_controller_mouse;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_mouse(controller, lbid);
		}
	} mouse;

	struct porttype_multitap : public porttype_info
	{
		porttype_multitap() : porttype_info("multitap", "Multitap", 2, generic_port_size<4, 0, 12>())
		{
			write = generic_port_write<4, 0, 12>;
			read = generic_port_read<4, 0, 12>;
			display = generic_port_display<4, 0, 12, 0>;
			serialize = generic_port_serialize<4, 0, 12, 0>;
			deserialize = generic_port_deserialize<4, 0, 12>;
			legal = generic_port_legal<3>;
			deviceflags = generic_port_deviceflags<4, 1>;
			ctrlname = "multitap";
			controllers = 4;
			set_core_controller = set_core_controller_multitap;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_multitap(controller, lbid);
		}
	} multitap;

	struct porttype_none : public porttype_info
	{
		porttype_none() : porttype_info("none", "None", 0, generic_port_size<0, 0, 0>())
		{
			write = generic_port_write<0, 0, 0>;
			read = generic_port_read<0, 0, 0>;
			display = generic_port_display<0, 0, 0, 0>;
			serialize = generic_port_serialize<0, 0, 0, 0>;
			deserialize = generic_port_deserialize<0, 0, 0>;
			legal = generic_port_legal<3>;
			deviceflags = generic_port_deviceflags<0, 0>;
			ctrlname = "";
			controllers = 0;
			set_core_controller = set_core_controller_none;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_none(controller, lbid);
		}
	} none;

	struct porttype_superscope : public porttype_info
	{
		porttype_superscope() : porttype_info("superscope", "Super Scope", 4, generic_port_size<1, 2, 4>())
		{
			write = generic_port_write<1, 2, 4>;
			read = generic_port_read<1, 2, 4>;
			display = generic_port_display<1, 2, 4, 0>;
			serialize = generic_port_serialize<1, 2, 4, 14>;
			deserialize = generic_port_deserialize<1, 2, 4>;
			deviceflags = generic_port_deviceflags<1, 3>;
			legal = generic_port_legal<2>;
			ctrlname = "superscope";
			controllers = 1;
			set_core_controller = set_core_controller_superscope;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return get_button_id_superscope(controller, lbid);
		}
	} superscope;

	my_interface my_interface_obj;
	SNES::Interface* old;
}

std::string get_logical_button_name(unsigned lbid) throw(std::bad_alloc)
{
	if(lbid >= sizeof(buttonnames) / sizeof(buttonnames[0]))
		return "";
	return buttonnames[lbid];
}

void core_install_handler()
{
	old = SNES::interface;
	SNES::interface = &my_interface_obj;
	SNES::system.init();
}

std::string get_core_default_port(unsigned port)
{
	if(port == 0)
		return "gamepad";
	else
		return "none";
}

void core_uninstall_handler()
{
	SNES::interface = old;
}

uint32_t get_snes_cpu_rate()
{
	return SNES::system.cpu_frequency();
}

uint32_t get_snes_apu_rate()
{
	return SNES::system.apu_frequency();
}

std::pair<unsigned, unsigned> get_core_logical_controller_limits()
{
	return std::make_pair(8U, (unsigned)(sizeof(buttonnames)/sizeof(buttonnames[0])));
}

bool get_core_need_analog()
{
	return true;
}

std::string get_core_identifier()
{
	std::ostringstream x;
	x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
	return x.str();
}

void do_basic_core_init()
{
	static my_interfaced i;
	SNES::interface = &i;
}

std::set<std::string> get_sram_set()
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

void set_preload_settings()
{
	SNES::config.random = false;
	SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;
}

core_region& core_get_region()
{
	if(SNES::system.region() == SNES::System::Region::PAL)
		return region_pal;
	else
		return region_ntsc;
}

void core_power()
{
	if(internal_rom)
		snes_power();
}

void core_unload_cartridge()
{
	if(!internal_rom)
		return;
	snes_term();
	snes_unload_cartridge();
	internal_rom = NULL;
}

//Get the current video rate.
std::pair<uint32_t, uint32_t> get_video_rate()
{
	if(!internal_rom)
		return std::make_pair(60, 1);
	uint32_t div;
	if(snes_get_region())
		div = last_interlace ? DURATION_PAL_FIELD : DURATION_PAL_FRAME;
	else
		div = last_interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
	return std::make_pair(get_snes_cpu_rate(), div);
}

//Get the current audio rate.
std::pair<uint32_t, uint32_t> get_audio_rate()
{
	if(!internal_rom)
		return std::make_pair(64081, 2);
	return std::make_pair(get_snes_apu_rate(), static_cast<uint32_t>(768));
}

std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc)
{
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

void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
{
	std::set<std::string> used;
	if(!internal_rom) {
		for(auto i : sram)
			messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge." << std::endl;
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
			messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge." << std::endl;
}

bool core_set_region(core_region& region)
{
	if(&region == &region_auto)		SNES::config.region = SNES::System::Region::Autodetect;
	else if(&region == &region_ntsc)	SNES::config.region = SNES::System::Region::NTSC;
	else if(&region == &region_pal)		SNES::config.region = SNES::System::Region::PAL;
	else
		return false;
	return true;
}

void core_serialize(std::vector<char>& out)
{
	if(!internal_rom)
		throw std::runtime_error("No ROM loaded");
	serializer s = SNES::system.serialize();
	out.resize(s.size());
	memcpy(&out[0], s.data(), s.size());
}

void core_unserialize(const char* in, size_t insize)
{
	if(!internal_rom)
		throw std::runtime_error("No ROM loaded");
	serializer s(reinterpret_cast<const uint8_t*>(in), insize);
	if(!SNES::system.unserialize(s))
		throw std::runtime_error("SNES core rejected savestate");
}

std::pair<bool, uint32_t> core_emulate_cycles(uint32_t cycles)
{
	if(!internal_rom)
		return std::make_pair(false, 0);
#if defined(BSNES_V084) || defined(BSNES_V085)
	messages << "Executing delayed reset... This can take some time!" << std::endl;
	video_refresh_done = false;
	delayreset_cycles_run = 0;
	delayreset_cycles_target = cycles;
	SNES::cpu.step_event = delayreset_fn;
	SNES::system.run();
	SNES::cpu.step_event = nall::function<bool()>();
	return std::make_pair(!video_refresh_done, delayreset_cycles_run);
#else
	messages << "Delayresets not supported on this bsnes version (needs v084 or v085)"
		<< std::endl;
	return std::make_pair(false, 0);
#endif
}

void core_emulate_frame()
{
	static unsigned frame_modulus = 0;
	if(!internal_rom) {
		init_norom_frame();
		framebuffer_info inf;
		inf.type = &_pixel_format_lrgb;
		inf.mem = const_cast<char*>(reinterpret_cast<const char*>(norom_frame));
		inf.physwidth = 512;
		inf.physheight = 448;
		inf.physstride = 2048;
		inf.width = 512;
		inf.height = 448;
		inf.stride = 2048;
		inf.offset_x = 0;
		inf.offset_y = 0;

		framebuffer_raw ls(inf);
		ecore_callbacks->output_frame(ls, 60, 1);

		for(unsigned i = 0; i < 534; i++) {
			platform::audio_sample(32768, 32768);
			information_dispatch::do_sample(0, 0);
		}
		if(frame_modulus++ == 0) {
			platform::audio_sample(32768, 32768);
			information_dispatch::do_sample(0, 0);
		}
		frame_modulus %= 120;
		ecore_callbacks->timer_tick(1, 60);
		return;
	}
	SNES::system.run();
}

void core_reset()
{
	if(!internal_rom)
		return;
	SNES::system.reset();
}

void core_runtosave()
{
	if(!internal_rom)
		return;
	stepping_into_save = true;
	SNES::system.runtosave();
	stepping_into_save = false;
}

std::list<vma_info> get_vma_list()
{
	std::list<vma_info> ret;
	if(!internal_rom)
		return ret;
	create_region(ret, "WRAM", 0x007E0000, SNES::cpu.wram, 131072, false);
	create_region(ret, "APURAM", 0x00000000, SNES::smp.apuram, 65536, false);
	create_region(ret, "VRAM", 0x00010000, SNES::ppu.vram, 65536, false);
	create_region(ret, "OAM", 0x00020000, SNES::ppu.oam, 544, false);
	create_region(ret, "CGRAM", 0x00021000, SNES::ppu.cgram, 512, false);
	if(SNES::cartridge.has_srtc()) create_region(ret, "RTC", 0x00022000, SNES::srtc.rtc, 20, false);
	if(SNES::cartridge.has_spc7110rtc()) create_region(ret, "RTC", 0x00022000, SNES::spc7110.rtc, 20, false);
	if(SNES::cartridge.has_necdsp()) {
		create_region(ret, "DSPRAM", 0x00023000, reinterpret_cast<uint8_t*>(SNES::necdsp.dataRAM), 4096,
			false, true);
		create_region(ret, "DSPPROM", 0xF0000000, reinterpret_cast<uint8_t*>(SNES::necdsp.programROM), 65536,
			true, true);
		create_region(ret, "DSPDROM", 0xF0010000, reinterpret_cast<uint8_t*>(SNES::necdsp.dataROM), 4096,
			true, true);
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
		create_region(ret, "GBROM", 0x90000000, GameBoy::cartridge.romdata, GameBoy::cartridge.romsize, true);
		create_region(ret, "GBRAM", 0x20000000, GameBoy::cartridge.ramdata, GameBoy::cartridge.ramsize, false);
	}
	return ret;
}

std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height)
{
	uint32_t h = 1, v = 1;
	if(width < 400)
		h = 2;
	if(height < 400)
		v = 2;
	return std::make_pair(h, v);
}

emucore_callbacks::~emucore_callbacks() throw()
{
}

std::pair<uint64_t, uint64_t> core_get_bus_map()
{
	return std::make_pair(0x1000000, 0x1000000);
}

function_ptr_command<arg_filename> dump_core("dump-core", "No description available",
	"No description available\n",
	[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
		std::vector<char> out;
		core_serialize(out);
		std::ofstream x(args, std::ios_base::out | std::ios_base::binary);
		x.write(&out[0], out.size());
	});


struct emucore_callbacks* ecore_callbacks;
#endif