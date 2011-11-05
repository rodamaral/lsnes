#include "callrom.hpp"
#include "common.hpp"

#define TNAME_SNES "SNES"
#define TNAME_BSX_NS "BS-X (non-slotted)"
#define TNAME_BSX_S "BS-X (slotted)"
#define TNAME_SUFAMITURBO "Sufami Turbo"
#define TNAME_SGB "SGB"
#define RNAME_AUTO "Autodetect"
#define RNAME_NTSC "NTSC"
#define RNAME_PAL "PAL"
#define WNAME_SNES_MAIN "ROM"
#define WNAME_SNES_MAIN_XML "ROM XML"
#define WNAME_BS_MAIN "BS-X BIOS"
#define WNAME_BS_MAIN_XML "BS-X BIOS XML"
#define WNAME_BS_SLOTA "BS FLASH"
#define WNAME_BS_SLOTA_XML "BS FLASH XML"
#define WNAME_ST_MAIN "ST BIOS"
#define WNAME_ST_MAIN_XML "ST BIOS XML"
#define WNAME_ST_SLOTA "SLOT A ROM"
#define WNAME_ST_SLOTA_XML "SLOT A XML"
#define WNAME_ST_SLOTB "SLOT B ROM"
#define WNAME_ST_SLOTB_XML "SLOT B XML"
#define WNAME_SGB_MAIN "SGB BIOS"
#define WNAME_SGB_MAIN_XML "SGB BIOS XML"
#define WNAME_SGB_SLOTA "DMG ROM"
#define WNAME_SGB_SLOTA_XML "BMG XML"


enum rom_type romtype_from_string(const std::string& str)
{
	if(str == TNAME_SNES)
		return ROMTYPE_SNES;
	if(str == TNAME_BSX_NS)
		return ROMTYPE_BSX;
	if(str == TNAME_BSX_S)
		return ROMTYPE_BSXSLOTTED;
	if(str == TNAME_SUFAMITURBO)
		return ROMTYPE_SUFAMITURBO;
	if(str == TNAME_SGB)
		return ROMTYPE_SGB;
	return ROMTYPE_NONE;
}

enum rom_type romtype_from_string(const wxString& str)
{
	return romtype_from_string(tostdstring(str));
}

wxString romname_wxs(enum rom_type rtype, unsigned index)
{
	return towxstring(romname_stds(rtype, index));
}

std::string romname_stds(enum rom_type rtype, unsigned index)
{
	switch(rtype) {
	case ROMTYPE_SNES:
		switch(index) {
		case 0:		return WNAME_SNES_MAIN;
		case 1:		return WNAME_SNES_MAIN_XML;
		};
		break;
	case ROMTYPE_BSX:
	case ROMTYPE_BSXSLOTTED:
		switch(index) {
		case 0:		return WNAME_BS_MAIN;
		case 1:		return WNAME_BS_MAIN_XML;
		case 2:		return WNAME_BS_SLOTA;
		case 3:		return WNAME_BS_SLOTA_XML;
		};
		break;
	case ROMTYPE_SUFAMITURBO:
		switch(index) {
		case 0:		return WNAME_ST_MAIN;
		case 1:		return WNAME_ST_MAIN_XML;
		case 2:		return WNAME_ST_SLOTA;
		case 3:		return WNAME_ST_SLOTA_XML;
		case 4:		return WNAME_ST_SLOTB;
		case 5:		return WNAME_ST_SLOTB_XML;
		};
		break;
	case ROMTYPE_SGB:
		switch(index) {
		case 0:		return WNAME_SGB_MAIN;
		case 1:		return WNAME_SGB_MAIN_XML;
		case 2:		return WNAME_SGB_SLOTA;
		case 3:		return WNAME_SGB_SLOTA_XML;
		};
		break;
	case ROMTYPE_NONE:
		if(index == 0)	return "dummy";
		break;
	}
	return "";
}

unsigned fill_rom_names(enum rom_type rtype, wxString* array)
{
	unsigned r = 0;
	for(unsigned i = 0; i < 6; i++) {
		wxString s = romname_wxs(rtype, i);
		if(s.Length())
			array[r++] = s;
	}
	return r;
}

unsigned romname_to_index(enum rom_type rtype, const wxString& name)
{
	std::string s = tostdstring(name);
	for(unsigned i = 0; i < 6; i++)
		if(romname_stds(rtype, i) == s)
			return i;
	return 6;

}

struct loaded_slot& get_rom_slot(struct loaded_rom& rom, unsigned index)
{
	switch(index) {
	case 0:		return rom.rom;
	case 1:		return rom.rom_xml;
	case 2:		return rom.slota;
	case 3:		return rom.slota_xml;
	case 4:		return rom.slotb;
	case 5:		return rom.slotb_xml;
	}
	return rom.rom;
}

enum rom_region region_from_string(const std::string& str)
{
	if(str == RNAME_NTSC)
		return REGION_NTSC;
	if(str == RNAME_PAL)
		return REGION_PAL;
	return REGION_AUTO;
}

enum rom_region region_from_string(const wxString& str)
{
	return region_from_string(tostdstring(str));
}

bool has_forced_region(enum rom_type rtype)
{
	return (rtype != ROMTYPE_SNES && rtype != ROMTYPE_SGB);
}

bool has_forced_region(const std::string& str)
{
	return has_forced_region(romtype_from_string(str));
}

bool has_forced_region(const wxString& str)
{
	return has_forced_region(romtype_from_string(str));
}

wxString forced_region_for_romtype(enum rom_type rtype)
{
	if(has_forced_region(rtype))
		return wxT(RNAME_NTSC);
	else
		return wxT("");
}

wxString forced_region_for_romtype(const std::string& str)
{
	return forced_region_for_romtype(romtype_from_string(str));
}

wxString forced_region_for_romtype(const wxString& str)
{
	return forced_region_for_romtype(romtype_from_string(str));
}

unsigned populate_region_choices(wxString* array)
{
	array[0] = wxT(RNAME_AUTO);
	array[1] = wxT(RNAME_NTSC);
	array[2] = wxT(RNAME_PAL);
	return 3;
}

unsigned populate_system_choices(wxString* array)
{
	array[0] = wxT(TNAME_SNES);
	array[1] = wxT(TNAME_BSX_NS);
	array[2] = wxT(TNAME_BSX_S);
	array[3] = wxT(TNAME_SUFAMITURBO);
	array[4] = wxT(TNAME_SGB);
	return 5;
}
