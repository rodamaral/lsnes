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
