#include "core/bsnes.hpp"
#include "interface/core.hpp"
#include <sstream>

/**
 * Number clocks per field/frame on NTSC/PAL
 */
#define DURATION_NTSC_FRAME 357366
#define DURATION_NTSC_FIELD 357368
#define DURATION_PAL_FRAME 425568
#define DURATION_PAL_FIELD 425568

namespace
{
	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	};
}

std::string emucore_get_version()
{
	std::ostringstream x;
	x << snes_library_id() << " (" << SNES::Info::Profile << " core)";
	return x.str();
}

std::pair<uint32_t, uint32_t> emucore_get_video_rate(bool interlace)
{
	uint32_t d;
	if(SNES::system.region() == SNES::System::Region::PAL)
		d = interlace ? DURATION_PAL_FIELD : DURATION_PAL_FRAME;
	else
		d = interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
	return std::make_pair(SNES::system.cpu_frequency(), d);
}

std::pair<uint32_t, uint32_t> emucore_get_audio_rate()
{
	return std::make_pair(SNES::system.apu_frequency(), 768);
}

void emucore_basic_init()
{
	static bool done = false;
	if(done)
		return;
	my_interfaced* intrf = new my_interfaced;
	SNES::interface = intrf;
	done = true;
}
