#include "library/recentfiles.hpp"
#include "library/zip.hpp"
#include <fstream>

recent_files::recent_files(const std::string& _cfgfile, size_t _maxcount)
{
	cfgfile = _cfgfile;
	maxcount = _maxcount;
}

void recent_files::add(const std::string& file)
{
	try {
		//Probe for existence.
		delete &open_file_relative(file, "");
	} catch(std::exception& e) {
		return;
	}

	std::list<std::string> ents;
	//Load the list.
	{
		std::ifstream in(cfgfile);
		std::string f;
		while(in) {
			std::getline(in, f);
			if(f != "") {
				bool exists = true;
				try {
					//Probe for existence.
					delete &open_file_relative(f, "");
				} catch(std::exception& e) {
					exists = false;
				}
				if(exists)
					ents.push_back(f);
			}
		}
	}
	//Search for matches. If found, move to front, otherwise push to first.
	auto itr = ents.begin();
	for(; itr != ents.end(); itr++)
		if(*itr == file)
			break;
	if(itr != ents.end())
		ents.erase(itr);
	ents.push_front(file);
	//Write up to maxcount entries.
	{
		size_t i = 0;
		std::ofstream out(cfgfile);
		for(itr = ents.begin(); itr != ents.end() && i < maxcount; itr++, i++)
			out << *itr << std::endl;
	}
	for(auto i : hooks)
		(*i)();
}

void recent_files::add_hook(hook& h)
{
	hooks.push_back(&h);
}

void recent_files::remove_hook(hook& h)
{
	for(auto itr = hooks.begin(); itr != hooks.end(); itr++)
		if(*itr == &h) {
			hooks.erase(itr);
			break;
		}
}

std::list<std::string> recent_files::get()
{
	size_t c = 0;
	std::list<std::string> ents;
	//Load the list.
	{
		std::ifstream in(cfgfile);
		std::string f;
		while(in) {
			std::getline(in, f);
			if(f != "") {
				bool exists = true;
				try {
					//Probe for existence.
					delete &open_file_relative(f, "");
				} catch(std::exception& e) {
					exists = false;
				}
				if(exists && c < maxcount) {
					ents.push_back(f);
					c++;
				}
			}
		}
	}
	return ents;
}

recent_files::recent_files::hook::~hook()
{
}
