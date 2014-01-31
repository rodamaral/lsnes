#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rrdata.hpp"
#include "library/zip.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/serialization.hpp"
#include "library/binarystream.hpp"
#include "interface/romtype.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
#include <windows.h>
#endif

#define DEFAULT_RTC_SECOND 1000000000ULL
#define DEFAULT_RTC_SUBSECOND 0ULL

namespace
{
	std::map<std::string, moviefile> memory_saves;
}

enum lsnes_movie_tags
{
	TAG_ANCHOR_SAVE = 0xf5e0fad7,
	TAG_AUTHOR = 0xafff97b4,
	TAG_CORE_VERSION = 0xe4344c7e,
	TAG_GAMENAME = 0xe80d6970,
	TAG_HOSTMEMORY = 0x3bf9d187,
	TAG_MACRO = 0xd261338f,
	TAG_MOVIE = 0xf3dca44b,
	TAG_MOVIE_SRAM = 0xbbc824b7,
	TAG_MOVIE_TIME = 0x18c3a975,
	TAG_PROJECT_ID = 0x359bfbab,
	TAG_ROMHASH = 0x0428acfc,
	TAG_RRDATA = 0xa3a07f71,
	TAG_SAVE_SRAM = 0xae9bfb2f,
	TAG_SAVESTATE = 0x2e5bc2ac,
	TAG_SCREENSHOT = 0xc6760d0e,
	TAG_SUBTITLE = 0x6a7054d3,
	TAG_RAMCONTENT = 0xd3ec3770,
	TAG_ROMHINT = 0x6f715830
};

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

	template<typename target>
	void write_settings(target& w, const std::map<std::string, std::string>& settings,
		core_setting_group& sgroup, std::function<void(target& w, const std::string& name,
		const std::string& value)> writefn)
	{
		for(auto i : settings) {
			if(!sgroup.settings.count(i.first))
				continue;
			if(sgroup.settings.find(i.first)->second.dflt == i.second)
				continue;
			writefn(w, i.first, i.second);
		}
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

	void write_active_macros(zip::writer& w, const std::string& member, const std::map<std::string, uint64_t>& ma)
	{
		if(ma.empty())
			return;
		std::ostream& m = w.create_file(member);
		try {
			for(auto i : ma)
				m << i.second << " " << i.first << std::endl;
			if(!m)
				throw std::runtime_error("Can't write ZIP file member");
			w.close_file();
		} catch(...) {
			w.close_file();
			throw;
		}
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
	uint64_t count = rrdata.count(out);
	std::ostringstream x;
	x << count;
	return x.str();
}

void write_rrdata(zip::writer& w) throw(std::bad_alloc, std::runtime_error)
{
	uint64_t count;
	std::vector<char> out;
	count = rrdata.write(out);
	w.write_raw_file("rrdata", out);
	std::ostream& m2 = w.create_file("rerecords");
	try {
		m2 << count << std::endl;
		if(!m2)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void write_authors_file(zip::writer& w, std::vector<std::pair<std::string, std::string>>& authors)
	throw(std::bad_alloc, std::runtime_error)
{
	std::ostream& m = w.create_file("authors");
	try {
		for(auto i : authors)
			if(i.second == "")
				m << i.first << std::endl;
			else
				m << i.first << "|" << i.second << std::endl;
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void write_input(zip::writer& w, controller_frame_vector& input)
	throw(std::bad_alloc, std::runtime_error)
{
	std::ostream& m = w.create_file("input");
	try {
		char buffer[MAX_SERIALIZED_SIZE];
		for(size_t i = 0; i < input.size(); i++) {
			input[i].serialize(buffer);
			m << buffer << std::endl;
		}
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
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
				s_unescape(r[3]);
		}
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
}

void write_subtitles(zip::writer& w, const std::string& file, std::map<moviefile_subtiming, std::string>& x)
{
	std::ostream& m = w.create_file(file);
	try {
		for(auto i : x)
			m << i.first.get_frame() << " " << i.first.get_length() << " " << s_escape(i.second)
				<< std::endl;
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

void read_input(zip::reader& r, controller_frame_vector& input, unsigned version) throw(std::bad_alloc,
	std::runtime_error)
{
	controller_frame tmp = input.blank_frame(false);
	std::istream& m = r["input"];
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

void write_pollcounters(zip::writer& w, const std::string& file, const std::vector<uint32_t>& pctr)
{
	std::ostream& m = w.create_file(file);
	try {
		for(auto i : pctr) {
			int32_t x = i & 0x7FFFFFFFUL;
			if((i & 0x80000000UL) == 0)
				x = -x - 1;
			m << x << std::endl;
		}
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

moviefile::brief_info::brief_info(const std::string& filename)
{
	regex_results rr;
	if(rr = regex("\\$MEMORY:(.*)", filename)) {
		if(!memory_saves.count(rr[1]))
			throw std::runtime_error("No such memory save");
		moviefile& mv = memory_saves[rr[1]];
		sysregion = mv.gametype->get_name();
		corename = mv.coreversion;
		projectid = mv.projectid;
		current_frame = mv.is_savestate ? mv.save_frame : 0;
		rerecords = mv.rerecords_mem;
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			hash[i] = mv.romimg_sha256[i];
			hashxml[i] = mv.romxml_sha256[i];
			hint[i] = mv.namehint[i];
		}
		return;
	}
	{
		std::istream& s = zip::openrel(filename, "");
		char buf[6] = {0};
		s.read(buf, 5);
		if(!strcmp(buf, "lsmv\x1A")) {
			binary_io(s);
			delete &s;
			return;
		}
		delete &s;
	}
	zip::reader r(filename);
	std::string tmp;
	r.read_linefile("systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	r.read_linefile("gametype", sysregion);
	r.read_linefile("coreversion", corename);
	r.read_linefile("projectid", projectid);
	if(r.has_member("savestate"))
		r.read_numeric_file("saveframe", current_frame);
	else
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

void moviefile::brief_info::binary_io(std::istream& _stream)
{
	binarystream::input in(_stream);
	sysregion = in.string();
	//Discard the settings.
	while(in.byte()) {
		in.string();
		in.string();
	}
	in.extension({
		{TAG_CORE_VERSION, [this](binarystream::input& s) {
			this->corename = s.string_implicit();
		}},{TAG_PROJECT_ID, [this](binarystream::input& s) {
			this->projectid = s.string_implicit();
		}},{TAG_SAVESTATE, [this](binarystream::input& s) {
			this->current_frame = s.number();
		}},{TAG_RRDATA, [this](binarystream::input& s) {
			std::vector<char> c_rrdata;
			s.blob_implicit(c_rrdata);
			this->rerecords = rrdata.count(c_rrdata);
		}},{TAG_ROMHASH, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				this->hashxml[n >> 1] = h;
			else
				this->hash[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			this->hint[n] = h;
		}}
	}, binarystream::null_default);
}

moviefile::moviefile() throw(std::bad_alloc)
{
	static port_type_set dummy_types;
	force_corrupt = false;
	gametype = NULL;
	coreversion = "";
	projectid = "";
	rerecords = "0";
	is_savestate = false;
	movie_rtc_second = rtc_second = DEFAULT_RTC_SECOND;
	movie_rtc_subsecond = rtc_subsecond = DEFAULT_RTC_SUBSECOND;
	start_paused = false;
	lazy_project_create = true;
	poll_flag = 0;
}

moviefile::moviefile(const std::string& movie, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
{
	regex_results rr;
	if(rr = regex("\\$MEMORY:(.*)", movie)) {
		if(!memory_saves.count(rr[1]))
			throw std::runtime_error("No such memory save");
		*this = memory_saves[rr[1]];
		return;
	}
	poll_flag = false;
	start_paused = false;
	force_corrupt = false;
	is_savestate = false;
	lazy_project_create = false;
	std::string tmp;
	{
		std::istream& s = zip::openrel(movie, "");
		char buf[6] = {0};
		s.read(buf, 5);
		if(!strcmp(buf, "lsmv\x1A")) {
			binary_io(s, romtype);
			delete &s;
			return;
		}
		delete &s;
	}
	zip::reader r(movie);
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

	input.clear(ports);
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
	read_input(r, input, 0);
}

void moviefile::save(const std::string& movie, unsigned compression, bool binary) throw(std::bad_alloc,
	std::runtime_error)
{
	regex_results rr;
	if(rr = regex("\\$MEMORY:(.*)", movie)) {
		memory_saves[rr[1]] = *this;
		return;
	}
	if(binary) {
		std::string tmp = movie + ".tmp";
		std::ofstream strm(tmp.c_str(), std::ios_base::binary);
		if(!strm)
			throw std::runtime_error("Can't open output file");
		char buf[5] = {'l', 's', 'm', 'v', 0x1A};
		strm.write(buf, 5);
		if(!strm)
			throw std::runtime_error("Failed to write to output file");
		binary_io(strm);
		if(!strm)
			throw std::runtime_error("Failed to write to output file");
		strm.close();
		std::string backup = movie + ".backup";
		zip::rename_overwrite(movie.c_str(), backup.c_str());
		if(zip::rename_overwrite(tmp.c_str(), movie.c_str()) < 0)
			throw std::runtime_error("Can't rename '" + tmp + "' -> '" + movie + "'");
		return;
	}
	zip::writer w(movie, compression);
	save(w);
}

void moviefile::save(std::ostream& stream) throw(std::bad_alloc, std::runtime_error)
{
	zip::writer w(stream, 0);
	save(w);
}

void moviefile::save(zip::writer& w) throw(std::bad_alloc, std::runtime_error)
{
	w.write_linefile("gametype", gametype->get_name());
	write_settings<zip::writer>(w, settings, gametype->get_type().get_settings(), [](zip::writer& w,
		const std::string& name, const std::string& value) -> void {
			if(regex_match("port[0-9]+", name))
				w.write_linefile(name, value);
			else
				w.write_linefile("setting." + name, value);
		});
	w.write_linefile("gamename", gamename, true);
	w.write_linefile("systemid", "lsnes-rr1");
	w.write_linefile("controlsversion", "0");
	coreversion = gametype->get_type().get_core_identifier();
	w.write_linefile("coreversion", coreversion);
	w.write_linefile("projectid", projectid);
	write_rrdata(w);
	w.write_linefile("rom.sha256", romimg_sha256[0], true);
	w.write_linefile("romxml.sha256", romxml_sha256[0], true);
	w.write_linefile("rom.hint", namehint[0], true);
	for(size_t i = 1; i < ROM_SLOT_COUNT; i++) {
		w.write_linefile((stringfmt() << "slot" << (char)(96 + i) << ".sha256").str(), romimg_sha256[i],
			true);
		w.write_linefile((stringfmt() << "slot" << (char)(96 + i) << "xml.sha256").str(), romxml_sha256[i],
			true);
		w.write_linefile((stringfmt() << "slot" << (char)(96 + i) << ".hint").str(), namehint[i],
			true);
	}
	write_subtitles(w, "subtitles", subtitles);
	for(auto i : movie_sram)
		w.write_raw_file("moviesram." + i.first, i.second);
	w.write_numeric_file("starttime.second", movie_rtc_second);
	w.write_numeric_file("starttime.subsecond", movie_rtc_subsecond);
	if(!anchor_savestate.empty())
			w.write_raw_file("savestate.anchor", anchor_savestate);
	if(is_savestate) {
		w.write_numeric_file("saveframe", save_frame);
		w.write_numeric_file("lagcounter", lagged_frames);
		write_pollcounters(w, "pollcounters", pollcounters);
		w.write_raw_file("hostmemory", host_memory);
		w.write_raw_file("savestate", savestate);
		w.write_raw_file("screenshot", screenshot);
		for(auto i : sram)
			w.write_raw_file("sram." + i.first, i.second);
		w.write_numeric_file("savetime.second", rtc_second);
		w.write_numeric_file("savetime.subsecond", rtc_subsecond);
		w.write_numeric_file("pollflag", poll_flag);
		write_active_macros(w, "macros", active_macros);
	}
	for(auto i : ramcontent)
		w.write_raw_file("initram." + i.first, i.second);
	write_authors_file(w, authors);
	write_input(w, input);
	w.commit();
}

/*
Following need to be saved: 
- gametype (string)
- settings (string name, value pairs)
- gamename (optional string)
- core version (string)
- project id (string
- rrdata (blob)
- ROM hashes (2*27 table of optional strings)
- Subtitles (list of number,number,string)
- SRAMs (dictionary string->blob.)
- Starttime (number,number)
- Anchor savestate (optional blob)
- Save frame (savestate-only, numeric).
- Lag counter (savestate-only, numeric).
- pollcounters (savestate-only, vector of numbers).
- hostmemory (savestate-only, blob).
- screenshot (savestate-only, blob).
- Save SRAMs (savestate-only, dictionary string->blob.)
- Save time (savestate-only, number,number)
- Poll flag (savestate-only, boolean)
- Macros (savestate-only, ???)
- Authors (list of string,string).
- Input (blob).
- Extensions (???)
*/
void moviefile::binary_io(std::ostream& _stream) throw(std::bad_alloc, std::runtime_error)
{
	binarystream::output out(_stream);
	out.string(gametype->get_name());
	write_settings<binarystream::output>(out, settings, gametype->get_type().get_settings(),
		[](binarystream::output& s, const std::string& name, const std::string& value) -> void {
			s.byte(0x01);
			s.string(name);
			s.string(value);
		});
	out.byte(0x00);

	out.extension(TAG_MOVIE_TIME, [this](binarystream::output& s) {
		s.number(this->movie_rtc_second);
		s.number(this->movie_rtc_subsecond);
	});

	out.extension(TAG_PROJECT_ID, [this](binarystream::output& s) {
		s.string_implicit(this->projectid);
	});

	out.extension(TAG_CORE_VERSION, [this](binarystream::output& s) {
		this->coreversion = this->gametype->get_type().get_core_identifier();
		s.string_implicit(this->coreversion);
	});

	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		out.extension(TAG_ROMHASH, [this, i](binarystream::output& s) {
			if(!this->romimg_sha256[i].length()) return;
			s.byte(2 * i);
			s.string_implicit(this->romimg_sha256[i]);
		});
		out.extension(TAG_ROMHASH, [this, i](binarystream::output& s) {
			if(!this->romxml_sha256[i].length()) return;
			s.byte(2 * i + 1);
			s.string_implicit(this->romxml_sha256[i]);
		});
		out.extension(TAG_ROMHINT, [this, i](binarystream::output& s) {
			if(!this->namehint[i].length()) return;
			s.byte(i);
			s.string_implicit(this->namehint[i]);
		});
	}

	out.extension(TAG_RRDATA, [this](binarystream::output& s) {
		uint64_t count;
		std::vector<char> rrd;
		count = rrdata.write(rrd);
		s.blob_implicit(rrd);
	});
	
	for(auto i : movie_sram)
		out.extension(TAG_MOVIE_SRAM, [&i](binarystream::output& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});

	out.extension(TAG_ANCHOR_SAVE, [this](binarystream::output& s) {
		s.blob_implicit(this->anchor_savestate);
	});
	if(is_savestate) {
		out.extension(TAG_SAVESTATE, [this](binarystream::output& s) {
			s.number(this->save_frame);
			s.number(this->lagged_frames);
			s.number(this->rtc_second);
			s.number(this->rtc_subsecond);
			s.number(this->pollcounters.size());
			for(auto i : this->pollcounters)
				s.number32(i);
			s.byte(this->poll_flag ? 0x01 : 0x00);
			s.blob_implicit(this->savestate);
		}, true, out.numberbytes(save_frame) + out.numberbytes(lagged_frames) + out.numberbytes(rtc_second) +
			out.numberbytes(rtc_subsecond) + out.numberbytes(pollcounters.size()) +
			4 * pollcounters.size() + 1 + savestate.size());

		out.extension(TAG_HOSTMEMORY, [this](binarystream::output& s) {
			s.blob_implicit(this->host_memory);
		});

		out.extension(TAG_SCREENSHOT, [this](binarystream::output& s) {
			s.blob_implicit(this->screenshot);
		}, true, screenshot.size());

		for(auto i : sram) {
			out.extension(TAG_SAVE_SRAM, [&i](binarystream::output& s) {
				s.string(i.first);
				s.blob_implicit(i.second);
			});
		}
	}

	out.extension(TAG_GAMENAME, [this](binarystream::output& s) {
		s.string_implicit(this->gamename);
	});

	for(auto i : subtitles)
		out.extension(TAG_SUBTITLE, [&i](binarystream::output& s) {
			s.number(i.first.get_frame());
			s.number(i.first.get_length());
			s.string_implicit(i.second);
		});

	for(auto i : authors)
		out.extension(TAG_AUTHOR, [&i](binarystream::output& s) {
			s.string(i.first);
			s.string_implicit(i.second);
		});

	for(auto i : active_macros)
		out.extension(TAG_MACRO, [&i](binarystream::output& s) {
			s.number(i.second);
			s.string_implicit(i.first);
		});

	for(auto i : ramcontent) {
		out.extension(TAG_RAMCONTENT, [&i](binarystream::output& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});
	}

	out.extension(TAG_MOVIE, [this](binarystream::output& s) {
		input.save_binary(s);
	}, true, input.binary_size());
}

void moviefile::binary_io(std::istream& _stream, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
{
	binarystream::input in(_stream);
	std::string tmp = in.string();
	try {
		gametype = &romtype.lookup_sysregion(tmp);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Illegal game type '" + tmp + "'");
	}
	while(in.byte()) {
		std::string name = in.string();
		settings[name] = in.string();
	}
	auto ctrldata = gametype->get_type().controllerconfig(settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex());
	input.clear(ports);

	in.extension({
		{TAG_ANCHOR_SAVE, [this](binarystream::input& s) {
			s.blob_implicit(this->anchor_savestate);
		}},{TAG_AUTHOR, [this](binarystream::input& s) {
			std::string a = s.string();
			std::string b = s.string_implicit();
			this->authors.push_back(std::make_pair(a, b));
		}},{TAG_CORE_VERSION, [this](binarystream::input& s) {
			this->coreversion = s.string_implicit();
		}},{TAG_GAMENAME, [this](binarystream::input& s) {
			this->gamename = s.string_implicit();
		}},{TAG_HOSTMEMORY, [this](binarystream::input& s) {
			s.blob_implicit(this->host_memory);
		}},{TAG_MACRO, [this](binarystream::input& s) {
			uint64_t n = s.number();
			this->active_macros[s.string_implicit()] = n;
		}},{TAG_MOVIE, [this](binarystream::input& s) {
			input.load_binary(s);
		}},{TAG_MOVIE_SRAM, [this](binarystream::input& s) {
			std::string a = s.string();
			s.blob_implicit(this->movie_sram[a]);
		}},{TAG_RAMCONTENT, [this](binarystream::input& s) {
			std::string a = s.string();
			s.blob_implicit(this->ramcontent[a]);
		}},{TAG_MOVIE_TIME, [this](binarystream::input& s) {
			this->movie_rtc_second = s.number();
			this->movie_rtc_subsecond = s.number();
		}},{TAG_PROJECT_ID, [this](binarystream::input& s) {
			this->projectid = s.string_implicit();
		}},{TAG_ROMHASH, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				romxml_sha256[n >> 1] = h;
			else
				romimg_sha256[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			namehint[n] = h;
		}},{TAG_RRDATA, [this](binarystream::input& s) {
			s.blob_implicit(this->c_rrdata);
			this->rerecords = (stringfmt() << rrdata.count(c_rrdata)).str();
		}},{TAG_SAVE_SRAM, [this](binarystream::input& s) {
			std::string a = s.string();
			s.blob_implicit(this->sram[a]);
		}},{TAG_SAVESTATE, [this](binarystream::input& s) {
			this->is_savestate = true;
			this->save_frame = s.number();
			this->lagged_frames = s.number();
			this->rtc_second = s.number();
			this->rtc_subsecond = s.number();
			this->pollcounters.resize(s.number());
			for(auto& i : this->pollcounters)
				i = s.number32();
			this->poll_flag = (s.byte() != 0);
			s.blob_implicit(this->savestate);
		}},{TAG_SCREENSHOT, [this](binarystream::input& s) {
			s.blob_implicit(this->screenshot);
		}},{TAG_SUBTITLE, [this](binarystream::input& s) {
			uint64_t f = s.number();
			uint64_t l = s.number();
			std::string x = s.string_implicit();
			this->subtitles[moviefile_subtiming(f, l)] = x;
		}}
	}, binarystream::null_default);
}

uint64_t moviefile::get_frame_count() throw()
{
	return input.count_frames();
}

namespace
{
	const int BLOCK_SECONDS = 0;
	const int BLOCK_FRAMES = 1;
	const int STEP_W = 2;
	const int STEP_N = 3;
}

uint64_t moviefile::get_movie_length() throw()
{
	uint64_t frames = get_frame_count();
	if(!gametype) {
		return 100000000ULL * frames / 6;
	}
	uint64_t _magic[4];
	gametype->fill_framerate_magic(_magic);
	uint64_t t = _magic[BLOCK_SECONDS] * 1000000000ULL * (frames / _magic[BLOCK_FRAMES]);
	frames %= _magic[BLOCK_FRAMES];
	t += frames * _magic[STEP_W] + (frames * _magic[STEP_N] / _magic[BLOCK_FRAMES]);
	return t;
}

moviefile& moviefile::memref(const std::string& slot)
{
	return memory_saves[slot];
}

namespace
{
	void emerg_write_bytes(int handle, const uint8_t* d, size_t dsize)
	{
		while(dsize > 0) {
			ssize_t r = write(handle, d, dsize);
			if(r > 0) {
				d += r;
				dsize -= r;
			}
		}
	}
	void emerg_write_number(int handle, uint64_t num)
	{
		uint8_t data[10];
		size_t len = 0;
		do {
			bool cont = (num > 127);
			data[len++] = (cont ? 0x80 : 0x00) | (num & 0x7F);
			num >>= 7;
		} while(num);
		emerg_write_bytes(handle, data, len);
	}
	size_t number_size(uint64_t num)
	{
		unsigned len = 0;
		do {
			num >>= 7;
			len++;
		} while(num);
		return len;
	}
	void emerg_write_number32(int handle, uint32_t num)
	{
		char buf[4];
		serialization::u32b(buf, num);
		emerg_write_bytes(handle, (const uint8_t*)buf, 4);
	}
	void emerg_write_member(int handle, uint32_t tag, uint64_t size)
	{
		emerg_write_number32(handle, 0xaddb2d86);
		emerg_write_number32(handle, tag);
		emerg_write_number(handle, size);
	}
	void emerg_write_blob_implicit(int handle, const std::vector<char>& v)
	{
		emerg_write_bytes(handle, (const uint8_t*)&v[0], v.size());
	}
	void emerg_write_byte(int handle, uint8_t byte)
	{
		emerg_write_bytes(handle, &byte, 1);
	}
	size_t string_size(const std::string& str)
	{
		return number_size(str.length()) + str.length();
	}
	void emerg_write_string_implicit(int handle, const std::string& str)
	{
		for(size_t i = 0; i < str.length(); i++)
			emerg_write_byte(handle, str[i]);
	}
	void emerg_write_string(int handle, const std::string& str)
	{
		emerg_write_number(handle, str.length());
		emerg_write_string_implicit(handle, str);
	}
	uint64_t append_number(char* ptr, uint64_t n)
	{
		unsigned digits = 0;
		uint64_t n2 = n;
		do {
			digits++;
			n2 /= 10;
		} while(n2);
		for(unsigned i = digits; i > 0; i--) {
			ptr[i - 1] = (n % 10) + '0';
			n /= 10;
		}
		ptr[digits] = 0;
	}
}

void emerg_save_movie(const moviefile& mv, const controller_frame_vector& v)
{
	//Whee, assume state of the emulator is totally busted.
	if(!mv.gametype)
		return;  //No valid movie. Trying to save would segfault.
	char header[] = {'l', 's', 'm', 'v', '\x1a'};
	int fd;
	char filename_buf[512];
	int number = 1;
name_again:
	filename_buf[0] = 0;
	strcpy(filename_buf + strlen(filename_buf), "crashsave-");
	append_number(filename_buf + strlen(filename_buf), time(NULL));
	strcpy(filename_buf + strlen(filename_buf), "-");
	append_number(filename_buf + strlen(filename_buf), number++);
	strcpy(filename_buf + strlen(filename_buf), ".lsmv");
	fd = open(filename_buf, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if(fd < 0 && errno == EEXIST) goto name_again;
	if(fd < 0) return;  //Can't open.
	//Headers.
	emerg_write_bytes(fd, (const uint8_t*)header, sizeof(header));
	emerg_write_string(fd, mv.gametype->get_name());
	for(auto& i : mv.settings) {
		emerg_write_byte(fd, 1);
		emerg_write_string(fd, i.first);
		emerg_write_string(fd, i.second);
	}
	emerg_write_byte(fd, 0);
	//The actual movie.
	uint64_t pages = v.get_page_count();
	uint64_t stride = v.get_stride();
	uint64_t pageframes = v.get_frames_per_page();
	uint64_t vsize = v.size();
	emerg_write_member(fd, TAG_MOVIE, vsize * stride);
	size_t pagenum = 0;
	while(vsize > 0) {
		uint64_t count = (vsize > pageframes) ? pageframes : vsize;
		size_t bytes = count * stride;
		const unsigned char* content = v.get_page_buffer(pagenum++);
		emerg_write_bytes(fd, content, bytes);
		vsize -= count;
	}
	//Movie starting time.
	emerg_write_member(fd, TAG_MOVIE_TIME, number_size(mv.movie_rtc_second) +
		number_size(mv.movie_rtc_subsecond));
	emerg_write_number(fd, mv.movie_rtc_second);
	emerg_write_number(fd, mv.movie_rtc_subsecond);
	//Project id.
	emerg_write_member(fd, TAG_PROJECT_ID, mv.projectid.length());
	emerg_write_string_implicit(fd, mv.projectid);
	//starting SRAM.
	for(auto& i : mv.movie_sram) {
		emerg_write_member(fd, TAG_MOVIE_SRAM, string_size(i.first) + i.second.size());
		emerg_write_string(fd, i.first);
		emerg_write_blob_implicit(fd, i.second);
	}
	//Anchor save.
	emerg_write_member(fd, TAG_ANCHOR_SAVE, mv.anchor_savestate.size());
	emerg_write_blob_implicit(fd, mv.anchor_savestate);
	//RRDATA.
	emerg_write_member(fd, TAG_RRDATA, rrdata.size_emerg());
	rrdata_set::esave_state estate;
	while(true) {
		char buf[4096];
		size_t w = rrdata.write_emerg(estate, buf, sizeof(buf));
		if(!w) break;
		emerg_write_bytes(fd, (const uint8_t*)buf, w);
	}
	//Core version.
	emerg_write_member(fd, TAG_CORE_VERSION, mv.coreversion.length());
	emerg_write_string_implicit(fd, mv.coreversion);
	//ROM slots data.
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		if(mv.romimg_sha256[i].length()) {
			emerg_write_member(fd, TAG_ROMHASH, mv.romimg_sha256[i].length() + 1);
			emerg_write_byte(fd, 2 * i);
			emerg_write_string_implicit(fd, mv.romimg_sha256[i]);
		}
		if(mv.romxml_sha256[i].length()) {
			emerg_write_member(fd, TAG_ROMHASH, mv.romxml_sha256[i].length() + 1);
			emerg_write_byte(fd, 2 * i + 1);
			emerg_write_string_implicit(fd, mv.romxml_sha256[i]);
		}
		if(mv.namehint[i].length()) {
			emerg_write_member(fd, TAG_ROMHINT, mv.namehint[i].length() + 1);
			emerg_write_byte(fd, i);
			emerg_write_string_implicit(fd, mv.namehint[i]);
		}
	}
	//Game name.
	emerg_write_member(fd, TAG_GAMENAME, mv.gamename.size());
	emerg_write_string_implicit(fd, mv.gamename);
	//Subtitles.
	for(auto& i : mv.subtitles) {
		emerg_write_member(fd, TAG_SUBTITLE, number_size(i.first.get_frame()) +
			number_size(i.first.get_length()) + i.second.length());
		emerg_write_number(fd, i.first.get_frame());
		emerg_write_number(fd, i.first.get_length());
		emerg_write_string_implicit(fd, i.second);
	}
	//Authors.
	for(auto& i : mv.authors) {
		emerg_write_member(fd, TAG_AUTHOR, string_size(i.first) + i.second.size());
		emerg_write_string(fd, i.first);
		emerg_write_string_implicit(fd, i.second);
		
	}
	//RAM contents.
	for(auto& i : mv.ramcontent) {
		emerg_write_member(fd, TAG_RAMCONTENT, string_size(i.first) + i.second.size());
		emerg_write_string(fd, i.first);
		emerg_write_blob_implicit(fd, i.second);
	}
	close(fd);
}
