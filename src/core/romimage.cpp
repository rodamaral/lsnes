#include "core/romimage.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"
#include "core/nullcore.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/zip.hpp"

fileimage::image rom_image::null_img;
fileimage::hash lsnes_image_hasher;

namespace
{
	core_type* prompt_core_fallback(const std::vector<core_type*>& choices)
	{
		if(choices.size() == 0)
			return NULL;
		if(choices.size() == 1)
			return choices[0];
		rom_request req;
		req.cores = choices;
		req.core_guessed = false;
		req.selected = 0;
		//Tell that all ROMs have been correctly guessed, leaving only core select.
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			req.has_slot[i] = false;
			req.guessed[i] = false;
		}
		req.canceled = false;
		graphics_driver_request_rom(req);
		if(req.canceled)
			throw std::runtime_error("Canceled ROM loading");
		if(req.selected < choices.size())
			return choices[req.selected];
		return choices[0];
	}

	core_type* find_core_by_extension(const std::string& ext, const std::string& tmpprefer)
	{
		std::string key = "ext:" + ext;
		std::list<core_type*> possible = core_type::get_core_types();
		core_type* preferred = preferred_core.count(key) ? preferred_core[key] : NULL;
		std::vector<core_type*> fallbacks;
		//Tmpprefer overrides normal preferred core.
		if(tmpprefer != "")
			for(auto i : possible)
				if(i->get_core_identifier() == tmpprefer)
					return i;
		for(auto i : possible)
			if(i->is_known_extension(ext)) {
				fallbacks.push_back(i);
				if(i == preferred)
					return i;
			}
		core_type* fallback = prompt_core_fallback(fallbacks);
		if(!fallback) throw std::runtime_error("No core available to load the ROM");
		return fallback;
	}

	core_type* find_core_by_name(const std::string& name, const std::string& tmpprefer)
	{
		std::string key = "type:" + name;
		std::list<core_type*> possible = core_type::get_core_types();
		std::vector<core_type*> fallbacks;
		core_type* preferred = preferred_core.count(key) ? preferred_core[key] : NULL;
		//Tmpprefer overrides normal preferred core.
		if(tmpprefer != "")
			for(auto i : possible)
				if(i->get_iname() == tmpprefer)
					return i;
		for(auto i : possible)
			if(i->get_iname() == name) {
				fallbacks.push_back(i);
				if(i == preferred)
					return i;
			}
		core_type* fallback = prompt_core_fallback(fallbacks);
		if(!fallback) throw std::runtime_error("No core available to load the ROM");
		return fallback;
	}

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

	void record_files(rom_image& rom)
	{
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			try {
				auto& j = rom.get_image(i, false);
				record_filehash(j.filename, j.stripped, j.sha_256.read());
			} catch(...) {}
			try {
				auto& j = rom.get_image(i, true);
				record_filehash(j.filename, j.stripped, j.sha_256.read());
			} catch(...) {}
		}
	}
}

void rom_image::setup_region(core_region& reg)
{
	if(!orig_region)
		orig_region = &reg;
}

rom_image::rom_image() throw()
{
	rtype = &get_null_type();
	region = orig_region = &get_null_region();
}

rom_image::rom_image(const std::string& file, core_type& ctype) throw(std::bad_alloc, std::runtime_error)
{
	rtype = &ctype;
	orig_region = &rtype->get_preferred_region();
	unsigned romidx = 0;
	std::string bios;
	unsigned pmand = 0, tmand = 0;
	for(unsigned i = 0; i < ctype.get_image_count(); i++)
		tmand |= ctype.get_image_info(i).mandatory;
	if((bios = ctype.get_biosname()) != "") {
		//This thing has a BIOS.
		romidx = 1;
		std::string basename = CORE().setcache->get("firmwarepath") + "/" + bios;
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

rom_image::rom_image(const std::string& file, const std::string& tmpprefer) throw(std::bad_alloc,
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
			std::string basename = CORE().setcache->get("firmwarepath") + "/" + bios;
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
	load_bundle(file, spec, tmpprefer);
}

void rom_image::load_bundle(const std::string& file, std::istream& spec, const std::string& tmpprefer) 
	throw(std::bad_alloc, std::runtime_error)
{
	std::string s;
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

rom_image::rom_image(const std::string& file, const std::string& core, const std::string& type,
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
		std::string basename = CORE().setcache->get("firmwarepath") + "/" + bios;
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

rom_image::rom_image(const std::string file[ROM_SLOT_COUNT], const std::string& core, const std::string& type,
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

bool rom_image::is_gamepak(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
{
	std::istream* spec = NULL;
	try {
		spec = &zip::openrel(filename, "");
		std::string line;
		std::getline(*spec, line);
		istrip_CR(line);
		bool ret = (line == "[GAMEPACK FILE]");
		delete spec;
		return ret;
	} catch(...) {
		delete spec;
		return false;
	}
}

void set_hasher_callback(std::function<void(uint64_t, uint64_t)> cb)
{
	lsnes_image_hasher.set_callback(cb);
}

std::map<std::string, core_type*> preferred_core;

