#include "filelist.hpp"
#include <fstream>
#include <string>
#include "filelist.hpp"
#include "directory.hpp"
#include "zip.hpp"
#include "string.hpp"

filelist::filelist(const std::string& _backingfile, const std::string& _directory)
	: backingfile(_backingfile), directory(_directory)
{
}

filelist::~filelist()
{
}

std::set<std::string> filelist::enumerate()
{
	auto contents = readfile();
	check_stale(contents);
	writeback(contents);
	std::set<std::string> ret;
	for(auto i : contents)
		if(i.second != 0)
			ret.insert(i.first);
	return ret;
}

void filelist::add(const std::string& filename)
{
	auto contents = readfile();
	int64_t ts = directory::mtime(directory + "/" + filename);
	contents[filename] = ts;
	writeback(contents);
}

void filelist::remove(const std::string& filename)
{
	auto contents = readfile();
	//FIXME: Do something with this?
	//int64_t ts = directory::mtime(directory + "/" + filename);
	contents.erase(filename);
	writeback(contents);
}

void filelist::rename(const std::string& oldname, const std::string& newname)
{
	auto contents = readfile();
	contents[newname] = contents[oldname];
	contents.erase(oldname);
	writeback(contents);
}

std::map<std::string, int64_t> filelist::readfile()
{
	std::map<std::string, int64_t> contents;
	std::ifstream strm(backingfile);
	std::string line;
	while(std::getline(strm, line)) {
		regex_results r = regex("([0-9]+) (.*)", line);
		if(!r) continue;
		int64_t ts;
		try { ts = parse_value<int64_t>(r[1]); } catch(...) { continue; }
		contents[r[2]] = ts;
	}
	return contents;
}

void filelist::check_stale(std::map<std::string, int64_t>& data)
{
	for(auto& i : data) {
		int64_t ts = directory::mtime(directory + "/" + i.first);
		//If file timestamp does not match, mark the file as stale.
		if(i.second != ts)
			i.second = 0;
	}
}

void filelist::writeback(const std::map<std::string, int64_t>& data)
{
	std::string backingtmp = backingfile + ".new";
	std::ofstream strm(backingtmp);
	if(!strm)
		return;
	for(auto& i : data)
		if(i.second != 0)
			strm << i.second << " " << i.first << std::endl;
	strm.close();
	directory::rename_overwrite(backingtmp.c_str(), backingfile.c_str());
}
