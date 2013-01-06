#ifndef _emucore__hpp__included__
#define _emucore__hpp__included__

#include <map>
#include <list>
#include <cstdlib>
#include <string>
#include <set>
#include <vector>
#include "library/framebuffer.hpp"
#include "library/controller-data.hpp"
#include "interface/romtype.hpp"


//Install handler.
void core_install_handler();
//Uninstall handler.
void core_uninstall_handler();
//Emulate a frame.
void core_emulate_frame();
//Run to point save.
void core_runtosave();
//Get set of SRAMs.
std::set<std::string> get_sram_set();
//Get poll flag (set to 1 on each real poll, except if 2.
unsigned core_get_poll_flag();
//Set poll flag (set to 1 on each real poll, except if 2.
void core_set_poll_flag(unsigned pflag);
//Request reset on next frame.
void core_request_reset(long delay);
//Valid port types.
extern port_type* core_port_types[];
//Emulator core.
extern core_core* emulator_core;

//Callbacks.
struct emucore_callbacks
{
public:
	virtual ~emucore_callbacks() throw();
	//Get the input for specified control.
	virtual int16_t get_input(unsigned port, unsigned index, unsigned control) = 0;
	//Set the input for specified control (used for system controls, only works in readwrite mode).
	//Returns the actual value of control (may differ if in readonly mode).
	virtual int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value) = 0;
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
	virtual void output_frame(framebuffer_raw& screen, uint32_t fps_n, uint32_t fps_d) = 0;
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
