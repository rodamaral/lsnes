#include "core/emustatus.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"

#include <sstream>

const int _lsnes_status::pause_none = 0;
const int _lsnes_status::pause_normal = 1;
const int _lsnes_status::pause_break = 2;
const uint64_t _lsnes_status::subframe_savepoint = 0xFFFFFFFFFFFFFFFEULL;
const uint64_t _lsnes_status::subframe_video = 0xFFFFFFFFFFFFFFFFULL;

slotinfo_cache::slotinfo_cache(movie_logic& _mlogic)
	: mlogic(_mlogic)
{
}

std::string slotinfo_cache::get(const std::string& _filename)
{
	std::string filename = resolve_relative_path(_filename);
	if(!cache.count(filename)) {
		std::ostringstream out;
		try {
			moviefile::brief_info info(filename);
			if(!mlogic)
				out << "No movie";
			else if(mlogic.get_mfile().projectid == info.projectid)
				out << info.rerecords << "R/" << info.current_frame << "F";
			else
				out << "Wrong movie";
		} catch(...) {
			out << "Nonexistent";
		}
		cache[filename] = out.str();
	}
	return cache[filename];
}

void slotinfo_cache::flush(const std::string& _filename)
{
	cache.erase(resolve_relative_path(_filename));
}

void slotinfo_cache::flush()
{
	cache.clear();
}
