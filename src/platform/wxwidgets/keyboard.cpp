#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "library/keyboard.hpp"
#include "library/keyboard-mapper.hpp"
#include "core/instance.hpp"
#include "core/instance-map.hpp"
#include "core/queue.hpp"
#include "core/window.hpp"
#include "core/ui-services.hpp"
#include "platform/wxwidgets/platform.hpp"

#include <cstdint>
#include <map>

int wx_escape_count = 0;

namespace
{
	//Modifier table.
	struct modifier_entry
	{
		int mod;
		const char* name;
		const char* lname;
	} modifiers[] = {
		{ wxMOD_ALT, "alt", NULL },
		{ wxMOD_CONTROL, "ctrl", NULL },
		{ wxMOD_SHIFT, "shift", NULL },
		{ wxMOD_META, "meta", NULL },
#ifdef __WXMAC__
		{ wxMOD_CMD, "cmd", NULL },
#endif
		{ 0, NULL, NULL }
	};

	struct key_entry
	{
		int keynum;
		const char* name;
		const char* clazz;
	} keys[] = {
		{ WXK_BACK, "back", "editing" },
		{ WXK_TAB, "tab", "editing" },
		{ WXK_RETURN, "return", "editing" },
		{ WXK_ESCAPE, "escape", "editing" },
		{ WXK_SPACE, "space", "characters" },
		{ 33, "exclaim", "characters" },
		{ 34, "quotedbl", "characters" },
		{ 35, "hash", "characters" },
		{ 36, "dollar", "characters" },
		{ 37, "percent", "characters" },
		{ 38, "ampersand", "characters" },
		{ 39, "quote", "characters" },
		{ 40, "leftparen", "characters" },
		{ 41, "rightparen", "characters" },
		{ 42, "asterisk", "characters" },
		{ 43, "plus", "characters" },
		{ 44, "comma", "characters" },
		{ 45, "minus", "characters" },
		{ 46, "period", "characters" },
		{ 47, "slash", "characters" },
		{ 48, "0", "numeric" },
		{ 49, "1", "numeric" },
		{ 50, "2", "numeric" },
		{ 51, "3", "numeric" },
		{ 52, "4", "numeric" },
		{ 53, "5", "numeric" },
		{ 54, "6", "numeric" },
		{ 55, "7", "numeric" },
		{ 56, "8", "numeric" },
		{ 57, "9", "numeric" },
		{ 58, "colon", "characters" },
		{ 59, "semicolon", "characters" },
		{ 60, "less", "characters" },
		{ 61, "equals", "characters" },
		{ 62, "greater", "characters" },
		{ 63, "question", "characters" },
		{ 64, "at", "characters" },
		{ 65, "a", "alphabetic" },
		{ 66, "b", "alphabetic" },
		{ 67, "c", "alphabetic" },
		{ 68, "d", "alphabetic" },
		{ 69, "e", "alphabetic" },
		{ 70, "f", "alphabetic" },
		{ 71, "g", "alphabetic" },
		{ 72, "h", "alphabetic" },
		{ 73, "i", "alphabetic" },
		{ 74, "j", "alphabetic" },
		{ 75, "k", "alphabetic" },
		{ 76, "l", "alphabetic" },
		{ 77, "m", "alphabetic" },
		{ 78, "n", "alphabetic" },
		{ 79, "o", "alphabetic" },
		{ 80, "p", "alphabetic" },
		{ 81, "q", "alphabetic" },
		{ 82, "r", "alphabetic" },
		{ 83, "s", "alphabetic" },
		{ 84, "t", "alphabetic" },
		{ 85, "u", "alphabetic" },
		{ 86, "v", "alphabetic" },
		{ 87, "w", "alphabetic" },
		{ 88, "x", "alphabetic" },
		{ 89, "y", "alphabetic" },
		{ 90, "z", "alphabetic" },
		{ 91, "leftbracket", "characters" },
		{ 92, "backslash", "characters" },
		{ 93, "rightbracket", "characters" },
		{ 94, "caret", "characters" },
		{ 95, "underscore", "characters" },
		{ 96, "backquote", "characters" },
		{ 97, "a", "alphabetic" },
		{ 98, "b", "alphabetic" },
		{ 99, "c", "alphabetic" },
		{ 100, "d", "alphabetic" },
		{ 101, "e", "alphabetic" },
		{ 102, "f", "alphabetic" },
		{ 103, "g", "alphabetic" },
		{ 104, "h", "alphabetic" },
		{ 105, "i", "alphabetic" },
		{ 106, "j", "alphabetic" },
		{ 107, "k", "alphabetic" },
		{ 108, "l", "alphabetic" },
		{ 109, "m", "alphabetic" },
		{ 110, "n", "alphabetic" },
		{ 111, "o", "alphabetic" },
		{ 112, "p", "alphabetic" },
		{ 113, "q", "alphabetic" },
		{ 114, "r", "alphabetic" },
		{ 115, "s", "alphabetic" },
		{ 116, "t", "alphabetic" },
		{ 117, "u", "alphabetic" },
		{ 118, "v", "alphabetic" },
		{ 119, "w", "alphabetic" },
		{ 120, "x", "alphabetic" },
		{ 121, "y", "alphabetic" },
		{ 122, "z", "alphabetic" },
		{ 123, "leftcurly", "characters" },
		{ 124, "pipe", "characters" },
		{ 125, "rightcurly", "characters" },
		{ 126, "tilde", "characters" },
		{ WXK_DELETE, "delete", "editing" },
		{ WXK_START, "start", "special" },
		{ WXK_LBUTTON, "lbutton", "special" },
		{ WXK_RBUTTON, "rbutton", "special" },
		{ WXK_CANCEL, "cancel", "special" },
		{ WXK_MBUTTON, "mbutton", "special" },
		{ WXK_CLEAR, "clear", "editing" },
		{ WXK_SHIFT, "shift", "modifiers" },
		{ WXK_ALT, "alt", "modifiers" },
		{ WXK_CONTROL, "control", "modifiers" },
		{ WXK_MENU, "menu", "special" },
		{ WXK_PAUSE, "pause", "special" },
		{ WXK_CAPITAL, "capital", "locks" },
		{ WXK_END, "end", "editing" },
		{ WXK_HOME, "home", "editing" },
		{ WXK_LEFT, "lefT", "editing" },
		{ WXK_UP, "up", "editing" },
		{ WXK_RIGHT, "right", "editing" },
		{ WXK_DOWN, "down", "editing" },
		{ WXK_SELECT, "select", "special" },
		{ WXK_PRINT, "print", "special" },
		{ WXK_EXECUTE, "execute", "special" },
		{ WXK_SNAPSHOT, "snapshot", "special" },
		{ WXK_INSERT, "insert", "editing" },
		{ WXK_HELP, "help", "special" },
		{ WXK_NUMPAD0, "numpad0", "numeric" },
		{ WXK_NUMPAD1, "numpad1", "numeric" },
		{ WXK_NUMPAD2, "numpad2", "numeric" },
		{ WXK_NUMPAD3, "numpad3", "numeric" },
		{ WXK_NUMPAD4, "numpad4", "numeric" },
		{ WXK_NUMPAD5, "numpad5", "numeric" },
		{ WXK_NUMPAD6, "numpad6", "numeric" },
		{ WXK_NUMPAD7, "numpad7", "numeric" },
		{ WXK_NUMPAD8, "numpad8", "numeric" },
		{ WXK_NUMPAD9, "numpad9", "numeric" },
		{ WXK_MULTIPLY, "multiply", "characters" },
		{ WXK_ADD, "add", "characters" },
		{ WXK_SEPARATOR, "separator", "characters" },
		{ WXK_SUBTRACT, "subtract", "characters" },
		{ WXK_DECIMAL, "decimal", "characters" },
		{ WXK_DIVIDE, "divide", "characters" },
		{ WXK_F1, "f1", "F-keys" },
		{ WXK_F2, "f2", "F-keys" },
		{ WXK_F3, "f3", "F-keys" },
		{ WXK_F4, "f4", "F-keys" },
		{ WXK_F5, "f5", "F-keys" },
		{ WXK_F6, "f6", "F-keys" },
		{ WXK_F7, "f7", "F-keys" },
		{ WXK_F8, "f8", "F-keys" },
		{ WXK_F9, "f9", "F-keys" },
		{ WXK_F10, "f10", "F-keys" },
		{ WXK_F11, "f11", "F-keys" },
		{ WXK_F12, "f12", "F-keys" },
		{ WXK_F13, "f13", "F-keys" },
		{ WXK_F14, "f14", "F-keys" },
		{ WXK_F15, "f15", "F-keys" },
		{ WXK_F16, "f16", "F-keys" },
		{ WXK_F17, "f17", "F-keys" },
		{ WXK_F18, "f18", "F-keys" },
		{ WXK_F19, "f19", "F-keys" },
		{ WXK_F20, "f20", "F-keys" },
		{ WXK_F21, "f21", "F-keys" },
		{ WXK_F22, "f22", "F-keys" },
		{ WXK_F23, "f23", "F-keys" },
		{ WXK_F24, "f24", "F-keys" },
		{ WXK_NUMLOCK, "numlock", "locks" },
		{ WXK_SCROLL, "scroll", "locks" },
		{ WXK_PAGEUP, "pageup", "editing" },
		{ WXK_PAGEDOWN, "pagedown", "editing" },
		{ WXK_NUMPAD_SPACE, "numpad_space", "editing" },
		{ WXK_NUMPAD_TAB, "numpad_tab", "editing" },
		{ WXK_NUMPAD_ENTER, "numpad_enter", "editing" },
		{ WXK_NUMPAD_F1, "numpad_f1", "F-keys" },
		{ WXK_NUMPAD_F2, "numpad_f2", "F-keys" },
		{ WXK_NUMPAD_F3, "numpad_f3", "F-keys" },
		{ WXK_NUMPAD_F4, "numpad_f4", "F-keys" },
		{ WXK_NUMPAD_HOME, "numpad_home", "editing" },
		{ WXK_NUMPAD_LEFT, "numpad_left", "editing" },
		{ WXK_NUMPAD_UP, "numpad_up", "editing" },
		{ WXK_NUMPAD_RIGHT, "numpad_right", "editing" },
		{ WXK_NUMPAD_DOWN, "numpad_down", "editing" },
		{ WXK_NUMPAD_PAGEUP, "numpad_pageup", "editing" },
		{ WXK_NUMPAD_PAGEDOWN, "numpad_pagedown", "editing" },
		{ WXK_NUMPAD_END, "numpad_end", "editing" },
		{ WXK_NUMPAD_BEGIN, "numpad_begin", "editing" },
		{ WXK_NUMPAD_INSERT, "numpad_insert", "editing" },
		{ WXK_NUMPAD_DELETE, "numpad_delete", "editing" },
		{ WXK_NUMPAD_EQUAL, "numpad_equal", "characters" },
		{ WXK_NUMPAD_MULTIPLY, "numpad_multiply", "characters" },
		{ WXK_NUMPAD_ADD, "numpad_add", "characters" },
		{ WXK_NUMPAD_SEPARATOR, "numpad_separator", "characters" },
		{ WXK_NUMPAD_SUBTRACT, "numpad_subtract", "characters" },
		{ WXK_NUMPAD_DECIMAL, "numpad_decimal", "characters" },
		{ WXK_NUMPAD_DIVIDE, "numpad_divide", "characters" },
		{ WXK_WINDOWS_LEFT, "windows_left", "modifiers" },
		{ WXK_WINDOWS_RIGHT, "windows_right", "modifiers" },
		{ WXK_WINDOWS_MENU, "windows_menu", "modifiers" },
		{ WXK_COMMAND, "command", "special" },
		{ WXK_SPECIAL1, "special1", "special" },
		{ WXK_SPECIAL2, "special2", "special" },
		{ WXK_SPECIAL3, "special3", "special" },
		{ WXK_SPECIAL4, "special4", "special" },
		{ WXK_SPECIAL5, "special5", "special" },
		{ WXK_SPECIAL6, "special6", "special" },
		{ WXK_SPECIAL7, "special7", "special" },
		{ WXK_SPECIAL8, "special8", "special" },
		{ WXK_SPECIAL9, "special9", "special" },
		{ WXK_SPECIAL10, "special10", "special" },
		{ WXK_SPECIAL11, "special11", "special" },
		{ WXK_SPECIAL12, "special12", "special" },
		{ WXK_SPECIAL13, "special13", "special" },
		{ WXK_SPECIAL14, "special14", "special" },
		{ WXK_SPECIAL15, "special15", "special" },
		{ WXK_SPECIAL16, "special16", "special" },
		{ WXK_SPECIAL17, "special17", "special" },
		{ WXK_SPECIAL18, "special18", "special" },
		{ WXK_SPECIAL19, "special19", "special" },
		{ WXK_SPECIAL20, "special20", "special" },
		{ 167, "§", "characters" },
		{ 246, "ö", "alphabetic" },
		{ 228, "ä", "alphabetic" },
		{ 229, "å", "alphabetic" },
		{ 0, NULL, NULL }
	};

	struct keyboard_state
	{
		keyboard_state(emulator_instance& inst)
		{
		}
		~keyboard_state()
		{
			modifier_map.clear();
			key_map.clear();
			keys_allocated.clear();
			keys_held.clear();
			for(auto& m : mallocated)
				delete m.second;
			for(auto& k : kallocated)
				delete k.second;
			mallocated.clear();
			kallocated.clear();
		}
		std::map<int, keyboard::modifier*> modifier_map;
		std::map<int, keyboard::key_key*> key_map;
		std::map<std::string, int> keys_allocated;
		std::set<int> keys_held;
		std::map<modifier_entry*, keyboard::modifier*> mallocated;
		std::map<key_entry*, keyboard::key_key*> kallocated;
	};
	instance_map<keyboard_state> keyboard_states;
}

std::string map_keycode_to_key(int kcode)
{
	key_entry* k = keys;
	while(k->name) {
		if(k->keynum == kcode)
			return k->name;
		k++;
	}
	std::cerr << "map_keycode_to_key: Unknown key " << kcode << std::endl;
	return "";
}

void handle_wx_keyboard(emulator_instance& inst, wxKeyEvent& e, bool polarity)
{
	CHECK_UI_THREAD;
	auto s = keyboard_states.lookup(inst);
	if(!s) return;
	int mods = e.GetModifiers();
	int keyc = e.GetKeyCode();
	if(polarity) {
		if(keyc == WXK_ESCAPE)
			wx_escape_count++;
		else
			wx_escape_count = 0;
	}
	keyboard::modifier_set mset;
	for(auto m : s->mallocated)  {
		if((mods & m.first->mod) == m.first->mod) {
			mset.add(*m.second);
		}
	}
	if(polarity) {
		if(s->keys_held.count(keyc)) {
			e.Skip();
			return;
		}
		s->keys_held.insert(keyc);
	} else
		s->keys_held.erase(keyc);
	keyboard::key_key* grp = NULL;
	for(auto k : s->kallocated) {
		if(k.first->keynum == keyc) {
			grp = k.second;
			break;
		}
	}
	if(grp)
		UI_do_keypress(inst, mset, *grp, polarity);
	e.Skip();
}

void initialize_wx_keyboard(emulator_instance& inst)
{
	if(keyboard_states.exists(inst))
		return;
	auto s = keyboard_states.create(inst);
	modifier_entry* m = modifiers;
	while(m->name) {
		if(m->lname)
			s->mallocated[m] = new keyboard::modifier(*inst.keyboard, m->name, m->lname);
		else
			s->mallocated[m] = new keyboard::modifier(*inst.keyboard, m->name);
		s->modifier_map[m->mod] = s->mallocated[m];
		m++;
	}
	key_entry* k = keys;
	while(k->name) {
		if(!s->keys_allocated.count(k->name)) {
			s->kallocated[k] = new keyboard::key_key(*inst.keyboard, k->name, k->clazz);
			s->key_map[k->keynum] = s->kallocated[k];
			s->keys_allocated[k->name] = k->keynum;
		} else
			s->key_map[k->keynum] = s->key_map[s->keys_allocated[k->name]];
		k++;
	}
}

void deinitialize_wx_keyboard(emulator_instance& inst)
{
	keyboard_states.destroy(inst);
}
