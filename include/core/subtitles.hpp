#ifndef _subtitles__hpp__included__
#define _subtitles__hpp__included__

#include "lua/lua.hpp"

class moviefile_subtiming
{
public:
	moviefile_subtiming(uint64_t frame);
	moviefile_subtiming(uint64_t first, uint64_t length);
	bool operator<(const moviefile_subtiming& a) const;
	bool operator==(const moviefile_subtiming& a) const;
	bool inrange(uint64_t x) const;
	uint64_t get_frame() const;
	uint64_t get_length() const;
private:
	uint64_t frame;
	uint64_t length;
	bool position_only;
};

std::set<std::pair<uint64_t, uint64_t>> get_subtitles();
std::string get_subtitle_for(uint64_t f, uint64_t l);
void set_subtitle_for(uint64_t f, uint64_t l, const std::string& x);
std::string s_unescape(std::string x);
std::string s_escape(std::string x);
void render_subtitles(lua_render_context& ctx);

#endif
