#ifndef _moviefile_common__hpp__included__
#define _moviefile_common__hpp__included__

#include "core/moviefile.hpp"
#define DEFAULT_RTC_SECOND 1000000000ULL
#define DEFAULT_RTC_SUBSECOND 0ULL

template<typename target>
static void moviefile_write_settings(target& w, const std::map<text, text>& settings,
	core_setting_group& sgroup, std::function<void(target& w, const text& name,
	const text& value)> writefn)
{
	for(auto i : settings) {
		if(!sgroup.settings.count(i.first))
			continue;
		if(sgroup.settings.find(i.first)->second.dflt == i.second)
			continue;
		writefn(w, i.first, i.second);
	}
}

struct moviefile_branch_extractor_text : public moviefile::branch_extractor
{
	moviefile_branch_extractor_text(const text& filename);
	~moviefile_branch_extractor_text();
	std::set<text> enumerate();
	void read(const text& name, portctrl::frame_vector& v);
private:
	zip::reader z;
};

struct moviefile_branch_extractor_binary : public moviefile::branch_extractor
{
	moviefile_branch_extractor_binary(const text& filename);
	~moviefile_branch_extractor_binary();
	std::set<text> enumerate();
	void read(const text& name, portctrl::frame_vector& v);
private:
	int s;
};

struct moviefile_sram_extractor_text : public moviefile::sram_extractor
{
	moviefile_sram_extractor_text(const text& filename);
	~moviefile_sram_extractor_text();
	std::set<text> enumerate();
	void read(const text& name, std::vector<char>& v);
private:
	zip::reader z;
};

struct moviefile_sram_extractor_binary : public moviefile::sram_extractor
{
	moviefile_sram_extractor_binary(const text& filename);
	~moviefile_sram_extractor_binary();
	std::set<text> enumerate();
	void read(const text& name, std::vector<char>& v);
private:
	int s;
};


#endif
