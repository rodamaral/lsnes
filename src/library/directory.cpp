#include "directory.hpp"
#include "string.hpp"
#include <dirent.h>
#include <boost/filesystem.hpp>

#ifdef BOOST_FILESYSTEM3
namespace boost_fs = boost::filesystem3;
#else
namespace boost_fs = boost::filesystem;
#endif

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
		if(regex_match(match, d2->d_name) && strcmp(d2->d_name, ".") && strcmp(d2->d_name, ".."))
			x.insert(dir + "/" + d2->d_name);
	closedir(d);
	return x;
}

std::string get_absolute_path(const std::string& relative)
{
	return boost_fs::absolute(boost_fs::path(relative)).string();
}

uintmax_t file_get_size(const std::string& path)
{
	return boost_fs::file_size(boost_fs::path(path));
}

time_t file_get_mtime(const std::string& path)
{
	boost::system::error_code ec;
	time_t t = boost_fs::last_write_time(boost_fs::path(path), ec);
	if(t == -1)
		return 0;
	return t;
}

bool file_exists(const std::string& filename)
{
	boost::system::error_code ec;
	return boost_fs::exists(boost_fs::path(filename), ec);
}

bool file_is_regular(const std::string& filename)
{
	boost::system::error_code ec;
	boost_fs::file_status stat = status(boost_fs::path(filename), ec);
	bool e = is_regular_file(stat);
	return e;
}

bool file_is_directory(const std::string& filename)
{
	boost_fs::path p(filename);
	return boost_fs::is_directory(p);
}

bool ensure_directory_exists(const std::string& path)
{
	boost_fs::path p(path);
	return boost_fs::create_directories(p) || boost_fs::is_directory(p);
}
