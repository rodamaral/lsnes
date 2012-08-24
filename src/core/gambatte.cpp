#include "lsnes.hpp"
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "core/misc.hpp"
#include "core/emucore.hpp"
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/window.hpp"
#include "library/pixfmt-rgb32.hpp"
#include "library/string.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"
#include "library/framebuffer.hpp"
#define HAVE_CSTDINT
#include "libgambatte/include/gambatte.h"

#define SAMPLES_PER_FRAME 35112

#define LOGICAL_BUTTON_LEFT 0
#define LOGICAL_BUTTON_RIGHT 1
#define LOGICAL_BUTTON_UP 2
#define LOGICAL_BUTTON_DOWN 3
#define LOGICAL_BUTTON_A 4
#define LOGICAL_BUTTON_B 5
#define LOGICAL_BUTTON_SELECT 6
#define LOGICAL_BUTTON_START 7

const char* button_symbols = "ABsSrlud";

namespace
{
	int regions_compatible(unsigned rom, unsigned run)
	{
		return 1;
	}

	unsigned header_fn(size_t r)
	{
		return 0;
	}


	core_type* internal_rom = NULL;
	extern core_type type_dmg;
	extern core_type type_gbc;
	extern core_type type_gbc_gba;
	bool rtc_fixed;
	time_t rtc_fixed_val;
	gambatte::GB* instance;
	unsigned frame_overflow = 0;
	std::vector<unsigned char> romdata;
	uint32_t primary_framebuffer[160*144];
	uint32_t norom_framebuffer[160*144];
	uint32_t accumulator_l = 0;
	uint32_t accumulator_r = 0;
	unsigned accumulator_s = 0;

	void init_norom_framebuffer()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		for(size_t i = 0; i < 160 * 144; i++)
			norom_framebuffer[i] = 0xFF8080;
	}

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
			for(unsigned i = 0; i < 8; i++)
				if(ecore_callbacks->get_input(0, 0, i))
					v |= (1 << i);
			return v;
		};
	} getinput;

	int load_rom_common(core_romimage* img, unsigned flags, uint64_t rtc_sec, uint64_t rtc_subsec,
		core_type* inttype)
	{
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
	}

	int load_rom_dmg(core_romimage* img, uint64_t rtc_sec, uint64_t rtc_subsec)
	{
		return load_rom_common(img, gambatte::GB::FORCE_DMG, rtc_sec, rtc_subsec, &type_dmg);
	}

	int load_rom_gbc(core_romimage* img, uint64_t rtc_sec, uint64_t rtc_subsec)
	{
		return load_rom_common(img, 0, rtc_sec, rtc_subsec, &type_gbc);
	}

	int load_rom_gbc_gba(core_romimage* img, uint64_t rtc_sec, uint64_t rtc_subsec)
	{
		return load_rom_common(img, gambatte::GB::GBA_CGB, rtc_sec, rtc_subsec, &type_gbc_gba);
	}

	uint64_t magic[4] = {35112, 2097152, 16742706, 626688};
	
	core_region region_world("world", "World", 0, 0, false, magic, regions_compatible);
	core_romimage_info image_rom_dmg("rom", "Cartridge ROM", 1, header_fn);
	core_romimage_info image_rom_gbc("rom", "Cartridge ROM", 1, header_fn);
	core_romimage_info image_rom_gbca("rom", "Cartridge ROM", 1, header_fn);
	core_type type_dmg("dmg", "Game Boy", 1, load_rom_dmg, "gb;dmg");
	core_type type_gbc("gbc", "Game Boy Color", 0, load_rom_gbc, "gbc;cgb");
	core_type type_gbc_gba("gbc_gba", "Game Boy Color (GBA)", 2, load_rom_gbc_gba, "");
	core_type_region_bind bind_A(type_dmg, region_world);
	core_type_region_bind bind_B(type_gbc, region_world);
	core_type_region_bind bind_C(type_gbc_gba, region_world);
	core_type_image_bind bind_D(type_dmg, image_rom_dmg, 0);
	core_type_image_bind bind_E(type_gbc, image_rom_gbc, 0);
	core_type_image_bind bind_F(type_gbc_gba, image_rom_gbca, 0);
	core_sysregion sr1("gdmg", type_dmg, region_world);
	core_sysregion sr2("ggbc", type_gbc, region_world);
	core_sysregion sr3("ggbca", type_gbc_gba, region_world);

	const char* buttonnames[] = {"left", "right", "up", "down", "A", "B", "select", "start"};
	
	void _set_core_controller(unsigned port) throw() {}
	
	struct porttype_gamepad : public porttype_info
	{
		porttype_gamepad() : porttype_info("gamepad", "Gamepad", 1, generic_port_size<1, 0, 8>())
		{
			write = generic_port_write<1, 0, 8>;
			read = generic_port_read<1, 0, 8>;
			display = generic_port_display<1, 0, 8, 0>;
			serialize = generic_port_serialize<1, 0, 8, 0>;
			deserialize = generic_port_deserialize<1, 0, 8>;
			legal = generic_port_legal<1>;
			deviceflags = generic_port_deviceflags<1, 1>;
			ctrlname = "gamepad";
			controllers = 1;
			set_core_controller = _set_core_controller;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller)
				return -1;
			if(lbid == LOGICAL_BUTTON_A)		return 0;
			if(lbid == LOGICAL_BUTTON_B)		return 1;
			if(lbid == LOGICAL_BUTTON_SELECT)	return 2;
			if(lbid == LOGICAL_BUTTON_START)	return 3;
			if(lbid == LOGICAL_BUTTON_RIGHT)	return 4;
			if(lbid == LOGICAL_BUTTON_LEFT)		return 5;
			if(lbid == LOGICAL_BUTTON_UP)		return 6;
			if(lbid == LOGICAL_BUTTON_DOWN)		return 7;
			return -1;
		}
	} gamepad;

	struct porttype_none : public porttype_info
	{
		porttype_none() : porttype_info("none", "None", 0, generic_port_size<0, 0, 0>())
		{
			write = generic_port_write<0, 0, 0>;
			read = generic_port_read<0, 0, 0>;
			display = generic_port_display<0, 0, 0, 0>;
			serialize = generic_port_serialize<0, 0, 0, 0>;
			deserialize = generic_port_deserialize<0, 0, 0>;
			legal = generic_port_legal<2>;
			deviceflags = generic_port_deviceflags<0, 0>;
			ctrlname = "";
			controllers = 0;
			set_core_controller = _set_core_controller;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return -1;
		}
	} none;

}

std::string get_logical_button_name(unsigned lbid) throw(std::bad_alloc)
{
	if(lbid >= sizeof(buttonnames) / sizeof(buttonnames[0]))
		return "";
	return buttonnames[lbid];
}

uint32_t get_snes_cpu_rate() { return 0; }
uint32_t get_snes_apu_rate() { return 0; }
std::string get_core_identifier()
{
	return "libgambatte "+gambatte::GB::version();
}

std::pair<unsigned, unsigned> get_core_logical_controller_limits()
{
	return std::make_pair(1, 8);
}

bool get_core_need_analog()
{
	return false;
}

std::string get_core_default_port(unsigned port)
{
	return (port == 0) ? "gamepad" : "none";
}

core_region& core_get_region()
{
	return region_world;
}

std::pair<uint32_t, uint32_t> get_video_rate()
{
	return std::make_pair(262144, 4389);
}

std::pair<uint32_t, uint32_t> get_audio_rate()
{
	return std::make_pair(32768, 1);
}

bool core_set_region(core_region& region)
{
	return (&region == &region_world);
}

std::pair<bool, uint32_t> core_emulate_cycles(uint32_t cycles)
{
	messages << "Delayed resets are not supported";
	return std::make_pair(false, 0);
}

void core_runtosave()
{
}

void core_reset()
{
	if(!internal_rom)
		return;
	instance->reset();
}

void do_basic_core_init()
{
	instance = new gambatte::GB;
	instance->setInputGetter(&getinput);
	instance->set_walltime_fn(walltime_fn);
}

void core_power()
{
}

void core_unload_cartridge()
{
}

void set_preload_settings()
{
}

void core_install_handler()
{
}

void core_uninstall_handler()
{
}

void core_emulate_frame_nocore()
{
	init_norom_framebuffer();
	while(true) {
		unsigned samples_emitted = SAMPLES_PER_FRAME - frame_overflow;
		for(unsigned i = 0; i < samples_emitted; i++) {
			accumulator_l += 32768;
			accumulator_r += 32768;
			accumulator_s++;
			if((accumulator_s & 63) == 0) {
				uint16_t l2 = accumulator_l >> 6;
				uint16_t r2 = accumulator_r >> 6;
				platform::audio_sample(l2, r2);
				information_dispatch::do_sample(l2 - 32768, r2 - 32768);
				accumulator_l = accumulator_r = 0;
				accumulator_s = 0;
			}
		}
		ecore_callbacks->timer_tick(samples_emitted, 2097152);
		frame_overflow = 0;
		break;
	}
	framebuffer_info inf;
	inf.type = &_pixel_format_rgb32;
	inf.mem = const_cast<char*>(reinterpret_cast<const char*>(norom_framebuffer));
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
}
void core_emulate_frame()
{
	if(!internal_rom) {
		core_emulate_frame_nocore();
		return;
	}
	uint32_t samplebuffer[SAMPLES_PER_FRAME + 2064];
	while(true) {
		unsigned samples_emitted = SAMPLES_PER_FRAME - frame_overflow;
		long ret = instance->runFor(primary_framebuffer, 160, samplebuffer, samples_emitted);
		for(unsigned i = 0; i < samples_emitted; i++) {
			uint32_t l = (int32_t)(int16_t)(samplebuffer[i]) + 32768;
			uint32_t r = (int32_t)(int16_t)(samplebuffer[i] >> 16) + 32768;
			accumulator_l += l;
			accumulator_r += r;
			accumulator_s++;
			if((accumulator_s & 63) == 0) {
				uint16_t l2 = accumulator_l >> 6;
				uint16_t r2 = accumulator_r >> 6;
				platform::audio_sample(l2, r2);
				information_dispatch::do_sample(l2 - 32768, r2 - 32768);
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
}

std::list<vma_info> get_vma_list()
{
	std::list<vma_info> vmas;
	if(!internal_rom)
		return vmas;
	vma_info sram;
	vma_info wram;
	vma_info vram;
	vma_info ioamhram;
	vma_info rom;

	auto g = instance->getSaveRam();
	sram.name = "SRAM";
	sram.base = 0x20000;
	sram.size = g.second;
	sram.backing_ram = g.first;
	sram.native_endian = false;
	sram.readonly = false;
	sram.iospace_rw = NULL;

	auto g2 = instance->getWorkRam();
	wram.name = "WRAM";
	wram.base = 0;
	wram.size = g2.second;
	wram.backing_ram = g2.first;
	wram.native_endian = false;
	wram.readonly = false;
	wram.iospace_rw = NULL;

	auto g3 = instance->getVideoRam();
	vram.name = "VRAM";
	vram.base = 0x10000;
	vram.size = g3.second;
	vram.backing_ram = g3.first;
	vram.native_endian = false;
	vram.readonly = false;
	vram.iospace_rw = NULL;

	auto g4 = instance->getIoRam();
	ioamhram.name = "IOAMHRAM";
	ioamhram.base = 0x18000;
	ioamhram.size = g4.second;
	ioamhram.backing_ram = g4.first;
	ioamhram.native_endian = false;
	ioamhram.readonly = false;
	ioamhram.iospace_rw = NULL;

	rom.name = "ROM";
	rom.base = 0x80000000;
	rom.size = romdata.size();
	rom.backing_ram = (void*)&romdata[0];
	rom.native_endian = false;
	rom.readonly = true;
	rom.iospace_rw = NULL;

	if(sram.size)
		vmas.push_back(sram);
	vmas.push_back(wram);
	vmas.push_back(rom);
	vmas.push_back(vram);
	vmas.push_back(ioamhram);
	return vmas;
}

std::set<std::string> get_sram_set()
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

std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc)
{
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

void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
{
	if(!internal_rom)
		return;
	std::vector<char>& x = sram["main"];
	std::vector<char>& x2 = sram["rtc"];
	auto g = instance->getSaveRam();
	if(x.size() != g.second)
		messages << "WARNING: SRAM 'main': Loaded " << x.size()
			<< " bytes, but the SRAM is " << g.second << "." << std::endl;
	memcpy(g.first, &x[0], min(x.size(), g.second));
	time_t timebase = 0;
	for(size_t i = 0; i < 8 && i < x2.size(); i++)
		timebase |= (unsigned long long)(unsigned char)x2[i] << (8 * i);
	instance->setRtcBase(timebase);
}

std::vector<char> cmp_save;

function_ptr_command<> cmp_save1("set-cmp-save", "", "\n", []() throw(std::bad_alloc, std::runtime_error) {
	if(!internal_rom)
		return;
	instance->saveState(cmp_save);
});

function_ptr_command<> cmp_save2("do-cmp-save", "", "\n", []() throw(std::bad_alloc, std::runtime_error) {
	std::vector<char> x;
	if(!internal_rom)
		return;
	instance->saveState(x, cmp_save);
});


void core_serialize(std::vector<char>& out)
{
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

void core_unserialize(const char* in, size_t insize)
{
	if(!internal_rom)
		throw std::runtime_error("Can't load without ROM");
	size_t foffset = insize - 2 - 4 * sizeof(primary_framebuffer) / sizeof(primary_framebuffer[0]);
	std::vector<char> tmp;
	tmp.resize(foffset);
	memcpy(&tmp[0], in, foffset);
	instance->loadState(tmp);
	for(size_t i = 0; i < sizeof(primary_framebuffer) / sizeof(primary_framebuffer[0]); i++)
		primary_framebuffer[i] = read32ube(&in[foffset + 4 * i]);

	unsigned x1 = (unsigned char)in[insize - 2];
	unsigned x2 = (unsigned char)in[insize - 1];
	frame_overflow = x1 * 256 + x2;
}

std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height)
{
	return std::make_pair(max(512 / width, (uint32_t)1), max(448 / height, (uint32_t)1));
}

emucore_callbacks::~emucore_callbacks() throw() {}

struct emucore_callbacks* ecore_callbacks;
