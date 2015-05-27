#include "core/moviefile-common.hpp"
#include "core/moviefile.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "library/binarystream.hpp"
#include "library/directory.hpp"
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
//FUCK YOU. SERIOUSLY.
#define EXTRA_OPENFLAGS O_BINARY
#else
#define EXTRA_OPENFLAGS 0
#endif

//Damn Windows.
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

namespace
{
	const char* movie_file_id = "Movie files";
	std::map<std::string, moviefile*> memory_saves;

	bool check_binary_magic(int s)
	{
		char buf[6] = {0};
		int x = 0;
		while(x < 5) {
			int r = read(s, buf + x, 5 - x);
			if(r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
				continue;
			if(r <= 0)		//0 => EOF, break on that too.
				return false;
			x += r;
		}
		return !strcmp(buf, "lsmv\x1A");
	}

	void write_whole(int s, const char* buf, size_t size)
	{
		size_t w = 0;
		while(w < size) {
			int maxw = 32767;
			if((size_t)maxw > (size - w))
				maxw = size - w;
			int r = write(s, buf + w, maxw);
			if(r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
				continue;
			if(r < 0) {
				int err = errno;
				(stringfmt() << strerror(err)).throwex();
			}
			w += r;
		}
	}
}

moviefile::brief_info::brief_info(const std::string& filename)
{
	regex_results rr;
	if(rr = regex("\\$MEMORY:(.*)", filename)) {
		if(!memory_saves.count(rr[1]) && memory_saves[rr[1]])
			throw std::runtime_error("No such memory save");
		moviefile& mv = *memory_saves[rr[1]];
		sysregion = mv.gametype->get_name();
		corename = mv.coreversion;
		projectid = mv.projectid;
		current_frame = mv.dyn.save_frame;
		rerecords = mv.rerecords_mem;
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			hash[i] = mv.romimg_sha256[i];
			hashxml[i] = mv.romxml_sha256[i];
			hint[i] = mv.namehint[i];
		}
		return;
	}
	{
		int s = open(filename.c_str(), O_RDONLY | EXTRA_OPENFLAGS);
		if(s < 0) {
			int err = errno;
			(stringfmt() << "Can't read file '" << filename << "': " << strerror(err)).throwex();
		}
		if(check_binary_magic(s)) {
			try { binary_io(s); } catch(...) { close(s); throw; }
			close(s);
			return;
		}
		close(s);
	}
	zip::reader r(filename);
	load(r);
}

moviefile::moviefile() throw(std::bad_alloc)
	: tracker(memtracker::singleton(), movie_file_id, sizeof(*this))
{
	force_corrupt = false;
	gametype = NULL;
	input = NULL;
	coreversion = "";
	projectid = "";
	rerecords = "0";
	movie_rtc_second = dyn.rtc_second = DEFAULT_RTC_SECOND;
	movie_rtc_subsecond = dyn.rtc_subsecond = DEFAULT_RTC_SUBSECOND;
	start_paused = false;
	lazy_project_create = true;
}

moviefile::moviefile(loaded_rom& rom, std::map<std::string, std::string>& c_settings, uint64_t rtc_sec,
	uint64_t rtc_subsec)
	: tracker(memtracker::singleton(), movie_file_id, sizeof(*this))
{
	force_corrupt = false;
	gametype = &rom.get_sysregion();
	coreversion = rom.get_core_identifier();
	projectid = get_random_hexstring(40);
	rerecords = "0";
	movie_rtc_second = dyn.rtc_second = rtc_sec;
	movie_rtc_subsecond = dyn.rtc_subsecond = rtc_subsec;
	start_paused = false;
	lazy_project_create = true;
	settings = c_settings;
	input = NULL;
	auto ctrldata = rom.controllerconfig(settings);
	portctrl::type_set& ports = portctrl::type_set::make(ctrldata.ports, ctrldata.portindex());
	create_default_branch(ports);
	if(!rom.isnull()) {
		//Initialize the remainder.
		rerecords = "0";
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			auto& img = rom.get_rom(i);
			auto& xml = rom.get_markup(i);
			romimg_sha256[i] = img.sha_256.read();
			romxml_sha256[i] = xml.sha_256.read();
			namehint[i] = img.namehint;
		}
	}
}

moviefile::moviefile(const std::string& movie, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
	: tracker(memtracker::singleton(), movie_file_id, sizeof(*this))
{
	regex_results rr;
	if(rr = regex("\\$MEMORY:(.*)", movie)) {
		if(!memory_saves.count(rr[1]) || !memory_saves[rr[1]])
			throw std::runtime_error("No such memory save");
		moviefile& s = *memory_saves[rr[1]];
		copy_fields(s);
		return;
	}
	input = NULL;
	start_paused = false;
	force_corrupt = false;
	lazy_project_create = false;
	{
		int s = open(movie.c_str(), O_RDONLY | EXTRA_OPENFLAGS);
		if(s < 0) {
			int err = errno;
			(stringfmt() << "Can't read file '" << movie << "': " << strerror(err)).throwex();
		}
		if(check_binary_magic(s)) {
			try { binary_io(s, romtype); } catch(...) { close(s); throw; }
			close(s);
			return;
		}
		close(s);
	}
	zip::reader r(movie);
	load(r, romtype);
}

void moviefile::fixup_current_branch(const moviefile& mv)
{
	input = NULL;
	for(auto& i : mv.branches)
		if(&i.second == mv.input)
			input = &branches[i.first];
}

void moviefile::save(const std::string& movie, unsigned compression, bool binary, rrdata_set& rrd, bool as_state)
	throw(std::bad_alloc, std::runtime_error)
{
	regex_results rr;
	if(rr = regex("\\$MEMORY:(.*)", movie)) {
		auto tmp = new moviefile();
		try {
			tmp->copy_fields(*this);
			memory_saves[rr[1]] = tmp;
		} catch(...) {
			delete tmp;
			throw;
		}
		return;
	}
	if(binary) {
		std::string tmp = movie + ".tmp";
		int strm = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | EXTRA_OPENFLAGS, 0644);
		if(strm < 0) {
			int err = errno;
			(stringfmt() << "Failed to open '" << tmp << "': " << strerror(err)).throwex();
		}
		try {
			char buf[5] = {'l', 's', 'm', 'v', 0x1A};
			write_whole(strm, buf, 5);
			binary_io(strm, rrd, as_state);
		} catch(std::exception& e) {
			close(strm);
			(stringfmt() << "Failed to write '" << tmp << "': " << e.what()).throwex();
		}
		if(close(strm) < 0) {
			int err = errno;
			(stringfmt() << "Failed to write '" << tmp << "': " << strerror(err)).throwex();
		}
		std::string backup = movie + ".backup";
		directory::rename_overwrite(movie.c_str(), backup.c_str());
		if(directory::rename_overwrite(tmp.c_str(), movie.c_str()) < 0)
			throw std::runtime_error("Can't rename '" + tmp + "' -> '" + movie + "'");
		return;
	}
	zip::writer w(movie, compression);
	save(w, rrd, as_state);
}

void moviefile::save(std::ostream& stream, rrdata_set& rrd, bool as_state) throw(std::bad_alloc, std::runtime_error)
{
	zip::writer w(stream, 0);
	save(w, rrd, as_state);
}

void moviefile::create_default_branch(portctrl::type_set& ports)
{
	if(input)
		return;
	//If there is a branch, it becomes default.
	if(!branches.empty()) {
		input = &(branches.begin()->second);
	} else {
		//Otherwise, just create a branch.
		branches[""].clear(ports);
		input = &branches[""];
	}
}

uint64_t moviefile::get_frame_count() throw()
{
	return input->count_frames();
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
		return (100ULL * frames + 3) / 6;
	}
	uint64_t _magic[4];
	gametype->fill_framerate_magic(_magic);
	uint64_t t = _magic[BLOCK_SECONDS] * 1000ULL * (frames / _magic[BLOCK_FRAMES]);
	frames %= _magic[BLOCK_FRAMES];
	t += frames * _magic[STEP_W] + ((frames * _magic[STEP_N] + _magic[BLOCK_FRAMES] - 1) / _magic[BLOCK_FRAMES]);
	return t;
}

moviefile*& moviefile::memref(const std::string& slot)
{
	return memory_saves[slot];
}

void moviefile::copy_fields(const moviefile& mv)
{
	force_corrupt = mv.force_corrupt;
	gametype = mv.gametype;
	settings = mv.settings;
	coreversion = mv.coreversion;
	gamename = mv.gamename;
	projectid = mv.projectid;
	rerecords = mv.rerecords;
	rerecords_mem = mv.rerecords_mem;
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		romimg_sha256[i] = mv.romimg_sha256[i];
		romxml_sha256[i] = mv.romxml_sha256[i];
		namehint[i] = mv.namehint[i];
	}
	authors = mv.authors;
	movie_sram = mv.movie_sram;
	ramcontent = mv.ramcontent;
	anchor_savestate = mv.anchor_savestate;
	c_rrdata = mv.c_rrdata;
	branches = mv.branches;

	//Copy the active branch.
	input = &branches.begin()->second;
	for(auto& i : branches)
		if(mv.branches.count(i.first) && &mv.branches.find(i.first)->second == mv.input)
			input = &i.second;

	movie_rtc_second = mv.movie_rtc_second;
	movie_rtc_subsecond = mv.movie_rtc_subsecond;
	start_paused = mv.start_paused;
	lazy_project_create = mv.lazy_project_create;
	subtitles = mv.subtitles;
	dyn = mv.dyn;
}

void moviefile::fork_branch(const std::string& oldname, const std::string& newname)
{
	if(oldname == newname || branches.count(newname))
		return;
	branches[newname] = branches[oldname];
}

const std::string& moviefile::current_branch()
{
	for(auto& i : branches)
		if(&i.second == input)
			return i.first;
	static std::string blank_string;
	return blank_string;
}

bool moviefile::is_movie_or_savestate(const std::string& filename)
{
	try {
		int s = open(filename.c_str(), O_RDONLY | EXTRA_OPENFLAGS);
		if(s < 0) {
			//Can't open.
			return false;
		}
		bool is_binary = check_binary_magic(s);
		close(s);
		if(is_binary)
			return true;
		//It is not binary, might be text.
		std::string tmp;
		zip::reader r(filename);
		r.read_linefile("systemid", tmp);
		if(tmp.substr(0, 8) != "lsnes-rr")
			return false;
		return true;
	} catch(...) {
		return false;
	}
}

moviefile::branch_extractor::~branch_extractor()
{
	delete real;
}

moviefile::branch_extractor::branch_extractor(const std::string& filename)
{
	bool binary = false;
	{
		std::istream& s = zip::openrel(filename, "");
		char buf[6] = {0};
		s.read(buf, 5);
		if(!strcmp(buf, "lsmv\x1A"))
			binary = true;
		delete &s;
	}
	if(binary)
		real = new moviefile_branch_extractor_binary(filename);
	else
		real = new moviefile_branch_extractor_text(filename);
}

moviefile::sram_extractor::~sram_extractor()
{
	delete real;
}

moviefile::sram_extractor::sram_extractor(const std::string& filename)
{
	bool binary = false;
	{
		std::istream& s = zip::openrel(filename, "");
		char buf[6] = {0};
		s.read(buf, 5);
		if(!strcmp(buf, "lsmv\x1A"))
			binary = true;
		delete &s;
	}
	if(binary)
		real = new moviefile_sram_extractor_binary(filename);
	else
		real = new moviefile_sram_extractor_text(filename);
}

void moviefile::clear_dynstate()
{
	dyn.clear(movie_rtc_second, movie_rtc_subsecond, movie_sram);
}

dynamic_state::dynamic_state()
{
	save_frame = 0;
	lagged_frames = 0;
	poll_flag = 0;
	rtc_second = DEFAULT_RTC_SECOND;
	rtc_subsecond = DEFAULT_RTC_SUBSECOND;
}

void dynamic_state::clear(int64_t sec, int64_t ssec, const std::map<std::string, std::vector<char>>& initsram)
{
	sram = initsram;
	savestate.clear();
	host_memory.clear();
	screenshot.clear();
	save_frame = 0;
	lagged_frames = 0;
	for(auto& i : pollcounters)
		i = 0;
	poll_flag = 0;
	rtc_second = sec;
	rtc_subsecond = ssec;
	active_macros.clear();
}

void dynamic_state::swap(dynamic_state& s) throw()
{
	std::swap(sram, s.sram);
	std::swap(savestate, s.savestate);
	std::swap(host_memory, s.host_memory);
	std::swap(screenshot, s.screenshot);
	std::swap(save_frame, s.save_frame);
	std::swap(lagged_frames, s.lagged_frames);
	std::swap(pollcounters, s.pollcounters);
	std::swap(poll_flag, s.poll_flag);
	std::swap(rtc_second, s.rtc_second);
	std::swap(rtc_subsecond, s.rtc_subsecond);
	std::swap(active_macros, s.active_macros);
}
