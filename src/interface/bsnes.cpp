#include "core/bsnes.hpp"
#include "interface/core.hpp"
#include <sstream>

/**
 * Number clocks per field/frame on NTSC/PAL
 */
#define DURATION_NTSC_FRAME 357366
#define DURATION_NTSC_FIELD 357368
#define DURATION_PAL_FRAME 425568
#define DURATION_PAL_FIELD 425568

namespace
{
	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	};
}

std::string emucore_get_version()
{
	std::ostringstream x;
	x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
	return x.str();
}

std::pair<uint32_t, uint32_t> emucore_get_video_rate(bool interlace)
{
	uint32_t d;
	if(SNES::system.region() == SNES::System::Region::PAL)
		d = interlace ? DURATION_PAL_FIELD : DURATION_PAL_FRAME;
	else
		d = interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
	return std::make_pair(SNES::system.cpu_frequency(), d);
}

std::pair<uint32_t, uint32_t> emucore_get_audio_rate()
{
	return std::make_pair(SNES::system.apu_frequency(), 768);
}

void emucore_basic_init()
{
	static bool done = false;
	if(done)
		return;
	my_interfaced* intrf = new my_interfaced;
	SNES::interface = intrf;
	done = true;
}

namespace
{
	//Wunderbar... bsnes v087 goes to change the sram names...
	std::string remap_sram_name(const std::string& name, SNES::Cartridge::Slot slotname)
	{
		std::string iname = name;
		if(iname == "bsx.ram")
			iname = ".bss";
		if(iname == "bsx.psram")
			iname = ".bsp";
		if(iname == "program.rtc")
			iname = ".rtc";
		if(iname == "upd96050.ram")
			iname = ".dsp";
		if(iname == "program.ram")
			iname = ".srm";
		if(slotname == SNES::Cartridge::Slot::SufamiTurboA)
			return "slota" + iname;
		if(slotname == SNES::Cartridge::Slot::SufamiTurboB)
			return "slotb" + iname;
		else
			return iname.substr(1);
	}

	struct bsnes_sram_slot : public sram_slot_structure
	{
		std::string name;
		size_t size;
		unsigned char* memory;

		bsnes_sram_slot(const nall::string& _id, SNES::Cartridge::Slot slotname, unsigned char* mem,
			size_t sramsize)
		{
			std::string id(_id, _id.length());
			name = remap_sram_name(id, slotname);
			memory = mem;
			size = sramsize;
		}

		std::string get_name()
		{
			return name;
		}

		void copy_to_core(const std::vector<char>& content)
		{
			memcpy(memory, &content[0], (content.size() < size) ? content.size() : size);
		}

		void copy_from_core(std::vector<char>& content)
		{
			content.resize(size);
			memcpy(&content[0], memory, size);
		}

		size_t get_size()
		{
			return size;
		}
	};

	std::vector<bsnes_sram_slot*> sram_slots;
}

size_t emucore_sram_slots()
{
	return sram_slots.size();
}

struct sram_slot_structure* emucore_sram_slot(size_t index)
{
	if(index >= sram_slots.size())
		return NULL;
	return sram_slots[index];
}

void emucore_refresh_cart()
{
	std::vector<bsnes_sram_slot*> new_sram_slots;
	size_t slots = SNES::cartridge.nvram.size();
	new_sram_slots.resize(slots);
	for(size_t i = 0; i < slots; i++)
		new_sram_slots[i] = NULL;

	try {
		for(unsigned i = 0; i < slots; i++) {
			SNES::Cartridge::NonVolatileRAM& s = SNES::cartridge.nvram[i];
			new_sram_slots[i] = new bsnes_sram_slot(s.id, s.slot, s.data, s.size);
		}
	} catch(...) {
		for(auto i : new_sram_slots)
			delete i;
		throw;
	}

	std::swap(sram_slots, new_sram_slots);
	for(auto i : new_sram_slots)
		delete i;
}
