#include "core/dispatch.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>

emulator_dispatch::emulator_dispatch()
	: autohold_reconfigure("autohold_reconfigure"), autohold_update("autohold_update"),
	autofire_update("autofire_update"), close("notify_close"),
	sound_change("sound_change"), screen_update("screen_update"), status_update("status_update"),
	sound_unmute("sound_unmute"), mode_change("mode_change"), core_change("core_change"),
	title_change("title_change"), branch_change("branch_change"), mbranch_change("mbranch_change"),
	core_changed("core_changed"), voice_stream_change("voice_stream_change"),
	vu_change("vu_change"), subtitle_change("subtitle_change"), multitrack_change("multitrack_change"),
	action_update("action_update")
{
}

void emulator_dispatch::set_error_streams(std::ostream* stream)
{
	autofire_update.errors_to(stream);
	autohold_reconfigure.errors_to(stream);
	autohold_update.errors_to(stream);
	close.errors_to(stream);
	core_change.errors_to(stream);
	mode_change.errors_to(stream);
	screen_update.errors_to(stream);
	sound_change.errors_to(stream);
	sound_unmute.errors_to(stream);
	status_update.errors_to(stream);
	subtitle_change.errors_to(stream);
	voice_stream_change.errors_to(stream);
	vu_change.errors_to(stream);
	core_changed.errors_to(stream);
	multitrack_change.errors_to(stream);
	title_change.errors_to(stream);
	branch_change.errors_to(stream);
	mbranch_change.errors_to(stream);
	action_update.errors_to(stream);
}

dispatch::source<> notify_new_core("new_core");
