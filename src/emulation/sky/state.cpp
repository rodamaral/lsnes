#include "state.hpp"
#include "romimage.hpp"

namespace sky
{
	const char* const statenames[] = {
		"MENU_FADEIN", "MENU", "MENU_FADEOUT", "LOAD_LEVEL", "LEVEL_FADEIN", "LEVEL_PLAY", "LEVEL_COMPLETE",
		"LEVEL_FADEOUT", "LOAD_MENU", "LEVEL_UNAVAIL", "DEMO_UNAVAIL", "LEVEL_FADEOUT_RETRY"
	};

	void gstate::level_init(uint8_t _stage)
	{
		stage = _stage;
		p.level_init(curlevel);
		paused = 0;
		speedind = 0;
		fuelind = 0;
		o2ind = 0;
		distind = 0;
		lockind = 0;
		beep_phase = 0;
	}
	uint8_t gstate::simulate_frame(noise_maker& sfx, int lr, int ad, bool jump)
	{
		uint8_t dstatus = p.simulate_frame(curlevel, sfx, lr, ad, jump);
		uint16_t lt = p.lpos >> 16;
		uint8_t ht = (p.hpos - 12160) / 5888;
		if(secret == 1 && p.is_set(physics::flag_landed) && ht <= 2 && lt >= 131 && lt <= 170)
			secret |= 0x80;
		else if(secret == 2 && p.is_set(physics::flag_landed) && ht == 3 && lt >= 58 && lt <= 62)
			secret |= 0x80;
		return dstatus;
	}
	void gstate::change_state(uint8_t newstate)
	{
		state = newstate;
		fadecount = 0;
	}

	std::pair<uint8_t*, size_t> gstate::as_ram()
	{
		return std::make_pair(reinterpret_cast<uint8_t*>(this), sizeof(*this));
	}
}
