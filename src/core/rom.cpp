#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/mainloop.hpp"
#include "core/memorymanip.hpp"
#include "core/misc.hpp"
#include "core/rom.hpp"
#include "core/romguess.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "library/framebuffer-pixfmt-rgb16.hpp"
#include "library/controller-data.hpp"
#include "library/fileimage-patch.hpp"
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

#ifdef USE_LIBGCRYPT_SHA256
#include <gcrypt.h>
#endif

namespace
{
	uint16_t null_cover_fbmem[512 * 448];

	settingvar::variable<settingvar::model_bool<settingvar::yes_no>> savestate_no_check(lsnes_vset,
		"dont-check-savestate", "Movie‣Loading‣Don't check savestates", false);

	//Framebuffer.
	struct framebuffer::info null_fbinfo = {
		&framebuffer::pixfmt_bgr16,		//Format.
		(char*)null_cover_fbmem,	//Memory.
		512, 448, 1024,			//Physical size.
		512, 448, 1024,			//Logical size.
		0, 0				//Offset.
	};

	struct interface_device_reg null_registers[] = {
		{NULL, NULL, NULL}
	};

	struct _core_null : public core_core, public core_type, public core_region, public core_sysregion
	{
		_core_null() : core_core({}, {}), core_type({{
			.iname = "null",
			.hname = "(null)",
			.id = 9999,
			.sysname = "System",
			.bios = NULL,
			.regions = {this},
			.images = {},
			.settings = {},
			.core = this,
		}}), core_region({{"null", "(null)", 0, 0, false, {1, 60}, {0}}}),
		core_sysregion("null", *this, *this) { hide(); }
		std::string c_core_identifier() { return "null core"; }
		bool c_set_region(core_region& reg) { return true; }
		std::pair<unsigned, unsigned> c_video_rate() { return std::make_pair(60, 1); }
		double c_get_PAR() { return 1.0; }
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
		framebuffer::raw& c_draw_cover() {
			static framebuffer::raw x(null_fbinfo);
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
			return x;
		}
		std::pair<uint64_t, uint64_t> c_get_bus_map() { return std::make_pair(0ULL, 0ULL); }
		std::list<core_vma_info> c_vma_list() { return std::list<core_vma_info>(); }
		std::set<std::string> c_srams() { return std::set<std::string>(); }
		unsigned c_action_flags(unsigned id) { return 0; }
		int c_reset_action(bool hard) { return -1; }
		bool c_isnull() { return true; }
		void c_set_debug_flags(uint64_t addr, unsigned int sflags, unsigned int cflags) {}
		void c_set_cheat(uint64_t addr, uint64_t value, bool set) {}
		void c_debug_reset() {}
		std::vector<std::string> c_get_trace_cpus()
		{
			return std::vector<std::string>();
		}
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
				if(i->is_known_extension(ext) && i->get_core_identifier() == tmpprefer)
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

	struct fileimage::image::info get_xml_info()
	{
		fileimage::image::info i;
		i.type = fileimage::image::info::IT_MARKUP;
		i.headersize = 0;
		return i;
	}

	struct fileimage::image::info xlate_info(core_romimage_info ri)
	{
		fileimage::image::info i;
		if(ri.pass_mode == 0) i.type = fileimage::image::info::IT_MEMORY;
		if(ri.pass_mode == 1) i.type = fileimage::image::info::IT_FILE;
		i.headersize = ri.headersize;
		return i;
	}

	void record_files(loaded_rom& rom)
	{
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			try {
				record_filehash(rom.romimg[i].filename, rom.romimg[i].stripped,
					rom.romimg[i].sha_256.read());
			} catch(...) {}
			try {
				record_filehash(rom.romxml[i].filename, rom.romxml[i].stripped,
					rom.romxml[i].sha_256.read());
			} catch(...) {}
		}
	}
}

fileimage::hash lsnes_image_hasher;

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
	unsigned pmand = 0, tmand = 0;
	for(unsigned i = 0; i < ctype.get_image_count(); i++)
		tmand |= ctype.get_image_info(i).mandatory;
	if((bios = ctype.get_biosname()) != "") {
		//This thing has a BIOS.
		romidx = 1;
		std::string basename = lsnes_vset["firmwarepath"].str() + "/" + bios;
		romimg[0] = fileimage::image(lsnes_image_hasher, basename, "", xlate_info(ctype.get_image_info(0)));
		if(zip::file_exists(basename + ".xml"))
			romxml[0] = fileimage::image(lsnes_image_hasher, basename + ".xml", "", get_xml_info());
		pmand |= ctype.get_image_info(0).mandatory;
	}
	romimg[romidx] = fileimage::image(lsnes_image_hasher, file, "", xlate_info(ctype.get_image_info(romidx)));
	if(zip::file_exists(file + ".xml"))
		romxml[romidx] = fileimage::image(lsnes_image_hasher, file + ".xml", "", get_xml_info());
	pmand |= ctype.get_image_info(romidx).mandatory;
	msu1_base = zip::resolverel(file, "");
	record_files(*this);
	if(pmand != tmand)
		throw std::runtime_error("Required ROM images missing");
	return;
}

loaded_rom::loaded_rom(const std::string& file, const std::string& tmpprefer) throw(std::bad_alloc,
	std::runtime_error)
{
	std::istream& spec = zip::openrel(file, "");
	std::string s;
	std::getline(spec, s);
	istrip_CR(s);
	if(!spec || s != "[GAMEPACK FILE]") {
		//This is a Raw ROM image.
		regex_results tmp;
		std::string ext = regex(".*\\.([^.]*)?", file, "Can't read file extension")[1];
		core_type* coretype = find_core_by_extension(ext, tmpprefer);
		if(!coretype)
			(stringfmt() << "Extension '" << ext << "' unknown").throwex();
		rtype = coretype;
		region = orig_region = &rtype->get_preferred_region();
		unsigned romidx = 0;
		std::string bios;
		unsigned pmand = 0, tmand = 0;
		for(unsigned i = 0; i < rtype->get_image_count(); i++)
			tmand |= rtype->get_image_info(i).mandatory;
		if((bios = coretype->get_biosname()) != "") {
			//This thing has a BIOS.
			romidx = 1;
			std::string basename = lsnes_vset["firmwarepath"].str() + "/" + bios;
			romimg[0] = fileimage::image(lsnes_image_hasher, basename, "",
				xlate_info(coretype->get_image_info(0)));
			if(zip::file_exists(basename + ".xml"))
				romxml[0] = fileimage::image(lsnes_image_hasher, basename + ".xml", "",
					get_xml_info());
			pmand |= rtype->get_image_info(0).mandatory;
		}
		romimg[romidx] = fileimage::image(lsnes_image_hasher, file, "",
			xlate_info(coretype->get_image_info(romidx)));
		if(zip::file_exists(file + ".xml"))
			romxml[romidx] = fileimage::image(lsnes_image_hasher, file + ".xml", "", get_xml_info());
		pmand |= rtype->get_image_info(romidx).mandatory;
		msu1_base = zip::resolverel(file, "");
		record_files(*this);
		if(pmand != tmand)
			throw std::runtime_error("Required ROM images missing");
		return;
	}
	load_filename = file;
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
	std::string cromimg[ROM_SLOT_COUNT];
	std::string cromxml[ROM_SLOT_COUNT];
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
		romimg[i] = fileimage::image(lsnes_image_hasher, cromimg[i], file,
			xlate_info(rtype->get_image_info(i)));
		romxml[i] = fileimage::image(lsnes_image_hasher, cromxml[i], file, get_xml_info());
	}
	record_files(*this);	//Have to do this before patching.

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
		romimg[idx].patch(zip::readrel(tmp[3], file), offset);
	}

	//MSU-1 base.
	if(cromimg[1] != "")
		msu1_base = zip::resolverel(cromimg[1], file);
	else
		msu1_base = zip::resolverel(cromimg[0], file);
}

namespace
{
	bool filter_by_core(core_type& ctype, const std::string& core)
	{
		return (core == "" || ctype.get_core_identifier() == core);
	}

	bool filter_by_type(core_type& ctype, const std::string& type)
	{
		return (type == "" || ctype.get_iname() == type);
	}

	bool filter_by_region(core_type& ctype, const std::string& region)
	{
		if(region == "")
			return true;
		for(auto i : ctype.get_regions())
			if(i->get_iname() == region)
				return true;
		return false;
	}

	bool filter_by_extension(core_type& ctype, const std::string& file)
	{
		regex_results tmp = regex(".*\\.([^.]*)", file);
		if(!tmp)
			return false;
		std::string ext = tmp[1];
		return ctype.is_known_extension(ext);
	}

	bool filter_by_fileset(core_type& ctype, const std::string file[ROM_SLOT_COUNT])
	{
		uint32_t m = 0, t = 0;
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			if(i >= ctype.get_image_count() && file[i] != "")
				return false;
			auto s = ctype.get_image_info(i);
			if(file[i] != "")
				m |= s.mandatory;
			t |= s.mandatory;
		}
		return (m == t);
	}

	core_region* detect_region(core_type* t, const std::string& region)
	{
		core_region* r = NULL;
		for(auto i: t->get_regions())
			if(i->get_iname() == region)
				r = i;
		if(!r && region != "")
			(stringfmt() << "Not a valid system region '" << region << "'").throwex();
		if(!r) r = &t->get_preferred_region();	//Default region.
		return r;
	}
}

loaded_rom::loaded_rom(const std::string& file, const std::string& core, const std::string& type,
	const std::string& _region)
{
	core_type* t = NULL;
	core_region* r = NULL;
	bool fullspec = (core != "" && type != "");
	for(auto i : core_type::get_core_types()) {
		if(!filter_by_core(*i, core))
			continue;
		if(!filter_by_type(*i, type))
			continue;
		if(!fullspec && !filter_by_region(*i, _region))
			continue;
		if(!fullspec && !filter_by_extension(*i, file))
			continue;
		t = i;
	}
	if(!t) throw std::runtime_error("No matching core found");
	r = detect_region(t, _region);
	unsigned pmand = 0, tmand = 0;
	for(unsigned i = 0; i < t->get_image_count(); i++)
		tmand |= t->get_image_info(i).mandatory;
	std::string bios = t->get_biosname();
	unsigned romidx = (bios != "") ? 1 : 0;
	if(bios != "") {
		std::string basename = lsnes_vset["firmwarepath"].str() + "/" + bios;
		romimg[0] = fileimage::image(lsnes_image_hasher, basename, "", xlate_info(t->get_image_info(0)));
		if(zip::file_exists(basename + ".xml"))
			romxml[0] = fileimage::image(lsnes_image_hasher, basename + ".xml", "", get_xml_info());
		pmand |= t->get_image_info(0).mandatory;
	}
	romimg[romidx] = fileimage::image(lsnes_image_hasher, file, "", xlate_info(t->get_image_info(romidx)));
	if(zip::file_exists(file + ".xml"))
		romxml[romidx] = fileimage::image(lsnes_image_hasher, file + ".xml", "", get_xml_info());
	pmand |= t->get_image_info(romidx).mandatory;
	msu1_base = zip::resolverel(file, "");
	record_files(*this);
	if(pmand != tmand)
		throw std::runtime_error("Required ROM images missing");
	rtype = t;
	orig_region = region = r;
}

loaded_rom::loaded_rom(const std::string file[ROM_SLOT_COUNT], const std::string& core, const std::string& type,
	const std::string& _region)
{
	core_type* t = NULL;
	core_region* r = NULL;
	bool fullspec = (core != "" && type != "");
	for(auto i : core_type::get_core_types()) {
		if(!filter_by_core(*i, core)) {
			continue;
		}
		if(!filter_by_type(*i, type)) {
			continue;
		}
		if(!fullspec && !filter_by_region(*i, _region)) {
			continue;
		}
		if(!fullspec && !filter_by_fileset(*i, file)) {
			continue;
		}
		t = i;
	}
	if(!t) throw std::runtime_error("No matching core found");
	r = detect_region(t, _region);
	std::string bios = t->get_biosname();
	unsigned romidx = (bios != "") ? 1 : 0;
	unsigned pmand = 0, tmand = 0;
	for(unsigned i = 0; i < 27; i++) {
		if(i >= t->get_image_count())
			continue;
		if(file[i] != "")
			pmand |= t->get_image_info(i).mandatory;
		tmand |= t->get_image_info(i).mandatory;
		romimg[i] = fileimage::image(lsnes_image_hasher, file[i], "", xlate_info(t->get_image_info(i)));
		if(zip::file_exists(file[i] + ".xml"))
			romxml[i] = fileimage::image(lsnes_image_hasher, file[i] + ".xml", "", get_xml_info());
	}
	msu1_base = zip::resolverel(file[romidx], "");
	record_files(*this);
	if(pmand != tmand)
		throw std::runtime_error("Required ROM images missing");
	rtype = t;
	orig_region = region = r;
}

void loaded_rom::load(std::map<std::string, std::string>& settings, uint64_t rtc_sec, uint64_t rtc_subsec)
	throw(std::bad_alloc, std::runtime_error)
{
	core_type* old_type = current_rom_type;
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

	core_romimage images[ROM_SLOT_COUNT];
	for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
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
	//If core changes, unload the cartridge.
	if(old_core != current_rom_type->get_core())
		try {
			old_core->debug_reset();
			old_core->unload_cartridge();
		} catch(...) {}
	refresh_cart_mappings();
	notify_core_changed(old_type != current_rom_type);
}

std::map<std::string, std::vector<char>> load_sram_commandline(const std::vector<std::string>& cmdline)
	throw(std::bad_alloc, std::runtime_error)
{
	std::map<std::string, std::vector<char>> ret;
	regex_results opt;
	for(auto i : cmdline) {
		if(opt = regex("--continue=(.+)", i)) {
			zip::reader r(opt[1]);
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
				ret[opt[1]] = zip::readrel(opt[2], "");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::runtime_error& e) {
				throw std::runtime_error("Can't load SRAM '" + opt[1] + "': " + e.what());
			}
		}
	}
	return ret;
}

std::vector<char> loaded_rom::save_core_state(bool nochecksum) throw(std::bad_alloc, std::runtime_error)
{
	std::vector<char> ret;
	rtype->serialize(ret);
	if(nochecksum)
		return ret;
	size_t offset = ret.size();
	unsigned char tmp[32];
#ifdef USE_LIBGCRYPT_SHA256
	gcry_md_hash_buffer(GCRY_MD_SHA256, tmp, &ret[0], offset);
#else
	sha256::hash(tmp, ret);
#endif
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
	if(!savestate_no_check) {
		unsigned char tmp[32];
#ifdef USE_LIBGCRYPT_SHA256
		gcry_md_hash_buffer(GCRY_MD_SHA256, tmp, &buf[0], buf.size() - 32);
#else
		sha256::hash(tmp, reinterpret_cast<const uint8_t*>(&buf[0]), buf.size() - 32);
#endif
		if(memcmp(tmp, &buf[buf.size() - 32], 32))
			throw std::runtime_error("Savestate corrupt");
	}
	rtype->unserialize(&buf[0], buf.size() - 32);
}

void set_hasher_callback(std::function<void(uint64_t, uint64_t)> cb)
{
	lsnes_image_hasher.set_callback(cb);
}

std::map<std::string, core_type*> preferred_core;
std::string preferred_core_default;
