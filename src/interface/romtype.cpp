#include "core/misc.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include <set>
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <list>
#include <limits>

namespace
{
	bool install_handlers_automatically;

	std::map<std::string, std::string>& sysreg_mapping()
	{
		static std::map<std::string, std::string> x;
		return x;
	}

	std::set<core_core*>& all_cores_set()
	{
		static std::set<core_core*> x;
		return x;
	}

	std::set<core_core*>& uninitialized_cores_set()
	{
		static std::set<core_core*> x;
		return x;
	}

	std::multimap<std::string, core_sysregion*>& sysregions()
	{
		static std::multimap<std::string, core_sysregion*> x;
		return x;
	}

	std::set<core_type*>& types()
	{
		static std::set<core_type*> x;
		return x;
	}

	bool compare_coretype(core_type* a, core_type* b)
	{
		return (a->get_id() < b->get_id());
	}

	bool compare_regions(core_region* a, core_region* b)
	{
		return (a->get_handle() < b->get_handle());
	}
}

bool interface_action::is_toggle() const
{
	for(auto i : params)
		if(!strcmp(i.model, "toggle"))
			return true;
	return false;
}

core_region::core_region(const core_region_params& params)
{
	iname = params.iname;
	hname = params.hname;
	multi = params.multi;
	handle = params.handle;
	priority = params.priority;
	magic[0] = params.framemagic[0];
	magic[1] = params.framemagic[1];
	magic[2] = 1000 * params.framemagic[0] / params.framemagic[1];
	magic[3] = 1000 * params.framemagic[0] % params.framemagic[1];
	compatible = params.compatible_runs;
}


bool core_region::compatible_with(core_region& run)
{
	for(auto i : compatible)
		if(i == run.handle)
			return true;
	return false;
}

bool core_region::is_multi()
{
	return multi;
}

const std::string& core_region::get_iname()
{
	return iname;
}

const std::string& core_region::get_hname()
{
	return hname;
}

unsigned core_region::get_priority()
{
	return priority;
}

unsigned core_region:: get_handle()
{
	return handle;
}

void core_region::fill_framerate_magic(uint64_t* _magic)
{
	for(size_t i = 0; i < 4; i++)
		_magic[i] = magic[i];
}

double core_region::approx_framerate()
{
	return (double)magic[1] / magic[0];
}

core_romimage_info::core_romimage_info(const core_romimage_info_params& params)
{
	iname = params.iname;
	hname = params.hname;
	mandatory = params.mandatory;
	pass_mode = params.pass_mode;
	headersize = params.headersize;
	if(params.extensions) {
		std::string tmp = params.extensions;
		for(auto& ext : token_iterator_foreach(tmp, {";"}))
			extensions.insert(ext);
	}
}

size_t core_romimage_info::get_headnersize(size_t imagesize)
{
	if(headersize && imagesize % (2 * headersize) == headersize)
		return headersize;
	return 0;
}

core_type::core_type(const core_type_params& params)
	: settings(params.settings)
{
	iname = params.iname;
	hname = params.hname;
	sysname = params.sysname;
	id = params.id;
	core = params.core;
	if(params.bios)
		biosname = params.bios;
	for(auto i : params.regions)
		regions.push_back(i);
	imageinfo = params.images.get();
	types().insert(this);
}

core_type::~core_type() throw()
{
	types().erase(this);
}

unsigned core_type::get_id()
{
	return id;
}

const std::string& core_type::get_iname()
{
	return iname;
}

const std::string& core_type::get_hname()
{
	return hname;
}

unsigned core_type::get_image_count()
{
	return imageinfo.size();
}

core_setting_group& core_type::get_settings()
{
	return settings;
}

core_romimage_info core_type::get_image_info(unsigned index)
{
	if(index >= imageinfo.size())
		throw std::runtime_error("Requested invalid image index");
	return imageinfo[index];
}

std::list<core_type*> core_type::get_core_types()
{
	std::list<core_type*> ret;
	for(auto i : types())
		ret.push_back(i);
	ret.sort(compare_coretype);
	return ret;
}

std::list<core_region*> core_type::get_regions()
{
	std::list<core_region*> ret = regions;
	ret.sort(compare_regions);
	return ret;
}

core_region& core_type::get_preferred_region()
{
	core_region* p = NULL;
	unsigned cutoff = 0;
	for(auto i : regions) {
		unsigned pri = i->get_priority();
		if(pri >= cutoff) {
			cutoff = max(pri + 1, pri);
			p = i;
		}
	}
	return *p;
}

bool core_type::load(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
	uint64_t rtc_subsec)
{
	return (t_load_rom(images, settings, rtc_sec, rtc_subsec) >= 0);
}

core_sysregion& core_type::combine_region(core_region& reg)
{
	for(auto i : sysregions())
		if(&(i.second->get_type()) == this && &(i.second->get_region()) == &reg)
			return *(i.second);
	throw std::runtime_error("Invalid region for system type");
}

std::list<std::string> core_type::get_extensions()
{
	static std::list<std::string> empty;
	unsigned base = (biosname != "") ? 1 : 0;
	if(imageinfo.size() <= base)
		return empty;
	auto s = imageinfo[base].extensions;
	std::list<std::string> ret;
	for(auto i : s)
		ret.push_back(i);
	return ret;
}

std::string core_type::get_biosname()
{
	return biosname;
}

bool core_type::is_known_extension(const std::string& ext)
{
	std::string _ext = ext;
	std::transform(_ext.begin(), _ext.end(), _ext.begin(), ::tolower);
	for(auto i : get_extensions())
		if(i == _ext)
			return true;
	return false;
}

controller_set core_type::controllerconfig(std::map<std::string, std::string>& settings)
{
	return t_controllerconfig(settings);
}

std::pair<uint64_t, uint64_t> core_core::get_bus_map()
{
	return c_get_bus_map();
}

double core_core::get_PAR()
{
	return c_get_PAR();
}

std::list<core_vma_info> core_core::vma_list()
{
	return c_vma_list();
}

std::set<std::string> core_core::srams()
{
	return c_srams();
}

bool core_core::isnull()
{
	return c_isnull();
}

std::pair<unsigned, unsigned> core_core::lightgun_scale()
{
	return c_lightgun_scale();
}

std::pair<unsigned, unsigned> core_core::c_lightgun_scale()
{
	return std::make_pair(0, 0);
}

void core_core::debug_reset()
{
	return c_debug_reset();
}

bool core_core::c_isnull()
{
	return false;
}

core_sysregion::core_sysregion(const std::string& _name, core_type& _type, core_region& _region)
	: name(_name), type(_type), region(_region)
{
	sysregions().insert(std::make_pair(_name, this));
}

core_sysregion::~core_sysregion() throw()
{
	for(auto i = sysregions().begin(); i != sysregions().end(); i++)
		if(i->second == this) {
			sysregions().erase(i);
			break;
		}
}

core_sysregion& core_type::lookup_sysregion(const std::string& sysreg)
{
	for(auto i : sysregions())
		if(i.first == sysreg && &i.second->get_type() == this)
			return *i.second;
	throw std::runtime_error("Bad system-region type");
}

const std::string& core_sysregion::get_name()
{
	return name;
}

core_region& core_sysregion::get_region()
{
	return region;
}

core_type& core_sysregion::get_type()
{
	return type;
}

void core_sysregion::fill_framerate_magic(uint64_t* magic)
{
	region.fill_framerate_magic(magic);
}

core_core::core_core(std::initializer_list<port_type*> ports, std::initializer_list<interface_action> x_actions)
{
	for(auto i : ports)
		port_types.push_back(i);
	for(auto i : x_actions)
		actions[i._symbol] = i;

	hidden = false;
	uninitialized_cores_set().insert(this);
	all_cores_set().insert(this);
	new_core_flag = true;
}

core_core::core_core(std::vector<port_type*> ports, std::vector<interface_action> x_actions)
{
	for(auto i : ports)
		port_types.push_back(i);
	for(auto i : x_actions)
		actions[i._symbol] = i;

	hidden = false;
	uninitialized_cores_set().insert(this);
	all_cores_set().insert(this);
	new_core_flag = true;
}

core_core::~core_core() throw()
{
	all_cores().erase(this);
}

void core_core::initialize_new_cores()
{
	for(auto i : uninitialized_cores_set())
		i->install_handler();
	uninitialized_cores_set().clear();
}

std::string core_core::get_core_shortname()
{
	return c_get_core_shortname();
}

bool core_core::set_region(core_region& region)
{
	return c_set_region(region);
}

std::pair<uint32_t, uint32_t> core_core::get_video_rate()
{
	return c_video_rate();
}

std::pair<uint32_t, uint32_t> core_core::get_audio_rate()
{
	return c_audio_rate();
}

std::string core_core::get_core_identifier()
{
	return c_core_identifier();
}

std::set<core_core*> core_core::all_cores()
{
	return all_cores_set();
}

void core_core::install_all_handlers()
{
	install_handlers_automatically = true;
	for(auto i : all_cores_set())
		i->install_handler();
}

void core_core::uninstall_all_handlers()
{
	install_handlers_automatically = false;
	for(auto i : all_cores_set())
		i->uninstall_handler();
}

std::map<std::string, std::vector<char>> core_core::save_sram() throw(std::bad_alloc)
{
	return c_save_sram();
}

void core_core::load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
{
	c_load_sram(sram);
}

void core_core::serialize(std::vector<char>& out)
{
	c_serialize(out);
}

void core_core::unserialize(const char* in, size_t insize)
{
	c_unserialize(in, insize);
}

core_region& core_core::get_region()
{
	return c_get_region();
}

void core_core::power()
{
	c_power();
}

void core_core::unload_cartridge()
{
	c_unload_cartridge();
}

std::pair<uint32_t, uint32_t> core_core::get_scale_factors(uint32_t width, uint32_t height)
{
	return c_get_scale_factors(width, height);
}

void core_core::install_handler()
{
	c_install_handler();
}

void core_core::uninstall_handler()
{
	c_uninstall_handler();
}

void core_core::emulate()
{
	c_emulate();
}

void core_core::runtosave()
{
	c_runtosave();
}

bool core_core::get_pflag()
{
	return c_get_pflag();
}

void core_core::set_pflag(bool pflag)
{
	return c_set_pflag(pflag);
}

framebuffer::raw& core_core::draw_cover()
{
	return c_draw_cover();
}

void core_core::pre_emulate_frame(controller_frame& cf)
{
	c_pre_emulate_frame(cf);
}

void core_core::execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
{
	return c_execute_action(id, p);
}

const struct interface_device_reg* core_core::get_registers()
{
	return c_get_registers();
}

unsigned core_core::action_flags(unsigned id)
{
	return c_action_flags(id);
}

std::set<const interface_action*> core_core::get_actions()
{
	threads::alock h(actions_lock);
	std::set<const interface_action*> r;
	for(auto& i : actions)
		r.insert(&i.second);
	return r;
}

int core_core::reset_action(bool hard)
{
	return c_reset_action(hard);
}

void core_core::set_debug_flags(uint64_t addr, unsigned flags_set, unsigned flags_clear)
{
	return c_set_debug_flags(addr, flags_set, flags_clear);
}

void core_core::set_cheat(uint64_t addr, uint64_t value, bool set)
{
	return c_set_cheat(addr, value, set);
}

std::vector<std::string> core_core::get_trace_cpus()
{
	return c_get_trace_cpus();
}

emucore_callbacks::~emucore_callbacks() throw()
{
}

core_romimage_info_collection::core_romimage_info_collection()
{
}

core_romimage_info_collection::core_romimage_info_collection(std::initializer_list<core_romimage_info_params> idata)
{
	for(auto i : idata)
		data.push_back(core_romimage_info(i));
}

core_romimage_info_collection::core_romimage_info_collection(std::vector<core_romimage_info_params> idata)
{
	for(auto i : idata)
		data.push_back(core_romimage_info(i));
}

std::set<core_sysregion*> core_sysregion::find_matching(const std::string& name)
{
	std::set<core_sysregion*> ret;
	auto u = sysregions().upper_bound(name);
	for(auto i = sysregions().lower_bound(name); i != u; ++i)
		ret.insert(i->second);
	return ret;
}

void register_sysregion_mapping(std::string from, std::string to)
{
	sysreg_mapping()[from] = to;
}

std::string lookup_sysregion_mapping(std::string from)
{
	if(sysreg_mapping().count(from))
		return sysreg_mapping()[from];
	else
		return "";
}

struct emucore_callbacks* ecore_callbacks;

bool new_core_flag = false;
uint32_t magic_flags = 0;
