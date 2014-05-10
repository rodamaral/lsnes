#ifndef _emustatus__hpp__included__
#define _emustatus__hpp__included__

#include <stdexcept>
#include <string>
#include <set>
#include <map>
#include "library/triplebuffer.hpp"

struct _lsnes_status
{
	const static int pause_none;			//pause: No pause.
	const static int pause_normal;			//pause: Normal pause.
	const static int pause_break;			//pause: Break pause.
	const static uint64_t subframe_savepoint;	//subframe: Point of save.
	const static uint64_t subframe_video;		//subframe: Point of video output.

	bool valid;
	bool movie_valid;				//The movie state variables are valid?
	uint64_t curframe;				//Current frame number.
	uint64_t length;				//Movie length.
	uint64_t lag;					//Lag counter.
	uint64_t subframe;				//Subframe number.
	bool dumping;					//Video dump active.
	unsigned speed;					//Speed%
	bool saveslot_valid;				//Save slot number/info valid.
	uint64_t saveslot;				//Save slot number.
	std::u32string slotinfo;			//Save slot info.
	bool branch_valid;				//Branch info valid?
	std::u32string branch;				//Current branch.
	bool mbranch_valid;				//Movie branch info valid?
	std::u32string mbranch;				//Current movie branch.
	std::u32string macros;				//Currently active macros.
	int pause;					//Pause mode.
	char mode;					//Movie mode: C:Corrupt, R:Readwrite, P:Readonly, F:Finished.
	bool rtc_valid;					//RTC time valid?
	std::u32string rtc;				//RTC time.
	std::vector<std::u32string> inputs;		//Input display.
	std::map<std::string, std::u32string> mvars;	//Memory watches.
	std::map<std::string, std::u32string> lvars;	//Lua variables.
};

#endif
