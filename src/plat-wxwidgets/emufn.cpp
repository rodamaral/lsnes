#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/coroutine.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/memorywatch.hpp"
#include "core/moviedata.hpp"
#include "core/render.hpp"
#include "core/window.hpp"
#include "core/zip.hpp"

#include "plat-wxwidgets/authorseditor.hpp"
#include "plat-wxwidgets/axeseditor.hpp"
#include "plat-wxwidgets/common.hpp"
#include "plat-wxwidgets/emufn.hpp"
#include "plat-wxwidgets/keyentry.hpp"
#include "plat-wxwidgets/settingseditor.hpp"
#include "plat-wxwidgets/status_window.hpp"

#include <cstdint>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
extern "C"
{
#ifndef UINT64_C
#define UINT64_C(val) val##ULL
#endif
#include <libswscale/swscale.h>
}
#define STACKSIZE (8 * 1024 * 1024)
#define REQ_POLL_JOYSTICK 1
#define REQ_KEY_PRESS 2
#define REQ_KEY_RELEASE 3
#define REQ_CONTINUE 4
#define REQ_COMMAND 5

extern std::string lsnes_version;

enum
{
	wxID_PAUSE = wxID_HIGHEST + 1,
	wxID_FRAMEADVANCE,
	wxID_SUBFRAMEADVANCE,
	wxID_NEXTPOLL,
	wxID_ERESET,
	wxID_AUDIO_ENABLED,
	wxID_SHOW_AUDIO_STATUS,
	wxID_AUDIODEV_FIRST,
	wxID_AUDIODEV_LAST = wxID_AUDIODEV_FIRST + 255,
	wxID_SAVE_STATE,
	wxID_SAVE_MOVIE,
	wxID_LOAD_STATE,
	wxID_LOAD_STATE_RO,
	wxID_LOAD_STATE_RW,
	wxID_LOAD_STATE_P,
	wxID_LOAD_MOVIE,
	wxID_RUN_SCRIPT,
	wxID_RUN_LUA,
	wxID_EVAL_LUA,
	wxID_SAVE_SCREENSHOT,
	wxID_READONLY_MODE,
	wxID_EDIT_AUTHORS,
	wxID_AUTOHOLD_FIRST,
	wxID_AUTOHOLD_LAST = wxID_AUTOHOLD_FIRST + 127,
	wxID_EDIT_AXES,
	wxID_EDIT_SETTINGS,
	wxID_EDIT_KEYBINDINGS,
	wxID_EDIT_ALIAS,
	wxID_EDIT_MEMORYWATCH,
	wxID_SAVE_MEMORYWATCH,
	wxID_LOAD_MEMORYWATCH,
	wxID_DUMP_AVICSCD,
	wxID_END_AVICSCD,
	wxID_DUMP_JMD,
	wxID_END_JMD,
	wxID_DUMP_SDMP,
	wxID_END_SDMP,
};

#define MAXCONTROLLERS 8
#define CONTROLS_COUNT 16

class controller_autohold_menu : public wxMenu
{
public:
	controller_autohold_menu(unsigned lid, enum devicetype_t dtype);
	void change_type(enum devicetype_t dtype);
	bool is_dummy();
	void on_select(wxCommandEvent& e);
	void update(unsigned pid, unsigned ctrlnum, bool newstate);
private:
	unsigned our_lid;
	devicetype_t devtype;
	wxMenuItem* entries[CONTROLS_COUNT];
};

class autohold_menu : public wxMenu
{
public:
	autohold_menu();
	void reconfigure();
	void on_select(wxCommandEvent& e);
	void update(unsigned pid, unsigned ctrlnum, bool newstate);
private:
	controller_autohold_menu* menus[MAXCONTROLLERS];
	wxMenuItem* entries[MAXCONTROLLERS];
};

controller_autohold_menu::controller_autohold_menu(unsigned lid, enum devicetype_t dtype)
{
	our_lid = lid;
	devtype = DT_NONE;
	for(unsigned i = 0; i < CONTROLS_COUNT; i++) {
		int id = wxID_AUTOHOLD_FIRST + CONTROLS_COUNT * lid + i;
		entries[i] = AppendCheckItem(id, towxstring(get_button_name(i)));
	}
	change_type(dtype);
}

void controller_autohold_menu::change_type(enum devicetype_t dtype)
{
	int pid = controller_index_by_logical(our_lid);
	for(unsigned i = 0; i < CONTROLS_COUNT; i++) {
		int pidx = get_physcial_id_for_control(dtype, i);
		if(pidx >= 0) {
			entries[i]->Check(pid > 0 && get_autohold(pid, pidx));
			entries[i]->Enable();
		} else {
			entries[i]->Check(false);
			entries[i]->Enable(false);
		}
	}
	devtype = dtype;
}

bool controller_autohold_menu::is_dummy()
{
	return (devtype == DT_NONE);
}

void controller_autohold_menu::on_select(wxCommandEvent& e)
{
	int x = e.GetId();
	if(x < wxID_AUTOHOLD_FIRST + our_lid * CONTROLS_COUNT || x >= wxID_AUTOHOLD_FIRST * (our_lid + 1) *
		 CONTROLS_COUNT) {
		return;
	}
	unsigned lidx = (x - wxID_AUTOHOLD_FIRST) % CONTROLS_COUNT;
	int pidx = get_physcial_id_for_control(devtype, lidx);
	int pid = controller_index_by_logical(our_lid);
	if(pid < 0 || pidx < 0 || !entries[lidx]) {
		return;
	}
	//Autohold change on pid=pid, ctrlindx=idx, state
	bool newstate = entries[lidx]->IsChecked();
	change_autohold(pid, pidx, newstate);
}

void controller_autohold_menu::update(unsigned pid, unsigned ctrlnum, bool newstate)
{
	int pid2 = controller_index_by_logical(our_lid);
	if(pid2 < 0 || static_cast<unsigned>(pid) != pid2)
		return;
	for(unsigned i = 0; i < CONTROLS_COUNT; i++) {
		int idx = get_physcial_id_for_control(devtype, i);
		if(idx < 0 || static_cast<unsigned>(idx) != ctrlnum)
			continue;
		entries[i]->Check(newstate);
	}
}

autohold_menu::autohold_menu()
{
	for(unsigned i = 0; i < MAXCONTROLLERS; i++) {
		std::ostringstream str;
		str << "Controller #&" << (i + 1);
		menus[i] = new controller_autohold_menu(i, DT_NONE);
		entries[i] = AppendSubMenu(menus[i], towxstring(str.str()));
		entries[i]->Enable(!menus[i]->is_dummy());
	}
	reconfigure();
}

void autohold_menu::reconfigure()
{
	for(unsigned i = 0; i < MAXCONTROLLERS; i++) {
		menus[i]->change_type(controller_type_by_logical(i));
		entries[i]->Enable(!menus[i]->is_dummy());
	}
}

void autohold_menu::on_select(wxCommandEvent& e)
{
	for(unsigned i = 0; i < MAXCONTROLLERS; i++)
		menus[i]->on_select(e);
}

void autohold_menu::update(unsigned pid, unsigned ctrlnum, bool newstate)
{
	for(unsigned i = 0; i < MAXCONTROLLERS; i++)
		menus[i]->update(pid, ctrlnum, newstate);
}


class emulator_main_window;

class sound_listener : public window_callback
{
public:
	sound_listener(emulator_main_window* w);
	~sound_listener() throw();
	void on_sound_unmute(bool unmute) throw();
	void on_sound_change(const std::string& dev) throw();
	void on_mode_change(bool readonly) throw();
	void on_autohold_update(unsigned pid, unsigned ctrlnum, bool newstate);
	void on_autohold_reconfigure();
private:
	emulator_main_window* win;
};

class emulator_main_panel : public wxPanel
{
public:
	emulator_main_panel(wxWindow* win);
	void on_paint(wxPaintEvent& e);
	void on_erase(wxEraseEvent& e);
	void on_keyboard_down(wxKeyEvent& e);
	void on_keyboard_up(wxKeyEvent& e);
	void on_mouse(wxMouseEvent& e);
};

class emulator_main_window : public wxFrame
{
public:
	emulator_main_window(const std::string& name);
	~emulator_main_window();
	void on_idle(wxIdleEvent& e);
	void on_close(wxCloseEvent& e);
	void menu_pause(wxCommandEvent& e);
	void menu_frameadvance(wxCommandEvent& e);
	void menu_exit(wxCommandEvent& e);
	void menu_subframeadvance(wxCommandEvent& e);
	void menu_nextpoll(wxCommandEvent& e);
	void menu_reset(wxCommandEvent& e);
	void menu_audio_enable(wxCommandEvent& e);
	void menu_audio_status(wxCommandEvent& e);
	void menu_choose_audio_device(wxCommandEvent& e);
	void menu_loadsave(wxCommandEvent& e);
	void menu_scripting(wxCommandEvent& e);
	void menu_readonly(wxCommandEvent& e);
	void menu_edit_authors(wxCommandEvent& e);
	void menu_edit_axes(wxCommandEvent& e);
	void menu_edit_settings(wxCommandEvent& e);
	void menu_edit_keybindings(wxCommandEvent& e);
	void menu_edit_aliases(wxCommandEvent& e);
	void menu_edit_memorywatch(wxCommandEvent& e);
	void menu_load_memorywatch(wxCommandEvent& e);
	void menu_save_memorywatch(wxCommandEvent& e);
	void menu_handle_dump(wxCommandEvent& e);
	void request_paint();
	wxMenuItem* sound_enable;
	wxMenuItem* readonly_enable;
	autohold_menu* ahmenu;
private:
	emulator_main_panel* gpanel;
	sound_listener* slistener;
};

namespace
{

	//Modifier table.
	struct modifier_entry
	{
		int mod;
		const char* name;
		const char* lname;
		modifier* allocated;
	} modifiers[] = {
		{ wxMOD_ALT, "alt", NULL, NULL },
		{ wxMOD_CONTROL, "ctrl", NULL, NULL },
		{ wxMOD_SHIFT, "shift", NULL, NULL },
		{ wxMOD_META, "meta", NULL, NULL },
#ifdef __WXMAC__
		{ wxMOD_CMD, "cmd", NULL, NULL },
#endif
		{ 0, NULL, NULL }
	};

	struct key_entry
	{
		int keynum;
		const char* name;
		keygroup* allocated;
	} keys[] = {
		{ WXK_BACK, "back", NULL },
		{ WXK_TAB, "tab", NULL },
		{ WXK_RETURN, "return", NULL },
		{ WXK_ESCAPE, "escape", NULL },
		{ WXK_SPACE, "space", NULL },
		{ 33, "exclaim", NULL },
		{ 34, "quotedbl", NULL },
		{ 35, "hash", NULL },
		{ 36, "dollar", NULL },
		{ 37, "percent", NULL },
		{ 38, "ampersand", NULL },
		{ 39, "quote", NULL },
		{ 40, "leftparen", NULL },
		{ 41, "rightparen", NULL },
		{ 42, "asterisk", NULL },
		{ 43, "plus", NULL },
		{ 44, "comma", NULL },
		{ 45, "minus", NULL },
		{ 46, "period", NULL },
		{ 47, "slash", NULL },
		{ 48, "0", NULL },
		{ 49, "1", NULL },
		{ 50, "2", NULL },
		{ 51, "3", NULL },
		{ 52, "4", NULL },
		{ 53, "5", NULL },
		{ 54, "6", NULL },
		{ 55, "7", NULL },
		{ 56, "8", NULL },
		{ 57, "9", NULL },
		{ 58, "colon", NULL },
		{ 59, "semicolon", NULL },
		{ 60, "less", NULL },
		{ 61, "equals", NULL },
		{ 62, "greater", NULL },
		{ 63, "question", NULL },
		{ 64, "at", NULL },
		{ 65, "a", NULL },
		{ 66, "b", NULL },
		{ 67, "c", NULL },
		{ 68, "d", NULL },
		{ 69, "e", NULL },
		{ 70, "f", NULL },
		{ 71, "g", NULL },
		{ 72, "h", NULL },
		{ 73, "i", NULL },
		{ 74, "j", NULL },
		{ 75, "k", NULL },
		{ 76, "l", NULL },
		{ 77, "m", NULL },
		{ 78, "n", NULL },
		{ 79, "o", NULL },
		{ 80, "p", NULL },
		{ 81, "q", NULL },
		{ 82, "r", NULL },
		{ 83, "s", NULL },
		{ 84, "t", NULL },
		{ 85, "u", NULL },
		{ 86, "v", NULL },
		{ 87, "w", NULL },
		{ 88, "x", NULL },
		{ 89, "y", NULL },
		{ 90, "z", NULL },
		{ 91, "leftbracket", NULL },
		{ 92, "backslash", NULL },
		{ 93, "rightbracket", NULL },
		{ 94, "caret", NULL },
		{ 95, "underscore", NULL },
		{ 96, "backquote", NULL },
		{ 97, "a", NULL },
		{ 98, "b", NULL },
		{ 99, "c", NULL },
		{ 100, "d", NULL },
		{ 101, "e", NULL },
		{ 102, "f", NULL },
		{ 103, "g", NULL },
		{ 104, "h", NULL },
		{ 105, "i", NULL },
		{ 106, "j", NULL },
		{ 107, "k", NULL },
		{ 108, "l", NULL },
		{ 109, "m", NULL },
		{ 110, "n", NULL },
		{ 111, "o", NULL },
		{ 112, "p", NULL },
		{ 113, "q", NULL },
		{ 114, "r", NULL },
		{ 115, "s", NULL },
		{ 116, "t", NULL },
		{ 117, "u", NULL },
		{ 118, "v", NULL },
		{ 119, "w", NULL },
		{ 120, "x", NULL },
		{ 121, "y", NULL },
		{ 122, "z", NULL },
		{ 123, "leftcurly", NULL },
		{ 124, "pipe", NULL },
		{ 125, "rightcurly", NULL },
		{ 126, "tilde", NULL },
		{ WXK_DELETE, "delete", NULL },
		{ WXK_START, "start", NULL },
		{ WXK_LBUTTON, "lbutton", NULL },
		{ WXK_RBUTTON, "rbutton", NULL },
		{ WXK_CANCEL, "cancel", NULL },
		{ WXK_MBUTTON, "mbutton", NULL },
		{ WXK_CLEAR, "clear", NULL },
		{ WXK_SHIFT, "shift", NULL },
		{ WXK_ALT, "alt", NULL },
		{ WXK_CONTROL, "control", NULL },
		{ WXK_MENU, "menu", NULL },
		{ WXK_PAUSE, "pause", NULL },
		{ WXK_CAPITAL, "capital", NULL },
		{ WXK_END, "end", NULL },
		{ WXK_HOME, "home", NULL },
		{ WXK_LEFT, "lefT", NULL },
		{ WXK_UP, "up", NULL },
		{ WXK_RIGHT, "right", NULL },
		{ WXK_DOWN, "down", NULL },
		{ WXK_SELECT, "select", NULL },
		{ WXK_PRINT, "print", NULL },
		{ WXK_EXECUTE, "execute", NULL },
		{ WXK_SNAPSHOT, "snapshot", NULL },
		{ WXK_INSERT, "insert", NULL },
		{ WXK_HELP, "help", NULL },
		{ WXK_NUMPAD0, "numpad0", NULL },
		{ WXK_NUMPAD1, "numpad1", NULL },
		{ WXK_NUMPAD2, "numpad2", NULL },
		{ WXK_NUMPAD3, "numpad3", NULL },
		{ WXK_NUMPAD4, "numpad4", NULL },
		{ WXK_NUMPAD5, "numpad5", NULL },
		{ WXK_NUMPAD6, "numpad6", NULL },
		{ WXK_NUMPAD7, "numpad7", NULL },
		{ WXK_NUMPAD8, "numpad8", NULL },
		{ WXK_NUMPAD9, "numpad9", NULL },
		{ WXK_MULTIPLY, "multiply", NULL },
		{ WXK_ADD, "add", NULL },
		{ WXK_SEPARATOR, "separator", NULL },
		{ WXK_SUBTRACT, "subtract", NULL },
		{ WXK_DECIMAL, "decimal", NULL },
		{ WXK_DIVIDE, "divide", NULL },
		{ WXK_F1, "f1", NULL },
		{ WXK_F2, "f2", NULL },
		{ WXK_F3, "f3", NULL },
		{ WXK_F4, "f4", NULL },
		{ WXK_F5, "f5", NULL },
		{ WXK_F6, "f6", NULL },
		{ WXK_F7, "f7", NULL },
		{ WXK_F8, "f8", NULL },
		{ WXK_F9, "f9", NULL },
		{ WXK_F10, "f10", NULL },
		{ WXK_F11, "f11", NULL },
		{ WXK_F12, "f12", NULL },
		{ WXK_F13, "f13", NULL },
		{ WXK_F14, "f14", NULL },
		{ WXK_F15, "f15", NULL },
		{ WXK_F16, "f16", NULL },
		{ WXK_F17, "f17", NULL },
		{ WXK_F18, "f18", NULL },
		{ WXK_F19, "f19", NULL },
		{ WXK_F20, "f20", NULL },
		{ WXK_F21, "f21", NULL },
		{ WXK_F22, "f22", NULL },
		{ WXK_F23, "f23", NULL },
		{ WXK_F24, "f24", NULL },
		{ WXK_NUMLOCK, "numlock", NULL },
		{ WXK_SCROLL, "scroll", NULL },
		{ WXK_PAGEUP, "pageup", NULL },
		{ WXK_PAGEDOWN, "pagedown", NULL },
		{ WXK_NUMPAD_SPACE, "numpad_space", NULL },
		{ WXK_NUMPAD_TAB, "numpad_tab", NULL },
		{ WXK_NUMPAD_ENTER, "numpad_enter", NULL },
		{ WXK_NUMPAD_F1, "numpad_f1", NULL },
		{ WXK_NUMPAD_F2, "numpad_f2", NULL },
		{ WXK_NUMPAD_F3, "numpad_f3", NULL },
		{ WXK_NUMPAD_F4, "numpad_f4", NULL },
		{ WXK_NUMPAD_HOME, "numpad_home", NULL },
		{ WXK_NUMPAD_LEFT, "numpad_left", NULL },
		{ WXK_NUMPAD_UP, "numpad_up", NULL },
		{ WXK_NUMPAD_RIGHT, "numpad_right", NULL },
		{ WXK_NUMPAD_DOWN, "numpad_down", NULL },
		{ WXK_NUMPAD_PAGEUP, "numpad_pageup", NULL },
		{ WXK_NUMPAD_PAGEDOWN, "numpad_pagedown", NULL },
		{ WXK_NUMPAD_END, "numpad_end", NULL },
		{ WXK_NUMPAD_BEGIN, "numpad_begin", NULL },
		{ WXK_NUMPAD_INSERT, "numpad_insert", NULL },
		{ WXK_NUMPAD_DELETE, "numpad_delete", NULL },
		{ WXK_NUMPAD_EQUAL, "numpad_equal", NULL },
		{ WXK_NUMPAD_MULTIPLY, "numpad_multiply", NULL },
		{ WXK_NUMPAD_ADD, "numpad_add", NULL },
		{ WXK_NUMPAD_SEPARATOR, "numpad_separator", NULL },
		{ WXK_NUMPAD_SUBTRACT, "numpad_subtract", NULL },
		{ WXK_NUMPAD_DECIMAL, "numpad_decimal", NULL },
		{ WXK_NUMPAD_DIVIDE, "numpad_divide", NULL },
		{ WXK_WINDOWS_LEFT, "windows_left", NULL },
		{ WXK_WINDOWS_RIGHT, "windows_right", NULL },
		{ WXK_WINDOWS_MENU, "windows_menu", NULL },
		{ WXK_COMMAND, "command", NULL },
		{ WXK_SPECIAL1, "special1", NULL },
		{ WXK_SPECIAL2, "special2", NULL },
		{ WXK_SPECIAL3, "special3", NULL },
		{ WXK_SPECIAL4, "special4", NULL },
		{ WXK_SPECIAL5, "special5", NULL },
		{ WXK_SPECIAL6, "special6", NULL },
		{ WXK_SPECIAL7, "special7", NULL },
		{ WXK_SPECIAL8, "special8", NULL },
		{ WXK_SPECIAL9, "special9", NULL },
		{ WXK_SPECIAL10, "special10", NULL },
		{ WXK_SPECIAL11, "special11", NULL },
		{ WXK_SPECIAL12, "special12", NULL },
		{ WXK_SPECIAL13, "special13", NULL },
		{ WXK_SPECIAL14, "special14", NULL },
		{ WXK_SPECIAL15, "special15", NULL },
		{ WXK_SPECIAL16, "special16", NULL },
		{ WXK_SPECIAL17, "special17", NULL },
		{ WXK_SPECIAL18, "special18", NULL },
		{ WXK_SPECIAL19, "special19", NULL },
		{ WXK_SPECIAL20, "special20", NULL },
		{ 0, NULL, NULL }
	};

	std::map<int, modifier*> modifier_map;
	std::map<int, keygroup*> key_map;
	std::map<std::string, int> keys_allocated;
	std::set<int> keys_held;

	void init_modifiers_and_keys()
	{
		static bool done = false;
		if(done)
			return;
		modifier_entry* m = modifiers;
		while(m->name) {
			if(m->lname)
				m->allocated = new modifier(m->name, m->lname);
			else
				m->allocated = new modifier(m->name);
			modifier_map[m->mod] = m->allocated;
			m++;
		}
		key_entry* k = keys;
		while(k->name) {
			if(!keys_allocated.count(k->name)) {
				k->allocated = new keygroup(k->name, keygroup::KT_KEY);
				key_map[k->keynum] = k->allocated;
				keys_allocated[k->name] = k->keynum;
			} else
				key_map[k->keynum] = key_map[keys_allocated[k->name]];
			k++;
		}
		done = true;
	}

	//The coroutine emulator itself runs in.
	coroutine* emu_cr;
	emulator_main_window* main_window = NULL;

	struct emulator_boot_state
	{
		loaded_rom* rom;
		moviefile* movie;
	};

	void emulator_bootup_fn(void* state)
	{
		try {
			struct emulator_boot_state* boot_state = reinterpret_cast<struct emulator_boot_state*>(state);
			main_loop(*boot_state->rom, *boot_state->movie);
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			messages << "FATAL: " << e.what() << std::endl;
			fatal_error();
			return;
		}
	}

	void show_fps()
	{
		auto& emustatus = window::get_emustatus();
		try {
			std::ostringstream y;
			y << get_framerate();
			emustatus["FPS"] = y.str();
		} catch(...) {
		}
	}

	//Set to true if the emulator is in paused mode.
	volatile bool e_paused;
	//Set to true if the emulator is in waiting mode. wait_until is set to get_utime() time when emulator is
	//to exit the wait.
	volatile bool waiting;
	volatile uint64_t wait_until;
	//Set to true if the screen needs updating. screen_updated_full is set if update needs to be full.
	volatile bool screen_updated;
	volatile bool screen_updated_full;
	//The backrequest type.
	volatile int request;
	//The modifier_set and the key (for REQ_KEY_PRESS and REQ_KEY_RELEASE).
	modifier_set keypress_modifiers;
	keygroup* presed_key;
	//Command to send (for REQ_COMMAND).
	std::string pending_command;
	//The main screen
	screen* main_screen;
	unsigned char* screen_buffer;
	uint32_t old_width = 0;
	uint32_t old_height = 0;
	//Message queue (undisplayed messages), and last message.
	bool messages_need_painting;
	std::string last_message;
	//In modal dialog flag.
	bool in_modal_dialog;
	//Painting.
	bool main_window_dirty = false;
	//Audio devices.
	std::map<int, std::string> audio_devs;
	std::map<int, wxMenuItem*> audio_devitems;

	void handle_idle(wxIdleEvent& e)
	{
		if(!emu_cr || in_modal_dialog) {
			wxMilliSleep(1);
			e.RequestMore();
			return;
		}
		request = REQ_POLL_JOYSTICK;
		emu_cr->resume();
		if(e_paused || (waiting && wait_until > get_utime())) {
			wxMilliSleep(1);
			e.RequestMore();
			return;
		}
		if(waiting)
			waiting = false;
		request = REQ_CONTINUE;
		uint64_t exec_start = get_utime();
loop:
		emu_cr->resume();
		if(emu_cr->is_dead()) {
			//Bye!
			if(window1)
				window1->Destroy();
			if(window2)
				window2->Destroy();
			delete emu_cr;
			delete our_rom;
			our_rom = NULL;
			main_window->Destroy();
			return;
		}
		if(get_utime() < exec_start + 10000 && !e_paused && !waiting)
			goto loop;
		show_fps();
		e.RequestMore();
	}

	//Request keypress event to happen.
	void do_keypress(modifier_set mods, keygroup& key, bool polarity)
	{
		if(!emu_cr)
			return;
		keypress_modifiers = mods;
		presed_key = &key;
		request = polarity ? REQ_KEY_PRESS : REQ_KEY_RELEASE;
		emu_cr->resume();
	}

	void handle_wx_keyboard(wxKeyEvent& e, bool polarity)
	{
		int mods = e.GetModifiers();
		int keyc = e.GetKeyCode();
		modifier_set mset;
		modifier_entry* m = modifiers;
		while(m->name) {
			if((keyc & m->mod) == m->mod) {
				mset.add(*m->allocated);
			}
			m++;
		}
		if(polarity) {
			if(keys_held.count(keyc)) {
				e.Skip();
				return;
			}
			keys_held.insert(keyc);
		} else
			keys_held.erase(keyc);
		key_entry* k = keys;
		keygroup* grp = NULL;
		while(k->name) {
			if(k->keynum == keyc) {
				grp = k->allocated;
				break;
			}
			k++;
		}
		if(grp)
			do_keypress(mset, *grp, polarity);
		e.Skip();
	}

	void handle_wx_mouse(wxMouseEvent& e)
	{
		static uint32_t mask = 0;
		if(e.LeftDown())
			mask |= 1;
		if(e.LeftUp())
			mask &= ~1;
		if(e.MiddleDown())
			mask |= 2;
		if(e.MiddleUp())
			mask &= ~2;
		if(e.RightDown())
			mask |= 4;
		if(e.RightUp())
			mask &= ~4;
		window_callback::do_click(e.GetX(), e.GetY(), mask);
	}
}

void emulator_main_panel::on_keyboard_down(wxKeyEvent& e)
{
	handle_wx_keyboard(e, true);
}

void emulator_main_panel::on_keyboard_up(wxKeyEvent& e)
{
	handle_wx_keyboard(e, false);
}

void emulator_main_panel::on_mouse(wxMouseEvent& e)
{
	handle_wx_mouse(e);
}


emulator_main_panel::emulator_main_panel(wxWindow* win)
	: wxPanel(win)
{
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(emulator_main_panel::on_paint), NULL, this);
	this->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(emulator_main_panel::on_erase), NULL, this);
	this->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(emulator_main_panel::on_keyboard_down), NULL, this);
	this->Connect(wxEVT_KEY_UP, wxKeyEventHandler(emulator_main_panel::on_keyboard_up), NULL, this);
	this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(emulator_main_panel::on_mouse), NULL, this);
	this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(emulator_main_panel::on_mouse), NULL, this);
	this->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(emulator_main_panel::on_mouse), NULL, this);
	this->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(emulator_main_panel::on_mouse), NULL, this);
	this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(emulator_main_panel::on_mouse), NULL, this);
	this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(emulator_main_panel::on_mouse), NULL, this);
	SetMinSize(wxSize(512, 448));
}

void emulator_main_panel::on_paint(wxPaintEvent& e)
{
	static struct SwsContext* ctx;
	uint8_t* srcp[1];
	int srcs[1];
	uint8_t* dstp[1];
	int dsts[1];
	wxPaintDC dc(this);
	if(!main_screen)
		return;
	if(main_screen->width != old_width || main_screen->height != old_height) {
		delete[] screen_buffer;
		screen_buffer = new unsigned char[main_screen->width * main_screen->height * 3];
		old_height = main_screen->height;
		old_width = main_screen->width;
		uint32_t w = main_screen->width;
		uint32_t h = main_screen->height;
		ctx = sws_getCachedContext(ctx, w, h, PIX_FMT_RGBA, w, h, PIX_FMT_BGR24, SWS_POINT |
			SWS_CPU_CAPS_MMX2, NULL, NULL, NULL);
		if(w < 512)
			w = 512;
		if(h < 448)
			h = 448;
		SetMinSize(wxSize(w, h));
	}
	srcs[0] = 4 * main_screen->width;
	dsts[0] = 3 * main_screen->width;
	srcp[0] = reinterpret_cast<unsigned char*>(main_screen->memory);
	dstp[0] = screen_buffer;
	memset(screen_buffer, 0, main_screen->width * main_screen->height * 3);
	uint64_t t1 = get_utime();
	sws_scale(ctx, srcp, srcs, 0, main_screen->height, dstp, dsts);
	uint64_t t2 = get_utime();
	wxBitmap bmp(wxImage(main_screen->width, main_screen->height, screen_buffer, true));
	uint64_t t3 = get_utime();
	dc.DrawBitmap(bmp, 0, 0, false);
	main_window_dirty = false;
}

void emulator_main_panel::on_erase(wxEraseEvent& e)
{
}

emulator_main_window::emulator_main_window(const std::string& name)
	: wxFrame(NULL, wxID_ANY, towxstring(name), wxDefaultPosition, wxSize(-1, -1), primary_window_style)
{
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(1, 1, 0, 0);
	top_s->Add(gpanel = new emulator_main_panel(this), 1, wxGROW);
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
	Connect(wxEVT_IDLE, wxIdleEventHandler(emulator_main_window::on_idle));
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(emulator_main_window::on_close));
	wxIdleEvent event;
	event.SetEventObject(this);
	AddPendingEvent(event);
	gpanel->SetFocus();

	wxMenuBar* menubar = new wxMenuBar;
	wxMenu* system = new wxMenu;
	wxMenu* sound = new wxMenu;
	wxMenu* file = new wxMenu;
	wxMenu* scripting = new wxMenu;
	wxMenu* settings = new wxMenu;
	wxMenu* dump = new wxMenu;
	ahmenu = new autohold_menu();
	SetMenuBar(menubar);
	//Main menubar: ACFOS
	menubar->Append(system, wxT("&System"));
	menubar->Append(file, wxT("&File"));
	menubar->Append(ahmenu, wxT("&Autohold"));
	menubar->Append(scripting, wxT("S&cripting"));
	menubar->Append(settings, wxT("Settings"));
	if(window::sound_initialized())
		menubar->Append(sound, wxT("S&ound"));
	//System menu: EMNPQRU
	system->Append(wxID_FRAMEADVANCE, wxT("Fra&me advance"));
	system->Append(wxID_SUBFRAMEADVANCE, wxT("S&ubframe advance"));
	system->Append(wxID_NEXTPOLL, wxT("&Next poll"));
	system->Append(wxID_PAUSE, wxT("&Pause/Unpause"));
	system->AppendSeparator();
	system->Append(wxID_ERESET, wxT("&Reset"));
	system->AppendSeparator();
	system->Append(wxID_EDIT_AUTHORS, wxT("&Edit game name && authors"));
	system->AppendSeparator();
	system->Append(wxID_EXIT, wxT("&Quit"));
	//File menu: DELMNPRTV
	readonly_enable = file->AppendCheckItem(wxID_READONLY_MODE, wxT("Reado&nly mode"));
	readonly_enable->Check(movb.get_movie().readonly_mode());
	file->AppendSeparator();
	file->Append(wxID_SAVE_STATE, wxT("Save stat&e"));
	file->Append(wxID_SAVE_MOVIE, wxT("Sa&ve movie"));
	file->AppendSeparator();
	file->Append(wxID_LOAD_STATE, wxT("&Load state"));
	file->Append(wxID_LOAD_STATE_RO, wxT("Loa&d state (readonly)"));
	file->Append(wxID_LOAD_STATE_RW, wxT("Load s&tate (read-write)"));
	file->Append(wxID_LOAD_STATE_P, wxT("Load state (&preserve)"));
	file->Append(wxID_LOAD_MOVIE, wxT("Load &movie"));
	file->AppendSeparator();
	file->Append(wxID_SAVE_SCREENSHOT, wxT("Save sc&reenshot"));
	file->AppendSeparator();
	file->AppendSubMenu(dump, wxT("Video dump"));
	dump->Append(wxID_DUMP_AVICSCD, wxT("Dump AVI(CSCD)"));
	dump->Append(wxID_END_AVICSCD, wxT("End AVI(CSCD) dump"));
	dump->AppendSeparator();
	dump->Append(wxID_DUMP_JMD, wxT("Dump JMD"));
	dump->Append(wxID_END_JMD, wxT("End JMD dump"));
	dump->AppendSeparator();
	dump->Append(wxID_DUMP_SDMP, wxT("Dump SDMP"));
	dump->Append(wxID_END_SDMP, wxT("End SDMP dump"));
	//Scripting menu: ERU
	scripting->Append(wxID_RUN_SCRIPT, wxT("&Run script"));
	if(lua_supported) {
		scripting->AppendSeparator();
		scripting->Append(wxID_EVAL_LUA, wxT("&Evaluate Lua statement"));
		scripting->Append(wxID_RUN_LUA, wxT("R&un Lua script"));
	}
	scripting->AppendSeparator();
	scripting->Append(wxID_EDIT_MEMORYWATCH, wxT("Edit memory watch"));
	scripting->AppendSeparator();
	scripting->Append(wxID_LOAD_MEMORYWATCH, wxT("Load memory watch"));
	scripting->Append(wxID_SAVE_MEMORYWATCH, wxT("Save memory watch"));
	//Settings menu.
	settings->Append(wxID_EDIT_AXES, wxT("Configure axes"));
	settings->Append(wxID_EDIT_SETTINGS, wxT("Configure settings"));
	settings->Append(wxID_EDIT_KEYBINDINGS, wxT("Configure keybindings"));
	settings->Append(wxID_EDIT_ALIAS, wxT("Configure aliases"));
	//Sound menu U
	sound_enable = NULL;
	if(window::sound_initialized()) {
		slistener = new sound_listener(this);
		sound_enable = sound->AppendCheckItem(wxID_AUDIO_ENABLED, wxT("So&unds enabled"));
		sound_enable->Check(window::is_sound_enabled());
		sound->Append(wxID_SHOW_AUDIO_STATUS, wxT("Show audio status"));
		sound->AppendSeparator();
		int j = wxID_AUDIODEV_FIRST;
		std::string curdev = window::get_current_sound_device();
		for(auto i : window::get_sound_devices()) {
			audio_devitems[j] = sound->AppendRadioItem(j, towxstring(i.first + "(" + i.second + ")"));
			audio_devs[j] = i.first;
			if(i.first == curdev)
				audio_devitems[j]->Check();
			j++;
		}
	}

	menu_action(this, wxID_PAUSE, &emulator_main_window::menu_pause);
	menu_action(this, wxID_FRAMEADVANCE, &emulator_main_window::menu_frameadvance);
	menu_action(this, wxID_SUBFRAMEADVANCE, &emulator_main_window::menu_subframeadvance);
	menu_action(this, wxID_NEXTPOLL, &emulator_main_window::menu_nextpoll);
	menu_action(this, wxID_ERESET, &emulator_main_window::menu_reset);
	menu_action(this, wxID_EXIT, &emulator_main_window::menu_exit);
	menu_action(this, wxID_READONLY_MODE, &emulator_main_window::menu_readonly);
	menu_action(this, wxID_SAVE_STATE, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_SAVE_MOVIE, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_LOAD_STATE, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_LOAD_STATE_RO, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_LOAD_STATE_RW, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_LOAD_STATE_P, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_LOAD_MOVIE, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_SAVE_SCREENSHOT, &emulator_main_window::menu_loadsave);
	menu_action(this, wxID_DUMP_AVICSCD, &emulator_main_window::menu_handle_dump);
	menu_action(this, wxID_DUMP_JMD, &emulator_main_window::menu_handle_dump);
	menu_action(this, wxID_DUMP_SDMP, &emulator_main_window::menu_handle_dump);
	menu_action(this, wxID_END_AVICSCD, &emulator_main_window::menu_handle_dump);
	menu_action(this, wxID_END_JMD, &emulator_main_window::menu_handle_dump);
	menu_action(this, wxID_END_SDMP, &emulator_main_window::menu_handle_dump);
	menu_action(this, wxID_RUN_SCRIPT, &emulator_main_window::menu_scripting);
	menu_action(this, wxID_EDIT_AUTHORS, &emulator_main_window::menu_edit_authors);
	Connect(wxID_AUTOHOLD_FIRST, wxID_AUTOHOLD_LAST, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(autohold_menu::on_select), NULL, ahmenu);
	if(lua_supported) {
		menu_action(this, wxID_EVAL_LUA, &emulator_main_window::menu_scripting);
		menu_action(this, wxID_RUN_LUA, &emulator_main_window::menu_scripting);
	}
	menu_action(this, wxID_EDIT_MEMORYWATCH, &emulator_main_window::menu_edit_memorywatch);
	menu_action(this, wxID_LOAD_MEMORYWATCH, &emulator_main_window::menu_load_memorywatch);
	menu_action(this, wxID_SAVE_MEMORYWATCH, &emulator_main_window::menu_save_memorywatch);
	menu_action(this, wxID_EDIT_AXES, &emulator_main_window::menu_edit_axes);
	menu_action(this, wxID_EDIT_SETTINGS, &emulator_main_window::menu_edit_settings);
	menu_action(this, wxID_EDIT_KEYBINDINGS, &emulator_main_window::menu_edit_keybindings);
	menu_action(this, wxID_EDIT_ALIAS, &emulator_main_window::menu_edit_aliases);
	if(window::sound_initialized()) {
		menu_action(this, wxID_AUDIO_ENABLED, &emulator_main_window::menu_audio_enable);
		menu_action(this, wxID_SHOW_AUDIO_STATUS, &emulator_main_window::menu_audio_status);
		for(auto i : audio_devs)
			menu_action(this, i.first, &emulator_main_window::menu_choose_audio_device);
	}
}

emulator_main_window::~emulator_main_window()
{
	delete slistener;
}

void emulator_main_window::request_paint()
{
	gpanel->Refresh();
}

void emulator_main_window::on_idle(wxIdleEvent& e)
{
	handle_idle(e);
}

void emulator_main_window::on_close(wxCloseEvent& e)
{
	//Veto it for now, latter things will delete it.
	e.Veto();
	exec_command("quit-emulator");
}

void emulator_main_window::menu_pause(wxCommandEvent& e)
{
	exec_command("pause-emulator");
}

void emulator_main_window::menu_frameadvance(wxCommandEvent& e)
{
	exec_command("+advance-frame");
	exec_command("-advance-frame");
}

void emulator_main_window::menu_subframeadvance(wxCommandEvent& e)
{
	exec_command("+advance-poll");
	exec_command("-advance-poll");
}

void emulator_main_window::menu_nextpoll(wxCommandEvent& e)
{
	exec_command("advance-skiplag");
}

void emulator_main_window::menu_reset(wxCommandEvent& e)
{
	exec_command("reset");
}

void emulator_main_window::menu_exit(wxCommandEvent& e)
{
	exec_command("quit-emulator");
}

void emulator_main_window::menu_audio_enable(wxCommandEvent& e)
{
	window::sound_enable(sound_enable->IsChecked());
}

void emulator_main_window::menu_readonly(wxCommandEvent& e)
{
	bool s = readonly_enable->IsChecked();
	movb.get_movie().readonly_mode(s);
	if(!s)
		lua_callback_do_readwrite();
	update_movie_state();
	window::notify_screen_update();
}

void emulator_main_window::menu_edit_authors(wxCommandEvent& e)
{
	wxDialog* editor = new wx_authors_editor(this);
	editor->ShowModal();
	editor->Destroy();
}

void emulator_main_window::menu_edit_axes(wxCommandEvent& e)
{
	wxDialog* editor = new wx_axes_editor(this);
	editor->ShowModal();
	editor->Destroy();
}

void emulator_main_window::menu_edit_settings(wxCommandEvent& e)
{
	wxDialog* editor = new wx_settings_editor(this);
	editor->ShowModal();
	editor->Destroy();
}

#define NEW_KEYBINDING "A new binding..."
#define NEW_ALIAS "A new alias..."
#define NEW_WATCH "A new watch..."

void emulator_main_window::menu_edit_keybindings(wxCommandEvent& e)
{
	std::set<std::string> bind = keymapper::get_bindings();
	std::vector<wxString> choices;
	choices.push_back(wxT(NEW_KEYBINDING));
	for(auto i : bind)
		choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, wxT("Select keybinding to edit"),
		wxT("Select binding"), choices.size(), &choices[0]);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	std::string key = tostdstring(d->GetStringSelection());
	d->Destroy();
	if(key == NEW_KEYBINDING) {
		wx_key_entry* d2 = new wx_key_entry(this);
		//wxTextEntryDialog* d2 = new wxTextEntryDialog(this, wxT("Enter key for binding:"),
		//	wxT("Edit binding"), wxT(""));
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			return;
		}
		key = d2->getkey();
		//key = tostdstring(d2->GetValue());
		d2->Destroy();
	}
	std::string old_command_value = keymapper::get_command_for(key);
	wxTextEntryDialog* d4 = new wxTextEntryDialog(this, wxT("Enter new command for binding:"), wxT("Edit binding"),
		towxstring(old_command_value));
	if(d4->ShowModal() == wxID_CANCEL) {
		d4->Destroy();
		return;
	}
	try {
		keymapper::bind_for(key, tostdstring(d4->GetValue()));
	} catch(std::exception& e) {
		wxMessageDialog* d3 = new wxMessageDialog(this, towxstring(std::string("Can't bind key: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d3->ShowModal();
		d3->Destroy();
	}
	d4->Destroy();
}

void emulator_main_window::menu_edit_aliases(wxCommandEvent& e)
{
	std::set<std::string> bind = command::get_aliases();
	std::vector<wxString> choices;
	choices.push_back(wxT(NEW_ALIAS));
	for(auto i : bind)
		choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, wxT("Select alias to edit"),
		wxT("Select alias"), choices.size(), &choices[0]);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	std::string alias = tostdstring(d->GetStringSelection());
	d->Destroy();
	if(alias == NEW_ALIAS) {
		wxTextEntryDialog* d2 = new wxTextEntryDialog(this, wxT("Enter name for the new alias:"),
			wxT("Enter alias name"));
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			return;
		}
		alias = tostdstring(d2->GetValue());
		d2->Destroy();
		if(!command::valid_alias_name(alias)) {
			wxMessageDialog* d3 = new wxMessageDialog(this, towxstring(std::string("Not a valid alias "
			"name: ") + alias), wxT("Error"), wxOK | wxICON_EXCLAMATION);
			d3->ShowModal();
			d3->Destroy();
			return;
		}
	}
	std::string old_alias_value = command::get_alias_for(alias);
	wxTextEntryDialog* d4 = new wxTextEntryDialog(this, wxT("Enter new commands for alias:"), wxT("Edit alias"),
		towxstring(old_alias_value), wxOK | wxCANCEL | wxCENTRE | wxTE_MULTILINE);
	if(d4->ShowModal() == wxID_CANCEL) {
		d4->Destroy();
		return;
	}
	command::set_alias_for(alias, tostdstring(d4->GetValue()));
	d4->Destroy();
}

void emulator_main_window::menu_load_memorywatch(wxCommandEvent& e)
{
	std::set<std::string> old_watches = get_watches();
	std::map<std::string, std::string> new_watches;
	std::string filename;

	wxFileDialog* d = new wxFileDialog(this, towxstring("Choose memory watch file"), wxT("."));
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	filename = tostdstring(d->GetPath());
	d->Destroy();
	//Did we pick a .zip file?
	try {
		zip_reader zr(filename);
		std::vector<wxString> files;
		for(auto i : zr)
			files.push_back(towxstring(i));
		wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(this, wxT("Select file within .zip"),
			wxT("Select member"), files.size(), &files[0]);
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			return;
		}
		filename = filename + "/" + tostdstring(d2->GetStringSelection());
		d2->Destroy();
	} catch(...) {
		//Ignore error.
	}

	try {
		std::istream& in = open_file_relative(filename, "");
		while(in) {
			std::string wname;
			std::string wexpr;
			std::getline(in, wname);
			std::getline(in, wexpr);
			new_watches[wname] = wexpr;
		}
		delete &in;
	} catch(std::exception& e) {
		wxMessageDialog* d3 = new wxMessageDialog(this, towxstring(std::string("Can't load memory "
			"watch: ") + e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d3->ShowModal();
		d3->Destroy();
	}

	for(auto i : new_watches)
		set_watchexpr_for(i.first, i.second);
	for(auto i : old_watches)
		if(!new_watches.count(i))
			set_watchexpr_for(i, "");
}

void emulator_main_window::menu_save_memorywatch(wxCommandEvent& e)
{
	std::set<std::string> old_watches = get_watches();
	std::string filename;

	wxFileDialog* d = new wxFileDialog(this, towxstring("Save watches to file"), wxT("."));
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	filename = tostdstring(d->GetPath());
	d->Destroy();

	std::ofstream out(filename.c_str());
	for(auto i : old_watches)
		out << i << std::endl << get_watchexpr_for(i) << std::endl;
	out.close();
}


void emulator_main_window::menu_edit_memorywatch(wxCommandEvent& e)
{
	std::set<std::string> bind = get_watches();
	std::vector<wxString> choices;
	choices.push_back(wxT(NEW_WATCH));
	for(auto i : bind)
		choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, wxT("Select watch to edit"),
		wxT("Select watch"), choices.size(), &choices[0]);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	std::string watch = tostdstring(d->GetStringSelection());
	d->Destroy();
	if(watch == NEW_WATCH) {
		wxTextEntryDialog* d2 = new wxTextEntryDialog(this, wxT("Enter name for the new watch:"),
			wxT("Enter watch name"));
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			return;
		}
		watch = tostdstring(d2->GetValue());
		d2->Destroy();
	}
	std::string old_watch_value = get_watchexpr_for(watch);
	wxTextEntryDialog* d4 = new wxTextEntryDialog(this, wxT("Enter new expression for watch:"), wxT("Edit watch"),
		towxstring(old_watch_value), wxOK | wxCANCEL | wxCENTRE);
	if(d4->ShowModal() == wxID_CANCEL) {
		d4->Destroy();
		return;
	}
	set_watchexpr_for(watch, tostdstring(d4->GetValue()));
	d4->Destroy();
}

void emulator_main_window::menu_handle_dump(wxCommandEvent& e)
{
	wxString choices[19];
	size_t choice_count = 0;
	switch(e.GetId()) {
	case wxID_END_AVICSCD:
		exec_command("end-avi");
		return;
	case wxID_END_JMD:
		exec_command("end-jmd");
		return;
	case wxID_END_SDMP:
		exec_command("end-sdmp");
		return;
	default:
		break;
	};
	bool is_prefix = false;
	bool has_level = false;
	std::string msg;
	std::string caption;
	std::string cmd;
	switch(e.GetId()) {
	case wxID_DUMP_AVICSCD:
		choices[choice_count++] = wxT("Compression Level 0 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 1 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 2 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 3 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 4 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 5 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 6 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 7 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 8 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 9 intraframe-only");
		choices[choice_count++] = wxT("Compression Level 1");
		choices[choice_count++] = wxT("Compression Level 2");
		choices[choice_count++] = wxT("Compression Level 3");
		choices[choice_count++] = wxT("Compression Level 4");
		choices[choice_count++] = wxT("Compression Level 5");
		choices[choice_count++] = wxT("Compression Level 6");
		choices[choice_count++] = wxT("Compression Level 7");
		choices[choice_count++] = wxT("Compression Level 8");
		choices[choice_count++] = wxT("Compression Level 9");
		msg = "Choose CSCD compression level: ";
		caption = "AVI(CSCD) dump";
		is_prefix = true;
		cmd = "dump-avi";
		has_level = true;
		break;
	case wxID_DUMP_JMD:
		choices[choice_count++] = wxT("Compression Level 0");
		choices[choice_count++] = wxT("Compression Level 1");
		choices[choice_count++] = wxT("Compression Level 2");
		choices[choice_count++] = wxT("Compression Level 3");
		choices[choice_count++] = wxT("Compression Level 4");
		choices[choice_count++] = wxT("Compression Level 5");
		choices[choice_count++] = wxT("Compression Level 6");
		choices[choice_count++] = wxT("Compression Level 7");
		choices[choice_count++] = wxT("Compression Level 8");
		choices[choice_count++] = wxT("Compression Level 9");
		msg = "Choose JMD compression level: ";
		caption = "JMD dump";
		is_prefix = false;
		cmd = "dump-jmd";
		has_level = true;
		break;
	case wxID_DUMP_SDMP:
		choices[choice_count++] = wxT("Segmented");
		choices[choice_count++] = wxT("Single segment");
		msg = "Choose SDMP settings: ";
		caption = "SDMP dump";
		is_prefix = false;
		cmd = "dump-sdmpss";
		has_level = false;
		break;
	}
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, towxstring(msg), towxstring(caption), choice_count,
		choices);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	int choice = d->GetSelection();
	if(e.GetId() == wxID_DUMP_SDMP && choice == 0) {
		cmd = "dump-sdmp";
		is_prefix = true;
	}
	d->Destroy();

	std::string prefix;
	wxFileDialog* d2 = new wxFileDialog(this, is_prefix ? wxT("Dump prefix") : wxT("Dump file"), wxT("."));
	if(d2->ShowModal() == wxID_OK)
		prefix = tostdstring(d2->GetPath());
	d2->Destroy();

	std::ostringstream str;
	str << cmd;
	if(has_level)
		str << " " << choice;
	str << " " << prefix;
	exec_command(str.str());
}


void emulator_main_window::menu_audio_status(wxCommandEvent& e)
{
	exec_command("show-sound-status");
}

void emulator_main_window::menu_choose_audio_device(wxCommandEvent& e)
{
	if(!audio_devs.count(e.GetId()))
		return;		//Not supposed to happen.
	window::set_sound_device(audio_devs[e.GetId()]);
}

void emulator_main_window::menu_loadsave(wxCommandEvent& e)
{
	int id = e.GetId();
	bool is_save = (id == wxID_SAVE_MOVIE || id == wxID_SAVE_STATE || id == wxID_SAVE_SCREENSHOT);
	std::string filename;

	wxFileDialog* d = new wxFileDialog(this, is_save ? wxT("Save") : wxT("Load"), wxT("."));
	if(d->ShowModal() == wxID_OK)
		filename = tostdstring(d->GetPath());
	d->Destroy();
	if(filename == "")
		return;

	switch(id) {
	case wxID_LOAD_MOVIE:
		exec_command("load-movie " + filename);
		break;
	case wxID_LOAD_STATE:
		exec_command("load " + filename);
		break;
	case wxID_LOAD_STATE_RO:
		exec_command("load-readonly " + filename);
		break;
	case wxID_LOAD_STATE_RW:
		exec_command("load-state " + filename);
		break;
	case wxID_SAVE_MOVIE:
		exec_command("save-movie " + filename);
		break;
	case wxID_SAVE_STATE:
		exec_command("save-state " + filename);
		break;
	case wxID_SAVE_SCREENSHOT:
		exec_command("take-screenshot " + filename);
		break;
	}
}

void emulator_main_window::menu_scripting(wxCommandEvent& e)
{
	int id = e.GetId();
	bool file = (id == wxID_RUN_LUA || id == wxID_RUN_SCRIPT);
	std::string name;
	
	if(file) {
		wxFileDialog* d = new wxFileDialog(this, wxT("Select Script"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			name = tostdstring(d->GetPath());
		d->Destroy();
	} else {
		wxTextEntryDialog* d = new wxTextEntryDialog(this, wxT("Enter Lua statement:"),
			wxT("Evaluate Lua"));
		if(d->ShowModal() == wxID_OK)
			name = tostdstring(d->GetValue());
		d->Destroy();
	}
	if(name == "")
		return;
	
	switch(id) {
	case wxID_RUN_SCRIPT:
		exec_command("run-script " + name);
		break;
	case wxID_EVAL_LUA:
		exec_command("evaluate-lua " + name);
		break;
	case wxID_RUN_LUA:
		exec_command("run-lua " + name);
		break;
	}
}

sound_listener::sound_listener(emulator_main_window* w)
{
	win = w;
}

sound_listener::~sound_listener() throw()
{
}

void sound_listener::on_sound_unmute(bool unmute) throw()
{
	if(win && win->sound_enable)
		win->sound_enable->Check(unmute);
}

void sound_listener::on_mode_change(bool readonly) throw()
{
	if(win && win->readonly_enable)
		win->readonly_enable->Check(readonly);
}

void sound_listener::on_autohold_update(unsigned pid, unsigned ctrlnum, bool newstate)
{
	if(win && win->ahmenu)
		win->ahmenu->update(pid, ctrlnum, newstate);
}

void sound_listener::on_autohold_reconfigure()
{
	if(win && win->ahmenu)
		win->ahmenu->reconfigure();
}

void sound_listener::on_sound_change(const std::string& dev) throw()
{
	int j = wxID_ANY;
	for(auto i : audio_devs) {
		if(dev == i.second) {
			j = i.first;
			break;
		}
	}
	if(j == wxID_ANY)
		return;
	audio_devitems[j]->Check();
}

void boot_emulator(loaded_rom& rom, moviefile& movie)
{
	wx_status_window* status = new wx_status_window();
	window2 = status;
	status->Show();
	std::string windowname = "lsnes-" + lsnes_version + "[" + bsnes_core_version + "]";
	main_window = new emulator_main_window(windowname);
	struct emulator_boot_state s;
	s.rom = &rom;
	s.movie = &movie;
	emu_cr = new coroutine(emulator_bootup_fn, &s, STACKSIZE);
	messages << "Started emulator main coroutine" << std::endl;
	//Delete the rom and movie. They aren't needed anymore.
	delete &movie;
	main_window->Show();
}

void exec_command(const std::string& cmd)
{
	if(!emu_cr)
		return;
	pending_command = cmd;
	request = REQ_COMMAND;
	emu_cr->resume();
}


void window::poll_inputs() throw(std::bad_alloc)
{
	do {
		coroutine::yield();
		if(request == REQ_POLL_JOYSTICK)
			window::poll_joysticks();
		if(request == REQ_KEY_PRESS)
			presed_key->set_position(1, keypress_modifiers);
		if(request == REQ_KEY_RELEASE)
			presed_key->set_position(0, keypress_modifiers);
		if(request == REQ_CONTINUE)
			break;
		if(request == REQ_COMMAND)
			command::invokeC(pending_command);
	} while(1);
}

void window::notify_screen_update(bool full) throw()
{
	screen_updated_full = true;
	screen_updated = true;
	if(main_window && !main_window_dirty) {
		main_window_dirty = true;
		main_window->request_paint();
	}
	if(wx_status_window::ptr)
		wx_status_window::ptr->notify_status_change();
}

void window::set_main_surface(screen& scr) throw()
{
	main_screen = &scr;
	screen_updated_full = true;
	screen_updated = true;
}

void window::paused(bool enable) throw()
{
	e_paused = enable;
	screen_updated = true;
}

void window::wait_usec(uint64_t usec) throw(std::bad_alloc)
{
	waiting = true;
	wait_until = get_utime() + usec;
	poll_inputs();
}

void window::cancel_wait() throw()
{
	waiting = false;
}

void window::fatal_error2() throw()
{
	in_modal_dialog = true;
	std::string err = "Unknown fatal error occured";
	if(window::msgbuf.get_msg_count() > 0)
		err = window::msgbuf.get_message(window::msgbuf.get_msg_first() + window::msgbuf.get_msg_count() - 1);
	wxMessageDialog* d = new wxMessageDialog(main_window, towxstring(err), wxT("Fatal Error"),
		wxOK | wxICON_ERROR);
	d->ShowModal();
	exit(1);
}

bool window::modal_message(const std::string& msg, bool confirm) throw(std::bad_alloc)
{
	in_modal_dialog = true;
	wxMessageDialog* d;
	if(confirm)
		d = new wxMessageDialog(main_window, towxstring(msg), wxT("Question"),
			wxOK | wxCANCEL | wxICON_QUESTION);
	else
		d = new wxMessageDialog(main_window, towxstring(msg), wxT("Information"),
			wxOK | wxICON_INFORMATION);
	auto r = d->ShowModal();
	d->Destroy();
	in_modal_dialog = false;
	if(r == wxID_OK)
		return confirm;
	return false;
}

void graphics_init()
{
	init_modifiers_and_keys();
}

void graphics_quit()
{
	for(auto i : modifier_map)
		delete i.second;
	for(auto i : keys_allocated)
		delete key_map[i.second];
	modifier_map.clear();
	key_map.clear();
	keys_allocated.clear();
	keys_held.clear();
}


const char* graphics_plugin_name = "Wxwidgets graphics plugin";
