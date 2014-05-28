#ifndef _dispatch__hpp__included__
#define _dispatch__hpp__included__

#include "library/framebuffer.hpp"
#include "library/dispatch.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <set>
#include <map>

/**
 * Video data region is NTSC.
 */
#define VIDEO_REGION_NTSC 0
/**
 * Video data region is PAL.
 */
#define VIDEO_REGION_PAL 1

void dispatch_set_error_streams(std::ostream* stream);

extern struct dispatch::source<> notify_autohold_reconfigure;
extern struct dispatch::source<unsigned, unsigned, unsigned, bool> notify_autohold_update;
extern struct dispatch::source<unsigned, unsigned, unsigned, unsigned, unsigned> notify_autofire_update;
extern struct dispatch::source<> notify_close;
extern struct dispatch::source<framebuffer::fb<false>&> notify_set_screen;
extern struct dispatch::source<std::pair<std::string, std::string>> notify_sound_change;
extern struct dispatch::source<> notify_screen_update;
extern struct dispatch::source<> notify_status_update;
extern struct dispatch::source<bool> notify_sound_unmute;
extern struct dispatch::source<bool> notify_mode_change;
extern struct dispatch::source<> notify_core_change;
extern struct dispatch::source<> notify_title_change;
extern struct dispatch::source<> notify_branch_change;
extern struct dispatch::source<> notify_mbranch_change;
extern struct dispatch::source<bool> notify_core_changed;
extern struct dispatch::source<> notify_new_core;
extern struct dispatch::source<> notify_voice_stream_change;
extern struct dispatch::source<> notify_vu_change;
extern struct dispatch::source<> notify_subtitle_change;
extern struct dispatch::source<unsigned, unsigned, int> notify_multitrack_change;

#endif
