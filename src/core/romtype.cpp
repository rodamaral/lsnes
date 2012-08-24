#include "core/romtype.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <list>

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

	std::map<std::string, core_region*>& regionss()
	{
		static std::map<std::string, core_region*> x;
		return x;
	}

	std::list<std::pair<core_type*, core_region*>>& reglist1()
	{
		static std::list<std::pair<core_type*, core_region*>> x;
		return x;
	}

	std::list<std::pair<core_type*, std::pair<core_romimage_info*, unsigned>>>& reglist2()
	{
		static std::list<std::pair<core_type*, std::pair<core_romimage_info*, unsigned>>> x;
		return x;
	}

	void process_registrations()
	{
		auto& l = reglist1();
		for(auto i = l.begin(); i != l.end();) {
			auto& m = types();
			bool done = false;
			for(auto j : m)
				if(j.second == i->first) {
					auto iold = i;
					i->first->add_region(*i->second);
					i++;
					l.erase(iold);
					done = true;
				}
			if(!done)
				i++;
		}
		auto& l2 = reglist2();
		for(auto i = l2.begin(); i != l2.end();) {
			auto& m = types();
			bool done = false;
			for(auto j : m)
				if(j.second == i->first) {
					auto iold = i;
					i->first->add_romimage(*i->second.first, i->second.second);
					i++;
					l2.erase(iold);
					done = true;
				}
			if(!done)
				i++;
		}
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

core_region::core_region(const std::string& _iname, const std::string& _hname, unsigned _priority, unsigned _handle,
	bool _multi, uint64_t* _fmagic, int (*_compatible)(unsigned rom, unsigned run))
	: iname(_iname), hname(_hname), priority(_priority), handle(_handle), compatible(_compatible),
	multi(_multi)
{
	for(size_t i = 0; i < 4; i++)
		magic[i] = _fmagic[i];
	regionss()[_iname] = this;
}

bool core_region::compatible_with(core_region& run)
{
	return (compatible(handle, run.handle) != 0);
}

core_region& core_region::lookup(const std::string& name)
{
	if(regionss().count(name))
		return *(regionss()[name]);
	else
		throw std::runtime_error("Bad region");
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

unsigned core_type::get_id()
{
	return id;
}

void core_type::add_region(core_region& reg)
{
	regions.push_back(&reg);
}

void core_type::add_romimage(core_romimage_info& info, unsigned index)
{
	if(imageinfo.size() <= index)
		imageinfo.resize(index + 1);
	imageinfo[index] = &info;
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

core_romimage_info::core_romimage_info(const std::string& _iname, const std::string& _hname, unsigned _mandatory,
	unsigned (*_headersize)(size_t imagesize))
	: iname(_iname), hname(_hname), headersize(_headersize), mandatory(_mandatory)
{
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

bool core_type::load(core_romimage* images, uint64_t rtc_sec, uint64_t rtc_subsec)
{
	return (loadimg(images, rtc_sec, rtc_subsec) >= 0);
}

core_sysregion& core_type::combine_region(core_region& reg)
{
	for(auto i : sysregions())
		if(&(i.second->get_type()) == this && &(i.second->get_region()) == &reg)
			return *(i.second);
	throw std::runtime_error("Invalid region for system type");
}

core_type::core_type(const std::string& _iname, const std::string& _hname, unsigned _id, 
	int (*_load)(core_romimage* images, uint64_t rtc_sec, uint64_t rtc_subsec), const std::string& _extensions)
	: iname(_iname), hname(_hname), loadimg(_load), id(_id)
{
	types()[iname] = this;
	std::string ext;
	std::string ext2 = _extensions;
	while(extract_token(ext2, ext, ";") >= 0)
		extensions.push_back(ext);
	process_registrations();
}

const std::list<std::string>& core_type::get_extensions()
{
	return extensions;
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

core_type_region_bind::core_type_region_bind(core_type& type, core_region& region)
{
	reglist1().push_back(std::make_pair(&type, &region));
	process_registrations();
}

core_type_image_bind::core_type_image_bind(core_type& type, core_romimage_info& imageinfo, unsigned index)
{
	reglist2().push_back(std::make_pair(&type, std::make_pair(&imageinfo, index)));
	process_registrations();
}
