#include "lsnes.hpp"
#include "core/emucore.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/mainloop.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/patchrom.hpp"
#include "core/rom.hpp"
#include "core/window.hpp"
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

/**
 * Recognize the slot this ROM goes to.
 *
 * parameter major: The major type.
 * parameter romname: Name of the ROM type.
 * returns: Even if this is main rom, odd if XML. 0/1 for main slot, 2/3 for slot A, 4/5 for slot B. -1 if not valid
 *	rom type.
 * throws std::bad_alloc: Not enough memory
 */
int recognize_commandline_rom(core_type& major, const std::string& romname) throw(std::bad_alloc);

/**
 * Recognize major type from flags.
 *
 * parameter flags: Flags telling what ROM parameters are present.
 * returns: The recognzed major type.
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Illegal flags.
 */
core_type& recognize_platform(const std::set<std::string>& present) throw(std::bad_alloc, std::runtime_error);


namespace
{
	bool option_set(const std::vector<std::string>& cmdline, const std::string& option)
	{
		for(auto i : cmdline)
			if(i == option)
				return true;
		return false;
	}

	core_type* current_rom_type = NULL;
	core_region* current_region = NULL;

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

	std::set<std::string> find_present_roms(const std::vector<std::string>& cmdline)
	{
		std::set<std::string> p;
		std::set<core_type*> types = core_type::get_core_types();
		for(auto i : types) {
			for(unsigned j = 0; j < i->get_image_count(); j++) {
				std::string iname = i->get_image_info(j).iname;
				if(findoption(cmdline, iname) != "")
					p.insert(iname);
				if(findoption(cmdline, iname + "-xml") != "")
					p.insert(iname + "-xml");
			}
		}
		return p;
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
	rtype = NULL;
	region = NULL;
}

rom_files::rom_files(const std::vector<std::string>& cmdline) throw(std::bad_alloc, std::runtime_error)
{
	for(size_t i = 0; i < sizeof(romimg) / sizeof(romimg[0]); i++) {
		romimg[i] = "";
		romxml[i] = "";
	}

	auto opts = find_present_roms(cmdline);
	rtype = &recognize_platform(opts);
	for(auto i : opts) {
		std::string o = findoption(cmdline, i);
		if(o != "") {
			int j = recognize_commandline_rom(*rtype, i);
			if(j >= 0 && j & 1)
				romxml[j / 2] = o;
			else if(j >= 0)
				romimg[j / 2] = o;
		}
	}
	region = &rtype->get_preferred_region();
	std::string _region = findoption(cmdline, "region");
	if(_region != "") {
		bool isset = false;
		for(auto i : rtype->get_regions()) {
			if(i->get_iname() == _region) {
				region = i;
				isset = true;
			}
		}
		if(!isset)
			throw std::runtime_error("Unknown region for system type");
	}
	base_file = "";
}

void rom_files::resolve_relative() throw(std::bad_alloc, std::runtime_error)
{
	for(size_t i = 0; i < sizeof(romimg)/sizeof(romimg[0]); i++) {
		romimg[i] = resolve_file_relative(romimg[i], base_file);
		romxml[i] = resolve_file_relative(romxml[i], base_file);
	}
	base_file = "";
}


std::pair<core_type*, core_region*> get_current_rom_info() throw()
{
	return std::make_pair(current_rom_type, current_region);
}

loaded_rom::loaded_rom() throw()
{
	rtype = NULL;
	region = orig_region = NULL;
}

loaded_rom::loaded_rom(const rom_files& files) throw(std::bad_alloc, std::runtime_error)
{
	std::string cromimg[sizeof(files.romimg)/sizeof(files.romimg[0])];
	std::string cromxml[sizeof(files.romimg)/sizeof(files.romimg[0])];
	for(size_t i = 0; i < sizeof(files.romimg)/sizeof(files.romimg[0]); i++) {
		cromimg[i] = files.romimg[i];
		cromxml[i] = files.romxml[i];
	}
	if(!files.rtype) {
		rtype = NULL;
		region = orig_region = files.region;
		return;
	}
	for(size_t i = 0; i < sizeof(files.romimg)/sizeof(files.romimg[0]); i++) {
		if((cromimg[i] != "" || cromxml[i] != "") && i > rtype->get_image_count()) {
			messages << "Warning: ROM slot #" << (i + 1) << " is not used for this console" << std::endl;
			cromimg[i] = "";
			cromxml[i] = "";
		}
		if(cromimg[i] == "" && cromxml[i] != "") {
			messages << "WARNING: " << name_subrom(*files.rtype, 2 * i + 1) << " specified without " 
				<< "corresponding " << name_subrom(*files.rtype, 2 * i + 0) << std::endl;
			cromxml[i] = "";
		}
	}

	rtype = files.rtype;
	for(size_t i = 0; i < sizeof(romimg) / sizeof(romimg[0]); i++) {
		romimg[i] = loaded_slot(cromimg[i], files.base_file, false);
		romxml[i] = loaded_slot(cromxml[i], files.base_file, true);
	}
	orig_region = region = files.region;
	if(cromimg[1] != "")
		msu1_base = resolve_file_relative(cromimg[1], files.base_file);
	else
		msu1_base = resolve_file_relative(cromimg[0], files.base_file);
}

void loaded_rom::load() throw(std::bad_alloc, std::runtime_error)
{
	if(!rtype)
		throw std::runtime_error("Can't insert cartridge of type NONE!");
	current_rom_type = NULL;
	if(!orig_region)
		orig_region = &rtype->get_preferred_region();
	if(!region)
		region = orig_region;
	if(!orig_region->compatible_with(*region))
		throw std::runtime_error("Trying to force incompatible region");
	if(!core_set_region(*region))
		throw std::runtime_error("Trying to force unknown region");

	core_romimage images[sizeof(romimg)/sizeof(romimg[0])];
	for(size_t i = 0; i < sizeof(romimg)/sizeof(romimg[0]); i++) {
		images[i].markup = (const char*)romxml[i];
		images[i].data = (const unsigned char*)romimg[i];
		images[i].size = (size_t)romimg[i];
	}
	if(!rtype->load(images))
		throw std::runtime_error("Can't load cartridge ROM");

	region = &core_get_region();
	core_power();
	auto nominal_fps = get_video_rate();
	auto nominal_hz = get_audio_rate();
	set_nominal_framerate(1.0 * nominal_fps.first / nominal_fps.second);
	information_dispatch::do_sound_rate(nominal_hz.first, nominal_hz.second);
	current_rom_type = rtype;
	current_region = region;
	msu1_base_path = msu1_base;
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
				int r_id = recognize_commandline_rom(*rtype, opt[1]);
				if(r_id < 0 || r_id > 2 * sizeof(romimg) / sizeof(romimg[0]))
					throw std::runtime_error("Invalid subROM '" + opt[1] + "' to patch");
				if(r_id & 1)
					romxml[r_id / 2].patch(ips, offset);
				else
					romimg[r_id / 2].patch(ips, offset);
			} catch(std::bad_alloc& e) {
				OOM_panic();
			} catch(std::exception& e) {
				throw std::runtime_error("Can't Patch with IPS '" + opt[2] + "': " + e.what());
			}
		}
	}
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

std::vector<char> save_core_state(bool nochecksum) throw(std::bad_alloc)
{
	std::vector<char> ret;
	core_serialize(ret);
	if(nochecksum)
		return ret;
	size_t offset = ret.size();
	unsigned char tmp[32];
	sha256::hash(tmp, ret);
	ret.resize(offset + 32);
	memcpy(&ret[offset], tmp, 32);
	return ret;
}

void load_core_state(const std::vector<char>& buf, bool nochecksum) throw(std::runtime_error)
{
	if(nochecksum) {
		core_unserialize(&buf[0], buf.size());
		return;
	}

	if(buf.size() < 32)
		throw std::runtime_error("Savestate corrupt");
	unsigned char tmp[32];
	sha256::hash(tmp, reinterpret_cast<const uint8_t*>(&buf[0]), buf.size() - 32);
	if(memcmp(tmp, &buf[buf.size() - 32], 32))
		throw std::runtime_error("Savestate corrupt");
	core_unserialize(&buf[0], buf.size() - 32);;
}

std::string name_subrom(core_type& major, unsigned romnumber) throw(std::bad_alloc)
{
	std::string name = "UNKNOWN";
	if(romnumber < 2 * major.get_image_count())
		name = major.get_image_info(romnumber / 2).hname;
	if(romnumber % 2)
		return name + " XML";
	else if(name != "ROM")
		return name + " ROM";
	else
		return "ROM";
}


int recognize_commandline_rom(core_type& major, const std::string& romname) throw(std::bad_alloc)
{
	for(unsigned i = 0; i < major.get_image_count(); i++) {
		std::string iname = major.get_image_info(i).iname;
		if(romname == iname)
			return 2 * i + 0;
		if(romname == iname + "-xml")
			return 2 * i + 1;
	}
	return -1;
}

core_type& recognize_platform(const std::set<std::string>& present) throw(std::bad_alloc, std::runtime_error)
{
	std::set<core_type*> possible = core_type::get_core_types();
	unsigned total = 0;
	for(auto i : present) {
		regex_results r;
		std::string base = i;
		if(r = regex("(.*)-xml", base)) {
			if(!present.count(r[1]))
				throw std::runtime_error("SubROM XML specified without corresponding subROM");
		} else
			total++;
	}
	for(auto i : possible) {
		unsigned pmask = 0;
		unsigned rmask = 0;
		unsigned found = 0;
		for(unsigned j = 0; j < i->get_image_count(); j++) {
			std::string iname = i->get_image_info(j).iname;
			if(present.count(iname)) {
				pmask |= i->get_image_info(j).mandatory;
				found++;
			}
			rmask |= i->get_image_info(j).mandatory;
		}
		if(pmask == rmask && found == total)
			return *i;
	}
	throw std::runtime_error("Invalid combination of subROMs");
}
