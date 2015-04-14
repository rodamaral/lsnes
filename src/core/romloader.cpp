#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/zip.hpp"

bool load_null_rom()
{
	auto& core = CORE();
	if(core.project->get()) {
		std::cerr << "Can't switch ROM with project active." << std::endl;
		return false;
	}
	loaded_rom newrom;
	*core.rom = newrom;
	if(*core.mlogic)
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			core.mlogic->get_mfile().romimg_sha256[i] = "";
			core.mlogic->get_mfile().romxml_sha256[i] = "";
			core.mlogic->get_mfile().namehint[i] = "";
		}
	core.dispatch->core_change();
	return true;
}

namespace
{
	void load_new_rom_inner(const romload_request& req)
	{
		auto& core = CORE();
		if(req.packfile != "") {
			messages << "Loading ROM " << req.packfile << std::endl;
			loaded_rom newrom(new rom_image(req.packfile));
			*core.rom = newrom;
			return;
		} else if(req.singlefile != "") {
			messages << "Loading ROM " << req.singlefile << std::endl;
			loaded_rom newrom(new rom_image(req.singlefile, req.core, req.system, req.region));
			*core.rom = newrom;
			return;
		} else {
			messages << "Loading multi-file ROM."  << std::endl;
			loaded_rom newrom(new rom_image(req.files, req.core, req.system, req.region));
			*core.rom = newrom;
			return;
		}
	}

	std::string call_rom(unsigned i, bool bios)
	{
		if(i == 0 && bios)
			return "BIOS";
		if(i == 26 && !bios)
			return "ROM @";
		if(bios) i--;
		char j[2] = {0, 0};
		j[0] = 'A' + i;
		return std::string("ROM ") + j;
	}

	void print_missing(core_type& t, unsigned present)
	{
		bool has_bios = (t.get_biosname() != "");
		unsigned total = 0;
		for(unsigned i = 0; i < t.get_image_count(); i++)
			total |= t.get_image_info(i).mandatory;
		unsigned bit = 1;
		std::string need = "";
		bool first = false;
		while(bit) {
			first = true;
			if((total & bit) && !(present & bit)) {
				if(need != "") need += ", ";
				for(unsigned i = 0; i < t.get_image_count(); i++) {
					if(t.get_image_info(i).mandatory & bit) {
						if(!first) need += "/";
						need += call_rom(i, has_bios);
						first = false;
					}
				}
			}
			bit <<= 1;
		}
		(stringfmt() << "Slots " << need << " are required.").throwex();
	}
}

bool _load_new_rom(const romload_request& req)
{
	auto& core = CORE();
	if(core.project->get()) {
		std::string msg = "Can't switch ROM with project active.";
		platform::error_message(msg);
		messages << msg << std::endl;
		return false;
	}
	try {
		load_new_rom_inner(req);
		if(*core.mlogic)
			for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
				auto& img = core.rom->get_rom(i);
				auto& xml = core.rom->get_markup(i);
				core.mlogic->get_mfile().romimg_sha256[i] = img.sha_256.read();
				core.mlogic->get_mfile().romxml_sha256[i] = xml.sha_256.read();
				core.mlogic->get_mfile().namehint[i] = img.namehint;
			}
	} catch(std::exception& e) {
		platform::error_message(std::string("Can't load ROM: ") + e.what());
		messages << "Can't reload ROM: " << e.what() << std::endl;
		return false;
	}
	messages << "Using core: " << core.rom->get_core_identifier() << std::endl;
	core.dispatch->core_change();
	return true;
}


bool reload_active_rom()
{
	auto& core = CORE();
	romload_request req;
	if(core.rom->isnull()) {
		platform::error_message("Can't reload ROM: No existing ROM");
		messages << "No ROM loaded" << std::endl;
		return false;
	}
	//Single-file ROM?
	std::string loadfile = core.rom->get_pack_filename();
	if(loadfile != "") {
		req.packfile = loadfile;
		return _load_new_rom(req);
	}
	//This is composite ROM.
	req.core = core.rom->get_core_identifier();
	req.system = core.rom->get_iname();
	req.region = core.rom->orig_region_get_iname();
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++)
		req.files[i] = core.rom->get_rom(i).filename;
	return _load_new_rom(req);
}

regex_results get_argument(const std::vector<std::string>& cmdline, const std::string& regexp)
{
	for(auto i : cmdline) {
		regex_results r;
		if(r = regex(regexp, i))
			return r;
	}
	return regex_results();
}

std::string get_requested_core(const std::vector<std::string>& cmdline)
{
	regex_results r;
	if(r = get_argument(cmdline, "--core=(.+)"))
		return r[1];
	return "";
};

rom_image_handle construct_rom_multifile(core_type* ctype, const moviefile::brief_info& info,
	const std::vector<std::string>& cmdline, bool have_movie)
{
	auto& core = CORE();
	std::string roms[ROM_SLOT_COUNT];
	std::string realcore = ctype->get_core_identifier();
	std::string realtype = ctype->get_iname();
	std::string bios = ctype->get_biosname();
	uint32_t pmand = 0, tmand = 0;
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		std::string optregex;
		bool isbios = false;
		std::string psetting;
		std::string romid;
		if(bios != "" && i == 0) {
			optregex = "--bios=(.*)";
			isbios = true;
			psetting = "firmwarepath";
			romid = "BIOS";
		} else {
			char j[2] = {0, 0};
			j[0] = i - ((bios != "") ? 1 : 0) + 'a';
			if(j[0] == 'a' + 26)
				j[0] = '@';
			optregex = std::string("--rom-") + j + "=(.*)";
			psetting = "rompath";
			j[0] = i - ((bios != "") ? 1 : 0) + 'A';
			if(j[0] == 'A' + 26)
				j[0] = '@';
			romid = std::string("ROM ") + j;
		}
		regex_results r = get_argument(cmdline, optregex);
		if(i >= ctype->get_image_count()) {
			if(r)
				throw std::runtime_error("This ROM type has no " + romid);
			else
				continue;
		}
		if(info.hash[i] == "" && have_movie && r)
			throw std::runtime_error("This movie has no " + romid);

		auto img = ctype->get_image_info(i);
		tmand |= img.mandatory;
		if(r) {
			//Explicitly specified, use that.
			roms[i] = r[1];
		} else if(info.hint[i] != "") {
			//Try to use hint.
			std::set<std::string> exts = img.extensions;
			for(auto j : exts) {
				std::string candidate = core.setcache->get(psetting) + "/" + info.hint[i] +
					"." + j;
				if(zip::file_exists(candidate)) {
					roms[i] = candidate;
					break;
				}
			}
		}
		if(isbios && roms[i] == "" && i == 0) {
			//Fallback default.
			roms[0] = core.setcache->get("firmwarepath") + "/" + bios;
		}
		if(roms[i] == "" && info.hash[i] != "")
			roms[i] = try_to_guess_rom(info.hint[i], info.hash[i], info.hashxml[i], *ctype, i);
		if(roms[i] == "" && info.hash[i] != "")
			throw std::runtime_error("Can't find " + romid + " (specify explicitly)");
		if(roms[i] != "")
			pmand |= img.mandatory;
		if(roms[i] != "" && !zip::file_exists(roms[i]))
			throw std::runtime_error(romid + " points to nonexistent file (" + roms[i] + ")");
	}
	if(pmand != tmand)
		print_missing(*ctype, pmand);
	return new rom_image(roms, realcore, realtype, "");
}

rom_image_handle construct_rom_nofile(const std::vector<std::string>& cmdline)
{
	std::string requested_core = get_requested_core(cmdline);
	//Handle bundle / single-file ROMs.
	for(auto i : cmdline) {
		regex_results r;
		if(r = regex("--rom=(.*)", i)) {
			//Okay, load as ROM bundle and check validity.
			return new rom_image(r[1], requested_core);
		}
	}

	for(auto i : core_core::all_cores())
		if(i->get_core_shortname() == requested_core || requested_core == "")
			goto valid;
	throw std::runtime_error("Specified unsupported core");
valid:	;

	//Multi-file ROMs.
	regex_results _type = get_argument(cmdline, "--rom-type=(.*)");
	if(!_type)
		return rom_image_handle();  //NULL rom.
	core_type* ctype = NULL;
	for(auto i : core_type::get_core_types()) {
		if(i->get_iname() != _type[1])
			continue;
		if(i->get_core_shortname() != requested_core && requested_core != "")
			continue;
		ctype = i;
	}
	if(!ctype)
		throw std::runtime_error("Specified impossible core/type combination");

	moviefile::brief_info info;
	return construct_rom_multifile(ctype, info, cmdline, false);
}


rom_image_handle construct_rom(const std::string& movie_filename, const std::vector<std::string>& cmdline)
{
	if(movie_filename == "")
		return construct_rom_nofile(cmdline);
	moviefile::brief_info info(movie_filename);
	std::string requested_core = get_requested_core(cmdline);
	auto sysregs = core_sysregion::find_matching(info.sysregion);
	if(sysregs.empty())
		throw std::runtime_error("No core supports '" + info.sysregion + "'");

	//Default to correct core.
	if(requested_core == "") {
		for(auto i : core_core::all_cores())
			if(i->get_core_identifier() == info.corename)
				requested_core = i->get_core_shortname();
	}

	//Handle bundle / single-file ROMs.
	for(auto i : cmdline) {
		regex_results r;
		if(r = regex("--rom=(.*)", i)) {
			//Okay, load as ROM bundle and check validity.
			auto cr = new rom_image(r[1], requested_core);
			for(auto j : sysregs) {
				if(cr->is_of_type(j->get_type()))
					continue;
				for(auto k : cr->get_regions())
					if(k == &j->get_region())
						goto valid;
			}
			throw std::runtime_error("Specified ROM is of wrong type ('" +
				cr->get_hname() + "') for movie ('" + info.sysregion + ")");
valid:
			return cr;
		}
	}

	//Multi-ROM case.
	core_type* ctype = NULL;
	for(auto i : sysregs) {
		ctype = &i->get_type();
		if(ctype->get_core_shortname() == requested_core)
			break;
	}
	if(requested_core != "" && ctype->get_core_shortname() != requested_core)
		throw std::runtime_error("Specified incomplatible or unsupported core");
	if(requested_core == "")
		messages << "Will use '" << ctype->get_core_identifier() << "'" << std::endl;
	return construct_rom_multifile(ctype, info, cmdline, true);
}
