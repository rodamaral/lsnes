#include "recentfiles.hpp"
#include "zip.hpp"
#include "eatarg.hpp"
#include "json.hpp"
#include "string.hpp"
#include <fstream>

namespace recentfiles
{
path::path()
{
}

path::path(const std::string& p)
{
	pth = p;
}

std::string path::serialize() const
{
	return pth;
}

path path::deserialize(const std::string& s)
{
	path p(s);
	return p;
}

std::string path::get_path() const
{
	return pth;
}

bool path::check() const
{
	if(pth == "")
		return false;
	try {
		return zip::file_exists(pth);
	} catch(...) {
		return false;
	}
}

std::string path::display() const
{
	return pth;
}

bool path::operator==(const path& p) const
{
	return p.pth == pth;
}

multirom::multirom()
{
	//Nothing to do.
}

std::string multirom::serialize() const
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

multirom multirom::deserialize(const std::string& s)
{
	multirom r;
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

bool multirom::check() const
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

std::string multirom::display() const
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

bool multirom::operator==(const multirom& p) const
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

namedobj::namedobj()
{
}

std::string namedobj::serialize() const
{
	if(_id == "" && _filename == "" && _display == "") return "";
	JSON::node output(JSON::object);
	output["id"] = JSON::string(_id);
	output["filename"] = JSON::string(_filename);
	output["display"] = JSON::string(_display);
	return output.serialize();
}

namedobj namedobj::deserialize(const std::string& s)
{
	namedobj obj;
	JSON::node d(s);
	if(d.field_exists("id")) obj._id = d["id"].as_string8();
	if(d.field_exists("filename")) obj._filename = d["filename"].as_string8();
	if(d.field_exists("display")) obj._display = d["display"].as_string8();
	return obj;
}

bool namedobj::check() const
{
	return zip::file_exists(_filename);
}

std::string namedobj::display() const
{
	return _display;
}

bool namedobj::operator==(const namedobj& p) const
{
	return (_id == p._id);
}

template<class T> set<T>::set(const std::string& _cfgfile, size_t _maxcount)
{
	cfgfile = _cfgfile;
	maxcount = _maxcount;
}

template<class T> void set<T>::add(const T& file)
{
	std::list<T> ents;
	//Load the list.
	{
		std::ifstream in(cfgfile);
		std::string f;
		while(in) {
			std::getline(in, f);
			T g;
			try {
				g = T::deserialize(f);
			} catch(...) {
				continue;
			}
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

template<class T> void set<T>::add_hook(hook& h)
{
	hooks.push_back(&h);
}

template<class T> void set<T>::remove_hook(hook& h)
{
	for(auto itr = hooks.begin(); itr != hooks.end(); itr++)
		if(*itr == &h) {
			hooks.erase(itr);
			break;
		}
}

template<class T> std::list<T> set<T>::get()
{
	size_t c = 0;
	std::list<T> ents;
	//Load the list.
	{
		std::ifstream in(cfgfile);
		std::string f;
		while(in) {
			std::getline(in, f);
			T g;
			try {
				g = T::deserialize(f);
			} catch(...) {
				continue;
			}
			if(c < maxcount && g.check()) {
				ents.push_back(g);
				c++;
			}
		}
	}
	return ents;
}

hook::~hook()
{
}

void _dummy_63263632747434353545()
{
	set<path> x("", 0);
	eat_argument(&set<path>::add);
	eat_argument(&set<path>::add_hook);
	eat_argument(&set<path>::remove_hook);
	eat_argument(&set<path>::get);
	set<multirom> y("", 0);
	eat_argument(&set<multirom>::add);
	eat_argument(&set<multirom>::add_hook);
	eat_argument(&set<multirom>::remove_hook);
	eat_argument(&set<multirom>::get);
	set<namedobj> z("", 0);
	eat_argument(&set<namedobj>::add);
	eat_argument(&set<namedobj>::add_hook);
	eat_argument(&set<namedobj>::remove_hook);
	eat_argument(&set<namedobj>::get);
}
}
