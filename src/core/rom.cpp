#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/mainloop.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "library/pixfmt-rgb16.hpp"
#include "library/controller-data.hpp"
#include "library/patch.hpp"
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
#include <boost/filesystem.hpp>

#ifdef BOOST_FILESYSTEM3
namespace boost_fs = boost::filesystem3;
#else
namespace boost_fs = boost::filesystem;
#endif

namespace
{
	const char* null_chars = "F";
	uint16_t null_cover_fbmem[512 * 448];

	//Framebuffer.
	struct framebuffer_info null_fbinfo = {
		&_pixel_format_bgr16,		//Format.
		(char*)null_cover_fbmem,	//Memory.
		512, 448, 1024,			//Physical size.
		512, 448, 1024,			//Logical size.
		0, 0				//Offset.
	};

	port_index_triple sync_triple = {true, 0, 0, 0 };

	struct interface_device_reg null_registers[] = {
		{NULL, NULL, NULL}
	};

	struct _core_null : public core_core, public core_type, public core_region, public core_sysregion
	{
		_core_null() : core_core({{{}}}), core_type({{
			.iname = "null",
			.hname = "(null)",
			.id = 9999,
			.sysname = "System",
			.extensions = "",
			.bios = NULL,
			.regions = {this},
			.images = {},
			.settings = {},
			.core = this,
		}}), core_region({{"null", "(null)", 0, 0, false, {1, 60}, {0}}}),
		core_sysregion("null", *this, *this) {}
		std::string c_core_identifier() { return "null core"; }
		bool c_set_region(core_region& reg) { return true; }
		std::pair<unsigned, unsigned> c_video_rate() { return std::make_pair(60, 1); }
		std::pair<unsigned, unsigned> c_audio_rate() { return std::make_pair(48000, 1); }
		std::map<std::string, std::vector<char>> c_save_sram() throw (std::bad_alloc) {
			std::map<std::string, std::vector<char>> x;
			return x;
		}
		void c_load_sram(std::map<std::string, std::vector<char>>& sram) throw (std::bad_alloc) {}
		void c_serialize(std::vector<char>& out) { out.clear(); }
		void c_unserialize(const char* in, size_t insize) {}
		core_region& c_get_region() { return *this; }
		void c_power() {}
		void c_unload_cartridge() {}
		std::pair<uint32_t, uint32_t> c_get_scale_factors(uint32_t width, uint32_t height) {
			return std::make_pair(1, 1);
		}
		void c_install_handler() {}
		void c_uninstall_handler() {}
		void c_emulate() {}
		void c_runtosave() {}
		bool c_get_pflag() { return false; }
		void c_set_pflag(bool pflag) {}
		framebuffer_raw& c_draw_cover() {
			static framebuffer_raw x(null_fbinfo);
			for(size_t i = 0; i < sizeof(null_cover_fbmem)/sizeof(null_cover_fbmem[0]); i++)
				null_cover_fbmem[i] = 0x0000;
			std::string message = "NO ROM LOADED";
			cover_render_string(null_cover_fbmem, 204, 220, message, 0xFFFF, 0x0000, 512, 448, 1024, 2);
			return x;
		}
		std::string c_get_core_shortname() { return "null"; }
		void c_pre_emulate_frame(controller_frame& cf) {}
		void c_execute_action(unsigned id, const std::vector<interface_action_paramval>& p) {}
		const interface_device_reg* c_get_registers() { return null_registers; }
		int t_load_rom(core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs)
		{
			return 0;
		}
		controller_set t_controllerconfig(std::map<std::string, std::string>& settings)
		{
			controller_set x;
			x.ports.push_back(&get_default_system_port_type());
			x.portindex.indices.push_back(sync_triple);
			return x;
		}
		std::pair<uint64_t, uint64_t> t_get_bus_map() { return std::make_pair(0ULL, 0ULL); }
		std::list<core_vma_info> t_vma_list() { return std::list<core_vma_info>(); }
		std::set<std::string> t_srams() { return std::set<std::string>(); }
		unsigned c_action_flags(unsigned id) { return 0; }
		int c_reset_action(bool hard) { return -1; }
	} core_null;

	core_type* current_rom_type = &core_null;
	core_region* current_region = &core_null;

	core_type* find_core_by_extension(const std::string& ext, const std::string& tmpprefer)
	{
		std::string key = "ext:" + ext;
		std::list<core_type*> possible = core_type::get_core_types();
		core_type* fallback = NULL;
		core_type* preferred = preferred_core.count(key) ? preferred_core[key] : NULL;
		//Tmpprefer overrides normal preferred core.
		if(tmpprefer != "")
			for(auto i : possible)
				if(i->get_iname() == tmpprefer)
					preferred = i;
		for(auto i : possible)
			if(i->is_known_extension(ext)) {
				fallback = i;
				if(!preferred && i->get_core_shortname() == preferred_core_default)
					return i;
				if(i == preferred)
					return i;
			}
		return fallback;
	}

	core_type* find_core_by_name(const std::string& name, const std::string& tmpprefer)
	{
		std::string key = "type:" + name;
		std::list<core_type*> possible = core_type::get_core_types();
		core_type* fallback = NULL;
		core_type* preferred = preferred_core.count(key) ? preferred_core[key] : NULL;
		//Tmpprefer overrides normal preferred core.
		if(tmpprefer != "")
			for(auto i : possible)
				if(i->get_iname() == tmpprefer)
					preferred = i;
		for(auto i : possible)
			if(i->get_iname() == name) {
				fallback = i;
				if(!preferred && i->get_core_shortname() == preferred_core_default)
					return i;
				if(i == preferred)
					return i;
			}
		return fallback;
	}

	bool file_exists(const std::string& name)
	{
		try {
			delete &open_file_relative(name, "");
			return true;
		} catch(...) {
			return false;
		}
	}
}

loaded_slot::loaded_slot() throw(std::bad_alloc)
{
	valid = false;
	xml = false;
	sha_256 = "";
	filename_flag = false;
}

loaded_slot::loaded_slot(const std::string& filename, const std::string& base,
	const struct core_romimage_info& imginfo, bool xml_flag) throw(std::bad_alloc, std::runtime_error)
{
	unsigned headered = 0;
	xml = xml_flag;
	if(filename == "") {
		valid = false;
		sha_256 = "";
		filename_flag = (!xml && imginfo.pass_mode);
		return;
	}
	//XMLs are always loaded, no matter what.
	if(!xml && imginfo.pass_mode) {
		std::string _filename = filename;
		//Translate the passed filename to absolute one.
		_filename = resolve_file_relative(_filename, base);
		_filename = boost_fs::absolute(boost_fs::path(_filename)).string();
		filename_flag = true;
		data.resize(_filename.length());
		std::copy(_filename.begin(), _filename.end(), data.begin());
		//Compute the SHA-256.
		std::istream& s = open_file_relative(filename, "");
		sha256 hash;
		char buffer[8192];
		size_t block;
		while((block = s.readsome(buffer, 8192)))
			hash.write(buffer, block);
		sha_256 = hash.read();
		delete &s;
		valid = true;
		return;
	}
	filename_flag = false;
	valid = true;
	data = read_file_relative(filename, base);
	if(!xml && imginfo.headersize)
		headered = ((data.size() % (2 * imginfo.headersize)) == imginfo.headersize) ? imginfo.headersize : 0;
	if(headered && !xml) {
		if(data.size() >= headered) {
			memmove(&data[0], &data[headered], data.size() - headered);
			data.resize(data.size() - headered);
		} else {
			data.resize(0);
		}
	}
	sha_256 = sha256::hash(data);
	if(xml) {
		size_t osize = data.size();
		data.resize(osize + 1);
		data[osize] = 0;
	}
}

void loaded_slot::patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	if(filename_flag)
		throw std::runtime_error("CD images can't be patched on the fly");
	try {
		std::vector<char> data2 = data;
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
		sha_256 = new_sha256;
	} catch(...) {
		throw;
	}
}

std::pair<core_type*, core_region*> get_current_rom_info() throw()
{
	return std::make_pair(current_rom_type, current_region);
}

loaded_rom::loaded_rom() throw()
{
	rtype = &core_null;
	region = orig_region = &core_null;
}

loaded_rom::loaded_rom(const std::string& file, core_type& ctype) throw(std::bad_alloc, std::runtime_error)
{
	rtype = &ctype;
	region = orig_region = &rtype->get_preferred_region();
	unsigned romidx = 0;
	std::string bios;
	if((bios = ctype.get_biosname()) != "") {
		//This thing has a BIOS.
		romidx = 1;
		std::string basename = lsnes_vset["firmwarepath"].str() + "/" + bios;
		romimg[0] = loaded_slot(basename, "", ctype.get_image_info(0), false);
		if(file_exists(basename + ".xml"))
			romxml[0] = loaded_slot(basename + ".xml", "", ctype.get_image_info(0), true);
	}
	romimg[romidx] = loaded_slot(file, "", ctype.get_image_info(romidx), false);
	if(file_exists(file + ".xml"))
		romxml[romidx] = loaded_slot(file + ".xml", "", ctype.get_image_info(romidx), true);
	msu1_base = resolve_file_relative(file, "");
	return;
}

loaded_rom::loaded_rom(const std::string& file, const std::string& tmpprefer) throw(std::bad_alloc,
	std::runtime_error)
{
	std::istream& spec = open_file_relative(file, "");
	std::string s;
	std::getline(spec, s);
	istrip_CR(s);
	load_filename = file;
	if(!spec || s != "[GAMEPACK FILE]") {
		//This is a Raw ROM image.
		regex_results tmp;
		std::string ext = regex(".*\\.([^.]*)?", file, "Unknown ROM file type")[1];
		core_type* coretype = find_core_by_extension(ext, tmpprefer);
		if(!coretype)
			throw std::runtime_error("Unknown ROM file type");
		rtype = coretype;
		region = orig_region = &rtype->get_preferred_region();
		unsigned romidx = 0;
		std::string bios;
		if((bios = coretype->get_biosname()) != "") {
			//This thing has a BIOS.
			romidx = 1;
			std::string basename = lsnes_vset["firmwarepath"].str() + "/" + bios;
			romimg[0] = loaded_slot(basename, "", coretype->get_image_info(0), false);
			if(file_exists(basename + ".xml"))
				romxml[0] = loaded_slot(basename + ".xml", "", coretype->get_image_info(0), true);
		}
		romimg[romidx] = loaded_slot(file, "", coretype->get_image_info(romidx), false);
		if(file_exists(file + ".xml"))
			romxml[romidx] = loaded_slot(file + ".xml", "", coretype->get_image_info(romidx), true);
		msu1_base = resolve_file_relative(file, "");
		return;
	}
	std::vector<std::string> lines;
	while(std::getline(spec, s))
		lines.push_back(strip_CR(s));
	std::string platname = "";
	std::string platreg = "";
	for(auto i : lines) {
		regex_results tmp;
		if(tmp = regex("type[ \t]+(.+)", i))
			platname = tmp[1];
		if(tmp = regex("region[ \t]+(.+)", i))
			platreg = tmp[1];
	}

	//Detect type.
	rtype = find_core_by_name(platname, tmpprefer);
	if(!rtype)
		(stringfmt() << "Not a valid system type '" << platname << "'").throwex();

	//Detect region.
	bool goodreg = false;
	orig_region = &rtype->get_preferred_region();
	for(auto i: rtype->get_regions())
		if(i->get_iname() == platreg) {
			orig_region = i;
			goodreg = true;
		}
	if(!goodreg && platreg != "")
		(stringfmt() << "Not a valid system region '" << platreg << "'").throwex();
	region = orig_region;

	//ROM files.
	std::string cromimg[sizeof(romimg)/sizeof(romimg[0])];
	std::string cromxml[sizeof(romimg)/sizeof(romimg[0])];
	for(auto i : lines) {
		regex_results tmp;
		if(!(tmp = regex("(rom|xml)[ \t]+([^ \t]+)[ \t]+(.*)", i)))
			continue;
		size_t idxs = rtype->get_image_count();
		size_t idx = idxs;
		for(size_t i = 0; i < idxs; i++)
			if(rtype->get_image_info(i).iname == tmp[2])
				idx = i;
		if(idx == idxs)
			(stringfmt() << "Not a valid ROM name '" << tmp[2] << "'").throwex();
		if(tmp[1] == "rom")
			cromimg[idx] = tmp[3];
		else
			cromxml[idx] = tmp[3];
	}

	//Check ROMs.
	unsigned mask1 = 0, mask2 = 0;
	for(size_t i = 0; i < rtype->get_image_count(); i++) {
		auto ii = rtype->get_image_info(i);
		mask1 |= ii.mandatory;
		if(cromimg[i] != "")
			mask2 |= ii.mandatory;
		if(cromimg[i] == "" && cromxml[i] != "") {
			messages << "WARNING: Slot " << ii.iname << ": XML without ROM." << std::endl;
			cromxml[i] = "";
		}
	}
	if(mask1 != mask2)
		throw std::runtime_error("Required ROM missing");

	//Load ROMs.
	for(size_t i = 0; i < rtype->get_image_count(); i++) {
		romimg[i] = loaded_slot(cromimg[i], file, rtype->get_image_info(i), false);
		romxml[i] = loaded_slot(cromxml[i], file, rtype->get_image_info(i), true);
	}

	//Patch ROMs.
	for(auto i : lines) {
		regex_results tmp;
		if(!(tmp = regex("patch([+-][0-9]+)?[ \t]+([^ \t]+)[ \t]+(.*)", i)))
			continue;
		size_t idxs = rtype->get_image_count();
		size_t idx = idxs;
		for(size_t i = 0; i < idxs; i++)
			if(rtype->get_image_info(i).iname == tmp[2])
				idx = i;
		if(idx == idxs)
			(stringfmt() << "Not a valid ROM name '" << tmp[2] << "'").throwex();
		int32_t offset = 0;
		if(tmp[1] != "")
			offset = parse_value<int32_t>(tmp[1]);
		romimg[idx].patch(read_file_relative(tmp[3], file), offset);
	}

	//MSU-1 base.
	if(cromimg[1] != "")
		msu1_base = resolve_file_relative(cromimg[1], file);
	else
		msu1_base = resolve_file_relative(cromimg[0], file);
}

void loaded_rom::load(std::map<std::string, std::string>& settings, uint64_t rtc_sec, uint64_t rtc_subsec)
	throw(std::bad_alloc, std::runtime_error)
{
	core_core* old_core = current_rom_type->get_core();
	current_rom_type = &core_null;
	if(!orig_region && rtype != &core_null)
		orig_region = &rtype->get_preferred_region();
	if(!region)
		region = orig_region;
	if(rtype && !orig_region->compatible_with(*region))
		throw std::runtime_error("Trying to force incompatible region");
	if(rtype && !rtype->set_region(*region))
		throw std::runtime_error("Trying to force unknown region");

	core_romimage images[sizeof(romimg)/sizeof(romimg[0])];
	for(size_t i = 0; i < sizeof(romimg)/sizeof(romimg[0]); i++) {
		images[i].markup = (const char*)romxml[i];
		images[i].data = (const unsigned char*)romimg[i];
		images[i].size = (size_t)romimg[i];
	}
	if(!rtype->load(images, settings, rtc_sec, rtc_subsec))
		throw std::runtime_error("Can't load cartridge ROM");

	region = &rtype->get_region();
	rtype->power();
	auto nominal_fps = rtype->get_video_rate();
	auto nominal_hz = rtype->get_audio_rate();
	set_nominal_framerate(1.0 * nominal_fps.first / nominal_fps.second);
	information_dispatch::do_sound_rate(nominal_hz.first, nominal_hz.second);
	current_rom_type = rtype;
	current_region = region;
	current_romfile = load_filename;
	//If core changes, unload the cartridge.
	if(old_core != current_rom_type->get_core())
		try { old_core->unload_cartridge(); } catch(...) {}
	refresh_cart_mappings();
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

std::vector<char> loaded_rom::save_core_state(bool nochecksum) throw(std::bad_alloc)
{
	std::vector<char> ret;
	rtype->serialize(ret);
	if(nochecksum)
		return ret;
	size_t offset = ret.size();
	unsigned char tmp[32];
	sha256::hash(tmp, ret);
	ret.resize(offset + 32);
	memcpy(&ret[offset], tmp, 32);
	return ret;
}

void loaded_rom::load_core_state(const std::vector<char>& buf, bool nochecksum) throw(std::runtime_error)
{
	if(nochecksum) {
		rtype->unserialize(&buf[0], buf.size());
		return;
	}

	if(buf.size() < 32)
		throw std::runtime_error("Savestate corrupt");
	unsigned char tmp[32];
	sha256::hash(tmp, reinterpret_cast<const uint8_t*>(&buf[0]), buf.size() - 32);
	if(memcmp(tmp, &buf[buf.size() - 32], 32))
		throw std::runtime_error("Savestate corrupt");
	rtype->unserialize(&buf[0], buf.size() - 32);;
}

std::map<std::string, core_type*> preferred_core;
std::string preferred_core_default;
std::string current_romfile;
