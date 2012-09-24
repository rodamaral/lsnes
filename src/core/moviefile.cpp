#include "core/emucore.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rrdata.hpp"
#include "library/zip.hpp"
#include "library/string.hpp"

#include <sstream>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#define DEFAULT_RTC_SECOND 1000000000ULL
#define DEFAULT_RTC_SUBSECOND 0ULL


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


void read_moviestate_file(zip_reader& r, const std::string& file, uint64_t& save_frame, uint64_t& lagged_frames,
	std::vector<uint32_t>& pollcounters) throw(std::bad_alloc, std::runtime_error)
{
	unsigned char buf[512];
	auto s = read_raw_file(r, file);
	if(s.size() != sizeof(buf))
		throw std::runtime_error("Invalid moviestate file");
	memcpy(buf, &s[0], sizeof(buf));
	//Interesting offsets: 32-39: Current frame, 40-439: Poll counters, 440-447 lagged frames. All bigendian.
	save_frame = decode_uint64(buf + 32);
	lagged_frames = decode_uint64(buf + 440);
	pollcounters.resize(100);
	for(unsigned i = 0; i < 100; i++)
		pollcounters[i] = decode_uint32(buf + 40 + 4 * i);
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

porttype_info& parse_controller_type(const std::string& type, unsigned port) throw(std::bad_alloc, std::runtime_error)
{
	try {
		porttype_info& i = porttype_info::lookup(type);
		if(!i.legal || !(i.legal(port)))
			throw 42;
		return i;
	} catch(...) {
		(stringfmt() << "Illegal port " << (port + 1) << " device '" << type << "'").throwex();
	}
}

moviefile::moviefile() throw(std::bad_alloc)
{
	force_corrupt = false;
	gametype = NULL;
	port1 = &porttype_info::default_type();
	port2 = &porttype_info::default_type();
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

moviefile::moviefile(const std::string& movie) throw(std::bad_alloc, std::runtime_error)
{
	poll_flag = false;
	start_paused = false;
	force_corrupt = false;
	is_savestate = false;
	lazy_project_create = false;
	std::string tmp;
	zip_reader r(movie);
	read_linefile(r, "systemid", tmp);
	if(tmp.substr(0, 8) != "lsnes-rr")
		throw std::runtime_error("Not lsnes movie");
	read_linefile(r, "controlsversion", tmp);
	if(tmp != "0")
		throw std::runtime_error("Can't decode movie data");
	read_linefile(r, "gametype", tmp);
	try {
		gametype = &core_sysregion::lookup(tmp);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Illegal game type '" + tmp + "'");
	}
	tmp = porttype_info::port_default(0).name;
	read_linefile(r, "port1", tmp, true);
	port1 = &porttype_info::lookup(tmp);
	tmp = porttype_info::port_default(1).name;
	read_linefile(r, "port2", tmp, true);
	port2 = &porttype_info::lookup(tmp);
	input.clear(*port1, *port2);
	read_linefile(r, "gamename", gamename, true);
	read_linefile(r, "projectid", projectid);
	rerecords = read_rrdata(r, c_rrdata);
	read_linefile(r, "coreversion", coreversion);
	read_linefile(r, "rom.sha256", romimg_sha256[0], true);
	read_linefile(r, "romxml.sha256", romxml_sha256[0], true);
	for(size_t i = 0; i < 26; i++) {
		read_linefile(r, (stringfmt() << "slot" << (char)(96 + i) << ".sha256").str(), romimg_sha256[i + 1],
			true);
		read_linefile(r, (stringfmt() << "slot" << (char)(96 + i) << "xml.sha256").str(),
			romxml_sha256[i + 1], true);
	}
	read_linefile(r, "prefix", prefix, true);
	read_subtitles(r, "subtitles", subtitles);
	prefix = sanitize_prefix(prefix);
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
		if(r.has_member("moviestate"))
			//Backwards compat stuff.
			read_moviestate_file(r, "moviestate", save_frame, lagged_frames, pollcounters);
		else {
			read_numeric_file(r, "saveframe", save_frame, true);
			read_numeric_file(r, "lagcounter", lagged_frames, true);
			read_pollcounters(r, "pollcounters", pollcounters);
		}
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
	}
	if(rtc_subsecond < 0 || movie_rtc_subsecond < 0)
		throw std::runtime_error("Invalid RTC subsecond value");
	std::string name = r.find_first();
	for(auto name : r)
		if(name.length() >= 10 && name.substr(0, 10) == "moviesram.")
			movie_sram[name.substr(10)] = read_raw_file(r, name);
	read_authors_file(r, authors);
	read_input(r, input, 0);
}

void moviefile::save(const std::string& movie, unsigned compression) throw(std::bad_alloc, std::runtime_error)
{
	zip_writer w(movie, compression);
	write_linefile(w, "gametype", gametype->get_name());
	if(port1->name != porttype_info::port_default(0).name)
		write_linefile(w, "port1", port1->name);
	if(port2->name != porttype_info::port_default(1).name)
		write_linefile(w, "port2", port2->name);
	write_linefile(w, "gamename", gamename, true);
	write_linefile(w, "systemid", "lsnes-rr1");
	write_linefile(w, "controlsversion", "0");
	coreversion = get_core_identifier();
	write_linefile(w, "coreversion", coreversion);
	write_linefile(w, "projectid", projectid);
	write_rrdata(w);
	write_linefile(w, "rom.sha256", romimg_sha256[0], true);
	write_linefile(w, "romxml.sha256", romxml_sha256[0], true);
	for(size_t i = 0; i < 26; i++) {
		write_linefile(w, (stringfmt() << "slot" << (char)(96 + i) << ".sha256").str(), romimg_sha256[i + 1],
			true);
		write_linefile(w, (stringfmt() << "slot" << (char)(96 + i) << "xml.sha256").str(),
			romxml_sha256[i + 1], true);
	}
	write_subtitles(w, "subtitles", subtitles);
	write_linefile(w, "prefix", prefix, true);
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
	}
	write_authors_file(w, authors);
	write_input(w, input);

	w.commit();
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

std::string sanitize_prefix(const std::string& in) throw(std::bad_alloc)
{
	std::ostringstream s;
	bool any = false;
	for(size_t i = 0; i < in.length(); i++) {
		char ch = in[i];
		if(ch < 33 || ch == '$' || ch == ':' || ch == '/' || ch == '\\')
			continue;	//Always disallowed.
		if(ch == '.' && !any)
			continue;	//Sometimes disallowed.
		any = true;
		s << ch;
	}
	return s.str();
}
