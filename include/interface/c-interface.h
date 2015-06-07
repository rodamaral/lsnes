#ifndef _interface__c_interface__h__included__
#define _interface__c_interface__h__included__

/*

Copyright (c) 2013 Ilari Liusvaara

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

/**
 * Some general information about the interface:
 * - The parameter type for LSNES_CORE_FOO is struct lsnes_core_foo*
 * - There are also some other enumerations/structures.
 * - The only exposed entry point (lsnes_core_entrypoint) has fp type of lsnes_core_func_t.
 * - By nature, this interface does not reference any symbols (exception is lsnes_register_builtin_core).
 * - Some fields may be conditional on API version.
 * - The following functions are required:
 *   * LSNES_CORE_ENUMERATE_CORES
 *   * LSNES_CORE_GET_CORE_INFO
 *   * LSNES_CORE_GET_TYPE_INFO
 *   * LSNES_CORE_GET_REGION_INFO
 *   * LSNES_CORE_GET_SYSREGION_INFO
 *   * LSNES_CORE_GET_AV_STATE
 *   * LSNES_CORE_EMULATE
 *   * LSNES_CORE_SAVESTATE
 *   * LSNES_CORE_LOADSTATE
 *   * LSNES_CORE_GET_CONTROLLERCONFIG
 *   * LSNES_CORE_LOAD_ROM
 * - Scratch buffers from emulator side last for duration of the call.
 * - Scratch buffers form core side last until next call.
 * - Never free buffer from emulator in core or vice versa.
 * - The spaces for Core, Type, Region and Sysregion IDs are distinct.
 * - If you only have one region, use ID of 0 for that and GET_REGION/SET_REGION are not needed.
 */

#include <unistd.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//API version.
#define LSNES_CORE_API 0

/** Constants. **/
//VMA is read-only.
#define LSNES_CORE_VMA_READONLY 1
//VMA has special semantics (no read/write consistency).
#define LSNES_CORE_VMA_SPECIAL 2
//VMA is volatile (and uninitialized on poweron).
#define LSNES_CORE_VMA_VOLATILE 4

/** Capabilities. **/
//Core supports multiple regions (By supporting LSNES_CORE_GET_REGION/LSNES_CORE_SET_REGION).
#define LSNES_CORE_CAP1_MULTIREGION 	0x00000001U
//Core supports poll flags (By supporting LSNES_CORE_GET_PFLAG/LSNES_CORE_SET_PFLAG).
#define LSNES_CORE_CAP1_PFLAG		0x00000002U
//Core supports actions (By supporting LSNES_CORE_GET_ACTION_FLAGS/LSNES_CORE_EXECUTE_ACTION).
#define LSNES_CORE_CAP1_ACTION		0x00000004U
//Core supports bus map (By supporting LSNES_CORE_GET_BUS_MAPPING).
#define LSNES_CORE_CAP1_BUSMAP		0x00000008U
//Core supports SRAM (By supporting LSNES_CORE_ENUMERATE_SRAM/LSNES_CORE_LOAD_SRAM/LSNES_CORE_SAVE_SRAM).
#define LSNES_CORE_CAP1_SRAM		0x00000010U
//Core supports resets (By supporting LSNES_CORE_GET_RESET_ACTION). LSNES_CORE_CAP1_ACTION is required.
#define LSNES_CORE_CAP1_RESET		0x00000020U
//Core supports custom scale computation (By supporting LSNES_CORE_COMPUTE_SCALE).
#define LSNES_CORE_CAP1_SCALE		0x00000040U
//Core supports explicit save points (By supporting LSNES_CORE_RUNTOSAVE).
#define LSNES_CORE_CAP1_RUNTOSAVE	0x00000080U
//Core supports powerons (By supporting LSNES_CORE_POWERON).
#define LSNES_CORE_CAP1_POWERON		0x00000100U
//Core supports cartridge unload (By supporting LSNES_CORE_UNLOAD_CARTRIDGE).
#define LSNES_CORE_CAP1_UNLOAD		0x00000200U
//Core supports debugging (By supporting LSNES_CORE_DEBUG_RESET and LSNES_CORE_SET_DEBUG_FLAGS[breakpoints]).
//LSNES_CORE_CAP1_MEMWATCH is required.
#define LSNES_CORE_CAP1_DEBUG		0x00000400U
//Core supports tracing (By supporting LSNES_CORE_DEBUG_RESET and LSNES_CORE_SET_DEBUG_FLAGS[tracing]).
//LSNES_CORE_CAP1_MEMWATCH is required.
#define LSNES_CORE_CAP1_TRACE		0x00000800U
//Core supports cheating (By supporting LSNES_CORE_DEBUG_RESET and LSNES_CORE_SET_CHEAT).
//LSNES_CORE_CAP1_MEMWATCH is required.
#define LSNES_CORE_CAP1_CHEAT		0x00001000U
//Core supports cover pages (By supporting LSNES_CORE_DRAW_COVER).
#define LSNES_CORE_CAP1_COVER		0x00002000U
//Core supports pre-emulate (By supporting LSNES_CORE_PRE_EMULATE).
#define LSNES_CORE_CAP1_PREEMULATE	0x00004000U
//Core supports registers (By supporting LSNES_CORE_GET_DEVICE_REGS).
#define LSNES_CORE_CAP1_REGISTERS	0x00008000U
//Core supports memory watch (By supporting LSNES_CORE_GET_VMA_LIST).
#define LSNES_CORE_CAP1_MEMWATCH	0x00010000U
//Core supports lightguns (By setting lightgun_height/lightgun_width in LSNES_CORE_GET_AV_STATE).
#define LSNES_CORE_CAP1_LIGHTGUN	0x00020000U
//Core supports fast reinit (By supporting LSNES_CORE_REINIT).
#define LSNES_CORE_CAP1_REINIT		0x00040000U
//Reserved capabilities.
#define LSNES_CORE_CAP1_RESERVED19	0x00080000U
#define LSNES_CORE_CAP1_RESERVED20	0x00100000U
#define LSNES_CORE_CAP1_RESERVED21	0x00200000U
#define LSNES_CORE_CAP1_RESERVED22	0x00400000U
#define LSNES_CORE_CAP1_RESERVED23	0x00800000U
#define LSNES_CORE_CAP1_RESERVED24	0x01000000U
#define LSNES_CORE_CAP1_RESERVED25	0x02000000U
#define LSNES_CORE_CAP1_RESERVED26	0x04000000U
#define LSNES_CORE_CAP1_RESERVED27	0x08000000U
#define LSNES_CORE_CAP1_RESERVED28	0x10000000U
#define LSNES_CORE_CAP1_RESERVED29	0x20000000U
#define LSNES_CORE_CAP1_RESERVED30	0x40000000U
#define LSNES_CORE_CAP1_CAPS2		0x80000000U

#define LSNES_END_OF_LIST 0xFFFFFFFFU

//Do action <action> on item <item> with parameters <params>.
//If successful, return 0.
//If supported but unsuccessful, return -1 and fill <error> (will not be released, only needs to be stable to next
//call).
//In dynamic libs, this is looked up as lsnes_core_fn.
typedef int(*lsnes_core_func_t)(unsigned action, unsigned item, void* params, const char** error);

//Pixel format.
enum lsnes_core_pixel_format
{
	LSNES_CORE_PIXFMT_RGB15,
	LSNES_CORE_PIXFMT_BGR15,
	LSNES_CORE_PIXFMT_RGB16,
	LSNES_CORE_PIXFMT_BGR16,
	LSNES_CORE_PIXFMT_RGB24,
	LSNES_CORE_PIXFMT_BGR24,
	LSNES_CORE_PIXFMT_RGB32,
	LSNES_CORE_PIXFMT_LRGB
};

//Framebuffer.
struct lsnes_core_framebuffer_info
{
	//Pixel format of framebuffer.
	enum lsnes_core_pixel_format type;
	//The physical memory backing the framebuffer.
	const char* mem;
	//Physical width of framebuffer.
	size_t physwidth;
	//Physical height of framebuffer.
	size_t physheight;
	//Physical stride of framebuffer (in bytes).
	size_t physstride;
	//Visible width of framebuffer.
	size_t width;
	//Visible height of framebuffer.
	size_t height;
	//Visible stride of framebuffer (in bytes).
	size_t stride;
	//Visible X offset of framebuffer.
	size_t offset_x;
	//Visible Y offset of framebuffer.
	size_t offset_y;
};

struct lsnes_core_sram
{
	//Name.
	const char* name;
	//Content size.
	size_t size;
	//Data.
	const char* data;
};

struct lsnes_core_system_setting
{
	//Name.
	const char* name;
	//Value.
	const char* value;
};

struct lsnes_core_disassembler
{
	//Name.
	const char* name;
	//Routine.
	const char* (*fn)(uint64_t base, unsigned char(*fetch)(void* ctx), void* ctx);
};

//Font rendering request.
struct lsnes_core_fontrender_req
{
	//Input: Context to pass to callbacks.
	void* cb_ctx;
	//Input: Allocate bitmap memory.
	//cb_ctx => cb_ctx from above.
	//mem_size => Number of bytes needed to allocate.
	//return buffer at least mem_size bytes. Or return NULL on error.
	void* (*alloc)(void* cb_ctx, size_t mem_size);
	//Input: Text to render (UTF-8).
	const char* text;
	//Input: Length of text in bytes. If negative, text is null-terminated.
	ssize_t text_len;
	//Input: Bytes per pixel to request. Can be 1, 2, 3 or 4.
	unsigned bytes_pp;
	//Input: Foreground color (native endian).
	unsigned fg_color;
	//Input: Background color (native endian).
	unsigned bg_color;
	//Output: The allocated bitmap (this comes from ->alloc). Not released on failure.
	void* bitmap;
	//Output: Width of the bitmap.
	size_t width;
	//Output: Height of the bitmap.
	size_t height;
};

//Request 0: Initialize and enumerate cores.
//Item: 0.
//Default action: (required)
//Called as the first call. Do the needed initialization and return list of core IDs.
#define LSNES_CORE_ENUMERATE_CORES 0
struct lsnes_core_enumerate_cores
{
	//Output: List of sysregion ids. Terminated by 0xFFFFFFFF.
	unsigned* sysregions;
	//Input: Emulator capability flags 1.
	unsigned emu_flags1;
	//Input: Function to print message line.
	void (*message)(const char* msg, size_t length);
	//Input: Get input.
	short (*get_input)(unsigned port, unsigned index, unsigned control);
	//Input: Notify that state of action has updated.
	void (*notify_action_update)();
	//Input: Notify that time has ticked.
	void (*timer_tick)(uint32_t increment, uint32_t per_second);
	//Input: Get firmware path. The return value is temporary.
	const char* (*get_firmware_path)();
	//Input: Get cartridge base path. The return value is temporary.
	const char* (*get_base_path)();
	//Input: Get current time.
	time_t (*get_time)();
	//Input: Get random seed.
	time_t (*get_randomseed)();
	//Input: Notify that memory has been read.
	void (*memory_read)(uint64_t addr, uint64_t value);
	//Input: Notify that memory is about to be written.
	void (*memory_write)(uint64_t addr, uint64_t value);
	//Input: Notify that memory is about to be executed.
	void (*memory_execute)(uint64_t addr, uint64_t cpunum);
	//Input: Notify memory trace event. Str needs to be valid during call.
	void (*memory_trace)(uint64_t proc, const char* str, int insn);
	//Input: Submit sound.
	void (*submit_sound)(const int16_t* samples, size_t count, int stereo, double rate);
	//Input: Notify latch event. Parameters are NULL-terminated and need to be remain during call.
	void (*notify_latch)(const char** params);
	//Input: Output a frame.
	void (*submit_frame)(struct lsnes_core_framebuffer_info* fb, uint32_t fps_n, uint32_t fps_d);
	//Input: Add disassembler. Only available if emu_flags1>=1.
	void* (*add_disasm)(struct lsnes_core_disassembler* disasm);
	//Input: Remove disassembler. Only available if emu_flags1>=1.
	void (*remove_disasm)(void* handle);
	//Input: Render text into bitmap. Returns 0 on success, -1 on failure. Only available if emu_flags>=2.
	int (*render_text)(struct lsnes_core_fontrender_req* req);
};

//Request 1: Request information about core.
//Item id: Core ID.
//Default action: (required)
//Obtain information about core.
#define LSNES_CORE_GET_CORE_INFO 1
struct lsnes_core_get_core_info_aparam
{
	//Name of the parameter.
	const char* name;
	//Model:
	//bool: boolean
	//int:<val>,<val>: Integer in specified range.
	//string[:<regex>]: String with regex.
	//enum:<json-array-of-strings>: Enumeration.
	//toggle: Not a real parameter, boolean toggle, must be the first.
	const char* model;
};
struct lsnes_core_get_core_info_action
{
	//Internal id of the action.
	int id;
	//Internal name of the action.
	const char* iname;
	//Human-readable Name of the action
	const char* hname;
	//Array of parameters. Terminated by parameter with NULL name.
	struct lsnes_core_get_core_info_aparam* parameters;
};
struct lsnes_core_get_core_info
{
	//Output: The JSON text.
	const char* json;
	//Output: JSON pointer to root of the port array.
	const char* root_ptr;
	//Output: Short core name.
	const char* shortname;
	//Output: Full core name.
	const char* fullname;
	//Output: Capability flags #1 (LSNES_CORE_CAP1_*).
	unsigned cap_flags1;
	//Output: Array of actions. Terminated by action with NUL iname. Only if LSNES_CORE_CAP1_ACTION is set.
	struct lsnes_core_get_core_info_action* actions;
	//Output: List of trace CPUs. Terminated by NULL. Only if LSNES_CORE_CAP1_TRACE is set.
	const char** trace_cpu_list;
};

//Request 2: Request information about system type.
//Item id: Type ID.
//Default action: (required)
//Obtain information about system type.
#define LSNES_CORE_GET_TYPE_INFO 2
struct lsnes_core_get_type_info_paramval
{
	const char* iname;
	const char* hname;
	signed index;
};
struct lsnes_core_get_type_info_param
{
	//Internal name.
	const char* iname;
	//Human-readable name.
	const char* hname;
	//Default value.
	const char* dflt;
	//Enumeration of values (if enumerated, ended by value with NULL iname).
	//If values are false and true, this is boolean.
	struct lsnes_core_get_type_info_paramval* values;
	//Validation regex (only for non-enumerated).
	const char* regex;
};
struct lsnes_core_get_type_info_romimage
{
	//Internal name.
	const char* iname;
	//Human-readable name.
	const char* hname;
	//Mandatory mask (OR of all preset ones must equal OR of all).
	unsigned mandatory;
	//Pass mode: 0 => Content. 1 => Filename, 2 => Directory.
	int pass_mode;
	//Optional header size.
	unsigned headersize;
	//Standard extensions (split by ;).
	const char* extensions;
};
struct lsnes_core_get_type_info
{
	//Output: ID of core emulating this.
	unsigned core;
	//Output: internal name.
	const char* iname;
	//Output: human-readable name.
	const char* hname;
	//Output: Short System name (e.g. SNES).
	const char* sysname;
	//Output: BIOS name.
	const char* bios;
	//Output: List of regions. Terminated by 0xFFFFFFFF.
	unsigned* regions;
	//Output: List of images. Terminated by image with NULL iname.
	struct lsnes_core_get_type_info_romimage* images;
	//Output: List of settings. Terminated by setting with NULL iname.
	struct lsnes_core_get_type_info_param* settings;
};

//Request 3: Request information about region.
//Item id: Region ID.
//Default action: (required)
//Obtain information about region.
#define LSNES_CORE_GET_REGION_INFO 3
struct lsnes_core_get_region_info
{
	//Output: Internal name.
	const char* iname;
	//Output: Human-readable name.
	const char* hname;
	//Output: Priority
	unsigned priority;
	//Output: Multi-region flag.
	int multi;
	//Output: Fps numerator.
	uint32_t fps_n;
	//Output: Fps denomerator.
	uint32_t fps_d;
	//Output: Compatible run regions, ended with 0xFFFFFFFF.
	unsigned* compatible_runs;
};

//Request 4: Get sysregion info.
//Item id: Sysregion ID.
//Default action: (required)
//Get information about specific sysregion.
#define LSNES_CORE_GET_SYSREGION_INFO 4
struct lsnes_core_get_sysregion_info
{
	//Output: The name of sysregion.
	const char* name;
	//Output: The type ID.
	unsigned type;
	//Output: The region ID.
	unsigned region;
	//Output: The system name this is for.
	const char* for_system;
};


//Request 5: Get current A/V state.
//Item: Core ID.
//Default action: (required)
//Fill the structure with current A/V state.
#define LSNES_CORE_GET_AV_STATE 5
struct lsnes_core_get_av_state
{
	uint32_t fps_n;
	uint32_t fps_d;
	double par;
	uint32_t rate_n;
	uint32_t rate_d;
	unsigned lightgun_width;
	unsigned lightgun_height;
};


//Request 6: Emulate a frame
//Item: Core ID.
//Default action: (required).
//Emulate a frame and output the video and audio data resulting.
#define LSNES_CORE_EMULATE 6
struct lsnes_core_emulate
{
};

//Request 7: Save state.
//Item: Core ID.
//Default action: (required).
//Save core state.
#define LSNES_CORE_SAVESTATE 7
struct lsnes_core_savestate
{
	//Output: Size of the savestate.
	size_t size;
	//Output: Savestate data. Only needs to be stable to next call.
	const char* data;
};

//Request 8: Load state.
//Item: Core ID.
//Default action: (required).
//Load core state.
#define LSNES_CORE_LOADSTATE 8
struct lsnes_core_loadstate
{
	//Input: Size of the savestate.
	size_t size;
	//Input: Savestate data. Only stable during call.
	const char* data;
};

//Request 9: Get controller set.
//Item id: Type ID.
//Default action: (required).
//Get the controller set.
#define LSNES_CORE_GET_CONTROLLERCONFIG 9
struct lsnes_core_get_controllerconfig_logical_entry
{
	//port
	unsigned port;
	//controller
	unsigned controller;
};
struct lsnes_core_get_controllerconfig
{
	//Input: System settings. Ended by entry with NULL name.
	struct lsnes_core_system_setting* settings;
	//Output: Port types, indexed by 0-based ID to port type root JSON array. Ended by 0xFFFFFFFF.
	unsigned* controller_types;
	//Output: Logical map (ended by 0,0).
	struct lsnes_core_get_controllerconfig_logical_entry* logical_map;
};

//Request 10: Load ROM.
//Item id: Type ID.
//Default action: (required).
//Load given ROM.
#define LSNES_CORE_LOAD_ROM 10
struct lsnes_core_load_rom_image
{
	//ROM image (or filename thereof)
	const char* data;
	//Size of ROM image.
	size_t size;
	//Markup.
	const char* markup;
};
struct lsnes_core_load_rom
{
	//Input: The image set.
	struct lsnes_core_load_rom_image* images;
	//Input: System settings. Ended by entry with NULL name.
	struct lsnes_core_system_setting* settings;
	//Input: RTC second.
	uint64_t rtc_sec;
	//Input: RTC subsecond.
	uint64_t rtc_subsec;
};

//Request 11: Get region.
//Item: Core ID.
//Default action: Fill region 0.
//Return the current region.
#define LSNES_CORE_GET_REGION 11
struct lsnes_core_get_region
{
	//Output: The region.
	unsigned region;
};

//Request 12: Set region.
//Item: Core ID.
//Default action: If region is 0, succeed, otherwise fail.
//Set current region.
#define LSNES_CORE_SET_REGION 12
struct lsnes_core_set_region
{
	//Input: The region ID.
	unsigned region;
};

//Request 13: Deinitialize
//Item: 0.
//Default action: No-op
//Deinitialize the state.
#define LSNES_CORE_DEINITIALIZE 13
struct lsnes_core_deinitialize
{
};

//Request 14: Get poll flag state.
//Item: Core ID.
//Default action: Return flag inferred from polls.
//Return the poll flag state.
//The poll flag gets automatically set to 1 if core reads controllers.
#define LSNES_CORE_GET_PFLAG 14
struct lsnes_core_get_pflag
{
	//Output: The poll flag state.
	int pflag;
};

//Request 15: Set poll flag state.
//Item: Core ID.
//Default action: Set flag inferred from polls.
//Sets the poll flag state.
#define LSNES_CORE_SET_PFLAG 15
struct lsnes_core_set_pflag
{
	//Input: The poll flag state.
	int pflag;
};

//Request 16: Get action flags.
//Item: Core ID.
//Default action: Act as if flags was 1.
//Get flags for given action.
#define LSNES_CORE_GET_ACTION_FLAGS 16
struct lsnes_core_get_action_flags
{
	//Input: The ID of action.
	unsigned action;
	//Output: The flags.
	//Bit 0: Enabled.
	//Bit 1: Checked.
	unsigned flags;
};

//Request 17: Execute action.
//Item: Core ID.
//Default action: Do nothing.
//Execute given action.
#define LSNES_CORE_EXECUTE_ACTION 17
struct lsnes_core_execute_action_param
{
	union {
		int boolean;
		int64_t integer;
		struct {
			const char* base;
			size_t length;
		} string;
	};
};
struct lsnes_core_execute_action
{
	//Input: The ID of action.
	unsigned action;
	//Parameters block (length is set by action info).
	struct lsnes_core_execute_action_param* params;
};

//Request 18: Get bus mapping.
//Item: Core ID.
//Default action: base=0, size=0 (no mapping).
//Get the base and size of bus mapping.
#define LSNES_CORE_GET_BUS_MAPPING 18
struct lsnes_core_get_bus_mapping
{
	//Output: The base address of the mapping.
	uint64_t base;
	//Output: The size of the mapping.
	uint64_t size;
};

//Request 19: Enumerate SRAMs.
//Item iD: Core ID.
//Default action: Return empty set.
//Get the set of SRAMs available.
#define LSNES_CORE_ENUMERATE_SRAM 19
struct lsnes_core_enumerate_sram
{
	//Output: List of SRAMs, NULL terminated (valid until next call).
	const char** srams;
};

//Request 20: Save SRAMs.
//Item id: Core ID.
//Default action: Return empty set.
//Save the contents of SRAMs.
#define LSNES_CORE_SAVE_SRAM 20
struct lsnes_core_save_sram
{
	//Output: The SRAMs, terminated by entry with NULL name. Valid until next call.
	struct lsnes_core_sram* srams;
};

//Request 21: Load SRAMs.
//Item id: Core ID.
//Default action: Warn about any SRAMs present.
//Load the contents of SRAMs.
#define LSNES_CORE_LOAD_SRAM 21
struct lsnes_core_load_sram
{
	//Intput: The SRAMs, terminated by entry with NULL name. Valid during call.
	struct lsnes_core_sram* srams;
};

//Request 22: Get reset action number.
//Item id: Core ID.
//Default action: Return -1 for both (not supported).
//Return the IDs for reset actions.
#define LSNES_CORE_GET_RESET_ACTION 22
struct lsnes_core_get_reset_action
{
	//Output: Soft reset action (-1 if not supported).
	int softreset;
	//Output: Hard reset action (-1 if not supported).
	int hardreset;
};

//Request 23: Get scale factors.
//Item id: Core ID.
//Default action: Scale to at least 360 width, 320 height.
//Compute scale factors for given resolution.
#define LSNES_CORE_COMPUTE_SCALE 23
struct lsnes_core_compute_scale
{
	//Input: Width
	unsigned width;
	//Input: Height.
	unsigned height;
	//Output: Horizontal scale factor.
	uint32_t hfactor;
	//Output: Vertical scale factor.
	uint32_t vfactor;
};

//Request 24: Run to save.
//Item id: Core ID.
//Default action: Do nothing.
//Run to next save point (can be at most frame).
#define LSNES_CORE_RUNTOSAVE 24
struct lsnes_core_runtosave
{
};

//Request 25: Poweron the system
//Item id: Core ID.
//Default action: Do nothing.
//Powers on the emulate system.
#define LSNES_CORE_POWERON 25
struct lsnes_core_poweron
{
};

//Request 26: Unload the ROM.
//Item id: Core ID.
//Default action: Do nothing.
//Signals that the ROM is no longer needed and can be unloaded.
#define LSNES_CORE_UNLOAD_CARTRIDGE 26
struct lsnes_core_unload_cartridge
{
};

//Request 27: Reset debugging.
//Item id: Core ID.
//Default action: Do nothing.
//Signals that debugging system should discard all breakpoints, cheats and tracks.
#define LSNES_CORE_DEBUG_RESET 27
struct lsnes_core_debug_reset
{
};

//Request 28: Set debug flags.
//Item id: Core ID.
//Default action: Do nothing.
//Set debugging flags.
#define LSNES_CORE_SET_DEBUG_FLAGS 28
struct lsnes_core_set_debug_flags
{
	//Input: Address or CPU id.
	uint64_t addr;
	//Input: Flags to set.
	//1 => Break on read.
	//2 => Break on write.
	//4 => Break on execute.
	//8 => Trace.
	unsigned set;
	//Input: Flags to clear.
	unsigned clear;
};

//Request 29: Set cheat.
//Item id: Core ID.
//Default action: Do nothing.
//Set or clear cheat.
#define LSNES_CORE_SET_CHEAT 29
struct lsnes_core_set_cheat
{
	//Input: Address to set on.
	uint64_t addr;
	//Input: Value to set.
	uint64_t value;
	//Input: If nonzero, set cheat, else clear it.
	int set;
};

//Request 30: Draw cover page.
//Item id: Core ID.
//Default action: Draw black screen.
//Draw the cover page.
#define LSNES_CORE_DRAW_COVER 30
struct lsnes_core_draw_cover
{
	//Output: The cover page. Needs to stay valid to next call.
	struct lsnes_core_framebuffer_info* coverpage;
};

//Request 31: Set system controls before emulating frame.
//Item id: Core ID.
//Default action: Do nothing.
//Set the system controls before frame is emulated.
#define LSNES_CORE_PRE_EMULATE 31
struct lsnes_core_pre_emulate
{
	//Input: Context to pass to the function.
	void* context;
	//Input: Set input function.
	void (*set_input)(void* context, unsigned port, unsigned controller, unsigned index, short value);
};

//Request 32: Get list of device registers.
//Item id: Core ID.
//Default action: Return no registers.
//Return list of device registers.
#define LSNES_CORE_GET_DEVICE_REGS 32
struct lsnes_core_get_device_regs_reg
{
	//Name of the register.
	const char* name;
	//Read function.
	uint64_t (*read)();
	//Write function.
	void (*write)(uint64_t v);
	//Is boolean?
	int boolean;
};
struct lsnes_core_get_device_regs
{
	//Output: List of registers. Terminated with register with NULL name.
	struct lsnes_core_get_device_regs_reg* regs;
};

//Request 33: Get VMA list.
//Item id: Core ID.
//Default action: Return no VMAs.
//Get the list of VMAs.
#define LSNES_CORE_GET_VMA_LIST 33
struct lsnes_core_get_vma_list_vma
{
	//Name of region.
	const char* name;
	//Base address.
	uint64_t base;
	//Size of region.
	uint64_t size;
	//Endianess of region (-1 => little, 0 => host, 1 => big).
	int endian;
	//flags (LSNES_CORE_VMA_*).
	int flags;
	//Direct mapping for the region. If not NULL, read/write will not be used, instead all operations directly
	//manipulate this buffer (faster). Must be NULL for special regions.
	unsigned char* direct_map;
	//Function to read region (if direct_map is NULL).
	uint8_t (*read)(uint64_t offset);
	//Function to write region (if direct_map is NULL and readonly is 0).
	void (*write)(uint64_t offset, uint8_t value);
};
struct lsnes_core_get_vma_list
{
	//Output: List of VMAs. NULL-terminated.
	struct lsnes_core_get_vma_list_vma** vmas;
};

//Request 34: Reinit core to last loaded state.
//Item id: Core ID.
//Default action: Emulate using loadstate.
//Signals that the core state should be reset to state just after last load (load savestate at moment of initial
//poweron).
#define LSNES_CORE_REINIT 27
struct lsnes_core_reinit
{
};


#ifdef LSNES_BUILD_AS_BUILTIN_CORE
void lsnes_register_builtin_core(lsnes_core_func_t fn);
#else
int lsnes_core_entrypoint(unsigned action, unsigned item, void* params, const char** error);
#endif

#ifdef __cplusplus
}
#endif

#endif
