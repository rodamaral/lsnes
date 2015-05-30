#ifndef _subtitles__hpp__included__
#define _subtitles__hpp__included__

#include <cstdint>
#include <set>
#include <string>
#include "library/command.hpp"

namespace lua { class render_context; }

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

class movie_logic;
class emu_framebuffer;
class emulator_dispatch;

struct subtitle_commentary
{
public:
	subtitle_commentary(movie_logic& _mlogic, emu_framebuffer& _fbuf, emulator_dispatch& _dispatch,
		command::group& _cmd);
	std::set<std::pair<uint64_t, uint64_t>> get_all();
	text get(uint64_t f, uint64_t l);
	void set(uint64_t f, uint64_t l, const text& x);
	static text s_unescape(text x);
	static text s_escape(text x);
	void render(lua::render_context& ctx);
private:
	void do_editsub(const text& a);
	void do_listsub();
	void do_savesub(const text& a);
	movie_logic& mlogic;
	emu_framebuffer& fbuf;
	emulator_dispatch& edispatch;
	command::group& cmd;
	command::_fnptr<const text&> editsub;
	command::_fnptr<> listsub;
	command::_fnptr<command::arg_filename> savesub;
};

#endif
