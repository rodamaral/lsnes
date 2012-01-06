#include "core/keymapper.hpp"
#include "core/window.hpp"

#include <cstdint>
#include <map>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

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

	struct keypress_request
	{
		modifier_set mods;
		keygroup* key;
		bool polarity;
	};

	//Request keypress event to happen.
	void do_keypress(modifier_set mods, keygroup& key, bool polarity)
	{
		struct keypress_request* req = new keypress_request;
		req->mods = mods;
		req->key = &key;
		req->polarity = polarity;
		platform::queue([](void* args) -> void {
			struct keypress_request* x = reinterpret_cast<struct keypress_request*>(args);
			x->key->set_position(x->polarity ? 1 : 0, x->mods);
			delete x;
			}, req, false);
	}
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

void initialize_wx_keyboard()
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
