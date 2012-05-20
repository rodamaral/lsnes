#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/mainloop.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/window.hpp"
#include "interface/core.hpp"
#include "library/patchrom.hpp"
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




namespace
{
	bool option_set(const std::vector<std::string>& cmdline, const std::string& option)
	{
		for(auto i : cmdline)
			if(i == option)
				return true;
		return false;
	}

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

loaded_slot::loaded_slot(const std::string& filename, const std::string& base, struct rom_info_structure& slot,
	bool xml_flag) throw(std::bad_alloc, std::runtime_error)
{
	size_t headered = 0;
	xml = xml_flag;
	if(filename == "") {
		valid = false;
		return;
	}
	valid = true;
	data = read_file_relative(filename, base);
	if(!xml)
		//Assume headered.
		headered = slot.headersize(data.size());
	if(headered) {
		if(data.size() >= headered) {
			memmove(&data[0], &data[headered], data.size() - headered);
			data.resize(data.size() - headered);
		} else
			data.resize(0);
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

void rom_files::resolve_relative() throw(std::bad_alloc, std::runtime_error)
{
	for(auto& i : main_slots)
		i = resolve_file_relative(i, base_file);
	for(auto& i : markup_slots)
		i = resolve_file_relative(i, base_file);
	base_file = "";
}

rom_files::rom_files(const std::vector<std::string>& cmdline) throw(std::bad_alloc, std::runtime_error)
{
	std::string systype = findoption(cmdline, "system");
	if(systype == "")
		throw std::runtime_error("System type (--system=<sys>) must be given");
	for(size_t i = 0; i < emucore_systype_slots(); i++) {
		auto j = emucore_systype_slot(i);
		if(j->get_iname() == systype) {
			rtype = j;
			break;
		}
	}
	if(!rtype)
		throw std::runtime_error("Unrecognized system type '" + systype + "'");

	std::string _region = findoption(cmdline, "region");
	if(_region == "")
		//Default.
		region = rtype->region_slot(0);
	else {
		for(size_t i = 0; i < rtype->region_slots(); i++) {
			auto j = rtype->region_slot(i);
			if(j->get_iname() == _region) {
				region = j;
				break;
			}
		}
	}
	if(!region)
		throw std::runtime_error("Unrecognized region '" + _region + "'");

	main_slots.resize(rtype->rom_slots());
	markup_slots.resize(rtype->rom_slots());

	unsigned complete = 0;
	unsigned flags = 0;
	for(size_t i = 0; i < rtype->rom_slots(); i++) {
		auto j = rtype->rom_slot(i);
		complete |= j->mandatory_flags();
		std::string f = findoption(cmdline, j->get_iname());
		if(f != "") {
			flags |= j->mandatory_flags();
			main_slots[i] = f;
		}
		f = findoption(cmdline, j->get_iname() + "-xml");
		if(f != "") {
			if(!j->has_markup())
				throw std::runtime_error("ROM type '" + j->get_iname() + "' does not have markup");
			markup_slots[i] = f;
		}
	}
	if(complete != flags)
		(stringfmt() << "Required ROM image missing (flags=" << flags << ", expected=" << complete << ")").
			throwex();
	base_file = "";
}


loaded_rom::loaded_rom() throw()
{
	rtype = NULL;
	region = orig_region = NULL;
}

loaded_rom::loaded_rom(const rom_files& files) throw(std::bad_alloc, std::runtime_error)
{
	if(files.rtype == NULL) {
		rtype = NULL;
		region = orig_region = NULL;
		return;
	}
	main_slots.resize(files.main_slots.size());
	markup_slots.resize(files.markup_slots.size());
	rtype = files.rtype;
	for(size_t i = 0; i < main_slots.size(); i++)
		main_slots[i] = loaded_slot(files.main_slots[i], files.base_file, *rtype->rom_slot(i), false);
	for(size_t i = 0; i < markup_slots.size(); i++)
		if(rtype->rom_slot(i)->has_markup())
			markup_slots[i] = loaded_slot(files.markup_slots[i], files.base_file, *rtype->rom_slot(i),
				true);
	orig_region = region = files.region;
	if(main_slots.size() > 1)
		msu1_base = resolve_file_relative(files.main_slots[1], files.base_file);
	else
		msu1_base = resolve_file_relative(files.main_slots[0], files.base_file);
}

void loaded_rom::load() throw(std::bad_alloc, std::runtime_error)
{
	if(!rtype)
		throw std::runtime_error("Can't insert cartridge of type NONE!");
	if(!orig_region->compatible(region->get_iname()))
		throw std::runtime_error("Trying to force incompatible region");

	std::vector<std::vector<char>> romslots;
	std::vector<std::vector<char>> markslots;
	romslots.resize(main_slots.size());
	markslots.resize(markup_slots.size());
	for(size_t i = 0; i < romslots.size(); i++)
		romslots[i] = main_slots[i].data;
	for(size_t i = 0; i < markslots.size(); i++)
		markslots[i] = markup_slots[i].data;
	emucore_load_rom(rtype, region, romslots, markslots);

	region = emucore_current_region();
	auto framerate = emucore_get_video_rate();
	auto soundrate = emucore_get_audio_rate();
	set_nominal_framerate(1.0 * framerate.first / framerate.second);
	information_dispatch::do_sound_rate(soundrate.first, soundrate.second);
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
				bool found = false;
				for(size_t j = 0; j < rtype->rom_slots(); j++)
					if(opt[1] == rtype->rom_slot(j)->get_iname()) {
						main_slots[j].patch(ips, offset);
						found = true;
						break;
					} else if(opt[1] == rtype->rom_slot(j)->get_iname() + "-xml" &&
						rtype->rom_slot(j)->has_markup()) {
						markup_slots[j].patch(ips, offset);
						found = true;
						break;
					}
				if(!found)
					throw std::runtime_error("Invalid subROM '" + opt[1] + "' to patch");
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

struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline) throw(std::bad_alloc,
	std::runtime_error)
{
	struct rom_files f;
	try {
		f = rom_files(cmdline);
		f.resolve_relative();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		throw std::runtime_error(std::string("Can't resolve ROM files: ") + e.what());
	}
	messages << "ROM type: " << f.rtype->get_iname() << "/" << f.region->get_iname() << std::endl;
	for(size_t i = 0; i < f.rtype->rom_slots(); i++) {
		if(f.main_slots[i] != "")
			messages << "'" << f.rtype->rom_slot(i)->get_hname() << "' file '" << f.main_slots[i]
				<< std::endl;
		if(f.markup_slots[i] != "" && f.rtype->rom_slot(i)->has_markup())
			messages << "'" << f.rtype->rom_slot(i)->get_hname() << "' markup file '"
				<< f.markup_slots[i] << std::endl;
	}

	struct loaded_rom r;
	try {
		r = loaded_rom(f);
		r.do_patch(cmdline);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		throw std::runtime_error(std::string("Can't load ROM: ") + e.what());
	}

	for(size_t i = 0; i < r.rtype->rom_slots(); i++) {
		if(r.main_slots[i].valid)
			messages << "'" << f.rtype->rom_slot(i)->get_hname() << "' hash " << r.main_slots[i].sha256
				<< std::endl;
		if(r.markup_slots[i].valid && r.rtype->rom_slot(i)->has_markup())
			messages << "'" << f.rtype->rom_slot(i)->get_hname() << "' markup hash '"
				<< r.markup_slots[i].sha256 << std::endl;
	}

	return r;
}

void dump_region_map() throw(std::bad_alloc)
{
	std::vector<vma_structure*> regions = get_regions();
	for(auto i : regions) {
		char buf[256];
		char echar;
		if(i->get_endian() == vma_structure::E_LITTLE)
			echar = 'L';
		if(i->get_endian() == vma_structure::E_BIG)
			echar = 'B';
		if(i->get_endian() == vma_structure::E_HOST)
			echar = 'N';
		sprintf(buf, "Region: %016X-%016X %016X %s%c %s", i->get_base(), i->get_base() + i->get_size() - 1,
			i->get_size(), i->is_readonly() ? "R-" : "RW", echar, i->get_name().c_str());
		messages << buf << std::endl;
	}
}
