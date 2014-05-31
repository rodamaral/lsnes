#ifndef _dispatch__hpp__included__
#define _dispatch__hpp__included__

#include "library/framebuffer.hpp"
#include "library/dispatch.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <set>
#include <map>

struct emulator_dispatch
{
	emulator_dispatch();
	void set_error_streams(std::ostream* stream);
	dispatch::source<> autohold_reconfigure;
	dispatch::source<unsigned, unsigned, unsigned, bool> autohold_update;
	dispatch::source<unsigned, unsigned, unsigned, unsigned, unsigned> autofire_update;
	dispatch::source<> close;
	dispatch::source<std::pair<std::string, std::string>> sound_change;
	dispatch::source<> screen_update;
	dispatch::source<> status_update;
	dispatch::source<bool> sound_unmute;
	dispatch::source<bool> mode_change;
	dispatch::source<> core_change;
	dispatch::source<> title_change;
	dispatch::source<> branch_change;
	dispatch::source<> mbranch_change;
	dispatch::source<bool> core_changed;
	dispatch::source<> voice_stream_change;
	dispatch::source<> vu_change;
	dispatch::source<> subtitle_change;
	dispatch::source<unsigned, unsigned, int> multitrack_change;
	dispatch::source<> action_update;
};

extern dispatch::source<> notify_new_core;


#endif
