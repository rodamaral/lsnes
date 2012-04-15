#include "interface/core.hpp"

sram_slot_structure::~sram_slot_structure()
{
}

vma_structure::vma_structure(const std::string& _name, uint64_t _base, uint64_t _size, endian _rendian, bool _readonly)
{
	name = _name;
	base = _base;
	size = _size;
	rendian = _rendian;
	readonly = _readonly;
}

vma_structure::~vma_structure()
{
}

region_info_structure::~region_info_structure()
{
}

sysregion_info_structure::~sysregion_info_structure()
{
}

rom_info_structure::~rom_info_structure()
{
}

systype_info_structure::~systype_info_structure()
{
}

struct region_info_structure* emucore_region_for_sysregion(const std::string& sysregion)
{
	for(size_t i = 0; i < emucore_systype_slots(); i++) {
		auto j = emucore_systype_slot(i);
		for(size_t k = 0; k < j->region_slots(); k++) {
			auto l = j->region_slot(k);
			auto n = l->get_iname();
			auto x = j->get_sysregion(n);
			if(x && x->get_iname() == sysregion)
				return l;
		}
	}
	return NULL;
}

struct systype_info_structure* emucore_systype_for_sysregion(const std::string& sysregion)
{
	for(size_t i = 0; i < emucore_systype_slots(); i++) {
		auto j = emucore_systype_slot(i);
		for(size_t k = 0; k < j->region_slots(); k++) {
			auto l = j->region_slot(k);
			auto n = l->get_iname();
			auto x = j->get_sysregion(n);
			if(x && x->get_iname() == sysregion)
				return j;
		}
	}
	return NULL;
}

struct sysregion_info_structure* emucore_sysregion_for_sysregion(const std::string& sysregion)
{
	for(size_t i = 0; i < emucore_systype_slots(); i++) {
		auto j = emucore_systype_slot(i);
		for(size_t k = 0; k < j->region_slots(); k++) {
			auto l = j->region_slot(k);
			auto n = l->get_iname();
			auto x = j->get_sysregion(n);
			if(x && x->get_iname() == sysregion)
				return x;
		}
	}
	return NULL;
}
