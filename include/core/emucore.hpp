#ifndef _emucore__hpp__included__
#define _emucore__hpp__included__

#include <map>
#include <list>
#include <cstdlib>
#include <string>
#include <set>
#include <vector>
#include "core/render.hpp"

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
#define MAX_LOGICAL_BUTTONS 16

#define EC_REGION_AUTO -1
#define EC_REGION_NTSC 0
#define EC_REGION_PAL 1

//Get the CPU rate.
uint32_t get_snes_cpu_rate();
//Get the APU rate.
uint32_t get_snes_apu_rate();
//Get the core identifier.
std::string get_core_identifier();
//Do basic core initialization (to get it to stable state).
void do_basic_core_init();
//Get set of SRAMs.
std::set<std::string> get_sram_set();
//Get region.
bool core_get_region();
//Get the current video rate.
std::pair<uint32_t, uint32_t> get_video_rate();
//Get the current audio rate.
std::pair<uint32_t, uint32_t> get_audio_rate();
//Set preload settings.
void set_preload_settings();
//Set controller type to none.
void set_core_controller_none(unsigned port) throw();
//Set controller type to gamepad.
void set_core_controller_gamepad(unsigned port) throw();
//Set controller type to mouse.
void set_core_controller_mouse(unsigned port) throw();
//Set controller type to multitap.
void set_core_controller_multitap(unsigned port) throw();
//Set controller type to superscope.
void set_core_controller_superscope(unsigned port) throw();
//Set controller type to justifier.
void set_core_controller_justifier(unsigned port) throw();
//Set controller type to justifiers.
void set_core_controller_justifiers(unsigned port) throw();
//Button ID function for none.
int get_button_id_none(unsigned controller, unsigned lbid) throw();
//Button ID function for gamepad.
int get_button_id_gamepad(unsigned controller, unsigned lbid) throw();
//Button ID function for mouse.
int get_button_id_mouse(unsigned controller, unsigned lbid) throw();
//Button ID function for multitap.
int get_button_id_multitap(unsigned controller, unsigned lbid) throw();
//Button ID function for superscope.
int get_button_id_superscope(unsigned controller, unsigned lbid) throw();
//Button ID function for justifier.
int get_button_id_justifier(unsigned controller, unsigned lbid) throw();
//Button ID function for justifiers.
int get_button_id_justifiers(unsigned controller, unsigned lbid) throw();
//Load SNES cartridge.
bool core_load_cartridge_normal(const char* rom_markup, const unsigned char* rom, size_t romsize);
//Load BS-X cartridge.
bool core_load_cartridge_bsx(const char* bios_markup, const unsigned char* bios, size_t biossize,
	const char* rom_markup, const unsigned char* rom, size_t romsize);
//Load slotted BS-X cartridge.
bool core_load_cartridge_bsx_slotted(const char* bios_markup, const unsigned char* bios, size_t biossize,
	const char* rom_markup, const unsigned char* rom, size_t romsize);
//Load Super Game Boy cartridge.
bool core_load_cartridge_super_game_boy(const char* bios_markup, const unsigned char* bios, size_t biossize,
	const char* rom_markup, const unsigned char* rom, size_t romsize);
//Load Sufami turbo cartridge.
bool core_load_cartridge_sufami_turbo(const char* bios_markup, const unsigned char* bios, size_t biossize,
	const char* romA_markup, const unsigned char* romA, size_t romAsize, const char* romB_markup,
	const unsigned char* romB, size_t romBsize);
//Set the region.
void core_set_region(int region);
//Power the core.
void core_power();
//Save SRAM set.
std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc);
//Load SRAM set.
void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
//Serialize state.
void core_serialize(std::vector<char>& out);
//Unserialize state.
void core_unserialize(const char* in, size_t insize);
//Install handler.
void core_install_handler();
//Uninstall handler.
void core_uninstall_handler();
//Emulate a frame.
void core_emulate_frame();
//Run specified number of frames inside frame.
std::pair<bool, uint32_t> core_emulate_cycles(uint32_t cycles);
//Do soft reset.
void core_reset();
//Run to point save.
void core_runtosave();

/**
 * Get name of logical button.
 *
 * Parameter lbid: ID of logical button.
 * Returns: The name of button.
 * Throws std::bad_alloc: Not enough memory.
 */
std::string get_logical_button_name(unsigned lbid) throw(std::bad_alloc);

//Callbacks.
struct emucore_callbacks
{
public:
	virtual ~emucore_callbacks() throw();
	//Get the input for specified control.
	virtual int16_t get_input(unsigned port, unsigned index, unsigned control) = 0;
	//Do timer tick.
	virtual void timer_tick(uint32_t increment, uint32_t per_second) = 0;
	//Get the firmware path.
	virtual std::string get_firmware_path() = 0;
	//Get the base path.
	virtual std::string get_base_path() = 0;
	//Get the current time.
	virtual time_t get_time() = 0;
	//Get random seed.
	virtual time_t get_randomseed() = 0;
	//Output frame.
	virtual void output_frame(lcscreen& screen, uint32_t fps_n, uint32_t fps_d) = 0;
};

extern struct emucore_callbacks* ecore_callbacks;

//A VMA.
struct vma_info
{
	std::string name;
	uint64_t base;
	uint64_t size;
	void* backing_ram;
	bool readonly;
	bool native_endian;
	uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write);
};

std::list<vma_info> get_vma_list();

#endif
