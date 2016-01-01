#include "cmdhelp/lua.hpp"
#include "cmdhelp/project.hpp"
#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/emustatus.hpp"
#include "core/instance.hpp"
#include "core/inthread.hpp"
#include "core/mainloop.hpp"
#include "core/memorywatch.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/project.hpp"
#include "core/queue.hpp"
#include "core/window.hpp"
#include "library/directory.hpp"
#include "library/minmax.hpp"
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

	void fill_stub_movie(struct moviefile& m, struct project_info& p, struct core_type& coretype)
	{
		//Create a dummy movie.
		m.lazy_project_create = false;
		m.start_paused = true;
		m.movie_rtc_second = p.movie_rtc_second;
		m.movie_rtc_subsecond = p.movie_rtc_subsecond;
		m.anchor_savestate = p.anchor_savestate;
		m.movie_sram = p.movie_sram;
		m.authors = p.authors;
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			m.romimg_sha256[i] = p.romimg_sha256[i];
			m.romxml_sha256[i] = p.romxml_sha256[i];
			m.namehint[i] = p.namehint[i];
		}
		m.projectid = p.projectid;
		m.coreversion = p.coreversion;
		m.gamename = p.gamename;
		m.settings = p.settings;
		auto ctrldata = coretype.controllerconfig(m.settings);
		portctrl::type_set& ports = portctrl::type_set::make(ctrldata.ports, ctrldata.portindex());
		m.create_default_branch(ports);
		m.clear_dynstate();
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

project_state::project_state(voice_commentary& _commentary, memwatch_set& _mwatch, command::group& _command,
	controller_state& _controls, settingvar::group& _setgroup, button_mapping& _buttons,
	emulator_dispatch& _edispatch, input_queue& _iqueue, loaded_rom& _rom, status_updater& _supdater)
	: commentary(_commentary), mwatch(_mwatch), command(_command), controls(_controls), setgroup(_setgroup),
	buttons(_buttons), edispatch(_edispatch), iqueue(_iqueue), rom(_rom), supdater(_supdater),
	branch_ls(command, CPROJECT::bls, [this]() { this->do_branch_ls(); }),
	branch_mk(command, CPROJECT::bmk, [this](const std::string& a) { this->do_branch_mk(a); }),
	branch_rm(command, CPROJECT::brm, [this](const std::string& a) { this->do_branch_rm(a); }),
	branch_set(command, CPROJECT::bset, [this](const std::string& a) { this->do_branch_set(a); }),
	branch_rp(command, CPROJECT::brp, [this](const std::string& a) { this->do_branch_rp(a); }),
	branch_mv(command, CPROJECT::bmv, [this](const std::string& a) { this->do_branch_mv(a); })
{
	active_project = NULL;
}

project_state::~project_state()
{
}

project_info& project_state::load(const std::string& id)
{
	std::string file = get_config_path() + "/" + id + ".prj";
	std::ifstream f(file);
	if(!f)
		throw std::runtime_error("Can't open project file");
	project_info& pi = *new project_info(edispatch);
	pi.id = id;
	pi.movie_rtc_second = 1000000000;
	pi.movie_rtc_subsecond = 0;
	pi.active_branch = 0;
	pi.next_branch = 0;
	pi.filename = file;
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
			pi.romimg_sha256[r[1][0] - 96] = r[2];
		else if(r = regex("romxml=([0-9a-f]+)", tmp))
			pi.romxml_sha256[0] = r[1];
		else if(r = regex("slotxml([a-z])=([0-9a-f]+)", tmp))
			pi.romxml_sha256[r[1][0] - 96] = r[2];
		else if(r = regex("romhint=(.*)", tmp))
			pi.namehint[0] = r[1];
		else if(r = regex("slothint([a-z])=(.*)", tmp))
			pi.namehint[r[1][0] - 96] = r[2];
		else if(r = regex("romrom=(.*)", tmp))
			pi.roms[0] = r[1];
		else if(r = regex("slotrom([a-z])=(.*)", tmp))
			pi.roms[r[1][0] - 96] = r[2];
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
		} else if(r = regex("branch([1-9][0-9]*)parent=([0-9]+)", tmp)) {
			uint64_t bid = parse_value<int64_t>(r[1]);
			uint64_t pbid = parse_value<int64_t>(r[2]);
			if(!pi.branches.count(bid))
				pi.branches[bid].name = "(Unnamed branch)";
			pi.branches[bid].pbid = pbid;
		} else if(r = regex("branch([1-9][0-9]*)name=(.*)", tmp)) {
			uint64_t bid = parse_value<int64_t>(r[1]);
			if(!pi.branches.count(bid))
				pi.branches[bid].pbid = 0;
			pi.branches[bid].name = r[2];
		} else if(r = regex("branchcurrent=([0-9]+)", tmp)) {
			pi.active_branch = parse_value<int64_t>(r[1]);
		} else if(r = regex("branchnext=([0-9]+)", tmp)) {
			pi.next_branch = parse_value<int64_t>(r[1]);
		}
	}
	for(auto& i : pi.branches) {
		uint64_t j = i.first;
		uint64_t m = j;
		while(j) {
			j = pi.branches[j].pbid;
			m = min(m, j);
			if(j == i.first) {
				//Cyclic dependency!
				messages << "Warning: Cyclic slot branch dependency, reparenting '" <<
					pi.branches[m].name << "' to be child of root..." << std::endl;
				pi.branches[j].pbid = 0;
				break;
			}
		}
	}
	if(pi.active_branch && !pi.branches.count(pi.active_branch)) {
		messages << "Warning: Current slot branch does not exist, using root..." << std::endl;
		pi.active_branch = 0;
	}
	return pi;
}

project_info* project_state::get()
{
	return active_project;
}

bool project_state::set(project_info* p, bool current)
{
	if(!p) {
		if(active_project)
			commentary.unload_collection();
		active_project = p;
		edispatch.core_change();
		edispatch.branch_change();
		return true;
	}

	loaded_rom newrom;
	moviefile* newmovie = NULL;
	bool switched = false;
	std::set<core_sysregion*> sysregs;
	bool used = false;
	try {
		if(current)
			goto skip_rom_movie;

		sysregs = core_sysregion::find_matching(p->gametype);
		if(sysregs.empty())
			throw std::runtime_error("No core supports '" + p->gametype + "'");

		//First, try to load the ROM and the last movie file into RAM...
		if(p->rom != "") {
			rom_image_handle _img(new rom_image(p->rom, p->coreversion));
			newrom = loaded_rom(_img);
		} else {
			core_type* ctype = NULL;
			for(auto i : sysregs) {
				ctype = &i->get_type();
				if(ctype->get_core_identifier() == p->coreversion)
					break;
			}
			rom_image_handle _img(new rom_image(p->roms, ctype->get_core_identifier(), ctype->get_iname(),
				""));
			newrom = loaded_rom(_img);
		}
		if(newrom.get_core_identifier() != p->coreversion) {
			messages << "Warning: Can't find matching core, using " << newrom.get_core_identifier()
				<< std::endl;
		}
		if(p->last_save != "")
			try {
				newmovie = new moviefile(p->last_save, newrom.get_internal_rom_type());
			} catch(std::exception& e) {
				messages << "Warning: Can't load last save: " << e.what() << std::endl;
				newmovie = new moviefile();
				fill_stub_movie(*newmovie, *p, newrom.get_internal_rom_type());
			}
		else {
			newmovie = new moviefile();
			fill_stub_movie(*newmovie, *p, newrom.get_internal_rom_type());
		}
		//Okay, loaded, load into core.
		newrom.load(p->settings, p->movie_rtc_second, p->movie_rtc_subsecond);
		rom = newrom;
		do_load_state(*newmovie, LOAD_STATE_DEFAULT, used);
skip_rom_movie:
		active_project = p;
		switched = true;
		//Calculate union of old and new.
		std::set<std::string> _watches = mwatch.enumerate();
		for(auto i : p->watches) _watches.insert(i.first);

		for(auto i : _watches)
			try {
				if(p->watches.count(i))
					mwatch.set(i, p->watches[i]);
				else
					mwatch.clear(i);
			} catch(std::exception& e) {
				messages << "Can't set/clear watch '" << i << "': " << e.what() << std::endl;
			}
		commentary.load_collection(p->directory + "/" + p->prefix + ".lsvs");
		command.invoke(CLUA::reset.name);
		for(auto i : p->luascripts)
			command.invoke(CLUA::run.name + (" " + i));
		buttons.load(controls, *active_project);
	} catch(std::exception& e) {
		if(newmovie && !used)
			delete newmovie;
		platform::error_message(std::string("Can't switch projects: ") + e.what());
		messages << "Can't switch projects: " << e.what() << std::endl;
	}
	if(switched) {
		do_flush_slotinfo();
		supdater.update();
		edispatch.core_change();
		edispatch.branch_change();
	}
	return switched;
}

std::map<std::string, std::string> project_state::enumerate()
{
	std::set<std::string> projects;
	std::map<std::string, std::string> projects2;

	projects = directory::enumerate(get_config_path(), ".*\\.prj");
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

std::string project_state::moviepath()
{
	if(active_project)
		return active_project->directory;
	else
		return SET_moviepath(setgroup);
}

std::string project_state::otherpath()
{
	if(active_project)
		return active_project->directory;
	else
		return ".";
}

std::string project_state::savestate_ext()
{
	return active_project ? "lss" : "lsmv";
}

void project_state::copy_watches(project_info& p)
{
	for(auto i : mwatch.enumerate()) {
		try {
			p.watches[i] = mwatch.get_string(i);
		} catch(std::exception& e) {
			messages << "Can't read memory watch '" << i << "': " << e.what() << std::endl;
		}
	}
}

void project_state::copy_macros(project_info& p, controller_state& s)
{
	for(auto i : s.enumerate_macro())
		p.macros[i] = s.get_macro(i).serialize();
}

project_info::project_info(emulator_dispatch& _dispatch)
	: edispatch(_dispatch)
{
}

uint64_t project_info::get_parent_branch(uint64_t bid)
{
	if(!bid)
		return 0;
	if(!branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	return branches[bid].pbid;
}

void project_info::set_current_branch(uint64_t bid)
{
	if(bid && !branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	active_branch = bid;
	edispatch.branch_change();
	messages << "Set current slot branch to " << get_branch_string() << std::endl;
}

const std::string& project_info::get_branch_name(uint64_t bid)
{
	static std::string rootname = "(root)";
	if(!bid)
		return rootname;
	if(!branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	return branches[bid].name;
}

void project_info::set_branch_name(uint64_t bid, const std::string& name)
{
	if(!bid)
		throw std::runtime_error("Root branch name can't be set");
	if(!branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	branches[bid].name = name;
	edispatch.branch_change();
}

void project_info::set_parent_branch(uint64_t bid, uint64_t pbid)
{
	if(!bid)
		throw std::runtime_error("Root branch never has parent");
	if(!branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	if(pbid && !branches.count(pbid))
		throw std::runtime_error("Invalid parent branch ID");
	if(bid == pbid)
		throw std::runtime_error("Branch can't be its own parent");
	for(auto& i : branches) {
		uint64_t j = i.first;
		while(j) {
			j = (j == bid) ? pbid : branches[j].pbid;
			if(j == i.first)
				throw std::runtime_error("Reparenting would create a circular dependency");
		}
	}
	branches[bid].pbid = pbid;
	edispatch.branch_change();
}

std::set<uint64_t> project_info::branch_children(uint64_t bid)
{
	if(bid && !branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	std::set<uint64_t> r;
	for(auto& i : branches)
		if(i.second.pbid == bid)
			r.insert(i.first);
	return r;
}

uint64_t project_info::create_branch(uint64_t pbid, const std::string& name)
{
	if(pbid && !branches.count(pbid))
		throw std::runtime_error("Invalid parent branch ID");
	uint64_t assign_bid = next_branch;
	uint64_t last_bid = (branches.empty() ? 1 : branches.rbegin()->first + 1);
	assign_bid = max(assign_bid, last_bid);
	branches[assign_bid].name = name;
	branches[assign_bid].pbid = pbid;
	next_branch = assign_bid + 1;
	edispatch.branch_change();
	return assign_bid;
}

void project_info::delete_branch(uint64_t bid)
{
	if(!bid)
		throw std::runtime_error("Root branch can not be deleted");
	if(bid == active_branch)
		throw std::runtime_error("Current branch can't be deleted");
	if(!branches.count(bid))
		throw std::runtime_error("Invalid branch ID");
	for(auto& i : branches)
		if(i.second.pbid == bid)
			throw std::runtime_error("Can't delete branch with children");
	branches.erase(bid);
	edispatch.branch_change();
}

std::string project_info::get_branch_string()
{
	std::string r;
	uint64_t j = active_branch;
	if(!j)
		return "(root)";
	while(j) {
		if(r == "")
			r = get_branch_name(j);
		else
			r = get_branch_name(j) + "→" + r;
		j = get_parent_branch(j);
	}
	return r;
}

void project_info::flush()
{
	std::string file = get_config_path() + "/" + id + ".prj";
	std::string tmpfile = get_config_path() + "/" + id + ".prj.tmp";
	std::string bakfile = get_config_path() + "/" + id + ".prj.bak";
	std::ofstream f(tmpfile);
	if(!f)
		throw std::runtime_error("Can't write project file");
	write(f);
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

void project_info::write(std::ostream& s)
{
	s << name << std::endl;
	s << "rom=" << rom << std::endl;
	if(last_save != "")
		s << "last-save=" << last_save << std::endl;
	s << "directory=" << directory << std::endl;
	s << "prefix=" << prefix << std::endl;
	for(auto i : luascripts)
		s << "luascript=" << i << std::endl;
	s << "gametype=" << gametype << std::endl;
	s << "coreversion=" << coreversion << std::endl;
	if(gamename != "")
		s << "gamename=" << gamename << std::endl;
	s << "projectid=" << projectid << std::endl;
	s << "time=" << movie_rtc_second << ":" << movie_rtc_subsecond << std::endl;
	for(auto i : authors)
		s << "author=" << i.first << "|" << i.second << std::endl;
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		if(romimg_sha256[i] != "") {
			if(i)
				s << "slotsha" << static_cast<char>(96 + i) << "=" << romimg_sha256[i] << std::endl;
			else
				s << "romsha=" << romimg_sha256[i] << std::endl;
		}
		if(romxml_sha256[i] != "") {
			if(i)
				s << "slotxml" << static_cast<char>(96 + i) << "=" << romxml_sha256[i] << std::endl;
			else
				s << "romxml=" << romxml_sha256[i] << std::endl;
		}
		if(namehint[i] != "") {
			if(i)
				s << "slothint" << static_cast<char>(96 + i) << "=" << namehint[i] << std::endl;
			else
				s << "romhint=" << namehint[i] << std::endl;
		}
		if(roms[i] != "") {
			if(i)
				s << "slotrom" << static_cast<char>(96 + i) << "=" << roms[i] << std::endl;
			else
				s << "romrom=" << roms[i] << std::endl;
		}
	}
	for(auto i : settings)
		s << "setting." << i.first << "=" << i.second << std::endl;
	for(auto i : watches)
		s << "watch." << eq_escape(i.first) << "=" << i.second << std::endl;
	for(auto i : macros)
		s << "macro." + i.first << "=" << i.second.serialize() << std::endl;
	for(auto i : movie_sram)
		save_binary(s, "sram." + i.first, i.second);
	if(anchor_savestate.size())
		save_binary(s, "anchor", anchor_savestate);
	for(auto& i : branches) {
		s << "branch" << i.first << "parent=" << i.second.pbid << std::endl;
		s << "branch" << i.first << "name=" << i.second.name << std::endl;
	}
	s << "branchcurrent=" << active_branch << std::endl;
	s << "branchnext=" << next_branch << std::endl;
}

void project_state::do_branch_mk(const std::string& args)
{
	regex_results r = regex("([0-9]+)[ \t]+(.*)", args);
	if(!r) {
		messages << "Syntax: create-branch <parentid> <name>" << std::endl;
		return;
	}
	try {
		auto prj = get();
		uint64_t pbid = parse_value<uint64_t>(r[1]);
		if(!prj)
			throw std::runtime_error("Not in project context");
		uint64_t bid = prj->create_branch(pbid, r[2]);
		messages << "Created branch #" << bid << std::endl;
		prj->flush();
	} catch(std::exception& e) {
		messages << "Can't create new branch: " << e.what() << std::endl;
	}
}

void project_state::do_branch_rm(const std::string& args)
{
	regex_results r = regex("([0-9]+)[ \t]*", args);
	if(!r) {
		messages << "Syntax: delete-branch <id>" << std::endl;
		return;
	}
	try {
		auto prj = get();
		uint64_t bid = parse_value<uint64_t>(r[1]);
		if(!prj)
			throw std::runtime_error("Not in project context");
		prj->delete_branch(bid);
		messages << "Deleted branch #" << bid << std::endl;
		prj->flush();
	} catch(std::exception& e) {
		messages << "Can't delete branch: " << e.what() << std::endl;
	}
}

void project_state::do_branch_set(const std::string& args)
{
	regex_results r = regex("([0-9]+)[ \t]*", args);
	if(!r) {
		messages << "Syntax: set-branch <id>" << std::endl;
		return;
	}
	try {
		auto prj = get();
		uint64_t bid = parse_value<uint64_t>(r[1]);
		if(!prj)
			throw std::runtime_error("Not in project context");
		prj->set_current_branch(bid);
		messages << "Set current branch to #" << bid << std::endl;
		prj->flush();
		supdater.update();
	} catch(std::exception& e) {
		messages << "Can't set branch: " << e.what() << std::endl;
	}
}

void project_state::do_branch_rp(const std::string& args)
{
	regex_results r = regex("([0-9]+)[ \t]+([0-9]+)[ \t]*", args);
	if(!r) {
		messages << "Syntax: reparent-branch <id> <newpid>" << std::endl;
		return;
	}
	try {
		auto prj = get();
		uint64_t bid = parse_value<uint64_t>(r[1]);
		uint64_t pbid = parse_value<uint64_t>(r[2]);
		if(!prj)
			throw std::runtime_error("Not in project context");
		prj->set_parent_branch(bid, pbid);
		messages << "Reparented branch #" << bid << std::endl;
		prj->flush();
		supdater.update();
	} catch(std::exception& e) {
		messages << "Can't reparent branch: " << e.what() << std::endl;
	}
}

void project_state::do_branch_mv(const std::string& args)
{
	regex_results r = regex("([0-9]+)[ \t]+(.*)", args);
	if(!r) {
		messages << "Syntax: rename-branch <id> <name>" << std::endl;
		return;
	}
	try {
		auto prj = get();
		uint64_t bid = parse_value<uint64_t>(r[1]);
		if(!prj)
			throw std::runtime_error("Not in project context");
		prj->set_branch_name(bid, r[2]);
		messages << "Renamed branch #" << bid << std::endl;
		prj->flush();
		supdater.update();
	} catch(std::exception& e) {
		messages << "Can't rename branch: " << e.what() << std::endl;
	}
}

void project_state::do_branch_ls()
{
	std::set<unsigned> dset;
	recursive_list_branch(0, dset, 0, false);
}

void project_state::recursive_list_branch(uint64_t bid, std::set<unsigned>& dset, unsigned depth, bool last_of)
{
	auto prj = get();
	if(!prj) {
		messages << "Not in project context." << std::endl;
		return;
	}
	std::set<uint64_t> children = prj->branch_children(bid);
	std::string prefix;
	for(unsigned i = 0; i + 1 < depth; i++)
		prefix += (dset.count(i) ? "\u2502" : " ");
	prefix += (dset.count(depth - 1) ? (last_of ? "\u2514" : "\u251c") : " ");
	if(last_of) dset.erase(depth - 1);
	messages << prefix
		<< ((bid == prj->get_current_branch()) ? "*" : "")
		<< bid << ":" << prj->get_branch_name(bid) << std::endl;
	dset.insert(depth);
	size_t c = 0;
	for(auto i : children) {
		bool last = (++c == children.size());
		recursive_list_branch(i, dset, depth + 1, last);
	}
	dset.erase(depth);
}
