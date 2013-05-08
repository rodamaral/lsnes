#include "directory.hpp"
#include "string.hpp"
#include <dirent.h>

std::set<std::string> enumerate_directory(const std::string& dir, const std::string& match)
{
	std::set<std::string> x;
	DIR* d;
	dirent* d2;
	d = opendir(dir.c_str());
	if(!d) {
		(stringfmt() << "Can't read directory '" << dir << "'").throwex();
		return x;
	}
	while((d2 = readdir(d)))
		if(regex_match(match, d2->d_name))
			x.insert(dir + "/" + d2->d_name);
	closedir(d);
	return x;
}
