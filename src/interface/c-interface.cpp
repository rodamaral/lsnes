#include "interface/c-interface.hpp"
#include "core/window.hpp"

namespace
{
	std::list<lsnes_core_func_t>& corequeue()
	{
		static std::list<lsnes_core_func_t> x;
		return x;
	}

	struct c_core_core
	{
		std::string c_core_identifier()
		{
			return fullname;
		}
		bool get_av_state(lsnes_core_get_av_state& s)
		{
			const char* err;
			int r = entrypoint(LSNES_CORE_GET_AV_STATE, id, &s, &err);
			if(r < 0)
				messages << "Warning: LSNES_CORE_GET_AV_STATE failed: " << err << std::endl;
			return !r;
		}
		std::pair<uint32_t, uint32_t> c_video_rate()
		{
			lsnes_core_get_av_state s;
			if(!get_av_state(s))
				return std::make_pair(60, 1);
			return std::make_pair(s.fps_n, s.fps_d);
		}
		double c_get_PAR()
		{
			lsnes_core_get_av_state s;
			if(!get_av_state(s))
				return 1.0;
			return s.par;
		}
		std::pair<uint32_t, uint32_t> c_audio_rate()
		{
			lsnes_core_get_av_state s;
			if(!get_av_state(s))
				return std::make_pair(48000, 1);
			return std::make_pair(s.rate_n, s.rate_d);
		}
		void c_power()
		{
			lsnes_core_poweron s;
			const char* err;
			if(caps1 & LSNES_CORE_CAP1_POWERON) {
				int r = entrypoint(LSNES_CORE_POWERON, id, &s, &err);
				if(r > 0)
					messages << "Warning: LSNES_CORE_POWERON failed: " << err << std::endl;
			}
		}
		void c_unload_cartridge()
		{
			lsnes_core_unload_cartridge s;
			const char* err;
			if(caps1 & LSNES_CORE_CAP1_UNLOAD) {
				int r = entrypoint(LSNES_CORE_UNLOAD_CARTRIDGE, id, &s, &err);
				if(r > 0)
					messages << "Warning: LSNES_CORE_UNLOAD_CARTRIDGE failed: " << err
						<< std::endl;
			}
		}
		void c_runtosave()
		{
			lsnes_core_runtosave s;
			const char* err;
			if(caps1 & LSNES_CORE_CAP1_RUNTOSAVE) {
				int r = entrypoint(LSNES_CORE_RUNTOSAVE, id, &s, &err);
				if(r > 0)
					messages << "Warning: LSNES_CORE_RUNTOSAVE failed: " << err << std::endl;
			}
		}
		void c_emulate()
		{
			lsnes_core_emulate s;
			const char* err;
			int r = entrypoint(LSNES_CORE_EMULATE, id, &s, &err);
			if(r < 0)
				messages << "Warning: LSNES_CORE_EMULATE failed: " << err << std::endl;
		}
		bool c_get_pflag()
		{
			lsnes_core_get_pflag s;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_PFLAG) {
				r = entrypoint(LSNES_CORE_GET_PFLAG, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_GET_PFLAG failed: " << err << std::endl;
			}
			if(r < 0)
				return internal_pflag;
			return (s.pflag != 0);
		}
		void c_set_pflag(bool pflag)
		{
			lsnes_core_set_pflag s;
			s.pflag = pflag ? 1 : 0;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_PFLAG) {
				r = entrypoint(LSNES_CORE_SET_PFLAG, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_SET_PFLAG failed: " << err << std::endl;
			}
			if(r)
				internal_pflag = pflag;
		}
		std::string c_get_core_shortname()
		{
			return shortname;
		}
		void c_debug_reset()
		{
			lsnes_core_debug_reset s;
			const char* err;
			if(caps1 & (LSNES_CORE_CAP1_DEBUG | LSNES_CORE_CAP1_TRACE | LSNES_CORE_CAP1_CHEAT)) {
				int r = entrypoint(LSNES_CORE_DEBUG_RESET, id, &s, &err);
				if(r > 0)
					messages << "Warning: LSNES_CORE_DEBUG_RESET failed: " << err << std::endl;
			}
		}
		std::pair<unsigned, unsigned> c_lightgun_scale()
		{
			lsnes_core_get_av_state s;
			bool r = false;
			if(caps1 & LSNES_CORE_CAP1_LIGHTGUN)
				r = get_av_state(s);
			if(!r)
				return std::make_pair(0, 0);
			return std::make_pair(s.lightgun_width, s.lightgun_height);
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map()
		{
			lsnes_core_get_bus_mapping s;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_BUSMAP) {
				r = entrypoint(LSNES_CORE_GET_BUS_MAPPING, id, &s, &err);
				if(r > 0)
					messages << "Warning: LSNES_CORE_GET_BUS_MAPPING failed: " << err
						<< std::endl;
			}
			if(r)
				return std::make_pair(0, 0);
			return std::make_pair(s.base, s.size);
		}
		int c_reset_action(bool hard)
		{
			lsnes_core_get_reset_action s;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_RESET) {
				r = entrypoint(LSNES_CORE_GET_RESET_ACTION, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_GET_RESET_ACTION failed: " << err
						<< std::endl;
			}
			if(r)
				return -1;
			return hard ? s.hardreset : s.softreset;
		}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height)
		{
			lsnes_core_compute_scale s;
			const char* err;
			int r = -1;
			uint32_t hscale, vscale;
			if(width >= 360) hscale = 1;
			else hscale = 360 / width + 1;
			if(height >= 320) hscale = 1;
			else hscale = 320 / height + 1;
			if(caps1 & LSNES_CORE_CAP1_RESET) {
				s.width = width;
				s.height = height;
				r = entrypoint(LSNES_CORE_GET_RESET_ACTION, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_GET_RESET_ACTION failed: " << err
						<< std::endl;
				else {
					hscale = s.hfactor;
					vscale = s.vfactor;
				}
			}
			return std::make_pair(hscale, vscale);
		}
		void c_pre_emulate_frame(controller_frame& cf)
		{
			lsnes_core_pre_emulate s;
			const char* err;
			if(caps1 & LSNES_CORE_CAP1_PREEMULATE) {
				s.context = &cf;
				s.set_input = [](void* context, unsigned port, unsigned controller, unsigned index,
					short value) -> void {
					controller_frame& cf = *(controller_frame*)context;
					cf.axis3(port, controller, index, value);
				};
				int r = entrypoint(LSNES_CORE_PRE_EMULATE, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_PRE_EMULATE failed: " << err
						<< std::endl;
			}
		}
		std::set<std::string> c_srams()
		{
			lsnes_core_enumerate_sram s;
			const char* err;
			std::set<std::string> ret;
			if(caps1 & LSNES_CORE_CAP1_SRAM) {
				int r = entrypoint(LSNES_CORE_ENUMERATE_SRAM, id, &s, &err);
				if(r < 0) {
					messages << "Warning: LSNES_CORE_ENUMERATE_SRAM failed: " << err
						<< std::endl;
				} else {
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
			const char* err;
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
				int r = entrypoint(LSNES_CORE_LOAD_SRAM, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_LOAD_SRAM failed: " << err
						<< std::endl;
			}
		}
		std::map<std::string, std::vector<char>> c_save_sram() throw(std::bad_alloc)
		{
			lsnes_core_save_sram s;
			const char* err;
			std::map<std::string, std::vector<char>> ret;
			if(caps1 & LSNES_CORE_CAP1_SRAM) {
				int r = entrypoint(LSNES_CORE_SAVE_SRAM, id, &s, &err);
				if(r < 0) {
					messages << "Warning: LSNES_CORE_SAVE_SRAM failed: " << err
						<< std::endl;
				} else {
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
			const char* err;
			s.data = in;
			s.size = insize;
			int r = entrypoint(LSNES_CORE_LOADSTATE, id, &s, &err);
			if(r < 0)
				throw std::runtime_error("Loadstate failed: " + std::string(err));
		}
		void c_serialize(std::vector<char>& out)
		{
			lsnes_core_savestate s;
			const char* err;
			int r = entrypoint(LSNES_CORE_SAVESTATE, id, &s, &err);
			if(r < 0)
				throw std::runtime_error("Savestate failed: " + std::string(err));
			out.resize(s.size);
			memcpy(&out[0], s.data, s.size);
		}
		unsigned c_action_flags(unsigned id)
		{
			lsnes_core_get_action_flags s;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_ACTION) {
				s.action = id;
				r = entrypoint(LSNES_CORE_GET_ACTION_FLAGS, id, &s, &err);
				if(r < 0)
					messages << "Warning: LSNES_CORE_GET_ACTION_FLAGS failed: " << err
						<< std::endl;
			}
			if(r < 0)
				return 0;
			return s.flags;
		}
		std::vector<std::string> c_get_trace_cpus()
		{
			return trace_cpus;
		}
		bool c_set_region(core_region& region)
		{
			lsnes_core_set_region s;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_MULTIREGION) {
				bool hit = false;
				for(auto i : regions)
					if(i.second == &region) {
						s.region = i.first;
						hit = true;
					}
				if(!hit)
					return false;	//Bad region.
				r = entrypoint(LSNES_CORE_GET_ACTION_FLAGS, id, &s, &err);
				if(r < 0) {
					messages << "Warning: LSNES_CORE_SET_REGION failed: " << err
						<< std::endl;
					return false;
				} else
					return true;
			} else {
				if(regions.count(0) && regions[0] == &region)
					return true;
				else
					return false;
			}
		}
		core_region& c_get_region()
		{
			lsnes_core_set_region s;
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_MULTIREGION) {
				r = entrypoint(LSNES_CORE_GET_ACTION_FLAGS, id, &s, &err);
				if(r < 0) {
					messages << "Warning: LSNES_CORE_SET_REGION failed: " << err
						<< std::endl;
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
			const char* err;
			int r = -1;
			if(caps1 & LSNES_CORE_CAP1_REGISTERS) {
				r = entrypoint(LSNES_CORE_GET_DEVICE_REGS, id, &s, &err);
				if(r < 0) {
					messages << "Warning: LSNES_CORE_GET_DEVICE_REGS failed: " << err
						<< std::endl;
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
	virtual void c_install_handler() = 0;
	virtual void c_uninstall_handler() = 0;
	virtual framebuffer::raw& c_draw_cover() = 0;
	virtual void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p) = 0;
	virtual std::list<core_vma_info> c_vma_list() = 0;
	virtual void c_set_debug_flags(uint64_t addr, unsigned flags_set, unsigned flags_clear) = 0;
	virtual void c_set_cheat(uint64_t addr, uint64_t value, bool set) = 0;
	private:
		std::string fullname;
		std::string shortname;
		unsigned id;
		lsnes_core_func_t entrypoint;
		bool internal_pflag;
		unsigned caps1;
		std::vector<std::string> trace_cpus;
		std::map<unsigned, core_region*> regions;
	};
}

void lsnes_register_builtin_core(lsnes_core_func_t fn)
{
	corequeue().push_back(fn);
}

lsnes_core_func_t lsnes_interface_get_builtin()
{
	lsnes_core_func_t fn = corequeue().front();
	corequeue().pop_front();
	return fn;
}
