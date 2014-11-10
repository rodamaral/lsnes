#include "core/messages.hpp"
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

	void write_rrdata(zip::writer& w, rrdata_set& rrd) throw(std::bad_alloc, std::runtime_error)
	{
		uint64_t count;
		std::vector<char> out;
		count = rrd.write(out);
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

	void write_input(zip::writer& w, const std::string& mname, portctrl::frame_vector& input)
		throw(std::bad_alloc, std::runtime_error)
	{
		std::ostream& m = w.create_file(mname);
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

	void write_subtitles(zip::writer& w, const std::string& file, std::map<moviefile_subtiming, std::string>& x)
	{
		std::ostream& m = w.create_file(file);
		try {
			for(auto i : x)
				m << i.first.get_frame() << " " << i.first.get_length() << " "
					<< subtitle_commentary::s_escape(i.second) << std::endl;
			if(!m)
				throw std::runtime_error("Can't write ZIP file member");
			w.close_file();
		} catch(...) {
			w.close_file();
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
}

void moviefile::save(zip::writer& w, rrdata_set& rrd) throw(std::bad_alloc, std::runtime_error)
{
	w.write_linefile("gametype", gametype->get_name());
	moviefile_write_settings<zip::writer>(w, settings, gametype->get_type().get_settings(), [](zip::writer& w,
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
	write_rrdata(w, rrd);
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

	std::map<std::string, uint64_t> branch_table;
	uint64_t next_branch = 1;
	for(auto& i : branches) {
		uint64_t id;
		if(&i.second == input)
			id = 0;
		else
			id = next_branch++;
		branch_table[i.first] = id;
		w.write_linefile((stringfmt() << "branchname." << id).str(), i.first);
		if(id)
			write_input(w, (stringfmt() << "input." << id).str(), i.second);
		else
			write_input(w, "input", i.second);
	}

	w.commit();
}
