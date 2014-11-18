#ifndef _emustatus__hpp__included__
#define _emustatus__hpp__included__

#include <stdexcept>
#include <string>
#include <set>
#include <map>
#include "library/command.hpp"
#include "library/triplebuffer.hpp"

class movie_logic;
class project_state;
class voice_commentary;
class emulator_runmode;
class master_dumper;
class save_jukebox;
class slotinfo_cache;
class framerate_regulator;
class controller_state;
class multitrack_edit;
class lua_state;
class loaded_rom;
class memwatch_set;
class emulator_dispatch;

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

struct slotinfo_cache
{
	slotinfo_cache(movie_logic& _mlogic, command::group& _cmd);
	std::string get(const std::string& _filename);
	void flush(const std::string& _filename);
	void flush();
private:
	std::map<std::string, std::string> cache;
	movie_logic& mlogic;
	command::group& cmd;
	command::_fnptr<> flushcmd;
};

struct status_updater
{
public:
	status_updater(project_state& _project, movie_logic& _mlogic, voice_commentary& _commentary,
	triplebuffer::triplebuffer<_lsnes_status>& _status, emulator_runmode& _runmode, master_dumper& _mdumper,
	save_jukebox& _jukebox, slotinfo_cache& _slotcache, framerate_regulator& _framerate,
	controller_state& _controls, multitrack_edit& _mteditor, lua_state& _lua2, loaded_rom& _rom,
	memwatch_set& _mwatch, emulator_dispatch& _dispatch);
	void update();
private:
	project_state& project;
	movie_logic& mlogic;
	voice_commentary& commentary;
	triplebuffer::triplebuffer<_lsnes_status>& status;
	emulator_runmode& runmode;
	master_dumper& mdumper;
	save_jukebox& jukebox;
	slotinfo_cache& slotcache;
	framerate_regulator& framerate;
	controller_state& controls;
	multitrack_edit& mteditor;
	lua_state& lua2;
	loaded_rom& rom;
	memwatch_set& mwatch;
	emulator_dispatch& dispatch;
};

#endif
