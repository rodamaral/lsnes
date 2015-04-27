#ifndef _interface__c_interface_translate__hpp__included__
#define _interface__c_interface_translate__hpp__included__

/*

Copyright (c) 2014 Ilari Liusvaara

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

#include "c-interface.h"

namespace lsnes_interface
{
template<int num> class e2t {};
template<> struct e2t<LSNES_CORE_ENUMERATE_CORES> {
	typedef lsnes_core_enumerate_cores* t;
	typedef lsnes_core_enumerate_cores& r;
};
template<> struct e2t<LSNES_CORE_GET_CORE_INFO> {
	typedef lsnes_core_get_core_info* t;
	typedef lsnes_core_get_core_info& r;
};
template<> struct e2t<LSNES_CORE_GET_TYPE_INFO> {
	typedef lsnes_core_get_type_info* t;
	typedef lsnes_core_get_type_info& r;
};
template<> struct e2t<LSNES_CORE_GET_REGION_INFO> {
	typedef lsnes_core_get_region_info* t;
	typedef lsnes_core_get_region_info& r;
};
template<> struct e2t<LSNES_CORE_GET_SYSREGION_INFO> {
	typedef lsnes_core_get_sysregion_info* t;
	typedef lsnes_core_get_sysregion_info& r;
};
template<> struct e2t<LSNES_CORE_GET_AV_STATE> {
	typedef lsnes_core_get_av_state* t;
	typedef lsnes_core_get_av_state& r;
};
template<> struct e2t<LSNES_CORE_EMULATE> {
	typedef lsnes_core_emulate* t;
	typedef lsnes_core_emulate& r;
};
template<> struct e2t<LSNES_CORE_SAVESTATE> {
	typedef lsnes_core_savestate* t;
	typedef lsnes_core_savestate& r;
};
template<> struct e2t<LSNES_CORE_LOADSTATE> {
	typedef lsnes_core_loadstate* t;
	typedef lsnes_core_loadstate& r;
};
template<> struct e2t<LSNES_CORE_GET_CONTROLLERCONFIG> {
	typedef lsnes_core_get_controllerconfig* t;
	typedef lsnes_core_get_controllerconfig& r;
};
template<> struct e2t<LSNES_CORE_LOAD_ROM> {
	typedef lsnes_core_load_rom* t;
	typedef lsnes_core_load_rom& r;
};
template<> struct e2t<LSNES_CORE_GET_REGION> {
	typedef lsnes_core_get_region* t;
	typedef lsnes_core_get_region& r;
};
template<> struct e2t<LSNES_CORE_SET_REGION> {
	typedef lsnes_core_set_region* t;
	typedef lsnes_core_set_region& r;
};
template<> struct e2t<LSNES_CORE_DEINITIALIZE> {
	typedef lsnes_core_deinitialize* t;
	typedef lsnes_core_deinitialize& r;
};
template<> struct e2t<LSNES_CORE_GET_PFLAG> {
	typedef lsnes_core_get_pflag* t;
	typedef lsnes_core_get_pflag& r;
};
template<> struct e2t<LSNES_CORE_SET_PFLAG> {
	typedef lsnes_core_set_pflag* t;
	typedef lsnes_core_set_pflag& r;
};
template<> struct e2t<LSNES_CORE_GET_ACTION_FLAGS> {
	typedef lsnes_core_get_action_flags* t;
	typedef lsnes_core_get_action_flags& r;
};
template<> struct e2t<LSNES_CORE_EXECUTE_ACTION> {
	typedef lsnes_core_execute_action* t;
	typedef lsnes_core_execute_action& r;
};
template<> struct e2t<LSNES_CORE_GET_BUS_MAPPING> {
	typedef lsnes_core_get_bus_mapping* t;
	typedef lsnes_core_get_bus_mapping& r;
};
template<> struct e2t<LSNES_CORE_ENUMERATE_SRAM> {
	typedef lsnes_core_enumerate_sram* t;
	typedef lsnes_core_enumerate_sram& r;
};
template<> struct e2t<LSNES_CORE_SAVE_SRAM> {
	typedef lsnes_core_save_sram* t;
	typedef lsnes_core_save_sram& r;
};
template<> struct e2t<LSNES_CORE_LOAD_SRAM> {
	typedef lsnes_core_load_sram* t;
	typedef lsnes_core_load_sram& r;
};
template<> struct e2t<LSNES_CORE_GET_RESET_ACTION> {
	typedef lsnes_core_get_reset_action* t;
	typedef lsnes_core_get_reset_action& r;
};
template<> struct e2t<LSNES_CORE_COMPUTE_SCALE> {
	typedef lsnes_core_compute_scale* t;
	typedef lsnes_core_compute_scale& r;
};
template<> struct e2t<LSNES_CORE_RUNTOSAVE> {
	typedef lsnes_core_runtosave* t;
	typedef lsnes_core_runtosave& r;
};
template<> struct e2t<LSNES_CORE_POWERON> {
	typedef lsnes_core_poweron* t;
	typedef lsnes_core_poweron& r;
};
template<> struct e2t<LSNES_CORE_UNLOAD_CARTRIDGE> {
	typedef lsnes_core_unload_cartridge* t;
	typedef lsnes_core_unload_cartridge& r;
};
template<> struct e2t<LSNES_CORE_DEBUG_RESET> {
	typedef lsnes_core_debug_reset* t;
	typedef lsnes_core_debug_reset& r;
};
template<> struct e2t<LSNES_CORE_SET_DEBUG_FLAGS> {
	typedef lsnes_core_set_debug_flags* t;
	typedef lsnes_core_set_debug_flags& r;
};
template<> struct e2t<LSNES_CORE_SET_CHEAT> {
	typedef lsnes_core_set_cheat* t;
	typedef lsnes_core_set_cheat& r;
};
template<> struct e2t<LSNES_CORE_DRAW_COVER> {
	typedef lsnes_core_draw_cover* t;
	typedef lsnes_core_draw_cover& r;
};
template<> struct e2t<LSNES_CORE_PRE_EMULATE> {
	typedef lsnes_core_pre_emulate* t;
	typedef lsnes_core_pre_emulate& r;
};
template<> struct e2t<LSNES_CORE_GET_DEVICE_REGS> {
	typedef lsnes_core_get_device_regs* t;
	typedef lsnes_core_get_device_regs& r;
};
template<> struct e2t<LSNES_CORE_GET_VMA_LIST> {
	typedef lsnes_core_get_vma_list* t;
	typedef lsnes_core_get_vma_list& r;
};
template<> struct e2t<LSNES_CORE_REINIT> {
	typedef lsnes_core_reinit* t;
	typedef lsnes_core_reinit& r;
};

template<typename T> class t2e {};
template<> struct t2e<lsnes_core_enumerate_cores> { const static int e = LSNES_CORE_ENUMERATE_CORES; };
template<> struct t2e<lsnes_core_get_core_info> { const static int e = LSNES_CORE_GET_CORE_INFO; };
template<> struct t2e<lsnes_core_get_type_info> { const static int e = LSNES_CORE_GET_TYPE_INFO; };
template<> struct t2e<lsnes_core_get_region_info> { const static int e = LSNES_CORE_GET_REGION_INFO; };
template<> struct t2e<lsnes_core_get_sysregion_info> { const static int e = LSNES_CORE_GET_SYSREGION_INFO; };
template<> struct t2e<lsnes_core_get_av_state> { const static int e = LSNES_CORE_GET_AV_STATE; };
template<> struct t2e<lsnes_core_emulate> { const static int e = LSNES_CORE_EMULATE; };
template<> struct t2e<lsnes_core_savestate> { const static int e = LSNES_CORE_SAVESTATE; };
template<> struct t2e<lsnes_core_loadstate> { const static int e = LSNES_CORE_LOADSTATE; };
template<> struct t2e<lsnes_core_get_controllerconfig> { const static int e = LSNES_CORE_GET_CONTROLLERCONFIG; };
template<> struct t2e<lsnes_core_load_rom> { const static int e = LSNES_CORE_LOAD_ROM; };
template<> struct t2e<lsnes_core_get_region> { const static int e = LSNES_CORE_GET_REGION; };
template<> struct t2e<lsnes_core_set_region> { const static int e = LSNES_CORE_SET_REGION; };
template<> struct t2e<lsnes_core_deinitialize> { const static int e = LSNES_CORE_DEINITIALIZE; };
template<> struct t2e<lsnes_core_get_pflag> { const static int e = LSNES_CORE_GET_PFLAG; };
template<> struct t2e<lsnes_core_set_pflag> { const static int e = LSNES_CORE_SET_PFLAG; };
template<> struct t2e<lsnes_core_get_action_flags> { const static int e = LSNES_CORE_GET_ACTION_FLAGS; };
template<> struct t2e<lsnes_core_execute_action> { const static int e = LSNES_CORE_EXECUTE_ACTION; };
template<> struct t2e<lsnes_core_get_bus_mapping> { const static int e = LSNES_CORE_GET_BUS_MAPPING; };
template<> struct t2e<lsnes_core_enumerate_sram> { const static int e = LSNES_CORE_ENUMERATE_SRAM; };
template<> struct t2e<lsnes_core_save_sram> { const static int e = LSNES_CORE_SAVE_SRAM; };
template<> struct t2e<lsnes_core_load_sram> { const static int e = LSNES_CORE_LOAD_SRAM; };
template<> struct t2e<lsnes_core_get_reset_action> { const static int e = LSNES_CORE_GET_RESET_ACTION; };
template<> struct t2e<lsnes_core_compute_scale> { const static int e = LSNES_CORE_COMPUTE_SCALE; };
template<> struct t2e<lsnes_core_runtosave> { const static int e = LSNES_CORE_RUNTOSAVE; };
template<> struct t2e<lsnes_core_poweron> { const static int e = LSNES_CORE_POWERON; };
template<> struct t2e<lsnes_core_unload_cartridge> { const static int e = LSNES_CORE_UNLOAD_CARTRIDGE; };
template<> struct t2e<lsnes_core_debug_reset> { const static int e = LSNES_CORE_DEBUG_RESET; };
template<> struct t2e<lsnes_core_set_debug_flags> { const static int e = LSNES_CORE_SET_DEBUG_FLAGS; };
template<> struct t2e<lsnes_core_set_cheat> { const static int e = LSNES_CORE_SET_CHEAT; };
template<> struct t2e<lsnes_core_draw_cover> { const static int e = LSNES_CORE_DRAW_COVER; };
template<> struct t2e<lsnes_core_pre_emulate> { const static int e = LSNES_CORE_PRE_EMULATE; };
template<> struct t2e<lsnes_core_get_device_regs> { const static int e = LSNES_CORE_GET_DEVICE_REGS; };
template<> struct t2e<lsnes_core_get_vma_list> { const static int e = LSNES_CORE_GET_VMA_LIST; };
template<> struct t2e<lsnes_core_reinit> { const static int e = LSNES_CORE_REINIT; };
}

#endif
