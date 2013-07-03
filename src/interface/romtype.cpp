#include "core/misc.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/register-queue.hpp"
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

	std::set<core_core*>& all_cores_set()
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

	unsigned default_headersize(size_t imagesize)
	{
		return 0;
	}
}

interface_action::interface_action(struct core_core& _core, unsigned _id, const std::string& _title,
	const std::string& _sym, std::initializer_list<interface_action_param> p)
	: core(_core)
{
	id = _id;
	title = _title;
	symbol = _sym;
	for(auto i : p)
		params.push_back(i);
        register_queue<core_core::_param_register_proxy, interface_action>::do_register(core.param_register_proxy,
		symbol, *this);
}

interface_action::~interface_action()
{
        register_queue<core_core::_param_register_proxy, interface_action>::do_unregister(core.param_register_proxy,
		symbol);
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
	magic[2] = 1000000000 * params.framemagic[0] / params.framemagic[1];
	magic[3] = 1000000000 * params.framemagic[0] % params.framemagic[1];
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
}

size_t core_romimage_info::get_headnersize(size_t imagesize)
{
	if(headersize && imagesize % (2 * headersize) == headersize)
		return headersize;
	return 0;
}

core_type::core_type(const core_type_params& params)
{
	iname = params.iname;
	hname = params.hname;
	sysname = params.sysname;
	id = params.id;
	loadimg = params.load_rom;
	_controllerconfig = params.controllerconfig;
	_get_bus_map = params.get_bus_map;
	_vma_list = params.vma_list;
	_srams = params.srams;
	core = params.core;
	settings = params.settings;
	if(params.bios)
		biosname = params.bios;
	for(auto i : params.regions)
		regions.push_back(i);
	for(auto i : params.images)
		imageinfo.push_back(i);
	if(params.extensions) {
		std::string tmp = params.extensions;
		while(tmp != "") {
			std::string ext;
			extract_token(tmp, ext, ";");
			extensions.push_back(ext);
		}
	}
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
	return *settings;
}

core_romimage_info core_type::get_image_info(unsigned index)
{
	if(index >= imageinfo.size())
		throw std::runtime_error("Requested invalid image index");
	return *imageinfo[index];
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
	return (loadimg(images, settings, rtc_sec, rtc_subsec) >= 0);
}

core_sysregion& core_type::combine_region(core_region& reg)
{
	for(auto i : sysregions())
		if(&(i.second->get_type()) == this && &(i.second->get_region()) == &reg)
			return *(i.second);
	throw std::runtime_error("Invalid region for system type");
}

const std::list<std::string>& core_type::get_extensions()
{
	return extensions;
}

std::string core_type::get_biosname()
{
	return biosname;
}

bool core_type::is_known_extension(const std::string& ext)
{
	std::string _ext = ext;
	std::transform(_ext.begin(), _ext.end(), _ext.begin(), ::tolower);
	for(auto i : extensions)
		if(i == _ext)
			return true;
	return false;
}

controller_set core_type::controllerconfig(std::map<std::string, std::string>& settings)
{
	return _controllerconfig(settings);
}

std::pair<uint64_t, uint64_t> core_type::get_bus_map()
{
	return _get_bus_map();
}

std::list<core_vma_info> core_type::vma_list()
{
	return _vma_list();
}

std::set<std::string> core_type::srams()
{
	return _srams();
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

core_core::core_core(const core_core_params& params)
	: param_register_proxy(*this)
{
	_core_identifier = params.core_identifier;
	_set_region = params.set_region;
	_video_rate = params.video_rate;
	_audio_rate = params.audio_rate;
	_save_sram = params.save_sram;
	_load_sram = params.load_sram;
	_serialize = params.serialize;
	_unserialize = params.unserialize;
	_get_region = params.get_region;
	_power = params.power;
	_unload_cartridge = params.unload_cartridge;
	_get_scale_factors = params.get_scale_factors;
	_install_handler = params.install_handler;
	_uninstall_handler = params.uninstall_handler;
	_emulate = params.emulate;
	_runtosave = params.runtosave;
	_get_pflag = params.get_pflag;
	_set_pflag = params.set_pflag;
	port_types = params.port_types;
	_draw_cover = params.draw_cover;
	_get_core_shortname = params.get_core_shortname;
	_pre_emulate_frame = params.pre_emulate_frame;
	_execute_action = params.execute_action;
	_get_registers = params.get_registers;
	hidden = false;
	all_cores_set().insert(this);
	if(install_handlers_automatically)
		install_handler();
	new_core_flag = true;
	register_queue<core_core::_param_register_proxy, interface_action>::do_ready(param_register_proxy, true);
	if(!in_global_ctors())
		messages << "Loaded core: " << _core_identifier() << std::endl;
}

core_core::~core_core() throw()
{
	register_queue<core_core::_param_register_proxy, interface_action>::do_ready(param_register_proxy, false);
	all_cores().erase(this);
}

std::string core_core::get_core_shortname()
{
	return _get_core_shortname();
}

bool core_core::set_region(core_region& region)
{
	return _set_region(region);
}

std::pair<uint32_t, uint32_t> core_core::get_video_rate()
{
	return _video_rate();
}

std::pair<uint32_t, uint32_t> core_core::get_audio_rate()
{
	return _audio_rate();
}

std::string core_core::get_core_identifier()
{
	return _core_identifier();
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
	return _save_sram();
}

void core_core::load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
{
	_load_sram(sram);
}

void core_core::serialize(std::vector<char>& out)
{
	_serialize(out);
}

void core_core::unserialize(const char* in, size_t insize)
{
	_unserialize(in, insize);
}

core_region& core_core::get_region()
{
	return _get_region();
}

void core_core::power()
{
	_power();
}

void core_core::unload_cartridge()
{
	_unload_cartridge();
}

std::pair<uint32_t, uint32_t> core_core::get_scale_factors(uint32_t width, uint32_t height)
{
	return _get_scale_factors(width, height);
}

void core_core::install_handler()
{
	_install_handler();
}

void core_core::uninstall_handler()
{
	_uninstall_handler();
}

void core_core::emulate()
{
	_emulate();
}

void core_core::runtosave()
{
	_runtosave();
}

bool core_core::get_pflag()
{
	return _get_pflag();
}

void core_core::set_pflag(bool pflag)
{
	return _set_pflag(pflag);
}

framebuffer_raw& core_core::draw_cover()
{
	return _draw_cover();
}

void core_core::pre_emulate_frame(controller_frame& cf)
{
	_pre_emulate_frame(cf);
}

void core_core::execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
{
	return _execute_action(id, p);
}

const struct interface_device_reg* core_core::get_registers()
{
	return _get_registers();
}

void core_core::do_register_action(const std::string& key, interface_action& act)
{
	umutex_class h(actions_lock);
	actions[key] = &act;
}

void core_core::do_unregister_action(const std::string& key)
{
	umutex_class h(actions_lock);
	actions.erase(key);
}

std::set<const interface_action*> core_core::get_actions()
{
	umutex_class h(actions_lock);
	std::set<const interface_action*> r;
	for(auto i : actions)
		r.insert(i.second);
	return r;
}

emucore_callbacks::~emucore_callbacks() throw()
{
}

struct emucore_callbacks* ecore_callbacks;

bool new_core_flag = false;
uint32_t magic_flags = 0;