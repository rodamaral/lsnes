#include "core/dispatch.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>

void dispatch_set_error_streams(std::ostream* stream)
{
	notify_autofire_update.errors_to(stream);
	notify_autohold_reconfigure.errors_to(stream);
	notify_autohold_update.errors_to(stream);
	notify_close.errors_to(stream);
	notify_core_change.errors_to(stream);
	notify_mode_change.errors_to(stream);
	notify_new_core.errors_to(stream);
	notify_screen_update.errors_to(stream);
	notify_set_screen.errors_to(stream);
	notify_sound_change.errors_to(stream);
	notify_sound_unmute.errors_to(stream);
	notify_status_update.errors_to(stream);
	notify_subtitle_change.errors_to(stream);
	notify_voice_stream_change.errors_to(stream);
	notify_vu_change.errors_to(stream);
	notify_core_changed.errors_to(stream);
	notify_multitrack_change.errors_to(stream);
	notify_title_change.errors_to(stream);
	notify_branch_change.errors_to(stream);
	notify_mbranch_change.errors_to(stream);
}

struct dispatch::source<> notify_autohold_reconfigure("autohold_reconfigure");
struct dispatch::source<unsigned, unsigned, unsigned, bool> notify_autohold_update("autohold_update");
struct dispatch::source<unsigned, unsigned, unsigned, unsigned, unsigned> notify_autofire_update("autofire_update");
struct dispatch::source<> notify_close("notify_close");
struct dispatch::source<framebuffer::fb<false>&> notify_set_screen("set_screen");
struct dispatch::source<std::pair<std::string, std::string>> notify_sound_change("sound_change");
struct dispatch::source<> notify_screen_update("screen_update");
struct dispatch::source<> notify_status_update("status_update");
struct dispatch::source<bool> notify_sound_unmute("sound_unmute");
struct dispatch::source<bool> notify_mode_change("mode_change");
struct dispatch::source<> notify_core_change("core_change");
struct dispatch::source<bool> notify_core_changed("core_changed");
struct dispatch::source<> notify_new_core("new_core");
struct dispatch::source<> notify_voice_stream_change("voice_stream_change");
struct dispatch::source<> notify_vu_change("vu_change");
struct dispatch::source<> notify_subtitle_change("subtitle_change");
struct dispatch::source<unsigned, unsigned, int> notify_multitrack_change("multitrack_change");
struct dispatch::source<> notify_title_change("title_change");
struct dispatch::source<> notify_branch_change("branch_change");
struct dispatch::source<> notify_mbranch_change("mbranch_change");
