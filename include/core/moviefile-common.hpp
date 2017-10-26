#ifndef _moviefile_common__hpp__included__
#define _moviefile_common__hpp__included__

#include <functional>
#include "core/moviefile.hpp"
#define DEFAULT_RTC_SECOND 1000000000ULL
#define DEFAULT_RTC_SUBSECOND 0ULL

template<typename target>
static void moviefile_write_settings(target& w, const std::map<std::string, std::string>& settings,
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

struct moviefile_branch_extractor_text : public moviefile::branch_extractor
{
	moviefile_branch_extractor_text(const std::string& filename);
	~moviefile_branch_extractor_text();
	std::set<std::string> enumerate();
	void read(const std::string& name, portctrl::frame_vector& v);
private:
	zip::reader z;
};

struct moviefile_branch_extractor_binary : public moviefile::branch_extractor
{
	moviefile_branch_extractor_binary(const std::string& filename);
	~moviefile_branch_extractor_binary();
	std::set<std::string> enumerate();
	void read(const std::string& name, portctrl::frame_vector& v);
private:
	int s;
};

struct moviefile_sram_extractor_text : public moviefile::sram_extractor
{
	moviefile_sram_extractor_text(const std::string& filename);
	~moviefile_sram_extractor_text();
	std::set<std::string> enumerate();
	void read(const std::string& name, std::vector<char>& v);
private:
	zip::reader z;
};

struct moviefile_sram_extractor_binary : public moviefile::sram_extractor
{
	moviefile_sram_extractor_binary(const std::string& filename);
	~moviefile_sram_extractor_binary();
	std::set<std::string> enumerate();
	void read(const std::string& name, std::vector<char>& v);
private:
	int s;
};


#endif
