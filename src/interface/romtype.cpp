#include "interface/romtype.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <list>
#include <limits>

namespace
{
	std::map<std::string, core_sysregion*>& sysregions()
	{
		static std::map<std::string, core_sysregion*> x;
		return x;
	}

	std::map<std::string, core_type*>& types()
	{
		static std::map<std::string, core_type*> x;
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

core_region::core_region(const core_region_params& params)
{
	iname = params.iname;
	hname = params.hname;
	multi = params.multi;
	handle = params.handle;
	priority = params.priority;
	for(size_t i = 0; i < 4; i++)
		magic[i] = params.framemagic[4];
	for(size_t i = 0;; i++)
		if(params.compatible_runs[i] == std::numeric_limits<unsigned>::max())
			break;
		else
			compatible.push_back(params.compatible_runs[i]);
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

core_type::core_type(core_type_params& params)
{
	iname = params.iname;
	hname = params.hname;
	id = params.id;
	reset_support = params.reset_support;
	loadimg = params.load_rom;
	_controllerconfig = params.controllerconfig;
	_set_region = params.set_region;
	_audio_rate = params.audio_rate;
	_video_rate = params.video_rate;
	_snes_rate = params.snes_rate;
	settings = params.settings;
	if(params.bios)
		biosname = params.bios;
	for(size_t i = 0; params.regions[i]; i++)
		regions.push_back(params.regions[i]);
	for(size_t i = 0; params.images[i]; i++)
		imageinfo.push_back(params.images[i]);
	if(params.extensions) {
		std::string tmp = params.extensions;
		while(tmp != "") {
			std::string ext;
			extract_token(tmp, ext, ";");
			extensions.push_back(ext);
		}
	}
	types()[iname] = this;
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
		ret.push_back(i.second);
	ret.sort(compare_coretype);
	return ret;
}

unsigned core_type::get_reset_support()
{
	return reset_support;
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

bool core_type::set_region(core_region& region)
{
	return _set_region(region);
}

std::pair<uint32_t, uint32_t> core_type::get_video_rate()
{
	return _video_rate();
}

std::pair<uint32_t, uint32_t> core_type::get_audio_rate()
{
	return _audio_rate();
}

std::pair<uint32_t, uint32_t> core_type::get_snes_rate()
{
	if(_snes_rate)
		return _snes_rate();
	else
		return std::make_pair(0, 0);
}


core_sysregion::core_sysregion(const std::string& _name, core_type& _type, core_region& _region)
	: name(_name), type(_type), region(_region)
{
	sysregions()[_name] = this;
}

core_sysregion& core_sysregion::lookup(const std::string& _name)
{
	if(sysregions().count(_name))
		return *(sysregions()[_name]);
	else
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
