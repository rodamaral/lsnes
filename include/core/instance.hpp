#ifndef _instance__hpp__included__
#define _instance__hpp__included__

#include "library/threads.hpp"

class movie_logic;
class memory_space;
class memwatch_set;
class voice_commentary;
class subtitle_commentary;
class movie_branches;
class multitrack_edit;
class _lsnes_status;
class alias_binds_manager;
class rrdata;
class cart_mappings_refresher;
class controller_state;
class project_state;
class debug_context;
class framerate_regulator;
class emu_framebuffer;
class input_queue;
class master_dumper;
class button_mapping;
namespace command
{
	class group;
}
namespace lua
{
	class state;
}
namespace settingvar
{
	class group;
	class cache;
}
namespace keyboard
{
	class keyboard;
	class mapper;
}
namespace triplebuffer
{
	template<typename T> class triplebuffer;
}

struct emulator_instance
{
	emulator_instance();
	~emulator_instance();
	movie_logic* mlogic;
	memory_space* memory;
	lua::state* lua;
	memwatch_set* mwatch;
	settingvar::group* settings;
	settingvar::cache* setcache;
	voice_commentary* commentary;
	subtitle_commentary* subtitles;
	movie_branches* mbranch;
	controller_state* controls;
	button_mapping* buttons;
	multitrack_edit* mteditor;
	_lsnes_status* status_A;
	_lsnes_status* status_B;
	_lsnes_status* status_C;
	triplebuffer::triplebuffer<_lsnes_status>* status;
	keyboard::keyboard* keyboard;
	command::group* command;
	keyboard::mapper* mapper;
	alias_binds_manager* abindmanager;
	rrdata* nrrdata;
	cart_mappings_refresher* cmapper;
	project_state* project;
	debug_context* dbg;
	framerate_regulator* framerate;
	emu_framebuffer* fbuf;
	input_queue* iqueue;
	master_dumper* mdumper;
	threads::id emu_thread;
};

extern emulator_instance lsnes_instance;

emulator_instance& CORE();

#endif
