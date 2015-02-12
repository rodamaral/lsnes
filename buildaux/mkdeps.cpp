#include <boost/filesystem.hpp>
#include <sys/time.h>
#include <fstream>
#include <cctype>
#include <set>
#include <map>
#include <iostream>
#include <string>

namespace boost_fs = boost::filesystem;

bool is_cmdhelp_file(const std::string& filename)
{
	std::string _filename = filename;
	return (_filename.length() > 8 && _filename.substr(0, 8) == "cmdhelp/");
}

std::string search_include(const std::list<std::string>& searchpath, const std::string& _filename,
	const std::string& ref_by)
{
	std::string filename = _filename;
	//Hack: process cmdhelp includes internally as the date were for the JSON include.
	if(is_cmdhelp_file(filename)) {
		if(filename != "cmdhelp/inverselist.hpp") {
			filename = "../src/" + filename;
			//Replace the extension with .json.
			size_t split = filename.find_last_of("./\\");
			if(split < filename.length() && filename[split] == '.') {
				filename = filename.substr(0, split) + ".json";
			}
		}
	}
	size_t p = ref_by.find_last_of("/");
	if(p < ref_by.length()) {
		std::string i = ref_by;
		i = i.substr(0, p);
		std::string real_fn = i + "/" + filename;
		boost_fs::path p(real_fn);
		if(boost_fs::exists(p) && boost_fs::is_regular_file(p))
			return real_fn;
	}
	for(auto& i : searchpath) {
		std::string real_fn = i + "/" + filename;
		boost_fs::path p(real_fn);
		if(boost_fs::exists(p) && boost_fs::is_regular_file(p))
			return real_fn;
	}
	std::cerr << "WARNING: Include file '" << filename << "' not found." << std::endl;
	return "";
}

time_t get_timestamp(const std::string& path)
{
	boost_fs::path p(path);
	if(!boost_fs::exists(p)) return 0;
	return boost_fs::last_write_time(p);
}

time_t recursive_scan(const std::list<std::string>& searchpath, const std::string& filename,
	std::map<std::string, time_t>& scanned)
{
	if(filename == "")
		return 0;
	if(scanned.count(filename))
		return 0;
	std::ifstream fp(filename);
	if(!fp) {
		std::cerr << "WARNING: File '" << filename << "' can't be opened." << std::endl;
		return 0;
	}
	time_t newest = get_timestamp(filename);
	scanned[filename] = newest;
	std::string tmp;
	while(std::getline(fp, tmp)) {
		if(tmp.length() > 0 && tmp[0] == '#') {
			//Possibly include.
			std::string included;
			if(strncmp(tmp.c_str(), "#include", 8))
				continue;
			size_t ptr = 8;
			while(ptr < tmp.length() && isspace((unsigned char)tmp[ptr]))
				ptr++;
			if(ptr == tmp.length())
				continue;
			if(tmp[ptr] != '\"')
				continue;
			size_t iptr = ++ptr;
			while(ptr < tmp.length() && tmp[ptr] != '\"')
				ptr++;
			if(ptr == tmp.length())
				continue;
			included = tmp.substr(iptr, ptr - iptr);
			newest = std::max(newest, recursive_scan(searchpath, search_include(searchpath, included,
				filename), scanned));
		}
	}
	return newest;
}

int main(int argc, char** argv)
{
	std::list<std::string> searchpath;
	std::list<std::string> files;
	bool step = false;
	for(int i = 1; i < argc; i++) {
		if(!step && !strcmp(argv[i], "--"))
			step = true;
		else if(!step)
			searchpath.push_back(argv[i]);
		else
			files.push_back(argv[i]);
	}
	searchpath.push_back(".");
	for(auto& i : files) {
		std::map<std::string, time_t> x;
		time_t t = recursive_scan(searchpath, i, x);
		if(get_timestamp(i + ".dep") < t) {
			std::ofstream y(i + ".dep");
			for(auto& j : x)
				y << j.second << " " << j.first << std::endl;
		}
	}
	return 0;
}
