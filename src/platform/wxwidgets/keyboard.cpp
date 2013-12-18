#include "library/keyboard.hpp"
#include "core/keymapper.hpp"
#include "core/window.hpp"

#include <cstdint>
#include <map>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

int wx_escape_count = 0;

namespace
{
	//Modifier table.
	struct modifier_entry
	{
		int mod;
		const char* name;
		const char* lname;
		keyboard::modifier* allocated;
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
		const char* clazz;
		keyboard::key_key* allocated;
	} keys[] = {
		{ WXK_BACK, "back", "editing", NULL },
		{ WXK_TAB, "tab", "editing", NULL },
		{ WXK_RETURN, "return", "editing", NULL },
		{ WXK_ESCAPE, "escape", "editing", NULL },
		{ WXK_SPACE, "space", "characters", NULL },
		{ 33, "exclaim", "characters", NULL },
		{ 34, "quotedbl", "characters", NULL },
		{ 35, "hash", "characters", NULL },
		{ 36, "dollar", "characters", NULL },
		{ 37, "percent", "characters", NULL },
		{ 38, "ampersand", "characters", NULL },
		{ 39, "quote", "characters", NULL },
		{ 40, "leftparen", "characters", NULL },
		{ 41, "rightparen", "characters", NULL },
		{ 42, "asterisk", "characters", NULL },
		{ 43, "plus", "characters", NULL },
		{ 44, "comma", "characters", NULL },
		{ 45, "minus", "characters", NULL },
		{ 46, "period", "characters", NULL },
		{ 47, "slash", "characters", NULL },
		{ 48, "0", "numeric", NULL },
		{ 49, "1", "numeric", NULL },
		{ 50, "2", "numeric", NULL },
		{ 51, "3", "numeric", NULL },
		{ 52, "4", "numeric", NULL },
		{ 53, "5", "numeric", NULL },
		{ 54, "6", "numeric", NULL },
		{ 55, "7", "numeric", NULL },
		{ 56, "8", "numeric", NULL },
		{ 57, "9", "numeric", NULL },
		{ 58, "colon", "characters", NULL },
		{ 59, "semicolon", "characters", NULL },
		{ 60, "less", "characters", NULL },
		{ 61, "equals", "characters", NULL },
		{ 62, "greater", "characters", NULL },
		{ 63, "question", "characters", NULL },
		{ 64, "at", "characters", NULL },
		{ 65, "a", "alphabetic", NULL },
		{ 66, "b", "alphabetic", NULL },
		{ 67, "c", "alphabetic", NULL },
		{ 68, "d", "alphabetic", NULL },
		{ 69, "e", "alphabetic", NULL },
		{ 70, "f", "alphabetic", NULL },
		{ 71, "g", "alphabetic", NULL },
		{ 72, "h", "alphabetic", NULL },
		{ 73, "i", "alphabetic", NULL },
		{ 74, "j", "alphabetic", NULL },
		{ 75, "k", "alphabetic", NULL },
		{ 76, "l", "alphabetic", NULL },
		{ 77, "m", "alphabetic", NULL },
		{ 78, "n", "alphabetic", NULL },
		{ 79, "o", "alphabetic", NULL },
		{ 80, "p", "alphabetic", NULL },
		{ 81, "q", "alphabetic", NULL },
		{ 82, "r", "alphabetic", NULL },
		{ 83, "s", "alphabetic", NULL },
		{ 84, "t", "alphabetic", NULL },
		{ 85, "u", "alphabetic", NULL },
		{ 86, "v", "alphabetic", NULL },
		{ 87, "w", "alphabetic", NULL },
		{ 88, "x", "alphabetic", NULL },
		{ 89, "y", "alphabetic", NULL },
		{ 90, "z", "alphabetic", NULL },
		{ 91, "leftbracket", "characters", NULL },
		{ 92, "backslash", "characters", NULL },
		{ 93, "rightbracket", "characters", NULL },
		{ 94, "caret", "characters", NULL },
		{ 95, "underscore", "characters", NULL },
		{ 96, "backquote", "characters", NULL },
		{ 97, "a", "alphabetic", NULL },
		{ 98, "b", "alphabetic", NULL },
		{ 99, "c", "alphabetic", NULL },
		{ 100, "d", "alphabetic", NULL },
		{ 101, "e", "alphabetic", NULL },
		{ 102, "f", "alphabetic", NULL },
		{ 103, "g", "alphabetic", NULL },
		{ 104, "h", "alphabetic", NULL },
		{ 105, "i", "alphabetic", NULL },
		{ 106, "j", "alphabetic", NULL },
		{ 107, "k", "alphabetic", NULL },
		{ 108, "l", "alphabetic", NULL },
		{ 109, "m", "alphabetic", NULL },
		{ 110, "n", "alphabetic", NULL },
		{ 111, "o", "alphabetic", NULL },
		{ 112, "p", "alphabetic", NULL },
		{ 113, "q", "alphabetic", NULL },
		{ 114, "r", "alphabetic", NULL },
		{ 115, "s", "alphabetic", NULL },
		{ 116, "t", "alphabetic", NULL },
		{ 117, "u", "alphabetic", NULL },
		{ 118, "v", "alphabetic", NULL },
		{ 119, "w", "alphabetic", NULL },
		{ 120, "x", "alphabetic", NULL },
		{ 121, "y", "alphabetic", NULL },
		{ 122, "z", "alphabetic", NULL },
		{ 123, "leftcurly", "characters", NULL },
		{ 124, "pipe", "characters", NULL },
		{ 125, "rightcurly", "characters", NULL },
		{ 126, "tilde", "characters", NULL },
		{ WXK_DELETE, "delete", "editing", NULL },
		{ WXK_START, "start", "special", NULL },
		{ WXK_LBUTTON, "lbutton", "special", NULL },
		{ WXK_RBUTTON, "rbutton", "special", NULL },
		{ WXK_CANCEL, "cancel", "special", NULL },
		{ WXK_MBUTTON, "mbutton", "special", NULL },
		{ WXK_CLEAR, "clear", "editing", NULL },
		{ WXK_SHIFT, "shift", "modifiers", NULL },
		{ WXK_ALT, "alt", "modifiers", NULL },
		{ WXK_CONTROL, "control", "modifiers", NULL },
		{ WXK_MENU, "menu", "special", NULL },
		{ WXK_PAUSE, "pause", "special", NULL },
		{ WXK_CAPITAL, "capital", "locks", NULL },
		{ WXK_END, "end", "editing", NULL },
		{ WXK_HOME, "home", "editing", NULL },
		{ WXK_LEFT, "lefT", "editing", NULL },
		{ WXK_UP, "up", "editing", NULL },
		{ WXK_RIGHT, "right", "editing", NULL },
		{ WXK_DOWN, "down", "editing", NULL },
		{ WXK_SELECT, "select", "special", NULL },
		{ WXK_PRINT, "print", "special", NULL },
		{ WXK_EXECUTE, "execute", "special", NULL },
		{ WXK_SNAPSHOT, "snapshot", "special", NULL },
		{ WXK_INSERT, "insert", "editing", NULL },
		{ WXK_HELP, "help", "special", NULL },
		{ WXK_NUMPAD0, "numpad0", "numeric", NULL },
		{ WXK_NUMPAD1, "numpad1", "numeric", NULL },
		{ WXK_NUMPAD2, "numpad2", "numeric", NULL },
		{ WXK_NUMPAD3, "numpad3", "numeric", NULL },
		{ WXK_NUMPAD4, "numpad4", "numeric", NULL },
		{ WXK_NUMPAD5, "numpad5", "numeric", NULL },
		{ WXK_NUMPAD6, "numpad6", "numeric", NULL },
		{ WXK_NUMPAD7, "numpad7", "numeric", NULL },
		{ WXK_NUMPAD8, "numpad8", "numeric", NULL },
		{ WXK_NUMPAD9, "numpad9", "numeric", NULL },
		{ WXK_MULTIPLY, "multiply", "characters", NULL },
		{ WXK_ADD, "add", "characters", NULL },
		{ WXK_SEPARATOR, "separator", "characters", NULL },
		{ WXK_SUBTRACT, "subtract", "characters", NULL },
		{ WXK_DECIMAL, "decimal", "characters", NULL },
		{ WXK_DIVIDE, "divide", "characters", NULL },
		{ WXK_F1, "f1", "F-keys", NULL },
		{ WXK_F2, "f2", "F-keys", NULL },
		{ WXK_F3, "f3", "F-keys", NULL },
		{ WXK_F4, "f4", "F-keys", NULL },
		{ WXK_F5, "f5", "F-keys", NULL },
		{ WXK_F6, "f6", "F-keys", NULL },
		{ WXK_F7, "f7", "F-keys", NULL },
		{ WXK_F8, "f8", "F-keys", NULL },
		{ WXK_F9, "f9", "F-keys", NULL },
		{ WXK_F10, "f10", "F-keys", NULL },
		{ WXK_F11, "f11", "F-keys", NULL },
		{ WXK_F12, "f12", "F-keys", NULL },
		{ WXK_F13, "f13", "F-keys", NULL },
		{ WXK_F14, "f14", "F-keys", NULL },
		{ WXK_F15, "f15", "F-keys", NULL },
		{ WXK_F16, "f16", "F-keys", NULL },
		{ WXK_F17, "f17", "F-keys", NULL },
		{ WXK_F18, "f18", "F-keys", NULL },
		{ WXK_F19, "f19", "F-keys", NULL },
		{ WXK_F20, "f20", "F-keys", NULL },
		{ WXK_F21, "f21", "F-keys", NULL },
		{ WXK_F22, "f22", "F-keys", NULL },
		{ WXK_F23, "f23", "F-keys", NULL },
		{ WXK_F24, "f24", "F-keys", NULL },
		{ WXK_NUMLOCK, "numlock", "locks", NULL },
		{ WXK_SCROLL, "scroll", "locks", NULL },
		{ WXK_PAGEUP, "pageup", "editing", NULL },
		{ WXK_PAGEDOWN, "pagedown", "editing", NULL },
		{ WXK_NUMPAD_SPACE, "numpad_space", "editing", NULL },
		{ WXK_NUMPAD_TAB, "numpad_tab", "editing", NULL },
		{ WXK_NUMPAD_ENTER, "numpad_enter", "editing", NULL },
		{ WXK_NUMPAD_F1, "numpad_f1", "F-keys", NULL },
		{ WXK_NUMPAD_F2, "numpad_f2", "F-keys", NULL },
		{ WXK_NUMPAD_F3, "numpad_f3", "F-keys", NULL },
		{ WXK_NUMPAD_F4, "numpad_f4", "F-keys", NULL },
		{ WXK_NUMPAD_HOME, "numpad_home", "editing", NULL },
		{ WXK_NUMPAD_LEFT, "numpad_left", "editing", NULL },
		{ WXK_NUMPAD_UP, "numpad_up", "editing", NULL },
		{ WXK_NUMPAD_RIGHT, "numpad_right", "editing", NULL },
		{ WXK_NUMPAD_DOWN, "numpad_down", "editing", NULL },
		{ WXK_NUMPAD_PAGEUP, "numpad_pageup", "editing", NULL },
		{ WXK_NUMPAD_PAGEDOWN, "numpad_pagedown", "editing", NULL },
		{ WXK_NUMPAD_END, "numpad_end", "editing", NULL },
		{ WXK_NUMPAD_BEGIN, "numpad_begin", "editing", NULL },
		{ WXK_NUMPAD_INSERT, "numpad_insert", "editing", NULL },
		{ WXK_NUMPAD_DELETE, "numpad_delete", "editing", NULL },
		{ WXK_NUMPAD_EQUAL, "numpad_equal", "characters", NULL },
		{ WXK_NUMPAD_MULTIPLY, "numpad_multiply", "characters", NULL },
		{ WXK_NUMPAD_ADD, "numpad_add", "characters", NULL },
		{ WXK_NUMPAD_SEPARATOR, "numpad_separator", "characters", NULL },
		{ WXK_NUMPAD_SUBTRACT, "numpad_subtract", "characters", NULL },
		{ WXK_NUMPAD_DECIMAL, "numpad_decimal", "characters", NULL },
		{ WXK_NUMPAD_DIVIDE, "numpad_divide", "characters", NULL },
		{ WXK_WINDOWS_LEFT, "windows_left", "modifiers", NULL },
		{ WXK_WINDOWS_RIGHT, "windows_right", "modifiers", NULL },
		{ WXK_WINDOWS_MENU, "windows_menu", "modifiers", NULL },
		{ WXK_COMMAND, "command", "special", NULL },
		{ WXK_SPECIAL1, "special1", "special", NULL },
		{ WXK_SPECIAL2, "special2", "special", NULL },
		{ WXK_SPECIAL3, "special3", "special", NULL },
		{ WXK_SPECIAL4, "special4", "special", NULL },
		{ WXK_SPECIAL5, "special5", "special", NULL },
		{ WXK_SPECIAL6, "special6", "special", NULL },
		{ WXK_SPECIAL7, "special7", "special", NULL },
		{ WXK_SPECIAL8, "special8", "special", NULL },
		{ WXK_SPECIAL9, "special9", "special", NULL },
		{ WXK_SPECIAL10, "special10", "special", NULL },
		{ WXK_SPECIAL11, "special11", "special", NULL },
		{ WXK_SPECIAL12, "special12", "special", NULL },
		{ WXK_SPECIAL13, "special13", "special", NULL },
		{ WXK_SPECIAL14, "special14", "special", NULL },
		{ WXK_SPECIAL15, "special15", "special", NULL },
		{ WXK_SPECIAL16, "special16", "special", NULL },
		{ WXK_SPECIAL17, "special17", "special", NULL },
		{ WXK_SPECIAL18, "special18", "special", NULL },
		{ WXK_SPECIAL19, "special19", "special", NULL },
		{ WXK_SPECIAL20, "special20", "special", NULL },
		{ 0, NULL, NULL, NULL }
	};

	std::map<int, keyboard::modifier*> modifier_map;
	std::map<int, keyboard::key_key*> key_map;
	std::map<std::string, int> keys_allocated;
	std::set<int> keys_held;

	struct keypress_request
	{
		keyboard::modifier_set mods;
		keyboard::key_key* key;
		bool polarity;
	};

	//Request keypress event to happen.
	void do_keypress(keyboard::modifier_set mods, keyboard::key_key& key, bool polarity)
	{
		struct keypress_request* req = new keypress_request;
		req->mods = mods;
		req->key = &key;
		req->polarity = polarity;
		platform::queue([](void* args) -> void {
			struct keypress_request* x = reinterpret_cast<struct keypress_request*>(args);
			x->key->set_state(x->mods, x->polarity ? 1 : 0);
			delete x;
			}, req, false);
	}
}

std::string map_keycode_to_key(int kcode)
{
	key_entry* k = keys;
	while(k->name) {
		if(k->keynum == kcode)
			return k->name;
		k++;
	}
	return "";
}

void handle_wx_keyboard(wxKeyEvent& e, bool polarity)
{
	int mods = e.GetModifiers();
	int keyc = e.GetKeyCode();
	if(polarity) {
		if(keyc == WXK_ESCAPE)
			wx_escape_count++;
		else
			wx_escape_count = 0;
	}
	keyboard::modifier_set mset;
	modifier_entry* m = modifiers;
	while(m->name) {
		if((mods & m->mod) == m->mod) {
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
	keyboard::key_key* grp = NULL;
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

void initialize_wx_keyboard()
{
	static bool done = false;
	if(done)
		return;
	modifier_entry* m = modifiers;
	while(m->name) {
		if(m->lname)
			m->allocated = new keyboard::modifier(lsnes_kbd, m->name, m->lname);
		else
			m->allocated = new keyboard::modifier(lsnes_kbd, m->name);
		modifier_map[m->mod] = m->allocated;
		m++;
	}
	key_entry* k = keys;
	while(k->name) {
		if(!keys_allocated.count(k->name)) {
			k->allocated = new keyboard::key_key(lsnes_kbd, k->name, k->clazz);
			key_map[k->keynum] = k->allocated;
			keys_allocated[k->name] = k->keynum;
		} else
			key_map[k->keynum] = key_map[keys_allocated[k->name]];
		k++;
	}
	done = true;
}
