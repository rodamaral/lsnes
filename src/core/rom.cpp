#include "core/bsnes.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/patchrom.hpp"
#include "core/rom.hpp"
#include "core/window.hpp"
#include "interface/core.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <set>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

//Some anti-typo defs.
#define SNES_TYPE "snes"
#define SNES_PAL "snes_pal"
#define SNES_NTSC "snes_ntsc"
#define BSX "bsx"
#define BSXSLOTTED "bsxslotted"
#define SUFAMITURBO "sufamiturbo"
#define SGB_TYPE "SGB"
#define SGB_PAL "sgb_pal"
#define SGB_NTSC "sgb_ntsc"

std::string gtype::tostring(rom_type rtype, rom_region region) throw(std::bad_alloc, std::runtime_error)
{
	switch(rtype) {
	case ROMTYPE_SNES:
		switch(region) {
		case REGION_AUTO:	return "snes";
		case REGION_NTSC:	return "snes_ntsc";
		case REGION_PAL:	return "snes_pal";
		};
	case ROMTYPE_SGB:
		switch(region) {
		case REGION_AUTO:	return "sgb";
		case REGION_NTSC:	return "sgb_ntsc";
		case REGION_PAL:	return "sgb_pal";
		};
	case ROMTYPE_BSX:		return "bsx";
	case ROMTYPE_BSXSLOTTED:	return "bsxslotted";
	case ROMTYPE_SUFAMITURBO:	return "sufamiturbo";
	default:			throw std::runtime_error("tostring: ROMTYPE_NONE");
	};
}

std::string gtype::tostring(gametype_t gametype) throw(std::bad_alloc, std::runtime_error)
{
	switch(gametype) {
	case GT_SNES_NTSC:		return "snes_ntsc";
	case GT_SNES_PAL:		return "snes_pal";
	case GT_SGB_NTSC:		return "sgb_ntsc";
	case GT_SGB_PAL:		return "sgb_pal";
	case GT_BSX:			return "bsx";
	case GT_BSX_SLOTTED:		return "bsxslotted";
	case GT_SUFAMITURBO:		return "sufamiturbo";
	default:			throw std::runtime_error("tostring: GT_INVALID");
	};
}

gametype_t gtype::togametype(rom_type rtype, rom_region region) throw(std::bad_alloc, std::runtime_error)
{
	switch(rtype) {
	case ROMTYPE_SNES:
		switch(region) {
		case REGION_AUTO:	return GT_SGB_NTSC;
		case REGION_NTSC:	return GT_SNES_NTSC;
		case REGION_PAL:	return GT_SNES_PAL;
		};
	case ROMTYPE_SGB:
		switch(region) {
		case REGION_AUTO:	return GT_SGB_NTSC;
		case REGION_NTSC:	return GT_SGB_NTSC;
		case REGION_PAL:	return GT_SGB_PAL;
		};
	case ROMTYPE_BSX:		return GT_BSX;
	case ROMTYPE_BSXSLOTTED:	return GT_BSX_SLOTTED;
	case ROMTYPE_SUFAMITURBO:	return GT_SUFAMITURBO;
	default:			throw std::runtime_error("togametype: ROMTYPE_NONE");
	};
}

gametype_t gtype::togametype(const std::string& gametype) throw(std::bad_alloc, std::runtime_error)
{
	if(gametype == "snes_ntsc")
		return GT_SNES_NTSC;
	if(gametype == "snes_pal")
		return GT_SNES_PAL;
	if(gametype == "sgb_ntsc")
		return GT_SGB_NTSC;
	if(gametype == "sgb_pal")
		return GT_SGB_PAL;
	if(gametype == "bsx")
		return GT_BSX;
	if(gametype == "bsxslotted")
		return GT_BSX_SLOTTED;
	if(gametype == "sufamiturbo")
		return GT_SUFAMITURBO;
	throw std::runtime_error("Unknown game type '" + gametype + "'");
}

rom_type gtype::toromtype(const std::string& gametype) throw(std::bad_alloc, std::runtime_error)
{
	if(gametype == "snes_ntsc")
		return ROMTYPE_SNES;
	if(gametype == "snes_pal")
		return ROMTYPE_SNES;
	if(gametype == "snes")
		return ROMTYPE_SNES;
	if(gametype == "sgb_ntsc")
		return ROMTYPE_SGB;
	if(gametype == "sgb_pal")
		return ROMTYPE_SGB;
	if(gametype == "sgb")
		return ROMTYPE_SGB;
	if(gametype == "bsx")
		return ROMTYPE_BSX;
	if(gametype == "bsxslotted")
		return ROMTYPE_BSXSLOTTED;
	if(gametype == "sufamiturbo")
		return ROMTYPE_SUFAMITURBO;
	throw std::runtime_error("Unknown game type '" + gametype + "'");
}

rom_type gtype::toromtype(gametype_t gametype) throw()
{
	switch(gametype) {
	case GT_SNES_NTSC:		return ROMTYPE_SNES;
	case GT_SNES_PAL:		return ROMTYPE_SNES;
	case GT_SGB_NTSC:		return ROMTYPE_SGB;
	case GT_SGB_PAL:		return ROMTYPE_SGB;
	case GT_BSX:			return ROMTYPE_BSX;
	case GT_BSX_SLOTTED:		return ROMTYPE_BSXSLOTTED;
	case GT_SUFAMITURBO:		return ROMTYPE_SUFAMITURBO;
	case GT_INVALID:		throw std::runtime_error("toromtype: GT_INVALID");
	};
}

rom_region gtype::toromregion(const std::string& gametype) throw(std::bad_alloc, std::runtime_error)
{
	if(gametype == "snes_ntsc")
		return REGION_NTSC;
	if(gametype == "snes_pal")
		return REGION_PAL;
	if(gametype == "snes")
		return REGION_AUTO;
	if(gametype == "sgb_ntsc")
		return REGION_NTSC;
	if(gametype == "sgb_pal")
		return REGION_PAL;
	if(gametype == "sgb")
		return REGION_AUTO;
	if(gametype == "bsx")
		return REGION_NTSC;
	if(gametype == "bsxslotted")
		return REGION_NTSC;
	if(gametype == "sufamiturbo")
		return REGION_NTSC;
	throw std::runtime_error("Unknown game type '" + gametype + "'");
}

rom_region gtype::toromregion(gametype_t gametype) throw()
{
	switch(gametype) {
	case GT_SNES_NTSC:		return REGION_NTSC;
	case GT_SNES_PAL:		return REGION_PAL;
	case GT_SGB_NTSC:		return REGION_NTSC;
	case GT_SGB_PAL:		return REGION_PAL;
	case GT_BSX:			return REGION_NTSC;
	case GT_BSX_SLOTTED:		return REGION_NTSC;
	case GT_SUFAMITURBO:		return REGION_NTSC;
	case GT_INVALID:		throw std::runtime_error("toromregion: GT_INVALID");
	};
}


namespace
{
	bool option_set(const std::vector<std::string>& cmdline, const std::string& option)
	{
		for(auto i : cmdline)
			if(i == option)
				return true;
		return false;
	}

	const char* romtypes_to_recognize[] = {
		"rom", "bsx", "bsxslotted", "dmg", "slot-a", "slot-b",
		"rom-xml", "bsx-xml", "bsxslotted-xml", "dmg-xml", "slot-a-xml", "slot-b-xml"
	};

	enum rom_type current_rom_type = ROMTYPE_NONE;
	enum rom_region current_region = REGION_NTSC;

	std::string findoption(const std::vector<std::string>& cmdline, const std::string& option)
	{
		std::string value;
		regex_results optp;
		for(auto i : cmdline) {
			if(!(optp = regex("--([^=]+)=(.*)", i)) || optp[1] != option)
				continue;
			if(value == "")
				value = optp[2];
			else
				std::cerr << "WARNING: Ignored duplicate option for '" << option << "'." << std::endl;
			if(value == "")
				throw std::runtime_error("Empty value for '" + option + "' is not allowed");
		}
		return value;
	}

}

loaded_slot::loaded_slot() throw(std::bad_alloc)
{
	valid = false;
}

loaded_slot::loaded_slot(const std::string& filename, const std::string& base, bool xml_flag)
	throw(std::bad_alloc, std::runtime_error)
{
	bool headered = false;
	xml = xml_flag;
	if(filename == "") {
		valid = false;
		return;
	}
	valid = true;
	data = read_file_relative(filename, base);
	if(!xml && data.size() % 1024 == 512)
		//Assume headered.
		headered = true;
	if(headered && !xml) {
		if(data.size() >= 512) {
			memmove(&data[0], &data[512], data.size() - 512);
			data.resize(data.size() - 512);
		} else {
			data.resize(0);
		}
	}
	sha256 = sha256::hash(data);
	if(xml) {
		size_t osize = data.size();
		data.resize(osize + 1);
		data[osize] = 0;
	}
}

void loaded_slot::patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	try {
		std::vector<char> data2 = data;
		size_t poffset = 0;
		if(xml && valid)
			data2.resize(data2.size() - 1);
		data2 = do_patch_file(data2, patch, offset);
		//Mark the slot as valid and update hash.
		valid = true;
		std::string new_sha256 = sha256::hash(data2);
		if(xml) {
			size_t osize = data2.size();
			data2.resize(osize + 1);
			data2[osize] = 0;
		}
		data = data2;
		sha256 = new_sha256;
	} catch(...) {
		throw;
	}
}

rom_files::rom_files() throw()
{
	rtype = ROMTYPE_NONE;
	region = REGION_AUTO;

}

rom_files::rom_files(const std::vector<std::string>& cmdline) throw(std::bad_alloc, std::runtime_error)
{
	rom = rom_xml = slota = slota_xml = slotb = slotb_xml = "";
	std::string arr[sizeof(romtypes_to_recognize) / sizeof(romtypes_to_recognize[0])];
	unsigned long flags = 0;
	for(size_t i = 0; i < sizeof(romtypes_to_recognize) / sizeof(romtypes_to_recognize[0]); i++) {
		arr[i] = findoption(cmdline, romtypes_to_recognize[i]);
		if(arr[i] != "")
			flags |= (1L << i);
	}
	rtype = recognize_platform(flags);
	for(size_t i = 0; i < sizeof(romtypes_to_recognize) / sizeof(romtypes_to_recognize[0]); i++) {
		if(arr[i] != "")
			switch(recognize_commandline_rom(rtype, romtypes_to_recognize[i])) {
			case 0:		rom = arr[i];		break;
			case 1:		rom_xml = arr[i];	break;
			case 2:		slota = arr[i];		break;
			case 3:		slota_xml = arr[i];	break;
			case 4:		slotb = arr[i];		break;
			case 5:		slotb_xml = arr[i];	break;
			};
	}
	region = (rtype == ROMTYPE_SGB || rtype == ROMTYPE_SNES) ? REGION_AUTO : REGION_NTSC;
	if(option_set(cmdline, "--ntsc"))
		region = REGION_NTSC;
	else if(option_set(cmdline, "--pal"))
		region = REGION_PAL;

	base_file = "";
}

void rom_files::resolve_relative() throw(std::bad_alloc, std::runtime_error)
{
	rom = resolve_file_relative(rom, base_file);
	rom_xml = resolve_file_relative(rom_xml, base_file);
	slota = resolve_file_relative(slota, base_file);
	slota_xml = resolve_file_relative(slota_xml, base_file);
	slotb = resolve_file_relative(slotb, base_file);
	slotb_xml = resolve_file_relative(slotb_xml, base_file);
	base_file = "";
}


std::pair<enum rom_type, enum rom_region> get_current_rom_info() throw()
{
	return std::make_pair(current_rom_type, current_region);
}

loaded_rom::loaded_rom() throw()
{
	rtype = ROMTYPE_NONE;
	region = orig_region = REGION_AUTO;
}

loaded_rom::loaded_rom(const rom_files& files) throw(std::bad_alloc, std::runtime_error)
{
	std::string _slota = files.slota;
	std::string _slota_xml = files.slota_xml;
	std::string _slotb = files.slotb;
	std::string _slotb_xml = files.slotb_xml;
	if(files.rtype == ROMTYPE_NONE) {
		rtype = ROMTYPE_NONE;
		region = orig_region = files.region;
		return;
	}
	if((_slota != "" || _slota_xml != "") && files.rtype == ROMTYPE_SNES) {
		messages << "WARNING: SNES takes only 1 ROM image" << std::endl;
		_slota = "";
		_slota_xml = "";
	}
	if((_slotb != "" || _slotb_xml != "") && files.rtype != ROMTYPE_SUFAMITURBO) {
		messages << "WARNING: Only Sufami Turbo takes 3 ROM images" << std::endl;
		_slotb = "";
		_slotb_xml = "";
	}
	if(files.rom_xml != "" && files.rom == "")
		messages << "WARNING: " << name_subrom(files.rtype, 0) << " specified without corresponding "
			<< name_subrom(files.rtype, 1) << std::endl;
	if(_slota_xml != "" && _slota == "")
		messages << "WARNING: " << name_subrom(files.rtype, 2) << " specified without corresponding "
			<< name_subrom(files.rtype, 3) << std::endl;
	if(_slotb_xml != "" && _slotb == "")
		messages << "WARNING: " << name_subrom(files.rtype, 4) << " specified without corresponding "
			<< name_subrom(files.rtype, 5) << std::endl;

	rtype = files.rtype;
	rom = loaded_slot(files.rom, files.base_file, false);
	rom_xml = loaded_slot(files.rom_xml, files.base_file, true);
	slota = loaded_slot(_slota, files.base_file, false);
	slota_xml = loaded_slot(_slota_xml, files.base_file, true);
	slotb = loaded_slot(_slotb, files.base_file, false);
	slotb_xml = loaded_slot(_slotb_xml, files.base_file, true);
	orig_region = region = files.region;
}

void loaded_rom::load() throw(std::bad_alloc, std::runtime_error)
{
	current_rom_type = ROMTYPE_NONE;
	if(region == REGION_AUTO && orig_region != REGION_AUTO)
		region = orig_region;
	if(region != orig_region && orig_region != REGION_AUTO)
		throw std::runtime_error("Trying to force incompatible region");
	if(rtype == ROMTYPE_NONE)
		throw std::runtime_error("Can't insert cartridge of type NONE!");
	switch(region) {
	case REGION_AUTO:
		SNES::config.region = SNES::System::Region::Autodetect;
		break;
	case REGION_NTSC:
		SNES::config.region = SNES::System::Region::NTSC;
		break;
	case REGION_PAL:
		SNES::config.region = SNES::System::Region::PAL;
		break;
	default:
		throw std::runtime_error("Trying to force unknown region");
	}
	switch(rtype) {
	case ROMTYPE_SNES:
		if(!snes_load_cartridge_normal(rom_xml, rom, rom))
			throw std::runtime_error("Can't load cartridge ROM");
		break;
	case ROMTYPE_BSX:
		if(region == REGION_PAL)
			throw std::runtime_error("BSX can't be PAL");
		if(!snes_load_cartridge_bsx(rom_xml, rom, rom, slota_xml, slota, slota))
			throw std::runtime_error("Can't load cartridge ROM");
		break;
	case ROMTYPE_BSXSLOTTED:
		if(region == REGION_PAL)
			throw std::runtime_error("Slotted BSX can't be PAL");
		if(!snes_load_cartridge_bsx_slotted(rom_xml, rom, rom, slota_xml, slota, slota))
			throw std::runtime_error("Can't load cartridge ROM");
		break;
	case ROMTYPE_SGB:
		if(!snes_load_cartridge_super_game_boy(rom_xml, rom, rom, slota_xml, slota, slota))
			throw std::runtime_error("Can't load cartridge ROM");
		break;
	case ROMTYPE_SUFAMITURBO:
		if(region == REGION_PAL)
			throw std::runtime_error("Sufami Turbo can't be PAL");
		if(!snes_load_cartridge_sufami_turbo(rom_xml, rom, rom, slota_xml, slota, slota, slotb_xml, slotb,
			slotb))
			throw std::runtime_error("Can't load cartridge ROM");
		break;
	default:
		throw std::runtime_error("Unknown cartridge type");
	}
	if(region == REGION_AUTO)
		region = snes_get_region() ? REGION_PAL : REGION_NTSC;
	snes_power();
	auto framerate = emucore_get_video_rate();
	auto soundrate = emucore_get_audio_rate();
	set_nominal_framerate(1.0 * framerate.first / framerate.second);
	information_dispatch::do_sound_rate(soundrate.first, soundrate.second);
	current_rom_type = rtype;
	current_region = region;
	emucore_refresh_cart();
	refresh_cart_mappings();
}

void loaded_rom::do_patch(const std::vector<std::string>& cmdline) throw(std::bad_alloc,
	std::runtime_error)
{
	int32_t offset = 0;
	regex_results opt;
	for(auto i : cmdline) {
		if(opt = regex("--ips-offset=(.*)", i)) {
			try {
				offset = parse_value<int32_t>(opt[1]);
			} catch(std::exception& e) {
				throw std::runtime_error("Invalid IPS offset option '" + i + "': " + e.what());
			}
			continue;
		} else if(opt = regex("--ips-([^=]*)=(.+)", i)) {
			messages << "Patching " << opt[1] << " using '" << opt[2] << "'" << std::endl;
			std::vector<char> ips;
			try {
				ips = read_file_relative(opt[2], "");
			} catch(std::bad_alloc& e) {
				OOM_panic();
			} catch(std::exception& e) {
				throw std::runtime_error("Can't read IPS '" + opt[2] + "': " + e.what());
			}
			try {
				switch(recognize_commandline_rom(rtype, opt[1])) {
				case 0:		rom.patch(ips, offset);			break;
				case 1:		rom_xml.patch(ips, offset);		break;
				case 2:		slota.patch(ips, offset);		break;
				case 3:		slota_xml.patch(ips, offset);		break;
				case 4:		slotb.patch(ips, offset);		break;
				case 5:		slotb_xml.patch(ips, offset);		break;
				default:
					throw std::runtime_error("Invalid subROM '" + opt[1] + "' to patch");
				}
			} catch(std::bad_alloc& e) {
				OOM_panic();
			} catch(std::exception& e) {
				throw std::runtime_error("Can't Patch with IPS '" + opt[2] + "': " + e.what());
			}
		}
	}
}

std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc)
{
	std::map<std::string, std::vector<char>> out;
	for(unsigned i = 0; i < emucore_sram_slots(); i++) {
		sram_slot_structure* r = emucore_sram_slot(i);
		std::vector<char> x;
		r->copy_from_core(x);
		out[r->get_name()] = x;
	}
	return out;
}

void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
{
	std::set<std::string> used;
	if(sram.empty())
		return;
	for(unsigned i = 0; i < emucore_sram_slots(); i++) {
		sram_slot_structure* r = emucore_sram_slot(i);
		if(sram.count(r->get_name())) {
			std::vector<char>& x = sram[r->get_name()];
			if(r->get_size() != x.size() && r->get_size())
				messages << "WARNING: SRAM '" << r->get_name() << "': Loaded " << x.size()
					<< " bytes, but the SRAM is " << r->get_size() << "." << std::endl;
			r->copy_to_core(x);
			used.insert(r->get_name());
		} else
			messages << "WARNING: SRAM '" << r->get_name() << ": No data." << std::endl;
	}
	for(auto i : sram)
		if(!used.count(i.first))
			messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge." << std::endl;
}

std::map<std::string, std::vector<char>> load_sram_commandline(const std::vector<std::string>& cmdline)
	throw(std::bad_alloc, std::runtime_error)
{
	std::map<std::string, std::vector<char>> ret;
	regex_results opt;
	for(auto i : cmdline) {
		if(opt = regex("--continue=(.+)", i)) {
			zip_reader r(opt[1]);
			for(auto j : r) {
				auto sramname = regex("sram\\.(.*)", j);
				if(!sramname)
					continue;
				std::istream& x = r[j];
				try {
					std::vector<char> out;
					boost::iostreams::back_insert_device<std::vector<char>> rd(out);
					boost::iostreams::copy(x, rd);
					delete &x;
					ret[sramname[1]] = out;
				} catch(...) {
					delete &x;
					throw;
				}
			}
			continue;
		} else if(opt = regex("--sram-([^=]+)=(.+)", i)) {
			try {
				ret[opt[1]] = read_file_relative(opt[2], "");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::runtime_error& e) {
				throw std::runtime_error("Can't load SRAM '" + opt[1] + "': " + e.what());
			}
		}
	}
	return ret;
}

std::string name_subrom(enum rom_type major, unsigned romnumber) throw(std::bad_alloc)
{
	if(romnumber == 0)
		return "ROM";
	else if(romnumber == 1)
		return "ROM XML";
	else if(major == ROMTYPE_BSX && romnumber == 2)
		return "BSX ROM";
	else if(major == ROMTYPE_BSX && romnumber == 3)
		return "BSX XML";
	else if(major == ROMTYPE_BSXSLOTTED && romnumber == 2)
		return "BSX ROM";
	else if(major == ROMTYPE_BSXSLOTTED && romnumber == 3)
		return "BSX XML";
	else if(major == ROMTYPE_SGB && romnumber == 2)
		return "DMG ROM";
	else if(major == ROMTYPE_SGB && romnumber == 3)
		return "DMG XML";
	else if(major == ROMTYPE_SUFAMITURBO && romnumber == 2)
		return "SLOT A ROM";
	else if(major == ROMTYPE_SUFAMITURBO && romnumber == 3)
		return "SLOT A XML";
	else if(major == ROMTYPE_SUFAMITURBO && romnumber == 4)
		return "SLOT B ROM";
	else if(major == ROMTYPE_SUFAMITURBO && romnumber == 5)
		return "SLOT B XML";
	else if(romnumber % 2)
		return "UNKNOWN XML";
	else
		return "UNKNOWN ROM";
}


int recognize_commandline_rom(enum rom_type major, const std::string& romname) throw(std::bad_alloc)
{
	if(romname == romtypes_to_recognize[0])
		return 0;
	else if(romname == romtypes_to_recognize[6])
		return 1;
	else if(major == ROMTYPE_BSX && romname == romtypes_to_recognize[1])
		return 2;
	else if(major == ROMTYPE_BSX && romname == romtypes_to_recognize[7])
		return 3;
	else if(major == ROMTYPE_BSX && romname == romtypes_to_recognize[2])
		return 2;
	else if(major == ROMTYPE_BSX && romname == romtypes_to_recognize[8])
		return 3;
	else if(major == ROMTYPE_SGB && romname == romtypes_to_recognize[3])
		return 2;
	else if(major == ROMTYPE_SGB && romname == romtypes_to_recognize[9])
		return 3;
	else if(major == ROMTYPE_SUFAMITURBO && romname == romtypes_to_recognize[4])
		return 2;
	else if(major == ROMTYPE_SUFAMITURBO && romname == romtypes_to_recognize[10])
		return 3;
	else if(major == ROMTYPE_SUFAMITURBO && romname == romtypes_to_recognize[5])
		return 4;
	else if(major == ROMTYPE_SUFAMITURBO && romname == romtypes_to_recognize[11])
		return 5;
	else
		return -1;
}

rom_type recognize_platform(unsigned long flags) throw(std::bad_alloc, std::runtime_error)
{
	if((flags & 07700) >> 6 & ~(flags & 077))
		throw std::runtime_error("SubROM XML specified without corresponding subROM");
	if((flags & 1) == 0)
		throw std::runtime_error("No SNES main cartridge ROM specified");
	if((flags & 077) == 1)
		return ROMTYPE_SNES;
	if((flags & 077) == 3)
		return ROMTYPE_BSX;
	if((flags & 077) == 5)
		return ROMTYPE_BSXSLOTTED;
	if((flags & 077) == 9)
		return ROMTYPE_SGB;
	if((flags & 060) != 0 && (flags & 017) == 1)
		return ROMTYPE_SUFAMITURBO;
	throw std::runtime_error("Not valid combination of rom/bsx/bsxslotted/dmg/slot-a/slot-b");
}
