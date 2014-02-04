#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/moviefile-common.hpp"
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

namespace
{
	std::map<std::string, moviefile*> memory_saves;
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
	load(r);
}

moviefile::moviefile() throw(std::bad_alloc)
{
	static port_type_set dummy_types;
	force_corrupt = false;
	gametype = NULL;
	input = NULL;
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

moviefile::moviefile(loaded_rom& rom, std::map<std::string, std::string>& c_settings, uint64_t rtc_sec,
	uint64_t rtc_subsec)
{
	static port_type_set dummy_types;
	force_corrupt = false;
	gametype = &rom.rtype->combine_region(*rom.region);
	coreversion = rom.rtype->get_core_identifier();
	projectid = get_random_hexstring(40);
	rerecords = "0";
	is_savestate = false;
	movie_rtc_second = rtc_second = rtc_sec;
	movie_rtc_subsecond = rtc_subsecond = rtc_subsec;
	start_paused = false;
	lazy_project_create = true;
	poll_flag = 0;
	settings = c_settings;
	input = NULL;
	auto ctrldata = rom.rtype->controllerconfig(settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex());
	create_default_branch(ports);
	if(!rom.rtype->isnull()) {
		//Initialize the remainder.
		rerecords = "0";
		for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
			romimg_sha256[i] = rom.romimg[i].sha_256.read();
			romxml_sha256[i] = rom.romxml[i].sha_256.read();
			namehint[i] = rom.romimg[i].namehint;
		}
	}
}

moviefile::moviefile(const std::string& movie, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
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
	poll_flag = false;
	start_paused = false;
	force_corrupt = false;
	is_savestate = false;
	lazy_project_create = false;
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
	load(r, romtype);
}

void moviefile::fixup_current_branch(const moviefile& mv)
{
	input = NULL;
	for(auto& i : mv.branches)
		if(&i.second == mv.input)
			input = &branches[i.first];
}

void moviefile::save(const std::string& movie, unsigned compression, bool binary, rrdata_set& rrd)
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
		std::ofstream strm(tmp.c_str(), std::ios_base::binary);
		if(!strm)
			throw std::runtime_error("Can't open output file");
		char buf[5] = {'l', 's', 'm', 'v', 0x1A};
		strm.write(buf, 5);
		if(!strm)
			throw std::runtime_error("Failed to write to output file");
		binary_io(strm, rrd);
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
	save(w, rrd);
}

void moviefile::save(std::ostream& stream, rrdata_set& rrd) throw(std::bad_alloc, std::runtime_error)
{
	zip::writer w(stream, 0);
	save(w, rrd);
}

void moviefile::create_default_branch(port_type_set& ports)
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
		return 100000000ULL * frames / 6;
	}
	uint64_t _magic[4];
	gametype->fill_framerate_magic(_magic);
	uint64_t t = _magic[BLOCK_SECONDS] * 1000000000ULL * (frames / _magic[BLOCK_FRAMES]);
	frames %= _magic[BLOCK_FRAMES];
	t += frames * _magic[STEP_W] + (frames * _magic[STEP_N] / _magic[BLOCK_FRAMES]);
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
	is_savestate = mv.is_savestate;
	sram = mv.sram;
	savestate = mv.savestate;
	anchor_savestate = mv.anchor_savestate;
	host_memory = mv.host_memory;
	screenshot = mv.screenshot;
	save_frame = mv.save_frame;
	lagged_frames = mv.lagged_frames;
	pollcounters = mv.pollcounters;
	poll_flag = mv.poll_flag;
	c_rrdata = mv.c_rrdata;
	branches = mv.branches;

	//Copy the active branch.
	input = &branches.begin()->second;
	for(auto& i : branches)
		if(mv.branches.count(i.first) && &mv.branches.find(i.first)->second == mv.input)
			input = &i.second;

	rtc_second = mv.rtc_second;
	rtc_subsecond = mv.rtc_subsecond;
	movie_rtc_second = mv.movie_rtc_second;
	movie_rtc_subsecond = mv.movie_rtc_subsecond;
	start_paused = mv.start_paused;
	lazy_project_create = mv.lazy_project_create;
	subtitles = mv.subtitles;
	active_macros = mv.active_macros;
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
	static std::string tmp;
	return tmp;
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
