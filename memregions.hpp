#pragma once

//Gambatte special memory classes.
#define GAMBATTE_MEM_BUS 0
#define GAMBATTE_MEM_WRAM 1
#define GAMBATTE_MEM_HRAM 2
#define GAMBATTE_MEM_ROM 3
#define GAMBATTE_MEM_SRAM 4
#define GAMBATTE_MEM_VRAM 5	/* Does not really exist. */

#define MAP_BASE_SNES_BUS 0x01000000
#define MAP_BASE_SNES_WRAM 0x007E0000
#define MAP_BASE_SNES_SRAM 0x10000000
#define MAP_BASE_SNES_APURAM 0x00000000
#define MAP_BASE_SNES_VRAM 0x00010000
#define MAP_BASE_SNES_OAM 0x00020000
#define MAP_BASE_SNES_CGRAM 0x00021000
#define MAP_BASE_SNES_ROM 0x80000000
#define MAP_BASE_GB_BUS 0x02000000
#define MAP_BASE_GB_ROM 0x90000000
#define MAP_BASE_GB_WRAM 0x00030000
#define MAP_BASE_GB_SRAM 0x20000000
#define MAP_BASE_GB_HRAM 0x00038000
#define MAP_BASE_GB_VRAM 0x00040000

//Some are by space allocated, not by real size.
#define MAP_SIZE_SNES_BUS (1 << 24)
#define MAP_SIZE_SNES_WRAM (1 << 17)
#define MAP_SIZE_SNES_SRAM (1 << 28)
#define MAP_SIZE_SNES_APURAM (1 << 16)
#define MAP_SIZE_SNES_VRAM (1 << 16)
#define MAP_SIZE_SNES_OAM ((1 << 9) + (1 << 5))
#define MAP_SIZE_SNES_CGRAM (1 << 9)
#define MAP_SIZE_SNES_ROM (1 << 28)
#define MAP_SIZE_GB_BUS (1 << 16)
#define MAP_SIZE_GB_ROM (1 << 28)
#define MAP_SIZE_GB_WRAM (1 << 15)
#define MAP_SIZE_GB_SRAM (1 << 28)
#define MAP_SIZE_GB_HRAM (1 << 9)
#define MAP_SIZE_GB_VRAM (1 << 15)

#define MAP_KIND_SNES_BUS 255
#define MAP_KIND_SNES_WRAM 3
#define MAP_KIND_SNES_SRAM 2
#define MAP_KIND_SNES_APURAM 16
#define MAP_KIND_SNES_VRAM 13
#define MAP_KIND_SNES_OAM 14
#define MAP_KIND_SNES_CGRAM 15
#define MAP_KIND_SNES_ROM 1
#define MAP_KIND_GB_BUS (100 + GAMBATTE_MEM_BUS)
#define MAP_KIND_GB_ROM (100 + GAMBATTE_MEM_ROM)
#define MAP_KIND_GB_WRAM (100 + GAMBATTE_MEM_WRAM)
#define MAP_KIND_GB_SRAM (100 + GAMBATTE_MEM_SRAM)
#define MAP_KIND_GB_HRAM (100 + GAMBATTE_MEM_HRAM)
#define MAP_KIND_GB_VRAM (100 + GAMBATTE_MEM_VRAM)

//Special kinds all and none.
#define MAP_KIND_ALL -1
#define MAP_KIND_NONE -2

//Address classes.
#define ADDR_CLASS_ALL -2
#define ADDR_CLASS_SNES 1
#define ADDR_CLASS_GB 2
#define ADDR_CLASS_NONE -1

//Global address (kind all).
#define ADDRESS_GLOBAL 0xFFFFFFFFFFFFFFFFULL

int classify_address_kind(unsigned kind)
{
	switch(kind) {
	case MAP_KIND_ALL:
		return ADDR_CLASS_ALL;
	case MAP_KIND_GB_BUS:
	case MAP_KIND_GB_ROM:
	case MAP_KIND_GB_WRAM:
	case MAP_KIND_GB_SRAM:
	case MAP_KIND_GB_HRAM:
	case MAP_KIND_GB_VRAM:
		return ADDR_CLASS_GB;
	case MAP_KIND_SNES_BUS:
	case MAP_KIND_SNES_WRAM:
	case MAP_KIND_SNES_SRAM:
	case MAP_KIND_SNES_APURAM:
	case MAP_KIND_SNES_VRAM:
	case MAP_KIND_SNES_OAM:
	case MAP_KIND_SNES_CGRAM:
	case MAP_KIND_SNES_ROM:
		return ADDR_CLASS_SNES;
	default:
		return ADDR_CLASS_NONE;
	}
}

uint8_t snes_bus_read(uint64_t offset)
{
	disable_breakpoints = true;
	uint8_t val = SNES::bus.read(offset, false);
	disable_breakpoints = false;
	return val;
}

void snes_bus_write(uint64_t offset, uint8_t data)
{
	disable_breakpoints = true;
	SNES::bus.write(offset, data);
	disable_breakpoints = false;
}

uint8_t gb_bus_read(uint64_t offset)
{
	disable_breakpoints = true;
	uint8_t val = gb_instance->bus_read(offset);
	disable_breakpoints = false;
	return val;
}

void gb_bus_write(uint64_t offset, uint8_t data)
{
	disable_breakpoints = true;
	gb_instance->bus_write(offset, data);
	disable_breakpoints = false;
}

lsnes_core_get_vma_list_vma vma_BUS = {
	"BUS", MAP_BASE_SNES_BUS, MAP_SIZE_SNES_BUS, -1, LSNES_CORE_VMA_SPECIAL, NULL, snes_bus_read, snes_bus_write
};

lsnes_core_get_vma_list_vma vma_WRAM = {
	"WRAM", MAP_BASE_SNES_WRAM, MAP_SIZE_SNES_WRAM, -1, LSNES_CORE_VMA_VOLATILE, (unsigned char*)SNES::cpu.wram,
	NULL, NULL
};

lsnes_core_get_vma_list_vma vma_SRAM = {
	//Note: The size and data gets rewritten
	"SRAM", MAP_BASE_SNES_SRAM, MAP_SIZE_SNES_SRAM, -1, 0, NULL, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_APURAM = {
	"APURAM", MAP_BASE_SNES_APURAM, MAP_SIZE_SNES_APURAM, -1, LSNES_CORE_VMA_VOLATILE,
	(unsigned char*)SNES::smp.apuram, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_VRAM = {
	"VRAM", MAP_BASE_SNES_VRAM, MAP_SIZE_SNES_VRAM, -1, LSNES_CORE_VMA_VOLATILE, (unsigned char*)SNES::ppu.vram,
	NULL, NULL
};

lsnes_core_get_vma_list_vma vma_OAM = {
	"OAM", MAP_BASE_SNES_OAM, MAP_SIZE_SNES_OAM, -1, LSNES_CORE_VMA_VOLATILE, (unsigned char*)SNES::ppu.oam, NULL,
	NULL
};

lsnes_core_get_vma_list_vma vma_CGRAM = {
	"CGRAM", MAP_BASE_SNES_CGRAM, MAP_SIZE_SNES_CGRAM, -1, LSNES_CORE_VMA_VOLATILE,
	(unsigned char*)SNES::ppu.cgram, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_ROM = {
	//Note: The size and data gets rewritten
	"ROM", MAP_BASE_SNES_ROM, MAP_SIZE_SNES_ROM, -1, 0, NULL, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_GBBUS = {
	"GBBUS", MAP_BASE_GB_BUS, MAP_SIZE_GB_BUS, -1, LSNES_CORE_VMA_SPECIAL, NULL, gb_bus_read, gb_bus_write
};

lsnes_core_get_vma_list_vma vma_GBROM = {
	//Note: The size and data gets rewritten
	"GBROM", MAP_BASE_GB_ROM, MAP_SIZE_GB_ROM, -1, LSNES_CORE_VMA_READONLY, NULL, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_GBWRAM = {
	"GBWRAM", MAP_BASE_GB_WRAM, MAP_SIZE_GB_WRAM, -1, 0, (unsigned char*)GameBoy::cpu.wram, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_GBSRAM = {
	//Note: The size and data gets rewritten
	"GBSRAM", MAP_BASE_GB_SRAM, MAP_SIZE_GB_SRAM, -1, 0, NULL, NULL, NULL
};

lsnes_core_get_vma_list_vma vma_GBHRAM = {
	//Note: The size and data gets rewritten
	"GBHRAM", MAP_BASE_GB_HRAM, MAP_SIZE_GB_HRAM, -1, LSNES_CORE_VMA_SPECIAL, NULL,
	NULL, NULL
};

lsnes_core_get_vma_list_vma vma_GBVRAM = {
	//Note: The size and data gets rewritten
	"GBVRAM", MAP_BASE_GB_VRAM, MAP_SIZE_GB_VRAM, -1, 0, NULL, NULL, NULL
};

lsnes_core_get_vma_list_vma* fixup_vma(lsnes_core_get_vma_list_vma& vma, uint8_t* data, size_t size)
{
	vma.direct_map = data;
	vma.size = size;
	return &vma;
}

lsnes_core_get_vma_list_vma* fixup_vma(lsnes_core_get_vma_list_vma& vma, std::pair<uint8_t*, size_t> a)
{
	return fixup_vma(vma, a.first, a.second);
}

inline bool in_range(uint64_t addr, uint64_t base, uint64_t size)
{
	return (addr >= base && addr < base + size);
}

#define MEMREGION_RECOG(addr, X) \
	if(in_range((addr), MAP_BASE_##X , MAP_SIZE_##X )) \
		return std::make_pair( MAP_KIND_##X , (addr) - MAP_BASE_##X )

std::pair<int, uint64_t> recognize_address(uint64_t addr)
{
	if(addr == 0xFFFFFFFFFFFFFFFFULL)
		return std::make_pair(MAP_KIND_ALL, 0);
	MEMREGION_RECOG(addr, SNES_BUS);
	MEMREGION_RECOG(addr, SNES_WRAM);
	MEMREGION_RECOG(addr, SNES_SRAM);
	MEMREGION_RECOG(addr, SNES_APURAM);
	MEMREGION_RECOG(addr, SNES_VRAM);
	MEMREGION_RECOG(addr, SNES_OAM);
	MEMREGION_RECOG(addr, SNES_CGRAM);
	MEMREGION_RECOG(addr, SNES_ROM);
	MEMREGION_RECOG(addr, GB_BUS);
	MEMREGION_RECOG(addr, GB_ROM);
	MEMREGION_RECOG(addr, GB_VRAM);
	MEMREGION_RECOG(addr, GB_SRAM);
	MEMREGION_RECOG(addr, GB_HRAM);
	MEMREGION_RECOG(addr, GB_VRAM);
	return std::make_pair(MAP_KIND_NONE, 0);
}

#undef MEMREGION_RECOG

uint64_t gambate_get_address(unsigned clazz, unsigned offset)
{
	//Only translate GB regions here.
	switch(clazz) {
	case GAMBATTE_MEM_BUS:	return MAP_BASE_GB_BUS + offset;
	case GAMBATTE_MEM_WRAM:	return MAP_BASE_GB_WRAM + offset;
	case GAMBATTE_MEM_HRAM:	return MAP_BASE_GB_HRAM + offset;
	case GAMBATTE_MEM_ROM:	return MAP_BASE_GB_ROM + offset;
	case GAMBATTE_MEM_SRAM:	return MAP_BASE_GB_SRAM + offset;
	//FIXME: Hook this from gambatte side.
	case GAMBATTE_MEM_VRAM:	return MAP_BASE_GB_VRAM + offset;
	default:		return ADDRESS_GLOBAL;
	}
}

uint64_t bsnes_get_address(uint8_t clazz, unsigned offset)
{
	//Only translate SNES regions here.
	switch(clazz) {
	case MAP_KIND_SNES_BUS:		return ADDRESS_GLOBAL;	//No double signaling.
	case MAP_KIND_SNES_WRAM:	return MAP_BASE_SNES_WRAM + offset;
	case MAP_KIND_SNES_SRAM:	return MAP_BASE_SNES_SRAM + offset;
	case MAP_KIND_SNES_APURAM:	return MAP_BASE_SNES_APURAM + offset;
	case MAP_KIND_SNES_VRAM:	return MAP_BASE_SNES_VRAM + offset;
	case MAP_KIND_SNES_OAM:		return MAP_BASE_SNES_OAM + offset;
	case MAP_KIND_SNES_CGRAM:	return MAP_BASE_SNES_CGRAM + offset;
	case MAP_KIND_SNES_ROM:		return MAP_BASE_SNES_ROM + offset;
	default:			return ADDRESS_GLOBAL;
	}
}

void bsnes_debug_read(uint8_t clazz, unsigned offset, unsigned addr, uint8_t val, bool exec)
{
	if(disable_breakpoints) return;
	auto cb_routine = exec ? cb_memory_execute : cb_memory_read;
	uint8_t rval = exec ? 0 : val;
	uint64_t _addr = bsnes_get_address(clazz, offset);
	if(_addr != ADDRESS_GLOBAL)
		cb_routine(_addr, rval); 
	cb_routine(MAP_BASE_SNES_BUS + addr, rval);
}

void bsnes_debug_write(uint8_t clazz, unsigned offset, unsigned addr, uint8_t val)
{
	if(disable_breakpoints) return;
	uint64_t _addr = bsnes_get_address(clazz, offset);
	if(_addr != ADDRESS_GLOBAL)
		cb_memory_write(_addr, val);
	cb_memory_write(MAP_BASE_SNES_BUS + addr, val);
}


void gambatte_debug_read(unsigned clazz, unsigned offset, uint8_t value, bool exec)
{
	auto cb_handler = exec ? cb_memory_execute : cb_memory_read;
	auto rval = exec ? 0 : value;
	if(disable_breakpoints) return;
	uint64_t _addr = gambate_get_address(clazz, offset);
	if(_addr != ADDRESS_GLOBAL)
		cb_handler(_addr, rval);
}

void gambatte_debug_write(unsigned clazz, unsigned offset, uint8_t value)
{
	if(disable_breakpoints) return;
	uint64_t _addr = gambate_get_address(clazz, offset);
	if(_addr != ADDRESS_GLOBAL)
		cb_memory_write(_addr, value);
}
