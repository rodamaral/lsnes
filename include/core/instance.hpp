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
class emulator_dispatch;
class slotinfo_cache;
class lua_state;
class audioapi_instance;
class loaded_rom;
class save_jukebox;
class emulator_runmode;
class status_updater;
namespace command { class group; }
namespace lua { class state; }
namespace settingvar { class group; }
namespace settingvar { class cache; }
namespace keyboard { class keyboard; }
namespace keyboard { class mapper; }
namespace triplebuffer { template<typename T> class triplebuffer; }

class dtor_list
{
	struct entry
	{
		void* ptr;
		void (*free1)(void* ptr);
		void (*free2)(void* ptr);
		entry* prev;
	};
public:
	dtor_list();
	~dtor_list();
	void destroy();
	template<typename T> void prealloc(T*& ptr)
	{
		entry e;
		e.ptr = ptr = reinterpret_cast<T*>(new char[sizeof(T) + 32]);
		memset(ptr, 0, sizeof(T) + 32);
		e.free1 = null;
		e.free2 = free2;
		e.prev = list;
		list = new entry(e);
	}
	template<typename T, typename... U> void init(T*& ptr, U&... args)
	{
		if(ptr) {
			//Find the entry.
			entry* e = list;
			while(e->ptr != ptr)
				e = e->prev;
			new(ptr) T(args...);
			e->free1 = free1_p<T>;
		} else {
			entry e;
			e.ptr = ptr = new T(args...);
			e.free1 = free1<T>;
			e.free2 = null;
			e.prev = list;
			list = new entry(e);
		}
	}
private:
	entry* list;
	static void null(void* ptr) {}
	static void free2(void* ptr) { delete[] reinterpret_cast<char*>(ptr); }
	template<typename T> static void free1(void* ptr) { delete reinterpret_cast<T*>(ptr); }
	template<typename T> static void free1_p(void* ptr) { reinterpret_cast<T*>(ptr)->~T(); }
};

struct emulator_instance
{
	emulator_instance();
	~emulator_instance();
	emulator_dispatch* dispatch;
	movie_logic* mlogic;
	memory_space* memory;
	lua::state* lua;
	lua_state* lua2;
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
	slotinfo_cache* slotcache;
	audioapi_instance* audio;
	loaded_rom* rom;
	save_jukebox* jukebox;
	emulator_runmode* runmode;
	status_updater* supdater;
	threads::id emu_thread;
	time_t random_seed_value;
	dtor_list D;
private:
	emulator_instance(const emulator_instance&);
	emulator_instance& operator=(const emulator_instance&);
};

extern emulator_instance lsnes_instance;

emulator_instance& CORE();

#endif
