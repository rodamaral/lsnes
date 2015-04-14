#include "core/nullcore.hpp"

#include "interface/callbacks.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "library/framebuffer-pixfmt-rgb16.hpp"

namespace
{
	uint16_t null_cover_fbmem[512 * 448];

	//Framebuffer.
	struct framebuffer::info null_fbinfo = {
		&framebuffer::pixfmt_bgr16,		//Format.
		(char*)null_cover_fbmem,	//Memory.
		512, 448, 1024,			//Physical size.
		512, 448, 1024,			//Logical size.
		0, 0				//Offset.
	};

	struct interface_device_reg null_registers[] = {
		{NULL, NULL, NULL}
	};

	struct _core_null : public core_core, public core_type, public core_region, public core_sysregion
	{
		_core_null() : core_core({}, {}), core_type({{
			.iname = "null",
			.hname = "(null)",
			.id = 9999,
			.sysname = "System",
			.bios = NULL,
			.regions = {this},
			.images = {},
			.settings = {},
			.core = this,
		}}), core_region({{"null", "(null)", 0, 0, false, {1, 60}, {0}}}),
		core_sysregion("null", *this, *this) { hide(); }
		std::string c_core_identifier() const { return "null core"; }
		bool c_set_region(core_region& reg) { return true; }
		std::pair<unsigned, unsigned> c_video_rate() { return std::make_pair(60, 1); }
		double c_get_PAR() { return 1.0; }
		std::pair<unsigned, unsigned> c_audio_rate() { return std::make_pair(48000, 1); }
		std::map<std::string, std::vector<char>> c_save_sram() throw (std::bad_alloc) {
			std::map<std::string, std::vector<char>> x;
			return x;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw (std::bad_alloc) {}
		void c_serialize(std::vector<char>& out) { out.clear(); }
		void c_unserialize(const char* in, size_t insize) {}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height) {
			return std::make_pair(1, 1);
		}
		void c_install_handler() {}
		void c_uninstall_handler() {}
		void c_emulate() {}
		void c_runtosave() {}
		bool c_get_pflag() { return false; }
		void c_set_pflag(bool pflag) {}
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(null_fbinfo);
			for(size_t i = 0; i < sizeof(null_cover_fbmem)/sizeof(null_cover_fbmem[0]); i++)
				null_cover_fbmem[i] = 0x0000;
			std::string message = "NO ROM LOADED";
			cover_render_string(null_cover_fbmem, 204, 220, message, 0xFFFF, 0x0000, 512, 448, 1024, 2);
			return x;
		}
		std::string c_get_core_shortname() const { return "null"; }
		void c_pre_emulate_frame(portctrl::frame& cf) {}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p) {}
		const interface_device_reg* c_get_registers() { return null_registers; }
		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			controller_set x;
			x.ports.push_back(&portctrl::get_default_system_port_type());
			return x;
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map() { return std::make_pair(0ULL, 0ULL); }
		std::list<core_vma_info> c_vma_list() { return std::list<core_vma_info>(); }
		std::set<std::string> c_srams() { return std::set<std::string>(); }
		unsigned c_action_flags(unsigned id) { return 0; }
		int c_reset_action(bool hard) { return -1; }
		bool c_isnull() const { return true; }
		void c_set_debug_flags(uint64_t addr, unsigned int sflags, unsigned int cflags) {}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set) {}
		void c_debug_reset() {}
		std::vector<std::string> c_get_trace_cpus()
		{
			return std::vector<std::string>();
		}
	} core_null;
}

core_core& get_null_core() { return core_null; }
core_type& get_null_type() { return core_null; }
core_region& get_null_region() { return core_null; }
