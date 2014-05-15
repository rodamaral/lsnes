#include "directory.hpp"
#include "urirewrite.hpp"
#include "string.hpp"
#include "zip.hpp"
#include <sstream>
#include <fstream>

namespace urirewrite
{
std::set<std::string> rewriter::get_schemes()
{
	threads::alock h(mlock);
	std::set<std::string> ret;
	for(auto& i : rewrites)
		ret.insert(i.first);
	return ret;
}

void rewriter::delete_rewrite(const std::string& scheme)
{
	threads::alock h(mlock);
	rewrites.erase(scheme);
}

void rewriter::set_rewrite(const std::string& scheme, const std::string& pattern)
{
	threads::alock h(mlock);
	if(!regex_match("[A-Za-z][A-Za-z0-9+.-]*", scheme))
		throw std::runtime_error("Invalid scheme name");
	rewrites[scheme] = pattern;
}

std::string rewriter::get_rewrite(const std::string& scheme)
{
	threads::alock h(mlock);
	if(!rewrites.count(scheme))
		throw std::runtime_error("No such scheme known");
	return rewrites[scheme];
}

std::string rewriter::operator()(const std::string& uri)
{
	threads::alock h(mlock);
	regex_results r = regex("([A-Za-z][A-Za-z0-9+.-]*):(.*)", uri);
	if(!r || !rewrites.count(r[1])) return uri;	//No rewrite.
	std::string pattern = rewrites[r[1]];
	std::string argument = r[2];

	//Strip spaces.
	size_t start = argument.find_first_not_of(" \t");
	size_t end = argument.find_last_not_of(" \t");
	if(start < argument.length())
		argument = argument.substr(start, end - start + 1);
	else
		argument = "";

	bool esc = false;
	bool placed = false;
	std::ostringstream out;
	for(size_t i = 0; i < pattern.length(); i++) {
		if(esc) {
			if(pattern[i] == '0') {
				out << argument;
				placed = true;
			} else
				out << pattern[i];
			esc = false;
		} else if(pattern[i] == '$')
			esc = true;
		else
			out << pattern[i];
	}
	if(!placed) out << argument;
	return out.str();
}

void rewriter::save(const std::string& filename)
{
	std::string tmpfile = filename + ".tmp";
	{
		std::ofstream x(tmpfile);
		threads::alock h(mlock);
		for(auto& i : rewrites)
			x << i.first << "=" << i.second << std::endl;
		if(!x)
			throw std::runtime_error("Error writing rewriter settings");
	}
	directory::rename_overwrite(tmpfile.c_str(), filename.c_str());
}

void rewriter::load(const std::string& filename)
{
	std::map<std::string, std::string> tmp;
	{
		std::ifstream x(filename);
		threads::alock h(mlock);
		std::string line;
		while(std::getline(x, line)) {
			istrip_CR(line);
			regex_results r = regex("([A-Za-z][A-Za-z0-9+.-]*)=(.*)", line);
			if(!r) continue;
			tmp[r[1]] = r[2];
		}
		std::swap(rewrites, tmp);
	}
}
}
