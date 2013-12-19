#include "recentfiles.hpp"
#include "zip.hpp"
#include "eatarg.hpp"
#include "json.hpp"
#include "string.hpp"
#include <fstream>

recentfile_path::recentfile_path()
{
}

recentfile_path::recentfile_path(const std::string& p)
{
	path = p;
}

std::string recentfile_path::serialize() const
{
	return path;
}

recentfile_path recentfile_path::deserialize(const std::string& s)
{
	recentfile_path p(s);
	return p;
}

std::string recentfile_path::get_path() const
{
	return path;
}

bool recentfile_path::check() const
{
	if(path == "")
		return false;
	try {
		return zip::file_exists(path);
		return true;
	} catch(...) {
		return false;
	}
}

std::string recentfile_path::display() const
{
	return path;
}

bool recentfile_path::operator==(const recentfile_path& p) const
{
	return p.path == path;
}

recentfile_multirom::recentfile_multirom()
{
	//Nothing to do.
}

std::string recentfile_multirom::serialize() const
{
	bool any = false;
	JSON::node output(JSON::object);
	if(packfile != "") {
		output["pack"] = JSON::string(packfile);
	} else if(singlefile != "") {
		output["file"] = JSON::string(singlefile);
		if(core != "") output["core"] = JSON::string(core);
		if(system != "") output["system"] = JSON::string(system);
		if(region != "") output["region"] = JSON::string(region);
	} else {
		output["files"] = JSON::array();
		JSON::node& f = output["files"];
		for(auto i : files) {
			f.append(JSON::string(i));
			if(i != "")
				any = true;
		}
		if(core != "") output["core"] = JSON::string(core);
		if(system != "") output["system"] = JSON::string(system);
		if(region != "") output["region"] = JSON::string(region);
	}
	if(packfile != "" || singlefile != "" || core != "" || system != "" || region != "") any = true;
	if(!any) return "";
	return output.serialize();
}

recentfile_multirom recentfile_multirom::deserialize(const std::string& s)
{
	recentfile_multirom r;
	if(s.length() > 0 && s[0] == '{') {
		//JSON.
		try {
			JSON::node d(s);
			if(d.field_exists("pack")) r.packfile = d["pack"].as_string8();
			if(d.field_exists("file")) r.singlefile = d["file"].as_string8();
			if(d.field_exists("core")) r.core = d["core"].as_string8();
			if(d.field_exists("system")) r.system = d["system"].as_string8();
			if(d.field_exists("region")) r.region= d["region"].as_string8();
			if(d.field_exists("files")) {
				size_t cnt = d["files"].index_count();
				r.files.resize(cnt);
				for(unsigned i = 0; i < cnt; i++)
					r.files[i] = d["files"].index(i).as_string8();
			}
		} catch(...) {
			r.packfile = "";
			r.singlefile = "";
			r.core = "";
			r.system = "";
			r.region = "";
			r.files.clear();
		}
	} else {
		//Legacy form.
		r.packfile = s;
	}
	return r;
}

bool recentfile_multirom::check() const
{
	if(packfile == "" && singlefile == "" && core == "" && system == "" && region == "" && files.empty())
		return false;
	if(packfile != "" && !zip::file_exists(packfile))
		return false;
	if(singlefile != "" && !zip::file_exists(singlefile))
		return false;
	for(auto i : files)
		if(i != "" && !zip::file_exists(i))
			return false;
	return true;
}

std::string recentfile_multirom::display() const
{
	if(packfile != "")
		return packfile;
	if(singlefile != "") {
		return singlefile + " <" + core + "/" + system + ">";
	} else {
		if(files.size() > 1 && files[1] != "")
			return (stringfmt() << files[1] << " (+" << files.size() << ")").str();
		else if(files.size() > 0)
			return (stringfmt() << files[0] << " (+" << files.size() << ")").str();
		else
			return "(blank) <" + core + "/" + system + ">";
	}
}

bool recentfile_multirom::operator==(const recentfile_multirom& p) const
{
	if(packfile != p.packfile)
		return false;
	if(singlefile != p.singlefile)
		return false;
	if(core != p.core)
		return false;
	if(system != p.system)
		return false;
	if(region != p.region)
		return false;
	if(files.size() != p.files.size())
		return false;
	for(unsigned i = 0; i < files.size(); i++)
		if(files[i] != p.files[i])
			return false;
	return true;
}

template<class T> recent_files<T>::recent_files(const std::string& _cfgfile, size_t _maxcount)
{
	cfgfile = _cfgfile;
	maxcount = _maxcount;
}

template<class T> void recent_files<T>::add(const T& file)
{
	std::list<T> ents;
	//Load the list.
	{
		std::ifstream in(cfgfile);
		std::string f;
		while(in) {
			std::getline(in, f);
			T g = T::deserialize(f);
			if(g.check())
				ents.push_back(g);
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
			out << itr->serialize() << std::endl;
	}
	for(auto i : hooks)
		(*i)();
}

template<class T> void recent_files<T>::add_hook(recent_files_hook& h)
{
	hooks.push_back(&h);
}

template<class T> void recent_files<T>::remove_hook(recent_files_hook& h)
{
	for(auto itr = hooks.begin(); itr != hooks.end(); itr++)
		if(*itr == &h) {
			hooks.erase(itr);
			break;
		}
}

template<class T> std::list<T> recent_files<T>::get()
{
	size_t c = 0;
	std::list<T> ents;
	//Load the list.
	{
		std::ifstream in(cfgfile);
		std::string f;
		while(in) {
			std::getline(in, f);
			T g = T::deserialize(f);
			if(c < maxcount && g.check()) {
				ents.push_back(g);
				c++;
			}
		}
	}
	return ents;
}

recent_files_hook::~recent_files_hook()
{
}

void _dummy_63263632747434353545()
{
	recent_files<recentfile_path> x("", 0);
	eat_argument(&recent_files<recentfile_path>::add);
	eat_argument(&recent_files<recentfile_path>::add_hook);
	eat_argument(&recent_files<recentfile_path>::remove_hook);
	eat_argument(&recent_files<recentfile_path>::get);
	recent_files<recentfile_multirom> y("", 0);
	eat_argument(&recent_files<recentfile_multirom>::add);
	eat_argument(&recent_files<recentfile_multirom>::add_hook);
	eat_argument(&recent_files<recentfile_multirom>::remove_hook);
	eat_argument(&recent_files<recentfile_multirom>::get);
}
