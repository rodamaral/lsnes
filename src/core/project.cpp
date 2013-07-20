#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/inthread.hpp"
#include "core/project.hpp"
#include "core/misc.hpp"
#include "core/mainloop.hpp"
#include "core/memorywatch.hpp"
#include "core/moviefile.hpp"
#include "core/moviedata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/directory.hpp"
#include "library/string.hpp"
#include <fstream>
#include <dirent.h>
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
#define FUCKED_SYSTEM
#include <windows.h>
#endif

void do_flush_slotinfo();

namespace
{
	project_info* active_project = NULL;

	void concatenate(std::vector<char>& data, const std::vector<char>& app)
	{
		size_t dsize = data.size();
		data.resize(dsize + app.size());
		std::copy(app.begin(), app.end(), data.begin() + dsize);
	}

	std::vector<char> base_decode(const std::string& str)
	{
		std::vector<char> r;
		size_t len = str.length();
		size_t i;
		uint32_t a, b, c, d, e, v;
		for(i = 0; i + 5 <= len; i += 5) {
			a = str[i + 0] - 33;
			b = str[i + 1] - 33;
			c = str[i + 2] - 33;
			d = str[i + 3] - 33;
			e = str[i + 4] - 33;
			v = 52200625 * a + 614125 * b + 7225 * c + 85 * d + e;
			r.push_back(v);
			r.push_back(v >> 8);
			r.push_back(v >> 16);
			r.push_back(v >> 24);
		}
		a = b = c = d = e = 0;
		if(i + 0 < len) e = str[len - 1] - 33;
		if(i + 1 < len) d = str[len - 2] - 33;
		if(i + 2 < len) c = str[len - 3] - 33;
		if(i + 3 < len) b = str[len - 4] - 33;
		v = 614125 * b + 7225 * c + 85 * d + e;
		if(i + 1 < len) r.push_back(v);
		if(i + 2 < len) r.push_back(v >> 8);
		if(i + 3 < len) r.push_back(v >> 16);
		return r;
	}

	template<int x> void blockcode(std::ostringstream& s, const std::vector<char>& data, size_t& ptr,
		size_t& chars)
	{
		uint32_t a = 0, b = 0, c = 0, d = 0, v;
		if(x >= 4) a = static_cast<uint8_t>(data[ptr++]);
		if(x >= 3) b = static_cast<uint8_t>(data[ptr++]);
		if(x >= 2) c = static_cast<uint8_t>(data[ptr++]);
		if(x >= 1) d = static_cast<uint8_t>(data[ptr++]);
		v = 16777216 * d + 65536 * c + 256 * b + a;
		v >>= (32 - 8 * x);
		if(x >= 4) s << static_cast<char>(v / 52200625 % 85 + 33);
		if(x >= 3) s << static_cast<char>(v / 614125 % 85 + 33);
		if(x >= 2) s << static_cast<char>(v / 7225 % 85 + 33);
		if(x >= 1) s << static_cast<char>(v / 85 % 85 + 33);
		if(x >= 1) s << static_cast<char>(v % 85 + 33);
		chars -= (x + 1);
	}

	std::pair<std::string, size_t> base_encode(const std::vector<char>& data, size_t ptr, size_t chars)
	{
		std::ostringstream s;
		while(chars >= 5 && ptr + 4 <= data.size())
			blockcode<4>(s, data, ptr, chars);
		if(chars >= 4 && ptr + 3 <= data.size())
			blockcode<3>(s, data, ptr, chars);
		if(chars >= 3 && ptr + 2 <= data.size())
			blockcode<2>(s, data, ptr, chars);
		if(chars >= 2 && ptr + 1 <= data.size())
			blockcode<1>(s, data, ptr, chars);
		return std::make_pair(s.str(), ptr);
	}
	std::string eq_escape(const std::string& str)
	{
		std::ostringstream s;
		size_t len = str.length();
		for(size_t i = 0; i < len; i++) {
			if(str[i] == '\\')
				s << "\\\\";
			else if(str[i] == '=')
				s << "\\e";
			else
				s << str[i];
		}
		return s.str();
	}

	std::string eq_unescape(const std::string& str)
	{
		std::ostringstream s;
		size_t len = str.length();
		bool escape = false;
		for(size_t i = 0; i < len; i++) {
			if(escape) {
				if(str[i] == 'e')
					s << "=";
				else
					s << str[i];
				escape = false;
			} else {
				if(str[i] == '\\')
					escape = true;
				else
					s << str[i];
			}
		}
		return s.str();
	}

	void save_binary(std::ostream& s, const std::string& key, const std::vector<char>& value)
	{
		size_t ptr = 0;
		while(ptr < value.size()) {
			auto v = base_encode(value, ptr, 60);
			s << key << "=" << v.first << std::endl;
			ptr = v.second;
		}
	}

	void project_write(std::ostream& s, project_info& p)
	{
		s << p.name << std::endl;
		s << "rom=" << p.rom << std::endl;
		if(p.last_save != "")
			s << "last-save=" << p.last_save << std::endl;
		s << "directory=" << p.directory << std::endl;
		s << "prefix=" << p.prefix << std::endl;
		for(auto i : p.luascripts)
			s << "luascript=" << i << std::endl;
		s << "gametype=" << p.gametype << std::endl;
		s << "coreversion=" << p.coreversion << std::endl;
		if(p.gamename != "")
			s << "gamename=" << p.gamename << std::endl;
		s << "projectid=" << p.projectid << std::endl;
		s << "time=" << p.movie_rtc_second << ":" << p.movie_rtc_subsecond << std::endl;
		for(auto i : p.authors)
			s << "author=" << i.first << "|" << i.second << std::endl;
		for(unsigned i = 0; i < 27; i++) {
			if(p.romimg_sha256[i] != "") {
				if(i)
					s << "slotsha" << static_cast<char>(96 + i) << "=" << p.romimg_sha256[i]
						<< std::endl;
				else
					s << "romsha=" << p.romimg_sha256[i] << std::endl;
			}
			if(p.romxml_sha256[i] != "") {
				if(i)
					s << "slotxml" << static_cast<char>(96 + i) << "=" << p.romxml_sha256[i]
						<< std::endl;
				else
					s << "romxml=" << p.romxml_sha256[i] << std::endl;
			}
		}
		for(auto i : p.settings)
			s << "setting." << i.first << "=" << i.second << std::endl;
		for(auto i : p.watches)
			s << "watch." << eq_escape(i.first) << "=" << i.second << std::endl;
		for(auto i : p.macros)
			s << "macro." + i.first << "=" << i.second.serialize() << std::endl;
		for(auto i : p.movie_sram)
			save_binary(s, "sram." + i.first, i.second);
		if(p.anchor_savestate.size())
			save_binary(s, "anchor", p.anchor_savestate);
	}

	void fill_stub_movie(struct moviefile& m, struct project_info& p, struct core_type& coretype)
	{
		//Create a dummy movie.
		m.lazy_project_create = false;
		m.start_paused = true;
		m.movie_rtc_second = p.movie_rtc_second;
		m.movie_rtc_subsecond = p.movie_rtc_subsecond;
		m.save_frame = 0;
		m.lagged_frames = 0;
		m.anchor_savestate = p.anchor_savestate;
		m.movie_sram = p.movie_sram;
		m.authors = p.authors;
		for(unsigned i = 0; i < 27; i++) {
			m.romimg_sha256[i] = p.romimg_sha256[i];
			m.romxml_sha256[i] = p.romxml_sha256[i];
		}
		m.projectid = p.projectid;
		m.coreversion = p.coreversion;
		m.gamename = p.gamename;
		m.settings = p.settings;
		auto ctrldata = coretype.controllerconfig(m.settings);
		port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex());
		m.input.clear(ports);
		try {
			m.gametype = &coretype.lookup_sysregion(p.gametype);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			throw std::runtime_error("Illegal game type '" + p.gametype + "'");
		}
	}

	std::string project_getname(const std::string& id)
	{
		std::string file = get_config_path() + "/" + id + ".prj";
		std::ifstream f(file);
		if(!f)
			throw std::runtime_error("Can't open project file");
		std::string name;
		std::getline(f, name);
		if(!f)
			throw std::runtime_error("Can't read project name");
		return name;
	}
}

project_info& project_load(const std::string& id)
{
	std::string file = get_config_path() + "/" + id + ".prj";
	std::ifstream f(file);
	if(!f)
		throw std::runtime_error("Can't open project file");
	project_info& pi = *new project_info();
	pi.id = id;
	//First line is always project name.
	std::getline(f, pi.name);
	if(!f || pi.name == "") {
		delete &pi;
		throw std::runtime_error("Can't read project file");
	}
	while(f) {
		std::string tmp;
		std::getline(f, tmp);
		regex_results r;
		if(r = regex("rom=(.+)", tmp))
			pi.rom = r[1];
		else if(r = regex("last-save=(.+)", tmp))
			pi.last_save = r[1];
		else if(r = regex("directory=(.+)", tmp))
			pi.directory = r[1];
		else if(r = regex("prefix=(.+)", tmp))
			pi.prefix = r[1];
		else if(r = regex("luascript=(.+)", tmp))
			pi.luascripts.push_back(r[1]);
		else if(r = regex("gametype=(.+)", tmp))
			pi.gametype = r[1];
		else if(r = regex("coreversion=(.+)", tmp))
			pi.coreversion = r[1];
		else if(r = regex("gamename=(.+)", tmp))
			pi.gamename = r[1];
		else if(r = regex("projectid=(.+)", tmp))
			pi.projectid = r[1];
		else if(r = regex("projectid=([0-9]+):([0-9]+)", tmp)) {
			pi.movie_rtc_second = parse_value<int64_t>(r[1]);
			pi.movie_rtc_subsecond = parse_value<int64_t>(r[2]);
		} else if(r = regex("author=(.*)\\|(.*)", tmp))
			pi.authors.push_back(std::make_pair(r[1], r[2]));
		else if(r = regex("author=(.+)", tmp))
			pi.authors.push_back(std::make_pair(r[1], ""));
		else if(r = regex("romsha=([0-9a-f]+)", tmp))
			pi.romimg_sha256[0] = r[1];
		else if(r = regex("slotsha([a-z])=([0-9a-f]+)", tmp))
			pi.romimg_sha256[r[2][0] - 96] = r[2];
		else if(r = regex("romxml=([0-9a-f]+)", tmp))
			pi.romxml_sha256[0] = r[1];
		else if(r = regex("slotxml([a-z])=([0-9a-f]+)", tmp))
			pi.romxml_sha256[r[2][0] - 96] = r[2];
		else if(r = regex("setting.([^=]+)=(.*)", tmp))
			pi.settings[r[1]] = r[2];
		else if(r = regex("watch.([^=]+)=(.*)", tmp))
			pi.watches[eq_unescape(r[1])] = r[2];
		else if(r = regex("sram.([^=]+)=(.*)", tmp))
			concatenate(pi.movie_sram[r[1]], base_decode(r[2]));
		else if(r = regex("macro.([^=]+)=(.*)", tmp))
			try {
				pi.macros[r[1]] = JSON::node(r[2]);
			} catch(std::exception& e) {
				messages << "Unable to load macro '" << r[1] << "': " << e.what() << std::endl;
			}
		else if(r = regex("anchor=(.*)", tmp))
			concatenate(pi.anchor_savestate, base_decode(r[1]));
		else if(r = regex("time=([0-9]+):([0-9]+)", tmp)) {
			pi.movie_rtc_second = parse_value<int64_t>(r[1]);
			pi.movie_rtc_subsecond = parse_value<int64_t>(r[2]);
		}
	}
	return pi;
}

void project_flush(project_info* p)
{
	if(!p)
		return;
	std::string file = get_config_path() + "/" + p->id + ".prj";
	std::string tmpfile = get_config_path() + "/" + p->id + ".prj.tmp";
	std::string bakfile = get_config_path() + "/" + p->id + ".prj.bak";
	std::ofstream f(tmpfile);
	if(!f)
		throw std::runtime_error("Can't write project file");
	project_write(f, *p);
	if(!f)
		throw std::runtime_error("Can't write project file");
	f.close();

	std::ifstream f2(file);
	if(f2) {
		std::ofstream f3(bakfile);
		if(!f3)
			throw std::runtime_error("Can't backup project file");
		while(f2) {
			std::string tmp;
			std::getline(f2, tmp);
			f3 << tmp << std::endl;
		}
		f2.close();
		f3.close();
	}
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
	if(MoveFileEx(tmpfile.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING) < 0)
#else
	if(rename(tmpfile.c_str(), file.c_str()) < 0)
#endif
		throw std::runtime_error("Can't replace project file");
}

project_info* project_get()
{
	return active_project;
}

bool project_set(project_info* p, bool current)
{
	if(!p) {
		if(active_project)
			voicesub_unload_collection();
		active_project = p;
		notify_core_change();
		return true;
	}

	loaded_rom* newrom = NULL;
	moviefile newmovie;
	bool switched = false;
	try {
		if(current)
			goto skip_rom_movie;
		//First, try to load the ROM and the last movie file into RAM...
		newrom = new loaded_rom(p->rom, p->coreversion);
		if(newrom->rtype->get_iname() != p->coreversion) {
			messages << "Warning: Can't find matching core, using " << newrom->rtype->get_iname()
				<< std::endl;
		}
		if(p->last_save != "")
			try {
				newmovie = moviefile(p->last_save, *newrom->rtype);
			} catch(std::exception& e) {
				messages << "Warning: Can't load last save: " << e.what() << std::endl;
				fill_stub_movie(newmovie, *p, *newrom->rtype);
			}
		else {
			fill_stub_movie(newmovie, *p, *newrom->rtype);
		}
		//Okay, loaded, load into core.
		newrom->load(p->settings, p->movie_rtc_second, p->movie_rtc_subsecond);
		std::swap(our_rom, newrom);
		delete newrom;
		newrom = NULL;
		do_load_state(newmovie, LOAD_STATE_DEFAULT);
skip_rom_movie:
		active_project = p;
		switched = true;
		for(auto i : get_watches())
			if(p->watches.count(i))
				set_watchexpr_for(i, p->watches[i]);
			else
				set_watchexpr_for(i, "");
		voicesub_load_collection(p->directory + "/" + p->prefix + ".lsvs");
		lsnes_cmd.invoke("reset-lua");
		for(auto i : p->luascripts)
			lsnes_cmd.invoke("run-lua " + i);
		load_project_macros(controls, *active_project);
	} catch(std::exception& e) {
		messages << "Can't switch projects: " << e.what() << std::endl;
		delete newrom;
	}
	if(switched) {
		do_flush_slotinfo();
		update_movie_state();
		notify_core_change();
	}
	return switched;
}

std::map<std::string, std::string> project_enumerate()
{
	std::set<std::string> projects;
	std::map<std::string, std::string> projects2;

	projects = enumerate_directory(get_config_path(), ".*\\.prj");
	for(auto i : projects) {
		std::string id = i;
		size_t split;
#ifdef FUCKED_SYSTEM
		split = id.find_last_of("\\/");
#else
		split = id.find_last_of("/");
#endif
		if(split < id.length())
			id = id.substr(split + 1);
		id = id.substr(0, id.length() - 4);
		try {
			projects2[id] = project_getname(id);
		} catch(...) {
			messages << "Failed to load name for ID '" << id << "'" << std::endl;
		}
	}
	return projects2;
}

std::string project_moviepath()
{
	if(active_project)
		return active_project->directory;
	else
		return lsnes_vset["moviepath"].str();
}

std::string project_otherpath()
{
	if(active_project)
		return active_project->directory;
	else
		return ".";
}

std::string project_savestate_ext()
{
	return active_project ? "lss" : "lsmv";
}

void project_copy_watches(project_info& p)
{
	for(auto i : get_watches())
		p.watches[i] = get_watchexpr_for(i);
}

void project_copy_macros(project_info& p, controller_state& s)
{
	for(auto i : s.enumerate_macro())
		p.macros[i] = s.get_macro(i).serialize();
}
