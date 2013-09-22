#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rrdata.hpp"
#include "library/zip.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/binarystream.hpp"
#include "interface/romtype.hpp"

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

void read_linefile(zip_reader& r, const std::string& member, std::string& out, bool conditional = false)
	throw(std::bad_alloc, std::runtime_error)
{
	if(conditional && !r.has_member(member))
		return;
	std::istream& m = r[member];
	try {
		std::getline(m, out);
		istrip_CR(out);
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
}

void write_linefile(zip_writer& w, const std::string& member, const std::string& value, bool conditional = false)
	throw(std::bad_alloc, std::runtime_error)
{
	if(conditional && value == "")
		return;
	std::ostream& m = w.create_file(member);
	try {
		m << value << std::endl;
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

namespace
{
	void binary_read_movie(binary_input_stream& in, controller_frame_vector& v)
	{
		uint64_t stride = v.get_stride();
		uint64_t pageframes = v.get_frames_per_page();
		uint64_t vsize = 0;
		size_t pagenum = 0;
		uint64_t pagesize = stride * pageframes;
		while(in.get_left()) {
			v.resize(vsize + pageframes);
			unsigned char* contents = v.get_page_buffer(pagenum++);
			uint64_t gcount = min(pagesize, in.get_left());
			in.raw(contents, gcount);
			vsize += (gcount / stride);
		}
		v.resize(vsize);
	}

	void binary_write_movie(binary_output_stream& out, controller_frame_vector& v)
	{
		uint64_t pages = v.get_page_count();
		uint64_t stride = v.get_stride();
		uint64_t pageframes = v.get_frames_per_page();
		uint64_t vsize = v.size();
		out.write_extension_tag(TAG_MOVIE, vsize * stride);
		size_t pagenum = 0;
		while(vsize > 0) {
			uint64_t count = (vsize > pageframes) ? pageframes : vsize;
			size_t bytes = count * stride;
			unsigned char* content = v.get_page_buffer(pagenum++);
			out.raw(content, bytes);
			vsize -= count;
		}
	}

	std::map<std::string, std::string> read_settings(zip_reader& r)
	{
		std::map<std::string, std::string> x;
		for(auto i : r) {
			if(!regex_match("port[0-9]+|setting\\..+", i))
				continue;
			std::string s;
			if(i.substr(0, 4) == "port")
				s = i;
			else
				s = i.substr(8);
			read_linefile(r, i, x[s], true);
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

	std::map<std::string, uint64_t> read_active_macros(zip_reader& r, const std::string& member)
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

	void write_active_macros(zip_writer& w, const std::string& member, const std::map<std::string, uint64_t>& ma)
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


template<typename T>
void read_numeric_file(zip_reader& r, const std::string& member, T& out, bool conditional = false)
	throw(std::bad_alloc, std::runtime_error)
{
	std::string _out;
	read_linefile(r, member, _out, conditional);
	if(conditional && _out == "")
		return;
	out = parse_value<int64_t>(_out);
}

template<typename T>
void write_numeric_file(zip_writer& w, const std::string& member, T value) throw(std::bad_alloc,
	std::runtime_error)
{
	std::ostringstream x;
	x << value;
	write_linefile(w, member, x.str());
}

void write_raw_file(zip_writer& w, const std::string& member, std::vector<char>& content) throw(std::bad_alloc,
	std::runtime_error)
{
	std::ostream& m = w.create_file(member);
	try {
		m.write(&content[0], content.size());
		if(!m)
			throw std::runtime_error("Can't write ZIP file member");
		w.close_file();
	} catch(...) {
		w.close_file();
		throw;
	}
}

std::vector<char> read_raw_file(zip_reader& r, const std::string& member) throw(std::bad_alloc, std::runtime_error)
{
	std::vector<char> out;
	std::istream& m = r[member];
	try {
		boost::iostreams::back_insert_device<std::vector<char>> rd(out);
		boost::iostreams::copy(m, rd);
		delete &m;
	} catch(...) {
		delete &m;
		throw;
	}
	return out;
}

uint64_t decode_uint64(unsigned char* buf)
{
	return ((uint64_t)buf[0] << 56) |
		((uint64_t)buf[1] << 48) |
		((uint64_t)buf[2] << 40) |
		((uint64_t)buf[3] << 32) |
		((uint64_t)buf[4] << 24) |
		((uint64_t)buf[5] << 16) |
		((uint64_t)buf[6] << 8) |
		((uint64_t)buf[7]);
}

uint32_t decode_uint32(unsigned char* buf)
{
	return ((uint32_t)buf[0] << 24) |
		((uint32_t)buf[1] << 16) |
		((uint32_t)buf[2] << 8) |
		((uint32_t)buf[3]);
}

void read_authors_file(zip_reader& r, std::vector<std::pair<std::string, std::string>>& authors) throw(std::bad_alloc,
	std::runtime_error)
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

std::string read_rrdata(zip_reader& r, std::vector<char>& out) throw(std::bad_alloc, std::runtime_error)
{
	out = read_raw_file(r, "rrdata");
	uint64_t count = rrdata::count(out);
	std::ostringstream x;
	x << count;
	return x.str();
}

void write_rrdata(zip_writer& w) throw(std::bad_alloc, std::runtime_error)
{
	uint64_t count;
	std::vector<char> out;
	count = rrdata::write(out);
	write_raw_file(w, "rrdata", out);
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

void write_authors_file(zip_writer& w, std::vector<std::pair<std::string, std::string>>& authors)
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

void write_input(zip_writer& w, controller_frame_vector& input)
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

void read_subtitles(zip_reader& r, const std::string& file, std::map<moviefile_subtiming, std::string>& x)
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

void write_subtitles(zip_writer& w, const std::string& file, std::map<moviefile_subtiming, std::string>& x)
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

void read_input(zip_reader& r, controller_frame_vector& input, unsigned version) throw(std::bad_alloc,
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

void read_pollcounters(zip_reader& r, const std::string& file, std::vector<uint32_t>& pctr)
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

void write_pollcounters(zip_writer& w, const std::string& file, const std::vector<uint32_t>& pctr)
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
	{
		std::istream& s = open_file_relative(filename, "");
		char buf[6] = {0};
		s.read(buf, 5);
		if(!strcmp(buf, "lsmv\x1A")) {
			binary_io(s);
			delete &s;
			return;
		}
		delete &s;
	}
	zip_reader r(filename);
	std::string tmp;
	read_linefile(r, "systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	read_linefile(r, "gametype", sysregion);
	read_linefile(r, "coreversion", corename);
	read_linefile(r, "projectid", projectid);
	if(r.has_member("savestate"))
		read_numeric_file(r, "saveframe", current_frame);
	else
		current_frame = 0;
	read_numeric_file(r, "rerecords", rerecords);
	read_linefile(r, "rom.sha256", hash[0], true);
	read_linefile(r, "romxml.sha256", hashxml[0], true);
	read_linefile(r, "rom.hint", hint[0], true);
	unsigned base = 97;
	if(r.has_member("slot`.sha256"))
		base = 96;
	for(size_t i = 1; i < ROM_SLOT_COUNT; i++) {
		read_linefile(r, (stringfmt() << "slot" << (char)(base + i - 1) << ".sha256").str(), hash[i],
			true);
		read_linefile(r, (stringfmt() << "slot" << (char)(base + i - 1) << "xml.sha256").str(),
			hashxml[i], true);
		read_linefile(r, (stringfmt() << "slot" << (char)(base + i - 1) << ".hint").str(), hint[i],
			true);
	}
}

void moviefile::brief_info::binary_io(std::istream& _stream)
{
	binary_input_stream in(_stream);
	sysregion = in.string();
	//Discard the settings.
	while(in.byte()) {
		in.string();
		in.string();
	}
	in.extension({
		{TAG_CORE_VERSION, [this](binary_input_stream& s) {
			this->corename = s.string_implicit();
		}},{TAG_PROJECT_ID, [this](binary_input_stream& s) {
			this->projectid = s.string_implicit();
		}},{TAG_SAVESTATE, [this](binary_input_stream& s) {
			this->current_frame = s.number();
		}},{TAG_RRDATA, [this](binary_input_stream& s) {
			std::vector<char> c_rrdata;
			s.blob_implicit(c_rrdata);
			this->rerecords = rrdata::count(c_rrdata);
		}},{TAG_ROMHASH, [this](binary_input_stream& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				this->hashxml[n >> 1] = h;
			else
				this->hash[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binary_input_stream& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			this->hint[n] = h;
		}}
	}, binary_null_default);
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
	poll_flag = false;
	start_paused = false;
	force_corrupt = false;
	is_savestate = false;
	lazy_project_create = false;
	std::string tmp;
	{
		std::istream& s = open_file_relative(movie, "");
		char buf[6] = {0};
		s.read(buf, 5);
		if(!strcmp(buf, "lsmv\x1A")) {
			binary_io(s, romtype);
			delete &s;
			return;
		}
		delete &s;
	}
	zip_reader r(movie);
	read_linefile(r, "systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	read_linefile(r, "controlsversion", tmp);
	if(tmp != "0")
		throw std::runtime_error("Can't decode movie data");
	read_linefile(r, "gametype", tmp);
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
	read_linefile(r, "gamename", gamename, true);
	read_linefile(r, "projectid", projectid);
	rerecords = read_rrdata(r, c_rrdata);
	read_linefile(r, "coreversion", coreversion);
	read_linefile(r, "rom.sha256", romimg_sha256[0], true);
	read_linefile(r, "romxml.sha256", romxml_sha256[0], true);
	read_linefile(r, "rom.hint", namehint[0], true);
	unsigned base = 97;
	if(r.has_member("slot`.sha256"))
		base = 96;
	for(size_t i = 1; i < ROM_SLOT_COUNT; i++) {
		read_linefile(r, (stringfmt() << "slot" << (char)(base + i - 1) << ".sha256").str(), romimg_sha256[i],
			true);
		read_linefile(r, (stringfmt() << "slot" << (char)(base + i - 1) << "xml.sha256").str(),
			romxml_sha256[i], true);
		read_linefile(r, (stringfmt() << "slot" << (char)(base + i - 1) << ".hint").str(), namehint[i],
			true);
	}
	read_subtitles(r, "subtitles", subtitles);
	movie_rtc_second = DEFAULT_RTC_SECOND;
	movie_rtc_subsecond = DEFAULT_RTC_SUBSECOND;
	read_numeric_file(r, "starttime.second", movie_rtc_second, true);
	read_numeric_file(r, "starttime.subsecond", movie_rtc_subsecond, true);
	rtc_second = movie_rtc_second;
	rtc_subsecond = movie_rtc_subsecond;
	if(r.has_member("savestate.anchor"))
		anchor_savestate = read_raw_file(r, "savestate.anchor");
	if(r.has_member("savestate")) {
		is_savestate = true;
		read_numeric_file(r, "saveframe", save_frame, true);
		read_numeric_file(r, "lagcounter", lagged_frames, true);
		read_pollcounters(r, "pollcounters", pollcounters);
		if(r.has_member("hostmemory"))
			host_memory = read_raw_file(r, "hostmemory");
		savestate = read_raw_file(r, "savestate");
		for(auto name : r)
			if(name.length() >= 5 && name.substr(0, 5) == "sram.")
				sram[name.substr(5)] = read_raw_file(r, name);
		screenshot = read_raw_file(r, "screenshot");
		//If these can't be read, just use some (wrong) values.
		read_numeric_file(r, "savetime.second", rtc_second, true);
		read_numeric_file(r, "savetime.subsecond", rtc_subsecond, true);
		uint64_t _poll_flag = 2;	//Legacy behaviour is the default.
		read_numeric_file(r, "pollflag", _poll_flag, true);
		poll_flag = _poll_flag;
		active_macros = read_active_macros(r, "macros");
	}
	for(auto name : r)
		if(name.length() >= 8 && name.substr(0, 8) == "initram.")
			ramcontent[name.substr(8)] = read_raw_file(r, name);
	if(rtc_subsecond < 0 || movie_rtc_subsecond < 0)
		throw std::runtime_error("Invalid RTC subsecond value");
	std::string name = r.find_first();
	for(auto name : r)
		if(name.length() >= 10 && name.substr(0, 10) == "moviesram.")
			movie_sram[name.substr(10)] = read_raw_file(r, name);
	read_authors_file(r, authors);
	read_input(r, input, 0);
}

void moviefile::save(const std::string& movie, unsigned compression, bool binary) throw(std::bad_alloc,
	std::runtime_error)
{
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
		rename_file_overwrite(movie.c_str(), backup.c_str());
		if(rename_file_overwrite(tmp.c_str(), movie.c_str()) < 0)
			throw std::runtime_error("Can't rename '" + tmp + "' -> '" + movie + "'");
		return;
	}
	zip_writer w(movie, compression);
	write_linefile(w, "gametype", gametype->get_name());
	write_settings<zip_writer>(w, settings, gametype->get_type().get_settings(), [](zip_writer& w,
		const std::string& name, const std::string& value) -> void {
			if(regex_match("port[0-9]+", name))
				write_linefile(w, name, value);
			else
				write_linefile(w, "setting." + name, value);
		});
	write_linefile(w, "gamename", gamename, true);
	write_linefile(w, "systemid", "lsnes-rr1");
	write_linefile(w, "controlsversion", "0");
	coreversion = gametype->get_type().get_core_identifier();
	write_linefile(w, "coreversion", coreversion);
	write_linefile(w, "projectid", projectid);
	write_rrdata(w);
	write_linefile(w, "rom.sha256", romimg_sha256[0], true);
	write_linefile(w, "romxml.sha256", romxml_sha256[0], true);
	write_linefile(w, "rom.hint", namehint[0], true);
	for(size_t i = 1; i < ROM_SLOT_COUNT; i++) {
		write_linefile(w, (stringfmt() << "slot" << (char)(96 + i) << ".sha256").str(), romimg_sha256[i],
			true);
		write_linefile(w, (stringfmt() << "slot" << (char)(96 + i) << "xml.sha256").str(), romxml_sha256[i],
			true);
		write_linefile(w, (stringfmt() << "slot" << (char)(96 + i) << ".hint").str(), namehint[i],
			true);
	}
	write_subtitles(w, "subtitles", subtitles);
	for(auto i : movie_sram)
		write_raw_file(w, "moviesram." + i.first, i.second);
	write_numeric_file(w, "starttime.second", movie_rtc_second);
	write_numeric_file(w, "starttime.subsecond", movie_rtc_subsecond);
	if(!anchor_savestate.empty())
			write_raw_file(w, "savestate.anchor", anchor_savestate);
	if(is_savestate) {
		write_numeric_file(w, "saveframe", save_frame);
		write_numeric_file(w, "lagcounter", lagged_frames);
		write_pollcounters(w, "pollcounters", pollcounters);
		write_raw_file(w, "hostmemory", host_memory);
		write_raw_file(w, "savestate", savestate);
		write_raw_file(w, "screenshot", screenshot);
		for(auto i : sram)
			write_raw_file(w, "sram." + i.first, i.second);
		write_numeric_file(w, "savetime.second", rtc_second);
		write_numeric_file(w, "savetime.subsecond", rtc_subsecond);
		write_numeric_file(w, "pollflag", poll_flag);
		write_active_macros(w, "macros", active_macros);
	}
	for(auto i : ramcontent)
		write_raw_file(w, "initram." + i.first, i.second);
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
	binary_output_stream out(_stream);
	out.string(gametype->get_name());
	write_settings<binary_output_stream>(out, settings, gametype->get_type().get_settings(),
		[](binary_output_stream& s, const std::string& name, const std::string& value) -> void {
			s.byte(0x01);
			s.string(name);
			s.string(value);
		});
	out.byte(0x00);

	out.extension(TAG_MOVIE_TIME, [this](binary_output_stream& s) {
		s.number(this->movie_rtc_second);
		s.number(this->movie_rtc_subsecond);
	});

	out.extension(TAG_PROJECT_ID, [this](binary_output_stream& s) {
		s.string_implicit(this->projectid);
	});

	out.extension(TAG_CORE_VERSION, [this](binary_output_stream& s) {
		this->coreversion = this->gametype->get_type().get_core_identifier();
		s.string_implicit(this->coreversion);
	});

	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		out.extension(TAG_ROMHASH, [this, i](binary_output_stream& s) {
			if(!this->romimg_sha256[i].length()) return;
			s.byte(2 * i);
			s.string_implicit(this->romimg_sha256[i]);
		});
		out.extension(TAG_ROMHASH, [this, i](binary_output_stream& s) {
			if(!this->romxml_sha256[i].length()) return;
			s.byte(2 * i + 1);
			s.string_implicit(this->romxml_sha256[i]);
		});
		out.extension(TAG_ROMHINT, [this, i](binary_output_stream& s) {
			if(!this->namehint[i].length()) return;
			s.byte(i);
			s.string_implicit(this->namehint[i]);
		});
	}

	out.extension(TAG_RRDATA, [this](binary_output_stream& s) {
		uint64_t count;
		std::vector<char> rrd;
		count = rrdata::write(rrd);
		s.blob_implicit(rrd);
	});
	
	for(auto i : movie_sram)
		out.extension(TAG_MOVIE_SRAM, [&i](binary_output_stream& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});

	out.extension(TAG_ANCHOR_SAVE, [this](binary_output_stream& s) {
		s.blob_implicit(this->anchor_savestate);
	});
	if(is_savestate) {
		out.extension(TAG_SAVESTATE, [this](binary_output_stream& s) {
			s.number(this->save_frame);
			s.number(this->lagged_frames);
			s.number(this->rtc_second);
			s.number(this->rtc_subsecond);
			s.number(this->pollcounters.size());
			for(auto i : this->pollcounters)
				s.number32(i);
			s.byte(this->poll_flag ? 0x01 : 0x00);
			s.blob_implicit(this->savestate);
		});

		out.extension(TAG_HOSTMEMORY, [this](binary_output_stream& s) {
			s.blob_implicit(this->host_memory);
		});

		out.extension(TAG_SCREENSHOT, [this](binary_output_stream& s) {
			s.blob_implicit(this->screenshot);
		});

		for(auto i : sram) {
			out.extension(TAG_SAVE_SRAM, [&i](binary_output_stream& s) {
				s.string(i.first);
				s.blob_implicit(i.second);
			});
		}
	}

	out.extension(TAG_GAMENAME, [this](binary_output_stream& s) {
		s.string_implicit(this->gamename);
	});

	for(auto i : subtitles)
		out.extension(TAG_SUBTITLE, [&i](binary_output_stream& s) {
			s.number(i.first.get_frame());
			s.number(i.first.get_length());
			s.string_implicit(i.second);
		});

	for(auto i : authors)
		out.extension(TAG_AUTHOR, [&i](binary_output_stream& s) {
			s.string(i.first);
			s.string_implicit(i.second);
		});

	for(auto i : active_macros)
		out.extension(TAG_MACRO, [&i](binary_output_stream& s) {
			s.number(i.second);
			s.string_implicit(i.first);
		});

	for(auto i : ramcontent) {
		out.extension(TAG_RAMCONTENT, [&i](binary_output_stream& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});
	}

	binary_write_movie(out, input);
}

void moviefile::binary_io(std::istream& _stream, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
{
	binary_input_stream in(_stream);
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
		{TAG_ANCHOR_SAVE, [this](binary_input_stream& s) {
			s.blob_implicit(this->anchor_savestate);
		}},{TAG_AUTHOR, [this](binary_input_stream& s) {
			std::string a = s.string();
			std::string b = s.string_implicit();
			this->authors.push_back(std::make_pair(a, b));
		}},{TAG_CORE_VERSION, [this](binary_input_stream& s) {
			this->coreversion = s.string_implicit();
		}},{TAG_GAMENAME, [this](binary_input_stream& s) {
			this->gamename = s.string_implicit();
		}},{TAG_HOSTMEMORY, [this](binary_input_stream& s) {
			s.blob_implicit(this->host_memory);
		}},{TAG_MACRO, [this](binary_input_stream& s) {
			uint64_t n = s.number();
			this->active_macros[s.string_implicit()] = n;
		}},{TAG_MOVIE, [this](binary_input_stream& s) {
			binary_read_movie(s, input);
		}},{TAG_MOVIE_SRAM, [this](binary_input_stream& s) {
			std::string a = s.string();
			s.blob_implicit(this->movie_sram[a]);
		}},{TAG_RAMCONTENT, [this](binary_input_stream& s) {
			std::string a = s.string();
			s.blob_implicit(this->ramcontent[a]);
		}},{TAG_MOVIE_TIME, [this](binary_input_stream& s) {
			this->movie_rtc_second = s.number();
			this->movie_rtc_subsecond = s.number();
		}},{TAG_PROJECT_ID, [this](binary_input_stream& s) {
			this->projectid = s.string_implicit();
		}},{TAG_ROMHASH, [this](binary_input_stream& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				romxml_sha256[n >> 1] = h;
			else
				romimg_sha256[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binary_input_stream& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			namehint[n] = h;
		}},{TAG_RRDATA, [this](binary_input_stream& s) {
			s.blob_implicit(this->c_rrdata);
			this->rerecords = (stringfmt() << rrdata::count(c_rrdata)).str();
		}},{TAG_SAVE_SRAM, [this](binary_input_stream& s) {
			std::string a = s.string();
			s.blob_implicit(this->sram[a]);
		}},{TAG_SAVESTATE, [this](binary_input_stream& s) {
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
		}},{TAG_SCREENSHOT, [this](binary_input_stream& s) {
			s.blob_implicit(this->screenshot);
		}},{TAG_SUBTITLE, [this](binary_input_stream& s) {
			uint64_t f = s.number();
			uint64_t l = s.number();
			std::string x = s.string_implicit();
			this->subtitles[moviefile_subtiming(f, l)] = x;
		}}
	}, binary_null_default);
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
