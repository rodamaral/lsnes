#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile-common.hpp"
#include "core/moviefile.hpp"
#include "library/binarystream.hpp"
#include "library/minmax.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <sstream>
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
#include <windows.h>
#endif

namespace
{
	std::map<std::string, std::string> read_settings(zip::reader& r)
	{
		std::map<std::string, std::string> x;
		for(auto i : r) {
			if(!regex_match("port[0-9]+|setting\\..+", i))
				continue;
			std::string s;
			std::string v;
			if(i.substr(0, 4) == "port")
				s = i;
			else
				s = i.substr(8);
			if(r.read_linefile(i, v, true))
				x[s] = v;
		}
		return x;
	}

	std::map<std::string, uint64_t> read_active_macros(zip::reader& r, const std::string& member)
	{
		std::map<std::string, uint64_t> x;
		if(!r.has_member(member))
			return x;
		std::istream& m = r[member];
		try {
			while(m) {
				std::string out;
				std::getline(m, out);
				istrip_CR(out);
				if(out == "")
					continue;
				regex_results rx = regex("([0-9]+) +(.*)", out);
				if(!rx) {
					messages << "Warning: Bad macro state: '" << out << "'" << std::endl;
					continue;
				}
				try {
					uint64_t f = parse_value<uint64_t>(rx[1]);
					x[rx[2]] = f;
				} catch(...) {
				}
			}
			delete &m;
		} catch(...) {
			delete &m;
			throw;
		}
		return x;
	}

	template<typename T> std::string pick_a_name(const std::map<std::string, T>& map, bool prefer_unnamed)
	{
		if(prefer_unnamed && !map.count(""))
			return "";
		size_t count = 1;
		while(true) {
			std::string c = (stringfmt() << "(unnamed branch #" << count++ << ")").str();
			if(!map.count(c))
				return c;
		}
	}

	void read_authors_file(zip::reader& r, std::vector<std::pair<std::string, std::string>>& authors)
		throw(std::bad_alloc, std::runtime_error)
	{
		std::istream& m = r["authors"];
		try {
			std::string x;
			while(std::getline(m, x)) {
				istrip_CR(x);
				auto g = split_author(x);
				authors.push_back(g);
			}
			delete &m;
		} catch(...) {
			delete &m;
			throw;
		}
	}

	std::string read_rrdata(zip::reader& r, std::vector<char>& out) throw(std::bad_alloc, std::runtime_error)
	{
		r.read_raw_file("rrdata", out);
		uint64_t count = rrdata_set::count(out);
		std::ostringstream x;
		x << count;
		return x.str();
	}

	void read_subtitles(zip::reader& r, const std::string& file, std::map<moviefile_subtiming, std::string>& x)
	{
		x.clear();
		if(!r.has_member(file))
			return;
		std::istream& m = r[file];
		try {
			while(m) {
				std::string out;
				std::getline(m, out);
				istrip_CR(out);
				auto r = regex("([0-9]+)[ \t]+([0-9]+)[ \t]+(.*)", out);
				if(!r)
					continue;
				x[moviefile_subtiming(parse_value<uint64_t>(r[1]), parse_value<uint64_t>(r[2]))] =
					subtitle_commentary::s_unescape(r[3]);
			}
			delete &m;
		} catch(...) {
			delete &m;
			throw;
		}
	}

	void read_input(zip::reader& r, const std::string& mname, controller_frame_vector& input)
		throw(std::bad_alloc, std::runtime_error)
	{
		controller_frame tmp = input.blank_frame(false);
		std::istream& m = r[mname];
		try {
			std::string x;
			while(std::getline(m, x)) {
				istrip_CR(x);
				if(x != "") {
					tmp.deserialize(x.c_str());
					input.append(tmp);
				}
			}
			delete &m;
		} catch(...) {
			delete &m;
			throw;
		}
	}

	void read_pollcounters(zip::reader& r, const std::string& file, std::vector<uint32_t>& pctr)
	{
		std::istream& m = r[file];
		try {
			std::string x;
			while(std::getline(m, x)) {
				istrip_CR(x);
				if(x != "") {
					int32_t y = parse_value<int32_t>(x);
					uint32_t z = 0;
					if(y < 0)
						z = -(y + 1);
					else {
						z = y;
						z |= 0x80000000UL;
					}
					pctr.push_back(z);
				}
			}
			delete &m;
		} catch(...) {
			delete &m;
			throw;
		}
	}

	std::string get_namefile(const std::string& input)
	{
		regex_results s;
		if(input == "input")
			return "branchname.0";
		else if(s = regex("input\\.([1-9][0-9]*)", input))
			return "branchname." + s[1];
		else
			return "";
	}
}

void moviefile::brief_info::load(zip::reader& r)
{
	std::string tmp;
	r.read_linefile("systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	r.read_linefile("gametype", sysregion);
	r.read_linefile("coreversion", corename);
	r.read_linefile("projectid", projectid);
	if(r.has_member("savestate")) {
		if(r.has_member("vicount"))
			r.read_numeric_file("vicount", current_frame);
		else
			r.read_numeric_file("saveframe", current_frame);
	} else
		current_frame = 0;
	r.read_numeric_file("rerecords", rerecords);
	r.read_linefile("rom.sha256", hash[0], true);
	r.read_linefile("romxml.sha256", hashxml[0], true);
	r.read_linefile("rom.hint", hint[0], true);
	unsigned base = 97;
	if(r.has_member("slot`.sha256"))
		base = 96;
	for(size_t i = 1; i < ROM_SLOT_COUNT; i++) {
		r.read_linefile((stringfmt() << "slot" << (char)(base + i - 1) << ".sha256").str(), hash[i],
			true);
		r.read_linefile((stringfmt() << "slot" << (char)(base + i - 1) << "xml.sha256").str(),
			hashxml[i], true);
		r.read_linefile((stringfmt() << "slot" << (char)(base + i - 1) << ".hint").str(), hint[i],
			true);
	}
}

void moviefile::load(zip::reader& r, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
{
	std::string tmp;
	r.read_linefile("systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	r.read_linefile("controlsversion", tmp);
	if(tmp != "0")
		throw std::runtime_error("Can't decode movie data");
	r.read_linefile("gametype", tmp);
	try {
		gametype = &romtype.lookup_sysregion(tmp);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Illegal game type '" + tmp + "'");
	}
	settings = read_settings(r);
	auto ctrldata = gametype->get_type().controllerconfig(settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex());

	vi_valid = true;
	vi_counter = 0;
	vi_this_frame = 0;

	branches.clear();
	r.read_linefile("gamename", gamename, true);
	r.read_linefile("projectid", projectid);
	rerecords = read_rrdata(r, c_rrdata);
	r.read_linefile("coreversion", coreversion);
	r.read_linefile("rom.sha256", romimg_sha256[0], true);
	r.read_linefile("romxml.sha256", romxml_sha256[0], true);
	r.read_linefile("rom.hint", namehint[0], true);
	unsigned base = 97;
	if(r.has_member("slot`.sha256"))
		base = 96;
	for(size_t i = 1; i < ROM_SLOT_COUNT; i++) {
		r.read_linefile((stringfmt() << "slot" << (char)(base + i - 1) << ".sha256").str(), romimg_sha256[i],
			true);
		r.read_linefile((stringfmt() << "slot" << (char)(base + i - 1) << "xml.sha256").str(),
			romxml_sha256[i], true);
		r.read_linefile((stringfmt() << "slot" << (char)(base + i - 1) << ".hint").str(), namehint[i],
			true);
	}
	read_subtitles(r, "subtitles", subtitles);
	movie_rtc_second = DEFAULT_RTC_SECOND;
	movie_rtc_subsecond = DEFAULT_RTC_SUBSECOND;
	r.read_numeric_file("starttime.second", movie_rtc_second, true);
	r.read_numeric_file("starttime.subsecond", movie_rtc_subsecond, true);
	rtc_second = movie_rtc_second;
	rtc_subsecond = movie_rtc_subsecond;
	if(r.has_member("savestate.anchor"))
		r.read_raw_file("savestate.anchor", anchor_savestate);
	if(r.has_member("savestate")) {
		vi_valid = false;
		if(r.has_member("vicounter")) {
			r.read_numeric_file("vicounter", vi_counter);
			r.read_numeric_file("vithisframe", vi_this_frame);
			vi_valid = true;
		}
		is_savestate = true;
		r.read_numeric_file("saveframe", save_frame, true);
		r.read_numeric_file("lagcounter", lagged_frames, true);
		read_pollcounters(r, "pollcounters", pollcounters);
		if(r.has_member("hostmemory"))
			r.read_raw_file("hostmemory", host_memory);
		r.read_raw_file("savestate", savestate);
		for(auto name : r)
			if(name.length() >= 5 && name.substr(0, 5) == "sram.")
				r.read_raw_file(name, sram[name.substr(5)]);
		r.read_raw_file("screenshot", screenshot);
		//If these can't be read, just use some (wrong) values.
		r.read_numeric_file("savetime.second", rtc_second, true);
		r.read_numeric_file("savetime.subsecond", rtc_subsecond, true);
		uint64_t _poll_flag = 2;	//Legacy behaviour is the default.
		r.read_numeric_file("pollflag", _poll_flag, true);
		poll_flag = _poll_flag;
		active_macros = read_active_macros(r, "macros");
	}
	for(auto name : r)
		if(name.length() >= 8 && name.substr(0, 8) == "initram.")
			 r.read_raw_file(name, ramcontent[name.substr(8)]);
	if(rtc_subsecond < 0 || movie_rtc_subsecond < 0)
		throw std::runtime_error("Invalid RTC subsecond value");
	std::string name = r.find_first();
	for(auto name : r)
		if(name.length() >= 10 && name.substr(0, 10) == "moviesram.")
			r.read_raw_file(name, movie_sram[name.substr(10)]);
	read_authors_file(r, authors);

	std::map<uint64_t, std::string> branch_table;
	//Load branch names.
	for(auto name : r) {
		regex_results s;
		if(s = regex("branchname\\.([0-9]+)", name)) {
			uint64_t n = parse_value<uint64_t>(s[1]);
			r.read_linefile(name, branch_table[n]);
			branches[branch_table[n]].clear(ports);
		}
	}

	for(auto name : r) {
		regex_results s;
		if(name == "input") {
			std::string bname = branch_table.count(0) ? branch_table[0] : pick_a_name(branches, true);
			if(!branches.count(bname)) branches[bname].clear(ports);
			read_input(r, name, branches[bname]);
			input = &branches[bname];
		} else if(s = regex("input\\.([1-9][0-9]*)", name)) {
			uint64_t n = parse_value<uint64_t>(s[1]);
			std::string bname = branch_table.count(n) ? branch_table[n] : pick_a_name(branches, false);
			if(!branches.count(bname)) branches[bname].clear(ports);
			read_input(r, name, branches[bname]);
		}
	}

	create_default_branch(ports);
}

moviefile_branch_extractor_text::moviefile_branch_extractor_text(const std::string& filename)
	: z(filename)
{
}

moviefile_branch_extractor_text::~moviefile_branch_extractor_text()
{
}

std::set<std::string> moviefile_branch_extractor_text::enumerate()
{
	std::set<std::string> r;
	for(auto& i : z) {
		std::string bname;
		std::string n = get_namefile(i);
		if(n != "") {
			if(z.has_member(n)) {
				z.read_linefile(n, bname);
				r.insert(bname);
			} else
				r.insert("");
		}
	}
	return r;
}

void moviefile_branch_extractor_text::read(const std::string& name, controller_frame_vector& v)
{
	std::set<std::string> r;
	bool done = false;
	for(auto& i : z) {
		std::string bname;
		std::string n = get_namefile(i);
		if(n != "") {
			std::string bname;
			if(z.has_member(n))
				z.read_linefile(n, bname);
			if(name == bname) {
				v.clear();
				read_input(z, i, v);
				done = true;
			}
		}
	}
	if(!done)
		(stringfmt() << "Can't find branch '" << name << "' in file.").throwex();
}
