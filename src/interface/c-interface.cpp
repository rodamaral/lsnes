#include "interface/c-interface.hpp"
#include "interface/callbacks.hpp"
#include "interface/romtype.hpp"
#include "interface/setting.hpp"
#include "interface/disassembler.hpp"
#include "library/string.hpp"
#include "library/controller-parse.hpp"
#include "library/framebuffer.hpp"
#include "library/framebuffer-pixfmt-rgb15.hpp"
#include "library/framebuffer-pixfmt-rgb16.hpp"
#include "library/framebuffer-pixfmt-rgb24.hpp"
#include "library/framebuffer-pixfmt-rgb32.hpp"
#include "library/framebuffer-pixfmt-lrgb.hpp"
#include "fonts/wrapper.hpp"
#include "core/audioapi.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"

template<> int ccore_call_param_map<lsnes_core_enumerate_cores>::id = LSNES_CORE_ENUMERATE_CORES;
template<> int ccore_call_param_map<lsnes_core_get_core_info>::id = LSNES_CORE_GET_CORE_INFO;
template<> int ccore_call_param_map<lsnes_core_get_type_info>::id = LSNES_CORE_GET_TYPE_INFO;
template<> int ccore_call_param_map<lsnes_core_get_region_info>::id = LSNES_CORE_GET_REGION_INFO;
template<> int ccore_call_param_map<lsnes_core_get_sysregion_info>::id = LSNES_CORE_GET_SYSREGION_INFO;
template<> int ccore_call_param_map<lsnes_core_get_av_state>::id = LSNES_CORE_GET_AV_STATE;
template<> int ccore_call_param_map<lsnes_core_emulate>::id = LSNES_CORE_EMULATE;
template<> int ccore_call_param_map<lsnes_core_savestate>::id = LSNES_CORE_SAVESTATE;
template<> int ccore_call_param_map<lsnes_core_loadstate>::id = LSNES_CORE_LOADSTATE;
template<> int ccore_call_param_map<lsnes_core_get_controllerconfig>::id = LSNES_CORE_GET_CONTROLLERCONFIG;
template<> int ccore_call_param_map<lsnes_core_load_rom>::id = LSNES_CORE_LOAD_ROM;
template<> int ccore_call_param_map<lsnes_core_get_region>::id = LSNES_CORE_GET_REGION;
template<> int ccore_call_param_map<lsnes_core_set_region>::id = LSNES_CORE_SET_REGION;
template<> int ccore_call_param_map<lsnes_core_deinitialize>::id = LSNES_CORE_DEINITIALIZE;
template<> int ccore_call_param_map<lsnes_core_get_pflag>::id = LSNES_CORE_GET_PFLAG;
template<> int ccore_call_param_map<lsnes_core_set_pflag>::id = LSNES_CORE_SET_PFLAG;
template<> int ccore_call_param_map<lsnes_core_get_action_flags>::id = LSNES_CORE_GET_ACTION_FLAGS;
template<> int ccore_call_param_map<lsnes_core_execute_action>::id = LSNES_CORE_EXECUTE_ACTION;
template<> int ccore_call_param_map<lsnes_core_get_bus_mapping>::id = LSNES_CORE_GET_BUS_MAPPING;
template<> int ccore_call_param_map<lsnes_core_enumerate_sram>::id = LSNES_CORE_ENUMERATE_SRAM;
template<> int ccore_call_param_map<lsnes_core_save_sram>::id = LSNES_CORE_SAVE_SRAM;
template<> int ccore_call_param_map<lsnes_core_load_sram>::id = LSNES_CORE_LOAD_SRAM;
template<> int ccore_call_param_map<lsnes_core_get_reset_action>::id = LSNES_CORE_GET_RESET_ACTION;
template<> int ccore_call_param_map<lsnes_core_compute_scale>::id = LSNES_CORE_COMPUTE_SCALE;
template<> int ccore_call_param_map<lsnes_core_runtosave>::id = LSNES_CORE_RUNTOSAVE;
template<> int ccore_call_param_map<lsnes_core_poweron>::id = LSNES_CORE_POWERON;
template<> int ccore_call_param_map<lsnes_core_unload_cartridge>::id = LSNES_CORE_UNLOAD_CARTRIDGE;
template<> int ccore_call_param_map<lsnes_core_debug_reset>::id = LSNES_CORE_DEBUG_RESET;
template<> int ccore_call_param_map<lsnes_core_set_debug_flags>::id = LSNES_CORE_SET_DEBUG_FLAGS;
template<> int ccore_call_param_map<lsnes_core_set_cheat>::id = LSNES_CORE_SET_CHEAT;
template<> int ccore_call_param_map<lsnes_core_draw_cover>::id = LSNES_CORE_DRAW_COVER;
template<> int ccore_call_param_map<lsnes_core_pre_emulate>::id = LSNES_CORE_PRE_EMULATE;
template<> int ccore_call_param_map<lsnes_core_get_device_regs>::id = LSNES_CORE_GET_DEVICE_REGS;
template<> int ccore_call_param_map<lsnes_core_get_vma_list>::id = LSNES_CORE_GET_VMA_LIST;

template<> const char* ccore_call_param_map<lsnes_core_enumerate_cores>::name = "LSNES_CORE_ENUMERATE_CORES";
template<> const char* ccore_call_param_map<lsnes_core_get_core_info>::name = "LSNES_CORE_GET_CORE_INFO";
template<> const char* ccore_call_param_map<lsnes_core_get_type_info>::name = "LSNES_CORE_GET_TYPE_INFO";
template<> const char* ccore_call_param_map<lsnes_core_get_region_info>::name = "LSNES_CORE_GET_REGION_INFO";
template<> const char* ccore_call_param_map<lsnes_core_get_sysregion_info>::name = "LSNES_CORE_GET_SYSREGION_INFO";
template<> const char* ccore_call_param_map<lsnes_core_get_av_state>::name = "LSNES_CORE_GET_AV_STATE";
template<> const char* ccore_call_param_map<lsnes_core_emulate>::name = "LSNES_CORE_EMULATE";
template<> const char* ccore_call_param_map<lsnes_core_savestate>::name = "LSNES_CORE_SAVESTATE";
template<> const char* ccore_call_param_map<lsnes_core_loadstate>::name = "LSNES_CORE_LOADSTATE";
template<> const char* ccore_call_param_map<lsnes_core_get_controllerconfig>::name =
	"LSNES_CORE_GET_CONTROLLERCONFIG";
template<> const char* ccore_call_param_map<lsnes_core_load_rom>::name = "LSNES_CORE_LOAD_ROM";
template<> const char* ccore_call_param_map<lsnes_core_get_region>::name = "LSNES_CORE_GET_REGION";
template<> const char* ccore_call_param_map<lsnes_core_set_region>::name = "LSNES_CORE_SET_REGION";
template<> const char* ccore_call_param_map<lsnes_core_deinitialize>::name = "LSNES_CORE_DEINITIALIZE";
template<> const char* ccore_call_param_map<lsnes_core_get_pflag>::name = "LSNES_CORE_GET_PFLAG";
template<> const char* ccore_call_param_map<lsnes_core_set_pflag>::name = "LSNES_CORE_SET_PFLAG";
template<> const char* ccore_call_param_map<lsnes_core_get_action_flags>::name = "LSNES_CORE_GET_ACTION_FLAGS";
template<> const char* ccore_call_param_map<lsnes_core_execute_action>::name = "LSNES_CORE_EXECUTE_ACTION";
template<> const char* ccore_call_param_map<lsnes_core_get_bus_mapping>::name = "LSNES_CORE_GET_BUS_MAPPING";
template<> const char* ccore_call_param_map<lsnes_core_enumerate_sram>::name = "LSNES_CORE_ENUMERATE_SRAM";
template<> const char* ccore_call_param_map<lsnes_core_save_sram>::name = "LSNES_CORE_SAVE_SRAM";
template<> const char* ccore_call_param_map<lsnes_core_load_sram>::name = "LSNES_CORE_LOAD_SRAM";
template<> const char* ccore_call_param_map<lsnes_core_get_reset_action>::name = "LSNES_CORE_GET_RESET_ACTION";
template<> const char* ccore_call_param_map<lsnes_core_compute_scale>::name = "LSNES_CORE_COMPUTE_SCALE";
template<> const char* ccore_call_param_map<lsnes_core_runtosave>::name = "LSNES_CORE_RUNTOSAVE";
template<> const char* ccore_call_param_map<lsnes_core_poweron>::name = "LSNES_CORE_POWERON";
template<> const char* ccore_call_param_map<lsnes_core_unload_cartridge>::name = "LSNES_CORE_UNLOAD_CARTRIDGE";
template<> const char* ccore_call_param_map<lsnes_core_debug_reset>::name = "LSNES_CORE_DEBUG_RESET";
template<> const char* ccore_call_param_map<lsnes_core_set_debug_flags>::name = "LSNES_CORE_SET_DEBUG_FLAGS";
template<> const char* ccore_call_param_map<lsnes_core_set_cheat>::name = "LSNES_CORE_SET_CHEAT";
template<> const char* ccore_call_param_map<lsnes_core_draw_cover>::name = "LSNES_CORE_DRAW_COVER";
template<> const char* ccore_call_param_map<lsnes_core_pre_emulate>::name = "LSNES_CORE_PRE_EMULATE";
template<> const char* ccore_call_param_map<lsnes_core_get_device_regs>::name = "LSNES_CORE_GET_DEVICE_REGS";
template<> const char* ccore_call_param_map<lsnes_core_get_vma_list>::name = "LSNES_CORE_GET_VMA_LIST";

namespace
{
	struct c_core_core;
	c_core_core* current_core = NULL;

	char* strduplicate(const char* x)
	{
		if(!x) return NULL;
		char* out = (char*)malloc(strlen(x) + 1);
		if(!out)
			throw std::bad_alloc();
		strcpy(out, x);
		return out;
	}

	std::list<lsnes_core_func_t>& corequeue()
	{
		static std::list<lsnes_core_func_t> x;
		return x;
	}

	void default_error_function(const char* callname, const char* err)
	{
		messages << "Warning: " << callname << " failed: " << err << std::endl;
	}

	framebuffer::info translate_info(lsnes_core_framebuffer_info* _fb)
	{
		framebuffer::info fbinfo;
		switch(_fb->type) {
		case LSNES_CORE_PIXFMT_RGB15:	fbinfo.type = &framebuffer::pixfmt_rgb15; break;
		case LSNES_CORE_PIXFMT_BGR15:	fbinfo.type = &framebuffer::pixfmt_bgr15; break;
		case LSNES_CORE_PIXFMT_RGB16:	fbinfo.type = &framebuffer::pixfmt_rgb16; break;
		case LSNES_CORE_PIXFMT_BGR16:	fbinfo.type = &framebuffer::pixfmt_bgr16; break;
		case LSNES_CORE_PIXFMT_RGB24:	fbinfo.type = &framebuffer::pixfmt_rgb24; break;
		case LSNES_CORE_PIXFMT_BGR24:	fbinfo.type = &framebuffer::pixfmt_bgr24; break;
		case LSNES_CORE_PIXFMT_RGB32:	fbinfo.type = &framebuffer::pixfmt_rgb32; break;
		case LSNES_CORE_PIXFMT_LRGB:	fbinfo.type = &framebuffer::pixfmt_lrgb; break;
		};
		fbinfo.mem = (char*)_fb->mem;
		fbinfo.physwidth = _fb->physwidth;
		fbinfo.physheight = _fb->physheight;
		fbinfo.physstride = _fb->physstride;
		fbinfo.width = _fb->width;
		fbinfo.height = _fb->height;
		fbinfo.stride = _fb->stride;
		fbinfo.offset_x = _fb->offset_x;
		fbinfo.offset_y = _fb->offset_y;
		return fbinfo;
	}

	struct entrypoint_fn
	{
		entrypoint_fn(lsnes_core_func_t _fn) : fn(_fn) {}
		template<typename T> bool operator()(unsigned item, T& args,
			std::function<void(const char* callname, const char* err)> onerror)
		{
			const char* err = NULL;
			int r = fn(ccore_call_param_map<T>::id, item, &args, &err);
			if(r < 0)
				onerror(ccore_call_param_map<T>::name, err);
			return (r >= 0);
		}
		template<typename T> bool operator()(unsigned item, T& args)
		{
			return (*this)(item, args, default_error_function);
		}
	private:
		lsnes_core_func_t fn;
	};

	struct c_lib_init
	{
	public:
		c_lib_init(entrypoint_fn _entrypoint)
			: entrypoint(_entrypoint)
		{
			count = 0;
		}
		void initialize()
		{
			count++;
		}
		void deinitialize()
		{
			if(count) count--;
			if(!count) {
				lsnes_core_deinitialize s;
				entrypoint(0, s);
			}
		}
		entrypoint_fn get_entrypoint()
		{
			return entrypoint;
		}
	private:
		int count;
		entrypoint_fn entrypoint;
	};

	struct c_core_core_params
	{
		std::vector<interface_action> actions;
		std::vector<std::string> trace_cpus;
		std::vector<port_type*> ports;
		const char* shortname;
		const char* fullname;
		unsigned flags;
		unsigned id;
		std::map<unsigned, core_region*> regions;
		c_lib_init* library;
	};

	struct c_core_core : public core_core
	{
		c_core_core(c_core_core_params& p)
			: core_core(p.ports, p.actions), entrypoint(p.library->get_entrypoint()), plugin(p.library)
		{
			fullname = p.fullname;
			shortname = p.shortname;
			id = p.id;
			internal_pflag = false;
			caps1 = p.flags;
			regions = p.regions;
			actions = p.actions;
			trace_cpus = p.trace_cpus;
			for(size_t i = 0; i < p.ports.size(); i++)
				ports[i] = p.ports[i];
		}
		~c_core_core() throw();
		std::string c_core_identifier()
		{
			return fullname;
		}
		bool get_av_state(lsnes_core_get_av_state& s)
		{
			return entrypoint(id, s);
		}
		std::pair<uint32_t, uint32_t> c_video_rate()
		{
			lsnes_core_get_av_state s;
			if(!entrypoint(id, s))
				return std::make_pair(60, 1);
			return std::make_pair(s.fps_n, s.fps_d);
		}
		double c_get_PAR()
		{
			lsnes_core_get_av_state s;
			return entrypoint(id, s) ? s.par : 1.0;
		}
		std::pair<uint32_t, uint32_t> c_audio_rate()
		{
			lsnes_core_get_av_state s;
			return entrypoint(id, s) ? std::make_pair(s.rate_n, s.rate_d) : std::make_pair(48000U, 1U);
		}
		void c_power()
		{
			lsnes_core_poweron s;
			if(caps1 & LSNES_CORE_CAP1_POWERON) entrypoint(id, s);
		}
		void c_unload_cartridge()
		{
			lsnes_core_unload_cartridge s;
			if(caps1 & LSNES_CORE_CAP1_UNLOAD) entrypoint(id, s);
		}
		void c_runtosave()
		{
			lsnes_core_runtosave s;
			if(caps1 & LSNES_CORE_CAP1_RUNTOSAVE) entrypoint(id, s);
		}
		void c_emulate()
		{
			current_core = this;
			lsnes_core_emulate s;
			entrypoint(id, s);
			current_core = NULL;
		}
		bool c_get_pflag()
		{
			lsnes_core_get_pflag s;
			if(!(caps1 & LSNES_CORE_CAP1_PFLAG) || !entrypoint(id, s))
				return internal_pflag;
			return (s.pflag != 0);
		}
		void c_set_pflag(bool pflag)
		{
			lsnes_core_set_pflag s;
			s.pflag = pflag ? 1 : 0;
			if(!(caps1 & LSNES_CORE_CAP1_PFLAG) || !entrypoint(id, s))
				internal_pflag = pflag;
		}
		std::string c_get_core_shortname()
		{
			return shortname;
		}
		void c_debug_reset()
		{
			lsnes_core_debug_reset s;
			if(caps1 & (LSNES_CORE_CAP1_DEBUG | LSNES_CORE_CAP1_TRACE | LSNES_CORE_CAP1_CHEAT))
				entrypoint(id, s);
		}
		std::pair<unsigned, unsigned> c_lightgun_scale()
		{
			lsnes_core_get_av_state s;
			if(!(caps1 & LSNES_CORE_CAP1_LIGHTGUN) || !entrypoint(id, s))
				return std::make_pair(0, 0);
			return std::make_pair(s.lightgun_width, s.lightgun_height);
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map()
		{
			lsnes_core_get_bus_mapping s;
			if(!(caps1 & LSNES_CORE_CAP1_BUSMAP) || !entrypoint(id, s))
				return std::make_pair(0, 0);
			return std::make_pair(s.base, s.size);
		}
		int c_reset_action(bool hard)
		{
			lsnes_core_get_reset_action s;
			if(!(caps1 & LSNES_CORE_CAP1_RESET) || !entrypoint(id, s))
				return -1;
			return hard ? s.hardreset : s.softreset;
		}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height)
		{
			lsnes_core_compute_scale s;
			uint32_t hscale, vscale;
			if(width >= 360) hscale = 1;
			else hscale = 360 / width + 1;
			if(height >= 320) vscale = 1;
			else vscale = 320 / height + 1;
			if(caps1 & LSNES_CORE_CAP1_SCALE) {
				s.width = width;
				s.height = height;
				if(entrypoint(id, s)) {
					hscale = s.hfactor;
					vscale = s.vfactor;
				}
			}
			return std::make_pair(hscale, vscale);
		}
		void c_pre_emulate_frame(controller_frame& cf)
		{
			lsnes_core_pre_emulate s;
			if(caps1 & LSNES_CORE_CAP1_PREEMULATE) {
				s.context = &cf;
				s.set_input = [](void* context, unsigned port, unsigned controller, unsigned index,
					short value) -> void {
					controller_frame& cf = *(controller_frame*)context;
					cf.axis3(port, controller, index, value);
				};
				entrypoint(id, s);
			}
		}
		std::set<std::string> c_srams()
		{
			lsnes_core_enumerate_sram s;
			std::set<std::string> ret;
			if(caps1 & LSNES_CORE_CAP1_SRAM) {
				if(entrypoint(id, s)) {
					const char** sr = s.srams;
					while(*sr) {
						ret.insert(*sr);
						sr++;
					}
				}
			}
			return ret;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
		{
			lsnes_core_load_sram s;
			if(caps1 & LSNES_CORE_CAP1_SRAM) {
				std::vector<lsnes_core_sram> srams;
				std::vector<lsnes_core_sram*> sramsp;
				std::vector<char> names;
				srams.resize(sram.size() + 1);
				srams[sram.size()].name = NULL;
				size_t idx = 0;
				size_t nlength = 0;
				for(auto& i : sram)
					nlength += i.first.length() + 1;
				names.resize(nlength);
				size_t nidx = 0;
				for(auto& i : sram) {
					size_t ntmp = nidx;
					std::copy(i.first.begin(), i.first.end(), names.begin() + nidx);
					nidx += i.first.length();
					names[nidx++] = '\0';
					srams[idx].size = i.second.size();
					srams[idx].data = &i.second[0];
					srams[idx].name = &names[ntmp];
					idx++;
				}
				s.srams = &srams[0];
				entrypoint(id, s);
			}
		}
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc)
		{
			lsnes_core_save_sram s;
			std::map<std::string, std::vector<char>> ret;
			if(caps1 & LSNES_CORE_CAP1_SRAM) {
				if(entrypoint(id, s)) {
					lsnes_core_sram* p = s.srams;
					while(p->name) {
						ret[p->name].resize(p->size);
						memcpy(&ret[p->name][0], p->data, p->size);
						p++;
					}
				}
			}
			return ret;
		}
		void c_unserialize(const char* in, size_t insize)
		{
			lsnes_core_loadstate s;
			s.data = in;
			s.size = insize;
			entrypoint(id, s, [](const char* name, const char* err) {
				throw std::runtime_error("Loadstate failed: " + std::string(err));
			});
		}
		void c_serialize(std::vector<char>& out)
		{
			lsnes_core_savestate s;
			entrypoint(id, s, [](const char* name, const char* err) {
				throw std::runtime_error("Savestate failed: " + std::string(err));
			});
			out.resize(s.size);
			memcpy(&out[0], s.data, s.size);
		}
		unsigned c_action_flags(unsigned _id)
		{
			lsnes_core_get_action_flags s;
			if(caps1 & LSNES_CORE_CAP1_ACTION) {
				s.action = _id;
				return entrypoint(id, s) ? s.flags : 0;
			}
			return 0;
		}
		std::vector<std::string> c_get_trace_cpus()
		{
			return trace_cpus;
		}
		bool c_set_region(core_region& region)
		{
			lsnes_core_set_region s;
			if(caps1 & LSNES_CORE_CAP1_MULTIREGION) {
				bool hit = false;
				for(auto i : regions)
					if(i.second == &region) {
						s.region = i.first;
						hit = true;
					}
				if(!hit)
					return false;	//Bad region.
				return entrypoint(id, s);
			} else {
				return (regions.count(0) && regions[0] == &region);
			}
		}
		core_region& c_get_region()
		{
			lsnes_core_get_region s;
			if(caps1 & LSNES_CORE_CAP1_MULTIREGION) {
				if(!entrypoint(id, s))  {
					if(regions.empty())
						throw std::runtime_error("No valid regions");
					return *(regions.begin()->second);
				} else {
					if(regions.count(s.region))
						return *regions[s.region];
					messages << "Internal error: Core gave invalid region number."
						<< std::endl;
					if(regions.empty())
						throw std::runtime_error("No valid regions");
					return *(regions.begin()->second);
				}
			} else {
				if(regions.count(0))
					return *regions[0];
				else {
					messages << "Internal error: Not multi-region core and region 0 not present."
						<< std::endl;
					if(regions.empty())
						throw std::runtime_error("No valid regions");
					return *(regions.begin()->second);
				}
			}
		}
		const struct interface_device_reg* c_get_registers()
		{
			static std::vector<interface_device_reg> regs;
			static std::vector<char> namebuf;
			static interface_device_reg reg_null = {NULL};
			lsnes_core_get_device_regs s;
			if(caps1 & LSNES_CORE_CAP1_REGISTERS) {
				if(!entrypoint(id, s))  {
					return &reg_null;
				} else {
					size_t count = 0;
					size_t namelen = 0;
					auto tregs = s.regs;
					while(tregs->name) {
						namelen += strlen(tregs->name) + 1;
						count++;
						tregs++;
					}
					tregs = s.regs;
					if(regs.size() < count + 1)
						regs.resize(count + 1);
					if(namelen > namebuf.size())
						namebuf.resize(namelen);
					size_t idx = 0;
					size_t nameptr = 0;
					while(tregs->name) {
						strcpy(&namebuf[nameptr], tregs->name);
						regs[idx].name = &namebuf[nameptr];
						regs[idx].read = tregs->read;
						regs[idx].write = tregs->write;
						regs[idx].boolean = tregs->boolean;
						nameptr += strlen(tregs->name) + 1;
						tregs++;
						idx++;
					}
					regs[idx].name = NULL;	//The sentinel.
				}
			} else {
				return &reg_null;
			}
			return &regs[0];
		}
		void c_install_handler()
		{
			plugin->initialize();
		}
		void c_uninstall_handler()
		{
			plugin->deinitialize();
		}
		void c_set_debug_flags(uint64_t addr, unsigned flags_set, unsigned flags_clear)
		{
			lsnes_core_set_debug_flags s;
			s.addr = addr;
			s.set = flags_set;
			s.clear = flags_clear;
			if(caps1 & LSNES_CORE_CAP1_DEBUG)
				entrypoint(id, s);
			else
				messages << "Debugging functions not supported by core" << std::endl;
		}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set)
		{
			lsnes_core_set_cheat s;
			s.addr = addr;
			s.value = value;
			s.set = set;
			if(caps1 & LSNES_CORE_CAP1_CHEAT)
				entrypoint(id, s);
			else
				messages << "Cheat functions not supported by core" << std::endl;
		}
		void c_execute_action(unsigned aid, const std::vector<interface_action_paramval>& p)
		{
			if(!(caps1 & LSNES_CORE_CAP1_ACTION)) {
				messages << "Core does not support actions." << std::endl;
				return;
			}
			interface_action* act = NULL;
			for(auto& i : actions) {
				if(i.id == aid) {
					act = &i;
					break;
				}
			}
			if(!act) {
				messages << "Unknown action id #" << aid << std::endl;
				return;
			}
			size_t j = 0;
			std::vector<lsnes_core_execute_action_param> parameters;
			std::list<std::vector<char>> strtmp;
			for(auto& b : act->params)  {
				std::string m = b.model;
				if(m == "bool") {
					//Boolean.
					lsnes_core_execute_action_param tp;
					tp.boolean = p[j++].b;
					parameters.push_back(tp);
				} else if(regex_match("int:.*", m)) {
					//Integer.
					lsnes_core_execute_action_param tp;
					tp.integer = p[j++].i;
					parameters.push_back(tp);
				} else if(regex_match("string(:.*)?", m) || regex_match("enum:.*", m)) {
					//String.
					strtmp.push_back(std::vector<char>());
					auto& i = strtmp.back();
					i.resize(p[j].s.length() + 1);
					std::copy(p[j].s.begin(), p[j].s.end(), i.begin());
					lsnes_core_execute_action_param tp;
					tp.string.base = &i[0];
					tp.string.length = p[j++].s.length();
					parameters.push_back(tp);
				} else if(m == "toggle") {
					//Skip.
				} else {
					messages << "Unknown model '" << m << "' in action id #" << aid << std::endl;
					return;
				}
			}
			lsnes_core_execute_action s;
			s.action = aid;
			s.params = &parameters[0];
			entrypoint(id, s);
		}
		framebuffer::raw& c_draw_cover()
		{
			lsnes_core_draw_cover r;
			if(caps1 & LSNES_CORE_CAP1_COVER) {
				if(!entrypoint(id, r))
					goto failed;
				framebuffer::info fbinfo = translate_info(r.coverpage);
				size_t needed = fbinfo.physwidth * fbinfo.physheight * fbinfo.type->get_bpp();
				if(covermem.size() < needed) covermem.resize(needed);
				memcpy(&covermem[0], fbinfo.mem, needed);
				fbinfo.mem = &covermem[0];
				cover = framebuffer::raw(fbinfo);
				return cover;
			}
failed:
			if(covermem.size() < 1024) covermem.resize(1024);
			framebuffer::info fbi;
			fbi.type = &framebuffer::pixfmt_rgb16;
			fbi.mem = &covermem[0];
			fbi.physwidth = 512;
			fbi.physheight = 448;
			fbi.physstride = 0;
			fbi.width = 512;
			fbi.height = 448;
			fbi.stride = 0;
			fbi.offset_x = 0;
			fbi.offset_y = 0;
			cover = framebuffer::raw(fbi);
			return cover;
		}
		std::list<core_vma_info> c_vma_list()
		{
			lsnes_core_get_vma_list r;
			if(caps1 & LSNES_CORE_CAP1_MEMWATCH) {
				if(!entrypoint(id, r))
					goto failed;
				std::list<core_vma_info> vmalist;
				for(lsnes_core_get_vma_list_vma** vmas = r.vmas; *vmas; vmas++) {
					lsnes_core_get_vma_list_vma* vma = *vmas;
					core_vma_info _vma;
					_vma.name = vma->name;
					_vma.base = vma->base;
					_vma.size = vma->size;
					_vma.endian = vma->endian;
					_vma.readonly = ((vma->flags & LSNES_CORE_VMA_READONLY) != 0);
					_vma.special = ((vma->flags & LSNES_CORE_VMA_SPECIAL) != 0);
					_vma.volatile_flag = ((vma->flags & LSNES_CORE_VMA_VOLATILE) != 0);
					_vma.backing_ram = vma->direct_map;
					_vma.read = vma->read;
					_vma.write = vma->write;
					vmalist.push_back(_vma);
				}
				return vmalist;
			}
failed:
			return std::list<core_vma_info>();
		}
		std::map<unsigned, port_type*> get_ports()
		{
			return ports;
		}
		void set_internal_pflag()
		{
			internal_pflag = true;
		}
	private:
		std::string fullname;
		std::string shortname;
		unsigned id;
		bool internal_pflag;
		unsigned caps1;
		std::vector<std::string> trace_cpus;
		std::map<unsigned, port_type*> ports;
		std::map<unsigned, core_region*> regions;
		std::vector<interface_action> actions;
		framebuffer::raw cover;
		std::vector<char> covermem;
		entrypoint_fn entrypoint;
		c_lib_init* plugin;
	};

	c_core_core::~c_core_core() throw()
	{
	}

	struct c_core_type : public core_type
	{
		c_core_type(c_lib_init& lib, core_type_params& p, std::map<unsigned, port_type*> _ports,
			unsigned _rcount, unsigned _id)
			: core_type(p), ports(_ports), entrypoint(lib.get_entrypoint()), rcount(_rcount), id(_id)
		{
		}
		~c_core_type() throw()
		{
		}
		int t_load_rom(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
			uint64_t rtc_subsec)
		{
			lsnes_core_load_rom r;
			std::vector<char> tmpmem;
			std::vector<lsnes_core_system_setting> tmpmem2;
			r.rtc_sec = rtc_sec;
			r.rtc_subsec = rtc_subsec;
			copy_settings(tmpmem, tmpmem2, settings);
			r.settings = &tmpmem2[0];

			std::vector<lsnes_core_load_rom_image> imgs;
			imgs.resize(rcount);
			for(unsigned i = 0; i < rcount; i++) {
				imgs[i].data = (const char*)images[i].data;
				imgs[i].size = images[i].size;
				imgs[i].markup = images[i].markup;
			}
			r.images = &imgs[0];
			unsigned _id = id;
			entrypoint(_id, r, [_id](const char* name, const char* err) {
				(stringfmt() << "LSNES_CORE_LOAD_ROM(" << _id << ") failed: " << err).throwex();
			});
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			lsnes_core_get_controllerconfig r;
			std::vector<char> tmpmem;
			std::vector<lsnes_core_system_setting> tmpmem2;
			copy_settings(tmpmem, tmpmem2, settings);
			r.settings = &tmpmem2[0];
			unsigned _id = id;
			entrypoint(_id, r, [_id](const char* name, const char* err) {
				(stringfmt() << "LSNES_CORE_GET_CONTROLLERCONFIG(" << _id << ") failed: "
					<< err).throwex();
			});
			controller_set cset;
			for(unsigned* pt = r.controller_types; *pt != 0xFFFFFFFFU; pt++) {
				unsigned _pt = *pt;
				if(!ports.count(_pt))
					throw std::runtime_error("Illegal port type selected by core");
				port_type* pt2 = ports[_pt];
				cset.ports.push_back(pt2);
			}
			for(lsnes_core_get_controllerconfig_logical_entry* le = r.logical_map;
				le->port | le->controller; le++) {
				cset.logical_map.push_back(std::make_pair(le->port, le->controller));
			}
			return cset;
		}
	private:
		void copy_settings(std::vector<char>& tmpmem, std::vector<lsnes_core_system_setting>& tmpmem2,
			std::map<std::string, std::string>& settings)
		{
			size_t asize = 0;
			for(auto i : settings)
				asize += i.first.length() + i.second.length() + 2;
			tmpmem.resize(asize);
			asize = 0;
			for(auto i : settings) {
				lsnes_core_system_setting s;
				std::copy(i.first.begin(), i.first.end(), tmpmem.begin() + asize);
				s.name = &tmpmem[asize];
				asize += i.first.length();
				tmpmem[asize++] = 0;
				std::copy(i.second.begin(), i.second.end(), tmpmem.begin() + asize);
				s.value = &tmpmem[asize];
				asize += i.second.length();
				tmpmem[asize++] = 0;
				tmpmem2.push_back(s);
			}
			lsnes_core_system_setting s;
			s.name = NULL;
			s.value = NULL;
			tmpmem2.push_back(s);
		}
		std::map<unsigned, port_type*> ports;
		entrypoint_fn entrypoint;
		unsigned rcount;
		unsigned id;
	};

	std::vector<char> msgbuf;

	void callback_message(const char* msg, size_t length)
	{
		std::string _msg(msg, msg + length);
		messages << _msg << std::endl;
	}

	short callback_get_input(unsigned port, unsigned index, unsigned control)
	{
		short v = ecore_callbacks->get_input(port, index, control);
		if(current_core && (port || index || v))
			current_core->set_internal_pflag();
		return v;
	}

	void callback_notify_action_update()
	{
		ecore_callbacks->action_state_updated();
	}

	void callback_timer_tick(uint32_t increment, uint32_t per_second)
	{
		ecore_callbacks->timer_tick(increment, per_second);
	}

	const char* callback_get_firmware_path()
	{
		std::string fwp = ecore_callbacks->get_firmware_path();
		msgbuf.resize(fwp.length() + 1);
		std::copy(fwp.begin(), fwp.end(), msgbuf.begin());
		msgbuf[fwp.length()] = 0;
		return &msgbuf[0];
	}

	const char* callback_get_base_path()
	{
		std::string fwp = ecore_callbacks->get_base_path();
		msgbuf.resize(fwp.length() + 1);
		std::copy(fwp.begin(), fwp.end(), msgbuf.begin());
		msgbuf[fwp.length()] = 0;
		return &msgbuf[0];
	}

	time_t callback_get_time()
	{
		return ecore_callbacks->get_time();
	}

	time_t callback_get_randomseed()
	{
		return ecore_callbacks->get_randomseed();
	}

	void callback_memory_read(uint64_t addr, uint64_t value)
	{
		ecore_callbacks->memory_read(addr, value);
	}

	void callback_memory_write(uint64_t addr, uint64_t value)
	{
		ecore_callbacks->memory_write(addr, value);
	}

	void callback_memory_execute(uint64_t addr, uint64_t cpunum)
	{
		ecore_callbacks->memory_execute(addr, cpunum);
	}

	void callback_memory_trace(uint64_t proc, const char* str, int insn)
	{
		ecore_callbacks->memory_trace(proc, str, insn);
	}

	void callback_submit_sound(const int16_t* samples, size_t count, int stereo, double rate)
	{
		CORE().audio->submit_buffer((int16_t*)samples, count, stereo, rate);
	}

	void callback_notify_latch(const char** params)
	{
		std::list<std::string> ps;
		if(params)
			for(const char** p = params; *p; p++)
				ps.push_back(*p);
		ecore_callbacks->notify_latch(ps);
	}

	void callback_submit_frame(struct lsnes_core_framebuffer_info* _fb, uint32_t fps_n, uint32_t fps_d)
	{
		framebuffer::info fbinfo = translate_info(_fb);
		framebuffer::raw fb(fbinfo);
		ecore_callbacks->output_frame(fb, fps_n, fps_d);
	}

	struct fpcfn
	{
		std::function<unsigned char()> fn;
		static unsigned char call(void* ctx)
		{
			return reinterpret_cast<fpcfn*>(ctx)->fn();
		}
	};

	class ccore_disasm : public disassembler
	{
	public:
		ccore_disasm(struct lsnes_core_disassembler* disasm)
			: disassembler(disasm->name)
		{
			fn = disasm->fn;
		}
		~ccore_disasm()
		{
		}
		std::string disassemble(uint64_t base, std::function<unsigned char()> fetchpc)
		{
			fpcfn y;
			y.fn = fetchpc;
			const char* out = fn(base, fpcfn::call, &y);
			return out;
		}
	private:
		const char* (*fn)(uint64_t base, unsigned char(*fetch)(void* ctx), void* ctx);
	};

	void* callback_add_disasm(struct lsnes_core_disassembler* disasm)
	{
		return new ccore_disasm(disasm);
	}

	void callback_remove_disasm(void* handle)
	{
		delete reinterpret_cast<ccore_disasm*>(handle);
	}

	struct utf8_strlen_iter
	{
		utf8_strlen_iter() { str_len = 0; }
		utf8_strlen_iter& operator++() { str_len++; return *this; }
		uint32_t& operator*() { return dummy; }
		size_t str_len;
		uint32_t dummy;
	};

	template<typename T>
	void callback_render_text2(struct lsnes_core_fontrender_req& req, const std::string& str)
	{
		auto size = main_font.get_metrics(str);
		auto layout = main_font.dolayout(str);
		size_t memreq = size.first * size.second * sizeof(T);
		if(!memreq) memreq = 1;
		if(size.first && memreq / size.first / sizeof(T) < size.second)
			throw std::bad_alloc();		//Not enough memory.
		req.bitmap = req.alloc(req.cb_ctx, memreq);
		if(!req.bitmap)
			throw std::bad_alloc();		//Not enough memory.
		T fg = (T)req.fg_color;
		T bg = (T)req.bg_color;
		T* bmp = (T*)req.bitmap;

		for(auto i : layout) {
			auto& g = *i.dglyph;
			T* _bmp = bmp + (i.y * size.first + i.x);
			size_t w = g.wide ? 16 : 8;
			size_t skip = size.first - w;
			for(size_t _y = 0; _y < 16; _y++) {
				uint32_t d = g.data[_y >> (g.wide ? 1 : 2)];
				if(g.wide)
					d >>= 16 - ((_y & 1) << 4);
				else
					d >>= 24 - ((_y & 3) << 3);
				for(size_t _x = 0; _x < w; _x++, _bmp++) {
					uint32_t b = w - _x - 1;
					*_bmp = ((d >> b) & 1) ? fg : bg;
				}
				_bmp = _bmp + skip;
			}
		}
		req.width = size.first;
		req.height = size.second;
	}

	void callback_render_text1(struct lsnes_core_fontrender_req& req, const std::string& str)
	{
		switch(req.bytes_pp) {
		case 1:
			callback_render_text2<uint8_t>(req, str);
			return;
		case 2:
			callback_render_text2<uint16_t>(req, str);
			return;
		case 3:
			callback_render_text2<ss_uint24_t>(req, str);
			return;
		case 4:
			callback_render_text2<uint32_t>(req, str);
			return;
		default:
			throw std::runtime_error("Invalid req.bytes_pp");
		}
	}

	int callback_render_text(struct lsnes_core_fontrender_req* req)
	{
		req->bitmap = NULL;
		//If indeterminate length, make it determinate.
		if(req->text_len < 0)
			req->text_len = strlen(req->text);
		const char* text_start = req->text;
		const char* text_end = req->text + req->text_len;
		try {
			std::string str(text_start, text_end);
			callback_render_text1(*req, str);
			return 0;
		} catch(...) {
			return -1;
		}
	}

	core_sysregion* create_sysregion(entrypoint_fn& entrypoint, std::map<unsigned, core_region*>& regions,
		std::map<unsigned, c_core_type*>& types, unsigned sysreg)
	{
		struct lsnes_core_get_sysregion_info r;
		entrypoint(sysreg, r, [sysreg](const char* name, const char* err) {
			(stringfmt() << "LSNES_CORE_GET_SYSREGION_INFO(" << sysreg << ") failed: " << err).throwex();
		});
		register_sysregion_mapping(r.name, r.for_system);
		if(!types.count(r.type))
			throw std::runtime_error("create_sysregion: Unknown type");
		if(!regions.count(r.region))
			throw std::runtime_error("create_sysregion: Unknown region");
		return new core_sysregion(r.name, *types[r.type], *regions[r.region]);
	}

	core_region* create_region(entrypoint_fn& entrypoint, unsigned region)
	{
		struct lsnes_core_get_region_info r;
		entrypoint(region, r, [region](const char* name, const char* err) {
			(stringfmt() << "LSNES_CORE_GET_REGION_INFO(" << region << ") failed: " << err).throwex();
		});
		core_region_params p;
		p.iname = r.iname;
		p.hname = r.hname;
		p.priority = r.priority;
		p.handle = region;
		p.multi = r.multi;
		p.framemagic[0] = r.fps_n;
		p.framemagic[1] = r.fps_d;
		for(size_t i = 0; r.compatible_runs[i] != 0xFFFFFFFFU; i++)
			p.compatible_runs.push_back(r.compatible_runs[i]);
		return new core_region(p);
	}

	c_core_core* create_core(c_lib_init& lib, entrypoint_fn& entrypoint,
		std::map<unsigned, core_region*>& regions, unsigned core)
	{
		c_core_core_params p;
		struct lsnes_core_get_core_info r;
		entrypoint(core, r, [core](const char* name, const char* err) {
			(stringfmt() << "LSNES_CORE_GET_CORE_INFO(" << core << ") failed: " << err).throwex();
		});
		//Read ports.
		JSON::node root(r.json);
		JSON::pointer rootptr(r.root_ptr);
		size_t count = root[rootptr].index_count();
		for(size_t i = 0; i < count; i++) {
			JSON::pointer j = rootptr.index(i);
			p.ports.push_back(new port_type_generic(root, j.as_string8()));
		}
		//Read actions.
		if(r.cap_flags1 & LSNES_CORE_CAP1_ACTION) {
			for(lsnes_core_get_core_info_action* i = r.actions; i->iname; i++) {
				interface_action a;
				a.id = i->id;
				a._symbol = i->iname;
				a._title = i->hname;
				if(!i->parameters)
					goto no_parameters;
				for(lsnes_core_get_core_info_aparam* k = i->parameters; k->name; k++) {
					interface_action_param b;
					b.name = strduplicate(k->name);
					b.model = strduplicate(k->model);
					a.params.push_back(b);
				}
no_parameters:
				p.actions.push_back(a);
			}
		}
		//Read trace CPUs.
		if(r.cap_flags1 & LSNES_CORE_CAP1_TRACE) {
			for(const char** i = r.trace_cpu_list; *i; i++) {
				p.trace_cpus.push_back(*i);
			}
		}
		for(auto & j : regions)
			p.regions[j.first] = j.second;
		p.shortname = r.shortname;
		p.fullname = r.fullname;
		p.flags = r.cap_flags1;
		p.id = core;
		p.library = &lib;
		return new c_core_core(p);
	}

	c_core_type* create_type(c_lib_init& lib, entrypoint_fn& entrypoint, std::map<unsigned, c_core_core*>& cores,
		std::map<unsigned, core_region*>& regions, unsigned type)
	{
		std::vector<core_romimage_info_params> rlist;
		std::vector<core_setting_param> plist;
		core_type_params p;
		struct lsnes_core_get_type_info r;
		entrypoint(type, r, [type](const char* name, const char* err) {
			(stringfmt() << "LSNES_CORE_GET_TYPE_INFO(" << type << ") failed: " << err).throwex();
		});
		if(r.settings) {
			for(lsnes_core_get_type_info_param* param = r.settings; param->iname; param++) {
				core_setting_param _pr;
				_pr.iname = strduplicate(param->iname);
				_pr.hname = strduplicate(param->hname);
				_pr.dflt = strduplicate(param->dflt);
				_pr.regex = strduplicate(param->regex);
				if(!param->values)
					goto no_values;
				for(lsnes_core_get_type_info_paramval* pval = param->values; pval->iname; pval++) {
					core_setting_value_param pv;
					pv.iname = strduplicate(pval->iname);
					pv.hname = strduplicate(pval->hname);
					pv.index = pval->index;
					_pr.values.push_back(pv);
				}
			no_values:
				plist.push_back(_pr);
			}
		}
		unsigned rcount = 0;
		if(r.images) {
			for(lsnes_core_get_type_info_romimage* rimg = r.images; rimg->iname; rimg++) {
				core_romimage_info_params rp;
				rp.iname = rimg->iname;
				rp.hname = rimg->hname;
				rp.mandatory = rimg->mandatory;
				rp.pass_mode = rimg->pass_mode;
				rp.headersize = rimg->headersize;
				rp.extensions = rimg->extensions;
				rlist.push_back(rp);
				rcount++;
			}
		}
		unsigned _core = r.core;
		p.id = type;
		p.iname = r.iname;
		p.hname = r.hname;
		p.sysname = r.sysname;
		p.bios = r.bios;
		p.settings = plist;
		p.images = rlist;
		if(!cores.count(_core))
			throw std::runtime_error("create_type: Unknown core");
		p.core = cores[_core];
		for(unsigned* reg = r.regions; *reg != 0xFFFFFFFFU; reg++) {
			if(!regions.count(*reg))
				throw std::runtime_error("create_type: Unknown region");
			p.regions.push_back(regions[*reg]);
		}
		return new c_core_type(lib, p, cores[_core]->get_ports(), rcount, type);
	}

	void initialize_core2(entrypoint_fn fn, std::map<unsigned, core_sysregion*>& sysregs,
		std::map<unsigned, core_region*>& regions, std::map<unsigned, c_core_type*>& types,
		std::map<unsigned, c_core_core*>& cores)
	{
		c_lib_init& lib = *new c_lib_init(fn);
		for(auto& i : regions)
			i.second = create_region(fn, i.first);
		for(auto& i : cores)
			i.second = create_core(lib, fn, regions, i.first);
		for(auto& i : types)
			i.second = create_type(lib, fn, cores, regions, i.first);
		for(auto& i : sysregs)
			i.second = create_sysregion(fn, regions, types, i.first);
		//We don't call install_handler, because that is done automatically.
	}

	void initialize_core(lsnes_core_func_t fn)
	{
		//Enumerate what the thing supports.
		entrypoint_fn entrypoint(fn);
		lsnes_core_enumerate_cores r;
		r.emu_flags1 = 2;
		r.message = callback_message;
		r.get_input = callback_get_input;
		r.notify_action_update = callback_notify_action_update;
		r.timer_tick = callback_timer_tick;
		r.get_firmware_path = callback_get_firmware_path;
		r.get_base_path = callback_get_base_path;
		r.get_time = callback_get_time;
		r.get_randomseed = callback_get_randomseed;
		r.memory_read = callback_memory_read;
		r.memory_write = callback_memory_write;
		r.memory_execute = callback_memory_execute;
		r.memory_trace = callback_memory_trace;
		r.submit_sound = callback_submit_sound;
		r.notify_latch = callback_notify_latch;
		r.submit_frame = callback_submit_frame;
		r.add_disasm = callback_add_disasm;
		r.remove_disasm = callback_remove_disasm;
		r.render_text = callback_render_text;
		entrypoint(0, r, [](const char* name, const char* err) {
			(stringfmt() << "LSNES_CORE_ENUMERATE_CORES(0) failed: " << err).throwex();
		});
		//Collect sysregions, types and cores.
		std::map<unsigned, core_region*> regions;
		std::map<unsigned, core_sysregion*> sysregs;
		std::map<unsigned, c_core_type*> types;
		std::map<unsigned, c_core_core*> cores;
		for(size_t i = 0; r.sysregions[i] != 0xFFFFFFFFU; i++) {
			unsigned sysreg = r.sysregions[i];
			sysregs.insert(std::make_pair(sysreg, nullptr));
			struct lsnes_core_get_sysregion_info r2;
			entrypoint(sysreg, r2, [sysreg](const char* name, const char* err) {
				(stringfmt() << "LSNES_CORE_GET_SYSREGION_INFO(" << sysreg << ") failed: "
					<< err).throwex();
			});
			unsigned type = r2.type;
			types.insert(std::make_pair(type, nullptr));
			struct lsnes_core_get_type_info r3;
			entrypoint(type, r3, [type](const char* name, const char* err) {
				(stringfmt() << "LSNES_CORE_GET_TYPE_INFO(" << type << ") failed: "
					<< err).throwex();
			});
			cores.insert(std::make_pair(r3.core, nullptr));
			for(size_t j = 0; r3.regions[j] != 0xFFFFFFFFU; j++) {
				regions.insert(std::make_pair(r3.regions[j], nullptr));
			}
		}
		//Do the rest.
		initialize_core2(entrypoint, sysregs, regions, types, cores);
	}
}

void lsnes_register_builtin_core(lsnes_core_func_t fn)
{
	corequeue().push_back(fn);
}

void try_init_c_module(const loadlib::module& module)
{
	try {
		lsnes_core_func_t fn = module.fn<int, unsigned, unsigned, void*,
			const char**>("lsnes_core_entrypoint");
		initialize_core(fn);
	} catch(std::exception& e) {
		messages << "Can't initialize core: " << e.what() << std::endl;
	}
}

void initialize_all_builtin_c_cores()
{
	while(!corequeue().empty()) {
		lsnes_core_func_t fn = corequeue().front();
		corequeue().pop_front();
		try {
			initialize_core(fn);
		} catch(std::exception& e) {
			messages << "Can't initialize core: " << e.what() << std::endl;
		}
	}
}
