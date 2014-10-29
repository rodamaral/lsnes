#include <cstddef>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "c-interface-translate.hpp"
#define PROFILE_COMPATIBILITY
#define DEBUGGER
#include "snes/snes.hpp"
#include "gameboy/gameboy.hpp"
#include "snes/chip/icd2/gameboy.hpp"
#include "gameboy/gameboy.hpp"
#include "ui-libsnes/libsnes.hpp"
#define HAVE_CSTDINT
#include "libgambatte/include/gambatte.h"

namespace
{
bool disable_breakpoints = false;	//Breakpoint temporary disable.
gambatte::GB* gb_instance;		//Gambatte instance to use.
bool trace_cpu_enable = false;		//True if tracing S-CPU is enabled.
bool trace_smp_enable = false;		//True if tracing S-SMP is enabled.
unsigned delayreset_cycles_run = 0;	//Number of cycles already ran for delay reset.
unsigned delayreset_cycles_target = 0;	//Number of cycles to run for delay reset.
bool video_refresh_done = false;	//Set to false on start of frame, set to true on video refresh.
bool forced_hook = false;		//True if there is forced SNES hook function.
std::string core_bsnes_gambatte_name;	//Name of core.
bool gb_raw_signal_enabled = false;	//If to enable GB raw signal display.
uint32_t gb_raw_signal[160*144];	//The GB raw signal display.
uint64_t gb_tsc;			//Gameboy TSC.

#include "tempmem.hpp"
#include "callbacks.hpp"
#include "controllerjson.hpp"
#include "memregions.hpp"
#include "registers.hpp"
#include "strfmt.hpp"
#include "gbdisasm.hpp"
#include "debug.hpp"
#include "sram.hpp"
#include "serialize.hpp"

using lsnes_interface::e2t;

//Interface functions return error, so NULL is success.
#define RET_OK NULL

#define def_sysregion_sgb_ntsc 0
#define def_core_bsnes_gambatte 0
#define def_region_ntsc 0
#define def_type_supergameboy 0
#define LSNES_END_LIST 0xFFFFFFFFU
#define TMP_ERROR 0
#define TMP_AUDIO 1
#define TMP_TXTBMP 2
#define TMP_TEXT 3
#define TMP_VIDEO 4
#define TMP_UNSPEC 5
#define DURATION_NTSC_FRAME 357366
#define DURATION_NTSC_FIELD 357368



#define CASE_CALL(X) \
	case X : \
	ret = interface(item, *(e2t<X>::t)params); \
	break

#define CASE_CALL_NOITEM(X) \
	case X : \
	ret = interface(*(e2t<X>::t)params); \
	break

#define EMIT_SUBCASE(X, I) \
	case def_##I : \
	ret = I (*(e2t<X>::t)params); \
	break

#define CASE_CALL_CORE(X) \
	case X : \
	switch(item) { \
	EMIT_SUBCASE(X, core_bsnes_gambatte ); \
	default: \
	ret = complain_unknown_core(item); \
	break; \
	} \
	break

#define CASE_CALL_TYPE(X) \
	case X : \
	switch(item) { \
	EMIT_SUBCASE(X, type_supergameboy ); \
	default: \
	ret = complain_unknown_type(item); \
	break; \
	} \
	break

#define CASE_CALL_REGION(X) \
	case X : \
	switch(item) { \
	EMIT_SUBCASE(X, region_ntsc ); \
	default: \
	ret = complain_unknown_type(item); \
	break; \
	} \
	break

#define CASE_CALL_SYSREGION(X) \
	case X : \
	switch(item) { \
	EMIT_SUBCASE(X, sysregion_sgb_ntsc ); \
	default: \
	ret = complain_unknown_type(item); \
	break; \
	} \
	break


unsigned sysregions_supported[] = { def_sysregion_sgb_ntsc, LSNES_END_LIST };
const char* trace_cpu_list[] = {"S-CPU", "S-SMP", "GB-CPU", NULL};
static unsigned compat_regions_ntsc[] = {def_region_ntsc, LSNES_END_LIST};
unsigned regions_sgb[] = {def_region_ntsc, LSNES_END_LIST};
unsigned controller_count[] = {0, 1, 1, 4, 4, 1, 1, 2, 1, 2};

lsnes_core_get_core_info_aparam delayparam[] = {
	{"delay", "int:0,999999999"},
	{NULL, NULL}
};

lsnes_core_get_core_info_aparam checkparam[] = {
	{"", "toggle"},
	{NULL, NULL}
};

lsnes_core_get_core_info_action actions_list[] = {
	{0, "reset", "Soft reset", NULL},
	{1, "hardreset", "Hard reset", NULL},
	{2, "delayreset", "Delayed soft reset", delayparam},
	{3, "delayhardreset", "Delayed hard reset", delayparam},
	{4, "bg1pri0", "Layers‣BG1 Priority 0", checkparam},
	{5, "bg1pri1", "Layers‣BG1 Priority 1", checkparam},
	{8, "bg2pri0", "Layers‣BG2 Priority 0", checkparam},
	{9, "bg2pri1", "Layers‣BG2 Priority 1", checkparam},
	{12, "bg3pri0", "Layers‣BG3 Priority 0", checkparam},
	{13, "bg3pri1", "Layers‣BG3 Priority 1", checkparam},
	{16, "bg4pri0", "Layers‣BG4 Priority 0", checkparam},
	{17, "bg4pri1", "Layers‣BG4 Priority 1", checkparam},
	{20, "oampri0", "Layers‣Sprite Priority 0", checkparam},
	{21, "oampri1", "Layers‣Sprite Priority 1", checkparam},
	{22, "oampri2", "Layers‣Sprite Priority 2", checkparam},
	{23, "oampri3", "Layers‣Sprite Priority 3", checkparam},
	{-1, NULL, NULL, NULL}
};

lsnes_core_get_type_info_romimage sgb_images[] = {
	{"rom", "SGB BIOS", 1, 0, 512, "sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh"},
	{"dmg", "DMG ROM", 2, 0, 512, "gb;dmg;sgb"},
	{NULL, NULL, 0, 0, 0, NULL}
};

lsnes_core_get_type_info_paramval boolean_values[] = {
	{"0", "false", 0},
	{"1", "true", 1},
	{NULL, NULL, -1}
};

lsnes_core_get_type_info_paramval port1_values[] = {
	{"none", "None", 0},
	{"gamepad", "Gamepad", 1},
	{"gamepad16", "Gamepad (16-button)", 2},
	{"ygamepad16", "Y-cabled gamepad (16-button)", 9},
	{"multitap", "Multitap", 3},
	{"multitap16", "Multitap (16-button)", 4},
	{"mouse", "Mouse", 5},
	{NULL, NULL, -1}
};

lsnes_core_get_type_info_paramval port2_values[] = {
	{"none", "None", 0},
	{"gamepad", "Gamepad", 1},
	{"gamepad16", "Gamepad (16-button)", 2},
	{"ygamepad16", "Y-cabled gamepad (16-button)", 9},
	{"multitap", "Multitap", 3},
	{"multitap16", "Multitap (16-button)", 4},
	{"mouse", "Mouse", 5},
	{"superscope", "Super Scope", 8},
	{"justifier", "Justifier", 6},
	{"justifiers", "2 Justifiers", 7},
	{NULL, NULL, -1}
};

lsnes_core_get_type_info_param bsnes_gambatte_settings[] = {
	{"port1", "Port 1 Type", "gamepad", port1_values, NULL},
	{"port2", "Port 2 Type", "none", port2_values, NULL},
	{"saveevery", "Emulate saving each frame", "0", boolean_values, NULL},
	{"radominit", "Random initial state", "0", boolean_values, NULL},
	{NULL, NULL, NULL, NULL, NULL}
};

//Variables
bool last_interlace = false;		//Last frame was interlaced.
int internal_rom = -1;			//Internal ROM type.
SNES::Interface* old = NULL;		//Old SNES interface object.
bool port_is_ycable[2];			//Port has Y-Cable connected flags for both ports.
int16_t soundbuf[8192] = {0};		//Sound buffer.
size_t soundbuf_fill = 0;		//Amount of data in sound buffer.
bool stepping_into_save = false;	//Set when running to save point, false otherwise.
bool have_saved_this_frame = false;	//True if state has been saved this frame.
int do_reset_flag = -1;			//Number of cycles to next reset (<0 if no reset).
bool do_hreset_flag = false;		//True if next reset will be hard.
bool last_hires = false;		//Last frame was hires.
bool save_every_frame = false;		//True to emulate saving each frame.
bool reallocate_debug = true;		//Next load will reallocate debug buffers.
gambatte::debugbuffer debugbuf;		//Gambatte debug buffer.
size_t gb_cur_romsize;			//Current GB ROM size.
size_t gb_cur_ramsize;			//Current GB RAM size.
bool rtc_fixed;				//RTC fixed?
time_t rtc_fixed_val;			//Value RTC is fixed to.
std::vector<unsigned char> gb_romdata;	//Gameboy ROM data.
unsigned current_ly;			//Current gameboy LY.
uint8_t mlt_req;			//Mlt_req value for SGB.
uint16_t current_scanline[160];		//Current scanline buffer (converted).
uint8_t sgb_button_state;		//Current button state from SNES.
uint8_t last_fdiv;			//Last frequency divider.
uint8_t p1_value;
bool pflag;				//Polled flag.
bool pending_poll;			//Poll is pending.
GameBoy::Interface* bsnes_if;		//BSNES GB interface.


const char* complain_unknown_item(const char* itype, unsigned item)
{
	return tmp_sprintf(TMP_ERROR, "Unknown %s type %u", itype, item);
}

const char* complain_unknown_core(unsigned type)
{
	return complain_unknown_item("core", type);
}

const char* complain_unknown_type(unsigned type)
{
	return complain_unknown_item("type", type);
}

const char* complain_unknown_region(unsigned type)
{
	return complain_unknown_item("region", type);
}

const char* complain_unknown_sysregion(unsigned type)
{
	return complain_unknown_item("sysregion", type);
}

lsnes_core_get_controllerconfig_logical_entry mklcid(unsigned p, unsigned c)
{
	lsnes_core_get_controllerconfig_logical_entry e;
	e.port = p;
	e.controller = c;
	return e;
}

struct settings_decoded
{
	settings_decoded(lsnes_core_system_setting* settings)
	{
		type1 = 1;
		type2 = 0;
		esave = 0;
		irandom = 0;
		for(lsnes_core_system_setting* i = settings; i->name; i++) {
			signed settings_decoded::*ptr = NULL;
			if(!strcmp(i->name, "port1")) ptr = &settings_decoded::type1;
			if(!strcmp(i->name, "port2")) ptr = &settings_decoded::type2;
			if(!strcmp(i->name, "saveevery")) ptr = &settings_decoded::esave;
			if(!strcmp(i->name, "radominit")) ptr = &settings_decoded::irandom;
			for(lsnes_core_get_type_info_param* j = bsnes_gambatte_settings; j->iname; j++) {
				if(!strcmp(j->iname, i->name)) {
					for(lsnes_core_get_type_info_paramval* k = j->values; k->iname; k++) {
						if(!strcmp(k->iname, i->value))
							this->*ptr = k->index;
					}
				}
			}
		}
	}
	signed type1;
	signed type2;
	signed esave;
	signed irandom;
};

struct bsnes_interface : public SNES::Interface
{
	string path(SNES::Cartridge::Slot slot, const string &hint)
	{
		const char* _hint = hint;
		std::string _hint2 = _hint;
		std::string fwp = cb_get_firmware_path();
		std::string finalpath = fwp + "/" + _hint2;
		return finalpath.c_str();
	}

	time_t currentTime()
	{
		return cb_get_time();
	}

	time_t randomSeed()
	{
		return cb_get_randomseed();
	}

	void notifyLatched()
	{
		cb_notify_latch(NULL);
	}

	void videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan);

	void audioSample(int16_t l_sample, int16_t r_sample)
	{
		soundbuf[soundbuf_fill++] = l_sample;
		soundbuf[soundbuf_fill++] = r_sample;
		//The SMP emits a sample every 768 ticks of its clock. Use this in order to keep track of
		//time.
		cb_timer_tick(768, SNES::system.apu_frequency());
	}

	int16_t inputPoll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
	{
		if(port_is_ycable[port ? 1 : 0]) {
			int16_t bit0 = cb_get_input(port ? 2 : 1, 0, id);
			int16_t bit1 = cb_get_input(port ? 2 : 1, 1, id);
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
		return cb_get_input(port ? 2 : 1, index, id) - offset;
	}
} my_interface_obj;

void bsnes_interface::videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan)
{
	last_hires = hires;
	last_interlace = interlace;
	if(stepping_into_save) {
		(message_output() << "Got video refresh in runtosave, expect desyncs!" << std::endl).end();
	}
	video_refresh_done = true;
	uint32_t fps_n, fps_d;
	fps_n = SNES::system.cpu_frequency();
	fps_d = interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;

	lsnes_core_framebuffer_info inf;
	inf.type = LSNES_CORE_PIXFMT_LRGB;
	inf.mem = const_cast<char*>(reinterpret_cast<const char*>(data));
	inf.physwidth = 512;
	inf.physheight = 512;
	inf.physstride = 2048;
	inf.width = hires ? 512 : (gb_raw_signal_enabled ? 416 : 256);
	inf.height = 224 * (interlace ? 2 : 1);
	inf.stride = interlace ? 2048 : 4096;
	inf.offset_x = 0;
	inf.offset_y = (overscan ? 16 : 9) * 2;
	if(gb_raw_signal_enabled && !hires) {
		uint32_t* tmp = tmpalloc_array<uint32_t>(TMP_VIDEO, 512 * 512);
		memcpy(tmp, inf.mem, 512 * 512 * sizeof(uint32_t));
		inf.mem = (const char*)tmp;
		for(uint32_t y = 0; y < 144; y++) {
			memcpy(tmp + (inf.offset_y + y) * (inf.stride / sizeof(uint32_t)) + (inf.offset_x + 256),
				gb_raw_signal + 160 * y, 160 * sizeof(uint32_t));
			for(uint32_t x = 0; x < 160; x++)
				gb_raw_signal[160 * y + x] &= 0x7801F;
		}
	}

	cb_submit_frame(&inf, fps_n, fps_d);
	if(soundbuf_fill > 0) {
		auto freq = SNES::system.apu_frequency();
		cb_submit_sound(soundbuf, soundbuf_fill / 2, true, freq / 768.0);
		soundbuf_fill = 0;
	}
}

class myinput : public gambatte::InputGetter
{
public:
	unsigned operator()()
	{
		//Input is read elsewhere.
		return 0;
	};
} getinput;

uint8_t read_gb_inputs(bool p14, bool p15)
{
	uint8_t u = 0xF;
	unsigned v = 0;
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::Right) ? 0x10 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::Left) ? 0x20 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::Up) ? 0x40 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::Down) ? 0x80 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::A) ? 0x01 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::B) ? 0x02 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::Select) ? 0x04 : 0);
	v |= (bsnes_if->inputPoll((unsigned)GameBoy::Input::Start) ? 0x08 : 0);
	if(p14 && p15) u -= mlt_req;
	if(!p14) u &= ((v >> 4) ^ 15);
	if(!p15) u &= ((v & 15) ^ 15);
	if(p14) u |= 0x10;
	if(p15) u |= 0x20;
	return u;
}

gambatte::extra_callbacks gambatte_sgb_hooks = {
	.context = NULL,
	.read_p1_high = [](void* context) -> uint8_t {
		return 0xF - mlt_req;
	},
        .write_p1 = [](void* context, uint8_t value) -> void {
		bool p14 = (value & 0x10);
		bool p15 = (value & 0x20);
		bsnes_if->joypWrite(p15, p14);
		p1_value = value;
		//If either line is low, mark poll to occur on next read.
		pending_poll = (!p14 || !p15);
	},
        .lcd_scan = [](void* context, uint8_t y, const uint32_t* data)
	{
		current_ly = y;
		if(y < 144) {
			for(size_t i = 0; i < 160; i++)
				current_scanline[i] = data[i];
			if(gb_raw_signal_enabled) {
				for(size_t i = 0; i < 160; i++)
					gb_raw_signal[160 * y + i] = 0x78000 + ((~data[i]) & 3) * 0x294A;
			}
			bsnes_if->lcdScanline();
		}
	},
	.read_p1 = [](void* context, uint8_t& v) -> bool
	{
		bool p14 = (p1_value & 0x10);
		bool p15 = (p1_value & 0x20);
		v = read_gb_inputs(p14, p15);
		//If there is pending poll, do the actual poll.
		if(pending_poll) pflag = true;
		return true;
	},
	.ppu_update_timeslice = true
};

//	FDIV	Soundrate
//	8	21477272/512
//	10	21477272/640
//	14	21477272/896
//	18	21477272/1152
//

SNES::gameboy_interface gambatte_gb_if = {
        .do_init = [](GameBoy::Interface* interface) -> void {
		//Record the interface and reset the system.
		bsnes_if = interface;
		gb_instance->reset();
	},
	.get_ly = []() -> unsigned {
		return current_ly;
	},
	.get_cur_scanline = []() -> uint16_t* {
		return current_scanline;
	},
	.set_mlt_req = [](uint8_t req) -> void {
		mlt_req = req;
	},
	.serialize = [](nall::serializer& s) -> void {
		//This can't be done right, do it outside bsnes. Nothing to do here.
	},
	.runtosave = []() -> void {
		//Nothing to do.
	},
	.run_timeslice = [](SNES::Coprocessor* proc) -> void {
		//Dummy framebuffer.
		uint32_t dummy_fb[160 * 144];
		int64_t slice;
		uint32_t* abuffer = NULL;
		unsigned _slice;
		uint16_t old_ip, new_ip;
		//Calculate number of SNES master clock ticks to run.
again:
		slice = -proc->clock / SNES::cpu.frequency;
		//Correct for frequency divider (FIXME: this doesn't correct for divier changes).
		//Also always run one cycle too long. Otherwise scheduler will lock up.
		slice = slice / SNES::icd2.fdiv + 1;
		if(slice <= 0)
			return;		//No time to run.
		//Now slice is number of GB clocks to run.
		if(last_fdiv != SNES::icd2.fdiv) {
			last_fdiv = SNES::icd2.fdiv;
			SNES::audio.coprocessor_frequency((float)SNES::cpu.frequency / last_fdiv);
		}
		_slice = slice;
		abuffer = tmpalloc_array<uint32_t>(TMP_AUDIO, slice + 2064);
		old_ip = gb_instance->get_cpureg(gambatte::GB::REG_PC);
		gb_instance->runFor(dummy_fb, 160, abuffer, _slice);
		new_ip = gb_instance->get_cpureg(gambatte::GB::REG_PC);
		for(unsigned i = 0; i < _slice; i++) {
			int16_t l, r;
			l = abuffer[i];
			r = abuffer[i] >> 16;
			SNES::audio.coprocessor_sample(l, r);
		}
		gb_tsc += _slice;
		proc->step(_slice * last_fdiv);
		goto again;
	},
	.cartridge_sha256 = []() -> nall::string {
		return sha256(&gb_romdata[0], gb_romdata.size());
	},
	.default_rates = false,
};

unsigned index_to_bsnes_type[] = {
	SNES_DEVICE_NONE, SNES_DEVICE_JOYPAD, SNES_DEVICE_JOYPAD, SNES_DEVICE_MULTITAP, SNES_DEVICE_MULTITAP,
	SNES_DEVICE_MOUSE, SNES_DEVICE_JUSTIFIER, SNES_DEVICE_JUSTIFIERS, SNES_DEVICE_SUPER_SCOPE,
	SNES_DEVICE_JOYPAD
};

std::string get_cartridge_name()
{
	std::ostringstream name;
	if(gb_romdata.size() < 0x200)
		return "";	//Bad.
	for(unsigned i = 0; i < 16; i++) {
		unsigned char ch = gb_romdata[0x134 + i];
		if(ch) {
			if(ch >= 32 && ch <= 126)
				name << (char)ch;
			else
				name << hexch[ch >> 4] << hexch[ch & 15];
				
		} else
			break;
	}
	return name.str();
}

time_t walltime_fn()
{
	if(rtc_fixed)
		return rtc_fixed_val;
	if(cb_get_time)
		return cb_get_time();
	else
		return time(0);
}

const char* load_rom(unsigned ctype, lsnes_core_load_rom_image* images, lsnes_core_system_setting* settings,
	uint64_t secs, uint64_t subsecs)
{
	settings_decoded ds(settings);

	snes_term();
	snes_unload_cartridge();
	SNES::config.random = (ds.irandom != 0);
	save_every_frame = (ds.esave != 0);
	SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;

	SNES::config.cpu.alt_poll_timings = true;

	//Reset Gambatte really.
	gb_instance->~GB();
	memset(gb_instance, 0, sizeof(gambatte::GB));
	new(gb_instance) gambatte::GB;
	gb_instance->setInputGetter(&getinput);
	gb_instance->set_walltime_fn(walltime_fn);
	gb_instance->setExtraCallbacks(&gambatte_sgb_hooks);

	rtc_fixed = true;
	rtc_fixed_val = secs;
	//Flags is always 0.
	if(gb_instance->load((const unsigned char*)images[1].data, images[1].size, 0,
		GameBoy::System::BootROM::sgb, sizeof(GameBoy::System::BootROM::sgb)) < 0)
		return "Error loading ROM";
	size_t sramsize = gb_instance->getSaveRam().second;
	size_t romsize = images[1].size;
	if(reallocate_debug || gb_cur_ramsize != sramsize || gb_cur_romsize != romsize) {
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
		gb_cur_ramsize = sramsize;
		gb_cur_romsize = romsize;
	}
	gb_instance->set_debug_buffer(debugbuf);
	gb_instance->set_emuflags(1);	//Crash on SIGILL.
	rtc_fixed = false;
	gb_romdata.resize(images[1].size);
	memcpy(&gb_romdata[0], images[1].data, images[1].size);

	//Setup palette (it is fixed for SGB).
	for(unsigned i = 0; i < 12; i++)
		gb_instance->setDmgPaletteColor(i >> 2, i & 3, i & 3);

	SNES::gb_override_flag = true;
	SNES::gb_if = &gambatte_gb_if;
	//Because we pass the GB ROM to bsnes, its checksum will be correct even with default loadif.
	bool r = snes_load_cartridge_super_game_boy(images[0].markup, (unsigned char*)images[0].data, images[0].size,
		images[1].markup, (unsigned char*)images[1].data, images[1].size);
	if(r) {
		last_fdiv = SNES::icd2.fdiv;
		internal_rom = ctype;
		snes_set_controller_port_device(false, index_to_bsnes_type[ds.type1]);
		snes_set_controller_port_device(true, index_to_bsnes_type[ds.type2]);
		port_is_ycable[0] = (ds.type1 == 9);
		port_is_ycable[1] = (ds.type2 == 9);
		have_saved_this_frame = false;
		do_reset_flag = -1;
		current_ly = 0;
		mlt_req = 0;
		sgb_button_state = 0;
		p1_value = 0;
		memset(current_scanline, 0, 320);
		pending_poll = false;
		pflag = false;
		gb_tsc = 0;
		cb_notify_action_update();
	}
	return r ? NULL : "Error loading ROM";
}

void bsnes_call_power_or_reset(bool hard)
{
	if(hard) {
		SNES::gb_override_flag = true;
		SNES::system.power();
		//Poweron initializes this.
		last_fdiv = SNES::icd2.fdiv;
	} else
		SNES::system.reset();
}

const char* interface(lsnes_core_enumerate_cores& arg)
{
	if(getenv("GB_SHOW_DEBUG_SCREEN"))
		gb_raw_signal_enabled = true;
	static bool basic_init_done = false;
	if(!basic_init_done) {
		SNES::bus.debug_read = bsnes_debug_read;
		SNES::bus.debug_write = bsnes_debug_write;
		old = SNES::interface;
		SNES::interface = &my_interface_obj;
		SNES::system.init();
		gb_instance = new gambatte::GB;
		gb_instance->setInputGetter(&getinput);
		gb_instance->set_walltime_fn(walltime_fn);
		uint8_t* tmp = new uint8_t[98816];
		memset(tmp, 0, 98816);
		debugbuf.wram = tmp;
		debugbuf.bus = tmp + 32768;
		debugbuf.ioamhram = tmp + 98304;
		debugbuf.read = gambatte_debug_read;
		debugbuf.write = gambatte_debug_write;
		debugbuf.trace = gambatte_debug_trace;
		debugbuf.trace_cpu = false;
		debugbuf.cart = NULL;
		debugbuf.sram = NULL;
		gb_instance->set_debug_buffer(debugbuf);
	}
	arg.sysregions = sysregions_supported;
	record_callbacks(arg);
	gb_add_disasms();
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_core_info& arg)
{
	arg.json = bsnes_gambatte_controller_json;
	arg.root_ptr = "ports";
	arg.shortname = "bsnes-gambatte";
	arg.fullname = "bsnes v085 / Gambatte r537";
	core_bsnes_gambatte_name = arg.fullname;
	arg.cap_flags1 = LSNES_CORE_CAP1_MULTIREGION | LSNES_CORE_CAP1_PFLAG | LSNES_CORE_CAP1_ACTION |
		LSNES_CORE_CAP1_BUSMAP | LSNES_CORE_CAP1_SRAM | LSNES_CORE_CAP1_RESET |
		LSNES_CORE_CAP1_SCALE | LSNES_CORE_CAP1_RUNTOSAVE | LSNES_CORE_CAP1_POWERON |
		LSNES_CORE_CAP1_UNLOAD | LSNES_CORE_CAP1_DEBUG | LSNES_CORE_CAP1_TRACE |
		LSNES_CORE_CAP1_CHEAT | LSNES_CORE_CAP1_COVER | LSNES_CORE_CAP1_PREEMULATE |
		LSNES_CORE_CAP1_REGISTERS | LSNES_CORE_CAP1_MEMWATCH | LSNES_CORE_CAP1_LIGHTGUN;
	arg.actions = actions_list;
	arg.trace_cpu_list = trace_cpu_list;
	return RET_OK;
}

const char* type_supergameboy(lsnes_core_get_type_info& arg)
{
	arg.core = def_core_bsnes_gambatte;
	arg.iname = "sgb";
	arg.hname = "Super Game Boy";
	arg.sysname = "SGB";
	arg.bios = "sgb.sfc";
	arg.regions = regions_sgb;
	arg.images = sgb_images;
	arg.settings = bsnes_gambatte_settings;
	return RET_OK;
}

const char* region_ntsc(lsnes_core_get_region_info& arg)
{
	arg.iname = "ntsc";
	arg.hname = "NTSC";
	arg.priority = 0;
	arg.multi = 0;
	//Apparently the fps is wrong way around.
	arg.fps_d = 10738636;
	arg.fps_n = 178683;
	arg.compatible_runs = compat_regions_ntsc;
	return RET_OK;
}

const char* sysregion_sgb_ntsc(lsnes_core_get_sysregion_info& arg)
{
	arg.name = "gambatte_sgb_ntsc";
	arg.type = def_type_supergameboy;
	arg.region = def_region_ntsc;
	arg.for_system = "SGB";
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_av_state& arg)
{
	if(internal_rom < 0) {
		arg.fps_n = 60;
		arg.fps_d = 1;
		arg.par = 1.0;
		arg.rate_n = 64081;
		arg.rate_d = 2;
	} else {
		arg.fps_n = SNES::system.cpu_frequency();
		arg.fps_d = last_interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
		arg.par = 1.146;
		arg.rate_n = SNES::system.apu_frequency();
		arg.rate_d = static_cast<uint32_t>(768);
	}
	arg.lightgun_width = 256;
	arg.lightgun_height = 224;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_emulate& arg)
{
	if(internal_rom < 0)
		return RET_OK;
	bool was_delay_reset = false;
	int16_t reset = cb_get_input(0, 0, 1);
	int16_t hreset = 0;
	hreset = cb_get_input(0, 0, 4);
	if(reset) {
		long hi = cb_get_input(0, 0, 2);
		long lo = cb_get_input(0, 0, 3);
		long delay = 10000 * hi + lo;
		if(delay > 0) {
			was_delay_reset = true;
			(message_output() << "Executing delayed reset... This can take some time!"
				<< std::endl).end();
			video_refresh_done = false;
			delayreset_cycles_run = 0;
			delayreset_cycles_target = delay;
			forced_hook = true;
			SNES::cpu.step_event = bsnes_delayreset_fn;
again:
			SNES::system.run();
			if(SNES::scheduler.exit_reason() == SNES::Scheduler::ExitReason::DebuggerEvent
				&& SNES::debugger.break_event == SNES::Debugger::BreakEvent::BreakpointHit) {
				goto again;
			}
			SNES::cpu.step_event = nall::function<bool()>();
			forced_hook = false;
			bsnes_update_trace_hook_state();
			if(video_refresh_done) {
				//Force the reset here.
				(message_output() << "SNES reset (forced at " << delayreset_cycles_run << ")"
					<< std::endl).end();
				bsnes_call_power_or_reset(hreset);
				goto out;
			}
			bsnes_call_power_or_reset(hreset);
			(message_output() << "SNES reset (delayed " << delayreset_cycles_run << ")"
				<< std::endl).end();
		} else if(delay == 0) {
			bsnes_call_power_or_reset(hreset);
			(message_output() << "SNES reset" << std::endl).end();
		}
	}
out:
	do_reset_flag = -1;

	if(!have_saved_this_frame && save_every_frame && !was_delay_reset)
		SNES::system.runtosave();

	if(trace_cpu_enable)
		SNES::cpu.step_event = bsnes_trace_fn;
again2:
	SNES::system.run();
	if(SNES::scheduler.exit_reason() == SNES::Scheduler::ExitReason::DebuggerEvent &&
		SNES::debugger.break_event == SNES::Debugger::BreakEvent::BreakpointHit) {
		goto again2;
	}
	SNES::cpu.step_event = nall::function<bool()>();
	have_saved_this_frame = false;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_savestate& arg)
{
	if(internal_rom < 0)
		return "No ROM loaded";
	//Grab gambatte state.
	std::vector<char> gb_state;
	gb_instance->saveState(gb_state);
	//A few other variables that need saving.
	std::vector<char> oth_state;
	oth_state.resize(340);
	write_val32(&oth_state[0], current_ly);
	write_val8(&oth_state[4], mlt_req);
	write_val8(&oth_state[5], sgb_button_state);
	write_val8(&oth_state[6], last_fdiv);
	write_val8(&oth_state[7], p1_value);
	write_val16a(&oth_state[8], current_scanline, 160);
	write_val8(&oth_state[328], (pending_poll ? 1 : 0) | (pflag ? 2 : 0));
	write_val32(&oth_state[332], gb_tsc >> 32);
	write_val32(&oth_state[336], gb_tsc);
	//Grab BSNES state.
	serializer snes_state = SNES::system.serialize();

	//Put the states all together.
	char* tmp;
	arg.size = 12 + gb_state.size() + oth_state.size() + snes_state.size();
	arg.data = tmp = tmpalloc_array<char>(TMP_UNSPEC, arg.size);
	write_val32(tmp + 0, gb_state.size());
	write_val32(tmp + 4, oth_state.size());
	write_val32(tmp + 8, snes_state.size());
	size_t ptr = 12;
	memcpy(tmp + ptr, &gb_state[0], gb_state.size());
	ptr += gb_state.size();
	memcpy(tmp + ptr, &oth_state[0], oth_state.size());
	ptr += oth_state.size();
	memcpy(tmp + ptr, snes_state.data(), snes_state.size());
	ptr += snes_state.size();

	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_loadstate& arg)
{
	if(internal_rom < 0)
		return "No ROM loaded";

	unsigned gb_size, oth_size, snes_size;
	if(arg.size < 12)
		return "Bad savestate, too small";
	read_val32(arg.data + 0, gb_size); 
	read_val32(arg.data + 4, oth_size);
	read_val32(arg.data + 8, snes_size);
	if(arg.size < (uint64_t)12 + gb_size + oth_size + snes_size)
		return "Bad savestate, truncated";
	if(oth_size != 340)
		return "Bad savestate, expected 340 bytes of other data";
	
	//Other state.
	const char* oth_base = (const char*)arg.data + 12 + gb_size;
	std::vector<char> oth_s(oth_base, oth_base + oth_size);
	read_val32(&oth_s[0], current_ly);
	read_val8(&oth_s[4], mlt_req);
	read_val8(&oth_s[5], sgb_button_state);
	read_val8(&oth_s[6], last_fdiv);
	read_val8(&oth_s[7], p1_value);
	read_val16a(&oth_s[8], current_scanline, 160);
	uint8_t tmp;
	uint32_t tsc_l, tsc_h;
	read_val8(&oth_s[328], tmp);
	read_val32(&oth_s[332], tsc_h);
	read_val32(&oth_s[336], tsc_l);
	//SNES state.
	serializer snes_s(reinterpret_cast<const uint8_t*>(arg.data + 12 + gb_size + oth_size), snes_size);
	SNES::gb_override_flag = true;	//Unserialize calls power.
	if(!SNES::system.unserialize(snes_s))
		return "SGB core rejected savestate";
	//Due to what SNES does, restore GB state after SNES state.
	const char* gb_base = (const char*)arg.data + 12;
	std::vector<char> gb_s(gb_base, gb_base + gb_size);
	gb_instance->loadState(gb_s);

	pending_poll = (tmp & 1);
	pflag = (tmp & 2);
	gb_tsc = ((uint64_t)tsc_h << 32) + tsc_l;
	have_saved_this_frame = true;
	do_reset_flag = -1;
	return RET_OK;
}

const char* type_supergameboy(lsnes_core_get_controllerconfig& arg)
{
	settings_decoded ds(arg.settings);
	static unsigned ports[4];
	static lsnes_core_get_controllerconfig_logical_entry lcid[9];
	ports[0] = 11;
	ports[1] = ds.type1;
	ports[2] = ds.type2;
	ports[3] = LSNES_END_LIST;
	arg.controller_types = ports;
	arg.logical_map = lcid;

	unsigned p1controllers = controller_count[ds.type1];
	unsigned p2controllers = controller_count[ds.type2];
	if(p1controllers == 4) {
		arg.logical_map[0] = mklcid(1, 0);
		for(size_t j = 0; j < p2controllers; j++)
			arg.logical_map[j + 1] = mklcid(2U, j);
		for(size_t j = 1; j < p1controllers; j++)
			arg.logical_map[j + p2controllers] = mklcid(1U, j);
	} else {
		for(size_t j = 0; j < p1controllers; j++)
			arg.logical_map[j] = mklcid(1, j);
		for(size_t j = 0; j < p2controllers; j++)
			arg.logical_map[j + p1controllers] = mklcid(2U, j);
	}
	arg.logical_map[p1controllers + p2controllers] = mklcid(0U, 0U);
	return RET_OK;
}

const char* type_supergameboy(lsnes_core_load_rom& arg)
{
	return load_rom(def_type_supergameboy, arg.images, arg.settings, arg.rtc_sec, arg.rtc_subsec);
}

const char* core_bsnes_gambatte(lsnes_core_get_region& arg)
{
	arg.region = def_region_ntsc;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_set_region& arg)
{
	switch(arg.region) {
	case def_region_ntsc:
		return RET_OK;
	default:
		return complain_unknown_region(arg.region);
	}
}

const char* interface(lsnes_core_deinitialize& arg)
{
	delete gb_instance;
	delete[] debugbuf.wram;
	delete[] debugbuf.cart;
	delete[] debugbuf.sram;
	debugbuf.wramcheat.clear();
	debugbuf.sramcheat.clear();
	debugbuf.cartcheat.clear();
	SNES::interface = old;
	gb_remove_disasms();
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_pflag& arg)
{
	arg.pflag = pflag ? 1 : 0;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_set_pflag& arg)
{
	pflag = arg.pflag;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_action_flags& arg)
{
	if(arg.action >= 0 && arg.action <= 3)
		arg.flags = 1;
	else if(arg.action >= 4 && arg.action <= 23) {
		unsigned y = (arg.action - 4) / 4;
		arg.flags = SNES::ppu.layer_enabled[y][arg.action % 4] ? 3 : 1;
	} else
		arg.flags = 0;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_execute_action& arg)
{
	switch(arg.action) {
	case 0:		//Soft reset.
		do_reset_flag = 0;
		do_hreset_flag = false;
		break;
	case 1:		//Hard reset.
		do_reset_flag = 0;
		do_hreset_flag = true;
		break;
	case 2:		//Delayed soft reset.
		do_reset_flag = arg.params[0].integer;
		do_hreset_flag = false;
		break;
	case 3:		//Delayed hard reset.
		do_reset_flag = arg.params[0].integer;
		do_hreset_flag = true;
		break;
	}
	if(arg.action >= 4 && arg.action <= 23) {
		unsigned y = (arg.action - 4) / 4;
		SNES::ppu.layer_enabled[y][arg.action % 4] = !SNES::ppu.layer_enabled[y][arg.action % 4];
		cb_notify_action_update();
	}
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_bus_mapping& arg)
{
	arg.base = MAP_BASE_SNES_BUS;
	arg.size = 0x1010000;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_enumerate_sram& arg)
{
	std::set<std::string> set;
	if(internal_rom >= 0) {
		for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
			SNES::Cartridge::NonVolatileRAM& s = SNES::cartridge.nvram[i];
			set.insert(sram_name(s.id, s.slot));
		}
		auto g = gb_instance->getSaveRam();
		if(g.second)
			set.insert("gbsram");
		set.insert("gbrtc");
	}
	arg.srams = tmpalloc_array<const char*>(TMP_UNSPEC, set.size() + 1);
	unsigned key = TMP_UNSPEC + 1;
	for(auto& i : set) {
		arg.srams[key - TMP_UNSPEC - 1] = tmpalloc_str(key, i);
		key++;
	}
	arg.srams[key - TMP_UNSPEC - 1] = NULL;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_save_sram& arg)
{
	std::vector<std::pair<const char*, std::pair<const char*, size_t>>> out;
	unsigned key = TMP_UNSPEC + 1;
	if(internal_rom < 0)
		goto out;
	for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
		//SNES SRAMs
		SNES::Cartridge::NonVolatileRAM& r = SNES::cartridge.nvram[i];
		add_sram_entry(out, key, sram_name(r.id, r.slot), r.data, r.size);
	}
	{
		//GB SRAMs
		auto g = gb_instance->getSaveRam();
		char gbrtc[8];
		time_t timebase = gb_instance->getRtcBase();
		for(size_t i = 0; i < sizeof(gbrtc); i++)
			gbrtc[i] = ((unsigned long long)timebase >> (8 * i));

		if(g.second)
			add_sram_entry(out, key, "gbsram", g.first, g.second);
		add_sram_entry(out, key, "gbrtc", gbrtc, sizeof(gbrtc));
	}
out:
	//Copy srams to output.
	arg.srams = tmpalloc_array<lsnes_core_sram>(TMP_UNSPEC, out.size() + 1);
	unsigned k = 0;
	for(auto& i : out) {
		arg.srams[k].name = i.first;
		arg.srams[k].data = i.second.first;
		arg.srams[k].size = i.second.second;
		k++;
	}
	arg.srams[k].name = NULL;
	arg.srams[k].data = NULL;
	arg.srams[k].size = 0;
	return 0;
}

const char* core_bsnes_gambatte(lsnes_core_load_sram& arg)
{
	std::set<std::string> used;
	if(internal_rom < 0) {
		//If no ROM, warn about everything.
		for(lsnes_core_sram* i = arg.srams; i->name; i++)  {
			(message_output() << "WARNING: SRAM '" << i->name << "': Not found on cartridge."
				<< std::endl).end();
		}
		return RET_OK;
	}
	//Check for no SRAM.
	if(!arg.srams->name)
		return RET_OK;

	for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
		SNES::Cartridge::NonVolatileRAM& _r = SNES::cartridge.nvram[i];
		handle_sram_load(arg, sram_name(_r.id, _r.slot), _r.data, _r.size, used);
	}
	auto g = gb_instance->getSaveRam();
	unsigned char gbrtc[8];
	if(g.second)
		handle_sram_load(arg, "gbsram", g.first, g.second, used);
	handle_sram_load(arg, "gbrtc", gbrtc, sizeof(gbrtc), used);

	//Unpack gbrtc.
	time_t timebase = 0;
	for(size_t i = 0; i < sizeof(gbrtc); i++)
		timebase |= (unsigned long long)gbrtc[i] << (8 * i);
	gb_instance->setRtcBase(timebase);

	//Warn about unused SRAMs.
	for(lsnes_core_sram* i = arg.srams; i->name; i++)  {
		if(!used.count(i->name)) {
			(message_output() << "WARNING: SRAM '" << i->name << "': Not found on cartridge."
				<< std::endl).end();
		}
	}
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_reset_action& arg)
{
	arg.hardreset = 1;
	arg.softreset = 0;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_compute_scale& arg)
{
	arg.hfactor = (arg.width < 400 || arg.width == 416) ? 2 : 1;
	arg.vfactor = (arg.height < 400) ? 2 : 1;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_runtosave& arg)
{
	if(internal_rom < 0)
		return RET_OK;
	stepping_into_save = true;
	SNES::system.runtosave();
	have_saved_this_frame = true;
	stepping_into_save = false;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_poweron& arg)
{
	if(internal_rom < 0)
		return RET_OK;
	bsnes_call_power_or_reset(true);
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_unload_cartridge& arg)
{
	if(internal_rom < 0)
		return RET_OK;
	delete[] debugbuf.cart;
	delete[] debugbuf.sram;
	debugbuf.cart = NULL;
	debugbuf.sram = NULL;
	gb_cur_ramsize = 0;
	gb_cur_romsize = 0;
	debugbuf.wramcheat.clear();
	debugbuf.sramcheat.clear();
	debugbuf.cartcheat.clear();
	snes_term();
	snes_unload_cartridge();
	internal_rom = -1;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_debug_reset& arg)
{
	//Next load will reset gambatte trace buffers.
	reallocate_debug = true;
	SNES::bus.clearDebugFlags();
	SNES::cheat.reset();
	trace_cpu_enable = false;
	trace_smp_enable = false;
	bsnes_update_trace_hook_state();
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_set_debug_flags& arg)
{
	unsigned sflags = arg.set;
	unsigned cflags = arg.clear;
	uint64_t addr = arg.addr;
	switch(addr) {
	case 0:
		//S-CPU.
		if(sflags & 8) trace_cpu_enable = true;
		if(cflags & 8) trace_cpu_enable = false;
		bsnes_update_trace_hook_state();
		break;
	case 1:
		//S-SMP.
		if(sflags & 8) trace_smp_enable = true;
		if(cflags & 8) trace_smp_enable = false;
		bsnes_update_trace_hook_state();
		break;
	case 2:
		//GB CPU.
		if(sflags & 8) debugbuf.trace_cpu = true;
		if(cflags & 8) debugbuf.trace_cpu = false;
		break;
	}
	auto _addr = recognize_address(addr);
	switch(classify_address_kind(_addr.first)) {
	case ADDR_CLASS_ALL:
		SNES::bus.debugFlags(sflags & 7, cflags & 7);
		//Set/Clear every known GB debug.
		gambatte_set_debugbuf_all(debugbuf.wram, 32768, sflags, cflags);
		gambatte_set_debugbuf_all(debugbuf.bus, 65536, sflags, cflags);
		gambatte_set_debugbuf_all(debugbuf.ioamhram, 512, sflags, cflags);
		gambatte_set_debugbuf_all(debugbuf.sram, gb_instance->getSaveRam().second, sflags, cflags);
		gambatte_set_debugbuf_all(debugbuf.cart, gb_romdata.size(), sflags, cflags);
		break;
	case ADDR_CLASS_GB:
		if((sflags | cflags) & 7) {
			switch(_addr.first) {
			case MAP_KIND_GB_BUS:
				gambatte_set_debugbuf(debugbuf.bus, _addr.second, 65536, sflags, cflags);
				break;
			case MAP_KIND_GB_HRAM:
				gambatte_set_debugbuf(debugbuf.ioamhram, _addr.second, 512, sflags, cflags);
				break;
			case MAP_KIND_GB_ROM:
				gambatte_set_debugbuf(debugbuf.cart, _addr.second, gb_romdata.size(), sflags, cflags);
				break;
			case MAP_KIND_GB_SRAM:
				gambatte_set_debugbuf(debugbuf.sram, _addr.second, gb_instance->getSaveRam().second,
					sflags, cflags);
				break;
			case MAP_KIND_GB_WRAM:
				gambatte_set_debugbuf(debugbuf.wram, _addr.second, 32768, sflags, cflags);
				break;
			}
		}
		break;
	case ADDR_CLASS_SNES:
		if((sflags | cflags) & 7) {
			SNES::bus.debugFlags(sflags & 7, cflags & 7, _addr.first, _addr.second);
		}
		break;
	//Can't be placed on NONE.
	}
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_set_cheat& arg)
{
	uint64_t addr = arg.addr;
	uint64_t value = arg.value;
	bool set = arg.set;
	bool s = false;
	auto _addr = recognize_address(addr);
	switch(classify_address_kind(_addr.first)) {
	//Can't be placed on ALL nor NONE.
	case ADDR_CLASS_GB:
		switch(_addr.first) {
		case MAP_KIND_GB_WRAM:
			gambatte_set_cheat(debugbuf.wram, debugbuf.wramcheat, _addr.second, 32768, set, value);
			break;
		case MAP_KIND_GB_SRAM:
			gambatte_set_cheat(debugbuf.sram, debugbuf.sramcheat, _addr.second, 
				gb_instance->getSaveRam().second, set, value);
			break;
		case MAP_KIND_GB_ROM:
			gambatte_set_cheat(debugbuf.cart, debugbuf.cartcheat, _addr.second, gb_romdata.size(), set,
				value);
			break;
		}
		break;
	case ADDR_CLASS_SNES: {
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
	}
	}
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_draw_cover& arg)
{
	lsnes_core_framebuffer_info* fbinfo = tmpalloc<lsnes_core_framebuffer_info>(TMP_UNSPEC);
	uint32_t* fbmem = tmpalloc_array<uint32_t>(TMP_UNSPEC + 1, 512 * 448);
	fbinfo->type = LSNES_CORE_PIXFMT_LRGB;
	fbinfo->mem = (const char*)fbmem;
	fbinfo->physwidth = 512;
	fbinfo->physheight = 448;
	fbinfo->physstride = 2048;
	fbinfo->width = 512;
	fbinfo->height = 448;
	fbinfo->stride = 2048;
	fbinfo->offset_x = 0;
	fbinfo->offset_y = 0;
	if(cb_render_text) {
		lsnes_core_fontrender_req req = {
			.cb_ctx = NULL,
			.alloc = [](void*, size_t mem) -> void* { return tmpalloc_array<char*>(TMP_TXTBMP, mem); },
			.text = NULL,
			.text_len = -1,
			.bytes_pp = 4,
			.fg_color = 0x783E0,		//Green.
			.bg_color = 0x00000,		//Black.
			.bitmap = NULL,
			.width = 0,
			.height = 0
		};
		req.text = tmp_sprintf(TMP_TEXT, "Core: %s\nGame: %s\n", core_bsnes_gambatte_name.c_str(),
			get_cartridge_name().c_str());
		cb_render_text(&req);
		//Blit the bitmap on screen.
		uint32_t* dbuf = (uint32_t*)req.bitmap;
		size_t cwidth = (req.width < 512) ? req.width : 512;
		for(unsigned y = 0; y < 448; y++) {
			for(unsigned x = 0; x < 512; x++)
				fbmem[y * 512 + x] = req.bg_color;
			if(y < req.height)
				memcpy(fbmem + y * 512, dbuf + y * req.width, sizeof(uint32_t) * cwidth);
		}
	} else {
		for(unsigned y = 0; y < 448; y++)
			for(unsigned x = 0; x < 512; x++) {
				unsigned _x = (x >> 3) & 1;
				unsigned _y = (y >> 3) & 1;
				fbmem[y * 512 + x] = (_x ^ _y) ? 0x7FC00 : 0x7801F;
			}
	}
	arg.coverpage = fbinfo;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_pre_emulate& arg)
{
	arg.set_input(arg.context, 0, 0, 1, (do_reset_flag >= 0) ? 1 : 0);
	arg.set_input(arg.context, 0, 0, 4, do_hreset_flag ? 1 : 0);
	if(do_reset_flag >= 0) {
		arg.set_input(arg.context, 0, 0, 2, do_reset_flag / 10000);
		arg.set_input(arg.context, 0, 0, 3, do_reset_flag % 10000);
	} else {
		arg.set_input(arg.context, 0, 0, 2, 0);
		arg.set_input(arg.context, 0, 0, 3, 0);
	}
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_device_regs& arg)
{
	arg.regs = snes_gb_registers;
	return RET_OK;
}

const char* core_bsnes_gambatte(lsnes_core_get_vma_list& arg)
{
	static lsnes_core_get_vma_list_vma* tmpmem[32];
	unsigned key = 0;
	if(internal_rom >= 0) {
		tmpmem[key++] = &vma_WRAM;
		tmpmem[key++] = &vma_APURAM;
		tmpmem[key++] = &vma_VRAM;
		tmpmem[key++] = &vma_OAM;
		tmpmem[key++] = &vma_CGRAM;
		tmpmem[key++] = fixup_vma(vma_SRAM, SNES::cartridge.ram.data(), SNES::cartridge.ram.size());
		tmpmem[key++] = fixup_vma(vma_ROM, SNES::cartridge.rom.data(), SNES::cartridge.rom.size());
		tmpmem[key++] = &vma_BUS;
		tmpmem[key++] = fixup_vma(vma_GBROM, &gb_romdata[0], gb_romdata.size());
		tmpmem[key++] = fixup_vma(vma_GBSRAM, gb_instance->getSaveRam());
		tmpmem[key++] = fixup_vma(vma_GBWRAM, gb_instance->getWorkRam());
		tmpmem[key++] = fixup_vma(vma_GBHRAM, gb_instance->getIoRam());
		tmpmem[key++] = fixup_vma(vma_GBVRAM, gb_instance->getVideoRam());
		tmpmem[key++] = &vma_GBBUS;
	}
	tmpmem[key] = NULL;
	arg.vmas = tmpmem;
	return RET_OK;
}
}

int lsnes_core_entrypoint(unsigned action, unsigned item, void* params, const char** error)
{
	try {
		const char* ret;
		switch(action) {
			CASE_CALL_NOITEM(LSNES_CORE_ENUMERATE_CORES);
			CASE_CALL_CORE(LSNES_CORE_GET_CORE_INFO);
			CASE_CALL_TYPE(LSNES_CORE_GET_TYPE_INFO);
			CASE_CALL_REGION(LSNES_CORE_GET_REGION_INFO);
			CASE_CALL_SYSREGION(LSNES_CORE_GET_SYSREGION_INFO);
			CASE_CALL_CORE(LSNES_CORE_GET_AV_STATE);
			CASE_CALL_CORE(LSNES_CORE_EMULATE);
			CASE_CALL_CORE(LSNES_CORE_SAVESTATE);
			CASE_CALL_CORE(LSNES_CORE_LOADSTATE);
			CASE_CALL_TYPE(LSNES_CORE_GET_CONTROLLERCONFIG);
			CASE_CALL_TYPE(LSNES_CORE_LOAD_ROM);
			CASE_CALL_CORE(LSNES_CORE_GET_REGION);
			CASE_CALL_CORE(LSNES_CORE_SET_REGION);
			CASE_CALL_NOITEM(LSNES_CORE_DEINITIALIZE);
			CASE_CALL_CORE(LSNES_CORE_GET_PFLAG);
			CASE_CALL_CORE(LSNES_CORE_SET_PFLAG);
			CASE_CALL_CORE(LSNES_CORE_GET_ACTION_FLAGS);
			CASE_CALL_CORE(LSNES_CORE_EXECUTE_ACTION);
			CASE_CALL_CORE(LSNES_CORE_GET_BUS_MAPPING);
			CASE_CALL_CORE(LSNES_CORE_ENUMERATE_SRAM);
			CASE_CALL_CORE(LSNES_CORE_SAVE_SRAM);
			CASE_CALL_CORE(LSNES_CORE_LOAD_SRAM);
			CASE_CALL_CORE(LSNES_CORE_GET_RESET_ACTION);
			CASE_CALL_CORE(LSNES_CORE_COMPUTE_SCALE);
			CASE_CALL_CORE(LSNES_CORE_RUNTOSAVE);
			CASE_CALL_CORE(LSNES_CORE_POWERON);
			CASE_CALL_CORE(LSNES_CORE_UNLOAD_CARTRIDGE);
			CASE_CALL_CORE(LSNES_CORE_DEBUG_RESET);
			CASE_CALL_CORE(LSNES_CORE_SET_DEBUG_FLAGS);
			CASE_CALL_CORE(LSNES_CORE_SET_CHEAT);
			CASE_CALL_CORE(LSNES_CORE_DRAW_COVER);
			CASE_CALL_CORE(LSNES_CORE_PRE_EMULATE);
			CASE_CALL_CORE(LSNES_CORE_GET_DEVICE_REGS);
			CASE_CALL_CORE(LSNES_CORE_GET_VMA_LIST);
		default:
			ret = tmp_sprintf(TMP_ERROR, "Unsupported request %u", action);
			break;
		}
		*error = ret;
		return *error ? -1 : 0;
	} catch(std::bad_alloc& e) {
		//Don't throw exceptions across module boundary.
		*error = "Out of memory";
		return -1;
	} catch(std::exception& e) {
		//Don't throw exceptions across module boundary.
		try {
			//Really, catch the exception if this fails due to OOM.
			*error = tmpalloc_str(TMP_ERROR, std::string("Exception: ") + e.what());
			return -1;
		} catch(std::bad_alloc& f) {
			*error = "Out of memory (while reporting exception)";
			return -1;
		}
	}
}
