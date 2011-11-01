#include "window.hpp"
#include "render.hpp"
#include "command.hpp"
#include "framerate.hpp"
#include "misc.hpp"
#include "lsnes.hpp"
#include "settings.hpp"
#include <vector>
#include <iostream>
#include <csignal>
#include "keymapper.hpp"
#include "framerate.hpp"
#include <sstream>
#include <fstream>
#include <cassert>

#define MAXHISTORY 1000
#define MAXMESSAGES 6
#define WATCHDOG_TIMEOUT 15

#include <SDL.h>
#include <string>
#include <map>
#include <stdexcept>

// Limit the emulator to ~30fps.
#define MIN_UPDATE_TIME 33333

namespace
{
	bool wait_canceled;
	SDL_TimerID tid;

	void sigalrm_handler(int s)
	{
		_exit(1);
	}

	Uint32 timer_cb(Uint32 interval, void* param)
	{
		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = 0;
		SDL_PushEvent(&e);
		return interval;
	}

	struct sdl_modifier
	{
		const char* name;
		const char* linkname;
		unsigned sdlvalue;
	} modifiers_table[] = {
		{ "ctrl",	NULL,		0		},
		{ "lctrl",	"ctrl",		KMOD_LCTRL	},
		{ "rctrl",	"ctrl",		KMOD_RCTRL	},
		{ "alt",	NULL,		0		},
		{ "lalt",	"alt",		KMOD_LALT	},
		{ "ralt",	"alt",		KMOD_RALT	},
		{ "shift",	NULL,		0		},
		{ "lshift",	"shift",	KMOD_LSHIFT	},
		{ "rshift",	"shift",	KMOD_RSHIFT	},
		{ "meta",	NULL,		0		},
		{ "lmeta",	"meta",		KMOD_LMETA	},
		{ "rmeta",	"meta",		KMOD_RMETA	},
		{ "num",	NULL,		KMOD_NUM	},
		{ "caps",	NULL,		KMOD_CAPS	},
		{ "mode",	NULL,		KMOD_MODE	},
		{ NULL,		NULL,		0		}
	};

	struct sdl_key
	{
		const char* name;
		unsigned symbol;
	} keys_table[] = {
		{"backspace",		SDLK_BACKSPACE		},
		{"tab",			SDLK_TAB		},
		{"clear",		SDLK_CLEAR		},
		{"return",		SDLK_RETURN		},
		{"pause",		SDLK_PAUSE		},
		{"escape",		SDLK_ESCAPE		},
		{"space",		SDLK_SPACE		},
		{"exclaim",		SDLK_EXCLAIM		},
		{"quotedbl",		SDLK_QUOTEDBL		},
		{"hash",		SDLK_HASH		},
		{"dollar",		SDLK_DOLLAR		},
		{"ampersand",		SDLK_AMPERSAND		},
		{"quote",		SDLK_QUOTE		},
		{"leftparen",		SDLK_LEFTPAREN		},
		{"rightparen",		SDLK_RIGHTPAREN		},
		{"asterisk",		SDLK_ASTERISK		},
		{"plus",		SDLK_PLUS		},
		{"comma",		SDLK_COMMA		},
		{"minus",		SDLK_MINUS		},
		{"period",		SDLK_PERIOD		},
		{"slash",		SDLK_SLASH		},
		{"0",			SDLK_0			},
		{"1",			SDLK_1			},
		{"2",			SDLK_2			},
		{"3",			SDLK_3			},
		{"4",			SDLK_4			},
		{"5",			SDLK_5			},
		{"6",			SDLK_6			},
		{"7",			SDLK_7			},
		{"8",			SDLK_8			},
		{"9",			SDLK_9			},
		{"colon",		SDLK_COLON		},
		{"semicolon",		SDLK_SEMICOLON		},
		{"less",		SDLK_LESS		},
		{"equals",		SDLK_EQUALS		},
		{"greater",		SDLK_GREATER		},
		{"question",		SDLK_QUESTION		},
		{"at",			SDLK_AT			},
		{"leftbracket",		SDLK_LEFTBRACKET	},
		{"backslash",		SDLK_BACKSLASH		},
		{"rightbracket",	SDLK_RIGHTBRACKET	},
		{"caret",		SDLK_CARET		},
		{"underscore",		SDLK_UNDERSCORE		},
		{"backquote",		SDLK_BACKQUOTE		},
		{"a",			SDLK_a			},
		{"b",			SDLK_b			},
		{"c",			SDLK_c			},
		{"d",			SDLK_d			},
		{"e",			SDLK_e			},
		{"f",			SDLK_f			},
		{"g",			SDLK_g			},
		{"h",			SDLK_h			},
		{"i",			SDLK_i			},
		{"j",			SDLK_j			},
		{"k",			SDLK_k			},
		{"l",			SDLK_l			},
		{"m",			SDLK_m			},
		{"n",			SDLK_n			},
		{"o",			SDLK_o			},
		{"p",			SDLK_p			},
		{"q",			SDLK_q			},
		{"r",			SDLK_r			},
		{"s",			SDLK_s			},
		{"t",			SDLK_t			},
		{"u",			SDLK_u			},
		{"v",			SDLK_v			},
		{"w",			SDLK_w			},
		{"x",			SDLK_x			},
		{"y",			SDLK_y			},
		{"z",			SDLK_z			},
		{"delete",		SDLK_DELETE		},
		{"world_0",		SDLK_WORLD_0		},
		{"world_1",		SDLK_WORLD_1		},
		{"world_2",		SDLK_WORLD_2		},
		{"world_3",		SDLK_WORLD_3		},
		{"world_4",		SDLK_WORLD_4		},
		{"world_5",		SDLK_WORLD_5		},
		{"world_6",		SDLK_WORLD_6		},
		{"world_7",		SDLK_WORLD_7		},
		{"world_8",		SDLK_WORLD_8		},
		{"world_9",		SDLK_WORLD_9		},
		{"world_10",		SDLK_WORLD_10		},
		{"world_11",		SDLK_WORLD_11		},
		{"world_12",		SDLK_WORLD_12		},
		{"world_13",		SDLK_WORLD_13		},
		{"world_14",		SDLK_WORLD_14		},
		{"world_15",		SDLK_WORLD_15		},
		{"world_16",		SDLK_WORLD_16		},
		{"world_17",		SDLK_WORLD_17		},
		{"world_18",		SDLK_WORLD_18		},
		{"world_19",		SDLK_WORLD_19		},
		{"world_20",		SDLK_WORLD_20		},
		{"world_21",		SDLK_WORLD_21		},
		{"world_22",		SDLK_WORLD_22		},
		{"world_23",		SDLK_WORLD_23		},
		{"world_24",		SDLK_WORLD_24		},
		{"world_25",		SDLK_WORLD_25		},
		{"world_26",		SDLK_WORLD_26		},
		{"world_27",		SDLK_WORLD_27		},
		{"world_28",		SDLK_WORLD_28		},
		{"world_29",		SDLK_WORLD_29		},
		{"world_30",		SDLK_WORLD_30		},
		{"world_31",		SDLK_WORLD_31		},
		{"world_32",		SDLK_WORLD_32		},
		{"world_33",		SDLK_WORLD_33		},
		{"world_34",		SDLK_WORLD_34		},
		{"world_35",		SDLK_WORLD_35		},
		{"world_36",		SDLK_WORLD_36		},
		{"world_37",		SDLK_WORLD_37		},
		{"world_38",		SDLK_WORLD_38		},
		{"world_39",		SDLK_WORLD_39		},
		{"world_40",		SDLK_WORLD_40		},
		{"world_41",		SDLK_WORLD_41		},
		{"world_42",		SDLK_WORLD_42		},
		{"world_43",		SDLK_WORLD_43		},
		{"world_44",		SDLK_WORLD_44		},
		{"world_45",		SDLK_WORLD_45		},
		{"world_46",		SDLK_WORLD_46		},
		{"world_47",		SDLK_WORLD_47		},
		{"world_48",		SDLK_WORLD_48		},
		{"world_49",		SDLK_WORLD_49		},
		{"world_50",		SDLK_WORLD_50		},
		{"world_51",		SDLK_WORLD_51		},
		{"world_52",		SDLK_WORLD_52		},
		{"world_53",		SDLK_WORLD_53		},
		{"world_54",		SDLK_WORLD_54		},
		{"world_55",		SDLK_WORLD_55		},
		{"world_56",		SDLK_WORLD_56		},
		{"world_57",		SDLK_WORLD_57		},
		{"world_58",		SDLK_WORLD_58		},
		{"world_59",		SDLK_WORLD_59		},
		{"world_60",		SDLK_WORLD_60		},
		{"world_61",		SDLK_WORLD_61		},
		{"world_62",		SDLK_WORLD_62		},
		{"world_63",		SDLK_WORLD_63		},
		{"world_64",		SDLK_WORLD_64		},
		{"world_65",		SDLK_WORLD_65		},
		{"world_66",		SDLK_WORLD_66		},
		{"world_67",		SDLK_WORLD_67		},
		{"world_68",		SDLK_WORLD_68		},
		{"world_69",		SDLK_WORLD_69		},
		{"world_70",		SDLK_WORLD_70		},
		{"world_71",		SDLK_WORLD_71		},
		{"world_72",		SDLK_WORLD_72		},
		{"world_73",		SDLK_WORLD_73		},
		{"world_74",		SDLK_WORLD_74		},
		{"world_75",		SDLK_WORLD_75		},
		{"world_76",		SDLK_WORLD_76		},
		{"world_77",		SDLK_WORLD_77		},
		{"world_78",		SDLK_WORLD_78		},
		{"world_79",		SDLK_WORLD_79		},
		{"world_80",		SDLK_WORLD_80		},
		{"world_81",		SDLK_WORLD_81		},
		{"world_82",		SDLK_WORLD_82		},
		{"world_83",		SDLK_WORLD_83		},
		{"world_84",		SDLK_WORLD_84		},
		{"world_85",		SDLK_WORLD_85		},
		{"world_86",		SDLK_WORLD_86		},
		{"world_87",		SDLK_WORLD_87		},
		{"world_88",		SDLK_WORLD_88		},
		{"world_89",		SDLK_WORLD_89		},
		{"world_90",		SDLK_WORLD_90		},
		{"world_91",		SDLK_WORLD_91		},
		{"world_92",		SDLK_WORLD_92		},
		{"world_93",		SDLK_WORLD_93		},
		{"world_94",		SDLK_WORLD_94		},
		{"world_95",		SDLK_WORLD_95		},
		{"kp0",			SDLK_KP0		},
		{"kp1",			SDLK_KP1		},
		{"kp2",			SDLK_KP2		},
		{"kp3",			SDLK_KP3		},
		{"kp4",			SDLK_KP4		},
		{"kp5",			SDLK_KP5		},
		{"kp6",			SDLK_KP6		},
		{"kp7",			SDLK_KP7		},
		{"kp8",			SDLK_KP8		},
		{"kp9",			SDLK_KP9		},
		{"kp_period",		SDLK_KP_PERIOD		},
		{"kp_divide",		SDLK_KP_DIVIDE		},
		{"kp_multiply",		SDLK_KP_MULTIPLY	},
		{"kp_minus",		SDLK_KP_MINUS		},
		{"kp_plus",		SDLK_KP_PLUS		},
		{"kp_enter",		SDLK_KP_ENTER		},
		{"kp_equals",		SDLK_KP_EQUALS		},
		{"up",			SDLK_UP			},
		{"down",		SDLK_DOWN		},
		{"right",		SDLK_RIGHT		},
		{"left",		SDLK_LEFT		},
		{"insert",		SDLK_INSERT		},
		{"home",		SDLK_HOME		},
		{"end",			SDLK_END		},
		{"pageup",		SDLK_PAGEUP		},
		{"pagedown",		SDLK_PAGEDOWN		},
		{"f1",			SDLK_F1			},
		{"f2",			SDLK_F2			},
		{"f3",			SDLK_F3			},
		{"f4",			SDLK_F4			},
		{"f5",			SDLK_F5			},
		{"f6",			SDLK_F6			},
		{"f7",			SDLK_F7			},
		{"f8",			SDLK_F8			},
		{"f9",			SDLK_F9			},
		{"f10",			SDLK_F10		},
		{"f11",			SDLK_F11		},
		{"f12",			SDLK_F12		},
		{"f13",			SDLK_F13		},
		{"f14",			SDLK_F14		},
		{"f15",			SDLK_F15		},
		{"numlock",		SDLK_NUMLOCK		},
		{"capslock",		SDLK_CAPSLOCK		},
		{"scrollock",		SDLK_SCROLLOCK		},
		{"rshift",		SDLK_RSHIFT		},
		{"lshift",		SDLK_LSHIFT		},
		{"rctrl",		SDLK_RCTRL		},
		{"lctrl",		SDLK_LCTRL		},
		{"ralt",		SDLK_RALT		},
		{"lalt",		SDLK_LALT		},
		{"rmeta",		SDLK_RMETA		},
		{"lmeta",		SDLK_LMETA		},
		{"lsuper",		SDLK_LSUPER		},
		{"rsuper",		SDLK_RSUPER		},
		{"mode",		SDLK_MODE		},
		{"compose",		SDLK_COMPOSE		},
		{"help",		SDLK_HELP		},
		{"print",		SDLK_PRINT		},
		{"sysreq",		SDLK_SYSREQ		},
		{"break",		SDLK_BREAK		},
		{"menu",		SDLK_MENU		},
		{"power",		SDLK_POWER		},
		{"euro",		SDLK_EURO		},
		{"undo",		SDLK_UNDO		},
		{NULL,			0			}
	};

	std::map<unsigned, modifier*> supported_modifiers;
	std::map<unsigned, keygroup*> scancodekeys;
	std::map<unsigned, keygroup*> symbolkeys;
#ifndef SDL_NO_JOYSTICK
	std::map<unsigned, keygroup*> joyaxis;
	std::map<unsigned, keygroup*> joybutton;
	std::map<unsigned, keygroup*> joyhat;
#endif

	void init_keys()
	{
		struct sdl_modifier* m = modifiers_table;
		while(m->name) {
			modifier* m2;
			if(m->linkname)
				m2 = new modifier(m->name, m->linkname);
			else
				m2 = new modifier(m->name);
			if(m->sdlvalue)
				supported_modifiers[m->sdlvalue] = m2;
			m++;
		}
		struct sdl_key* k = keys_table;
		while(k->name) {
			symbolkeys[k->symbol] = new keygroup(k->name, keygroup::KT_KEY);
			k++;
		}
		for(unsigned i = 0; i < 256; i++) {
			std::ostringstream x;
			x << "key" << i;
			scancodekeys[i] = new keygroup(x.str(), keygroup::KT_KEY);
		}
	}

	void init_joysticks()
	{
#ifndef SDL_NO_JOYSTICK
		int joysticks = SDL_NumJoysticks();
		if(!joysticks) {
			window::out() << "No joysticks detected." << std::endl;
		} else {
			window::out() << joysticks << " joystick(s) detected." << std::endl;
			for(int i = 0; i < joysticks; i++) {
				SDL_Joystick* j = SDL_JoystickOpen(i);
				if(!j) {
					window::out() << "Joystick #" << i << ": Can't open!" << std::endl;
					continue;
				}
				window::out() << "Joystick #" << i << ": " << SDL_JoystickName(i) << "("
					<< SDL_JoystickNumAxes(j) << " axes, " << SDL_JoystickNumButtons(j)
					<< " buttons, " << SDL_JoystickNumHats(j) << " hats)." << std::endl;
				for(int k = 0; k < SDL_JoystickNumAxes(j); k++) {
					unsigned num = 256 * i + k;
					std::ostringstream x;
					x << "joystick" << i << "axis" << k;
					joyaxis[num] = new keygroup(x.str(), keygroup::KT_AXIS_PAIR);
				}
				for(int k = 0; k < SDL_JoystickNumButtons(j); k++) {
					unsigned num = 256 * i + k;
					std::ostringstream x;
					x << "joystick" << i << "button" << k;
					joybutton[num] = new keygroup(x.str(), keygroup::KT_KEY);
				}
				for(int k = 0; k < SDL_JoystickNumHats(j); k++) {
					unsigned num = 256 * i + k;
					std::ostringstream x;
					x << "joystick" << i << "hat" << k;
					joyhat[num] = new keygroup(x.str(), keygroup::KT_HAT);
				}
			}
		}
#endif
	}

	struct identify_helper : public keygroup::key_listener
	{
		void key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned subkey,
			bool polarity, const std::string& name)
		{
			if(!polarity)
				_keys = _keys + "Name: " + name + "\n";
		}
		bool got_it()
		{
			return (_keys != "");
		}
		std::string keys()
		{
			return _keys;
		}
		std::string _keys;
	};

	struct key_eater : public keygroup::key_listener
	{
		void key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned subkey,
			bool polarity, const std::string& name)
		{
			//Just eat it.
		}
	} keyeater;

	void process_input_event(SDL_Event* e)
	{
		modifier_set modifiers;
		if(e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
			SDL_keysym sym = e->key.keysym;
			uint8_t scancode = sym.scancode;
			unsigned symbol = sym.sym;
			for(auto k : supported_modifiers)
				if(sym.mod & k.first)
					modifiers.add(*k.second);
			scancodekeys[scancode]->set_position((e->type == SDL_KEYDOWN) ? 1 : 0, modifiers);
			if(symbolkeys.count(symbol))
				symbolkeys[symbol]->set_position((e->type == SDL_KEYDOWN) ? 1 : 0, modifiers);
#ifndef SDL_NO_JOYSTICK
		} else if(e->type == SDL_JOYAXISMOTION) {
			unsigned num = static_cast<unsigned>(e->jaxis.which) * 256 +
				static_cast<unsigned>(e->jaxis.axis);
			if(joyaxis.count(num))
				joyaxis[num]->set_position(e->jaxis.value, modifiers);
		} else if(e->type == SDL_JOYHATMOTION) {
			unsigned num = static_cast<unsigned>(e->jhat.which) * 256 +
				static_cast<unsigned>(e->jhat.hat);
			short v = 0;
			if(e->jhat.value & SDL_HAT_UP)
				v |= 1;
			if(e->jhat.value & SDL_HAT_RIGHT)
				v |= 2;
			if(e->jhat.value & SDL_HAT_DOWN)
				v |= 4;
			if(e->jhat.value & SDL_HAT_LEFT)
				v |= 8;
			if(joyhat.count(num))
				joyhat[num]->set_position(v, modifiers);
		} else if(e->type == SDL_JOYBUTTONDOWN || e->type == SDL_JOYBUTTONUP) {
			unsigned num = static_cast<unsigned>(e->jbutton.which) * 256 +
				static_cast<unsigned>(e->jbutton.button);
			if(joybutton.count(num))
				joybutton[num]->set_position((e->type == SDL_JOYBUTTONDOWN), modifiers);
#endif
		}
	}
}

extern uint32_t fontdata[];

namespace
{
	bool SDL_initialized = false;
	uint32_t mouse_mask = 0;
	bool sdl_init;
	bool modconfirm;
	bool modal_return_flag;
	bool delayed_close_flag;
	std::string modmsg;
	std::string command_buf;
	bool command_overwrite;
	size_t command_cursor;
	unsigned old_screen_w;
	unsigned old_screen_h;
	unsigned state;
	bool console_mode;
	std::list<std::string> commandhistory;
	std::list<std::string>::iterator commandhistory_itr;
	screen* current_screen;
	SDL_Surface* hwsurf;
	std::pair<uint32_t, uint32_t> current_windowsize;
	bool pause_active;
	uint64_t last_ui_update;
	bool screen_is_dirty;
	std::ofstream system_log;
	SDL_keysym autorepeating_key;
	unsigned autorepeat_phase = 0;
	unsigned autorepeat_timecounter = 0;
	numeric_setting autorepeat_first("autorepeat-first-delay", 1, 999999999, 15);
	numeric_setting autorepeat_subsequent("autorepeat-subsequent-delay", 1, 999999999, 4);
};

void poll_inputs_internal() throw(std::bad_alloc);

namespace
{
	void identify()
	{
		state = WINSTATE_IDENTIFY;
		window::message("Press key to identify.");
		window::notify_screen_update();
		poll_inputs_internal();
	}

	std::string decode_string(std::string e)
	{
		std::string x;
		for(size_t i = 0; i < e.length(); i += 4) {
			char tmp[5] = {0};
			uint32_t c1 = e[i] - 33;
			uint32_t c2 = e[i + 1] - 33;
			uint32_t c3 = e[i + 2] - 33;
			uint32_t c4 = e[i + 3] - 33;
			uint32_t c = (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
			if(c < 0x80) {
				tmp[0] = c;
			} else if(c < 0x800) {
				tmp[0] = 0xC0 | (c >> 6);
				tmp[1] = 0x80 | (c & 0x3F);
			} else if(c < 0x10000) {
				tmp[0] = 0xE0 | (c >> 12);
				tmp[1] = 0x80 | ((c >> 6) & 0x3F);
				tmp[2] = 0x80 | (c & 0x3F);
			} else {
				tmp[0] = 0xF0 | (c >> 18);
				tmp[1] = 0x80 | ((c >> 12) & 0x3F);
				tmp[2] = 0x80 | ((c >> 6) & 0x3F);
				tmp[3] = 0x80 | (c & 0x3F);
			}
			x = x + tmp;
		}
		return x;
	}

	void draw_rectangle(uint8_t* data, uint32_t pitch, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
		uint32_t color, uint32_t thickness)
	{
		for(uint32_t i = x1; i < x2; i++)
			for(uint32_t j = 0; j < thickness; j++) {
				reinterpret_cast<uint32_t*>(data + pitch * (y1 + j))[i] = color;
				reinterpret_cast<uint32_t*>(data + pitch * (y2 - 1 - j))[i] = color;
			}
		for(uint32_t i = y1; i < y2; i++)
			for(uint32_t j = 0; j < thickness; j++) {
				reinterpret_cast<uint32_t*>(data + pitch * i)[x1 + j] = color;
				reinterpret_cast<uint32_t*>(data + pitch * i)[x2 - 1 - j] = color;
			}
	}

	std::vector<uint32_t> decode_utf8(std::string s)
	{
		std::vector<uint32_t> ret;
		for(auto i = s.begin(); i != s.end(); i++) {
			uint32_t j = static_cast<uint8_t>(*i);
			if(j < 128)
				ret.push_back(j);
			else if(j < 192)
				continue;
			else if(j < 224) {
				uint32_t j2 = static_cast<uint8_t>(*(++i));
				ret.push_back((j - 192) * 64 + (j2 - 128));
			} else if(j < 240) {
				uint32_t j2 = static_cast<uint8_t>(*(++i));
				uint32_t j3 = static_cast<uint8_t>(*(++i));
				ret.push_back((j - 224) * 4096 + (j2 - 128) * 64 + (j3 - 128));
			} else {
				uint32_t j2 = static_cast<uint8_t>(*(++i));
				uint32_t j3 = static_cast<uint8_t>(*(++i));
				uint32_t j4 = static_cast<uint8_t>(*(++i));
				ret.push_back((j - 240) * 262144 + (j2 - 128) * 4096 + (j3 - 128) * 64 + (j4 - 128));
			}
		}
		return ret;
	}

	void draw_string(uint8_t* base, uint32_t pitch, std::vector<uint32_t> s, uint32_t x, uint32_t y,
		uint32_t maxwidth, uint32_t hilite_mode = 0, uint32_t hilite_pos = 0)
	{
		base += y * static_cast<size_t>(pitch) + 4 * x;
		int32_t pos_x = 0;
		int32_t pos_y = 0;
		unsigned c = 0;
		for(auto si : s) {
			uint32_t old_x = pos_x;
			uint32_t curstart = 16;
			if(c == hilite_pos && hilite_mode == 1)
				curstart = 14;
			if(c == hilite_pos && hilite_mode == 2)
				curstart = 0;
			auto g = find_glyph(si, pos_x, pos_y, 0, pos_x, pos_y);
			if(pos_y)
				pos_x = old_x;
			if(g.second == 0) {
				//Empty glyph.
				for(unsigned j = 0; j < 16; j++) {
					uint32_t* ptr = reinterpret_cast<uint32_t*>(base + pitch * j);
					for(unsigned i = 0; i < g.first && old_x + i < maxwidth; i++)
						ptr[old_x + i] = (j >= curstart) ? 0xFFFFFFFFU : 0;
				}
			} else {
				//Narrow/Wide glyph.
				for(unsigned j = 0; j < 16; j++) {
					uint32_t* ptr = reinterpret_cast<uint32_t*>(base + pitch * j);
					uint32_t dataword = fontdata[g.second + j / 4];
					for(uint32_t i = 0; i < g.first && old_x + i < maxwidth; i++) {
						bool b = (((dataword >> (31 - (j % (32 / g.first)) * g.first - i)) &
							1));
						b ^= (j >= curstart);
						ptr[old_x + i] = b ? 0xFFFFFFFFU : 0;
					}
				}
			}
			c++;
		}
		for(unsigned j = 0; j < 16; j++) {
			uint32_t* ptr = reinterpret_cast<uint32_t*>(base + pitch * j);
			uint32_t curstart = 16;
			if(c == hilite_pos && hilite_mode == 1)
				curstart = 14;
			if(c == hilite_pos && hilite_mode == 2)
				curstart = 0;
			for(uint32_t i = pos_x; i < maxwidth; i++)
				ptr[i] = ((i - pos_x) < 8 && j >= curstart) ? 0xFFFFFFFFU : 0;
		}
	}

	void draw_string(uint8_t* base, uint32_t pitch, std::string s, uint32_t x, uint32_t y, uint32_t maxwidth,
		uint32_t hilite_mode = 0, uint32_t hilite_pos = 0)
	{
		draw_string(base, pitch, decode_utf8(s), x, y, maxwidth, hilite_mode, hilite_pos);
	}

	void draw_command(uint8_t* base, uint32_t pitch, std::string s, size_t cursor, uint32_t x, uint32_t y,
		uint32_t maxwidth, bool overwrite)
	{
		//FIXME, scroll text if too long.
		uint32_t hilite_mode = overwrite ? 2 : 1;
		auto s2 = decode_utf8(s);
		draw_string(base, pitch, s2, x, y, maxwidth, hilite_mode, cursor);
	}

	void draw_modal_dialog(SDL_Surface* surf, std::string msg, bool confirm)
	{
		int32_t pos_x = 0;
		int32_t pos_y = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		if(confirm)
			msg = msg + "\n\nHit Enter to confirm, Esc to cancel";
		else
			msg = msg + "\n\nHit Enter or Esc to dismiss";
		auto s2 = decode_utf8(msg);
		for(auto i : s2) {
			auto g = find_glyph(i, pos_x, pos_y, 0, pos_x, pos_y);
			if(pos_x + g.first > width)
				width = static_cast<uint32_t>(pos_x + g.first);
			if(pos_y + 16 > static_cast<int32_t>(height))
				height = static_cast<uint32_t>(pos_y + 16);
		}
		uint32_t x1;
		uint32_t x2;
		uint32_t y1;
		uint32_t y2;
		if(width + 12 >= static_cast<uint32_t>(surf->w)) {
			x1 = 6;
			x2 = surf->w - 6;
			width = x2 - x1;
		} else {
			x1 = (surf->w - width) / 2;
			x2 = x1 + width;
		}
		if(height + 12 >= static_cast<uint32_t>(surf->h)) {
			y1 = 6;
			y2 = surf->h - 6;
			height = y2 - y1;
		} else {
			y1 = (surf->h - height) / 2;
			y2 = y1 + height;
		}
		for(uint32_t j = y1 - 6; j < y2 + 6; j++)
			memset(reinterpret_cast<uint8_t*>(surf->pixels) + j * surf->pitch + 4 * (x1 - 6), 0,
				4 * (x2 - x1 + 12));
		uint32_t bordercolor = (0xFF << surf->format->Rshift) | (0x80 << surf->format->Gshift);
		draw_rectangle(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, x1 - 4, y1 - 4, x2 + 4, y2 + 4,
			bordercolor, 2);

		pos_x = 0;
		pos_y = 0;
		for(auto i : s2) {
			uint32_t ox = pos_x;
			uint32_t oy = pos_y;
			auto g = find_glyph(i, pos_x, pos_y, 0, pos_x, pos_y);
			if(static_cast<uint32_t>(pos_y) > height)
				break;
			uint8_t* base = reinterpret_cast<uint8_t*>(surf->pixels) + (y1 + oy) * surf->pitch +
				4 * (x1 + ox);
			if(g.second) {
				//Narrow/Wide glyph.
				for(unsigned j = 0; j < 16; j++) {
					uint32_t* ptr = reinterpret_cast<uint32_t*>(base + surf->pitch * j);
					uint32_t dataword = fontdata[g.second + j / 4];
					for(uint32_t i = 0; i < g.first && (ox + i) < width; i++) {
						bool b = (((dataword >> (31 - (j % (32 / g.first)) * g.first - i)) &
							1));
						ptr[i] = b ? bordercolor : 0;
					}
				}
			}
		}
	}

	void do_keyboard_command_edit(SDL_keysym k)
	{
		//These are not command edit!
		if(k.sym == SDLK_ESCAPE)
			return;
		if(k.sym == SDLK_RETURN)
			return;
		if(k.sym == SDLK_KP_ENTER)
			return;
		//Map keys a bit if numlock is off.
		if((k.mod & KMOD_NUM) == 0) {
			switch(k.sym) {
			case SDLK_KP0:		k.sym = SDLK_INSERT;		break;
			case SDLK_KP1:		k.sym = SDLK_END;		break;
			case SDLK_KP2:		k.sym = SDLK_DOWN;		break;
			case SDLK_KP3:		k.sym = SDLK_PAGEDOWN;		break;
			case SDLK_KP4:		k.sym = SDLK_LEFT;		break;
			case SDLK_KP5:		return;
			case SDLK_KP6:		k.sym = SDLK_RIGHT;		break;
			case SDLK_KP7:		k.sym = SDLK_HOME;		break;
			case SDLK_KP8:		k.sym = SDLK_UP;		break;
			case SDLK_KP9:		k.sym = SDLK_PAGEUP;		break;
			case SDLK_KP_PERIOD:	k.sym = SDLK_DELETE;		break;
			default:
				break;
			};
		}
		//Special editing operations.
		switch(k.sym) {
		case SDLK_INSERT:
			command_overwrite = !command_overwrite;
			window::notify_screen_update();
			return;
		case SDLK_END:
			command_cursor = command_buf.length();
			window::notify_screen_update();
			return;
		case SDLK_DOWN:
		case SDLK_PAGEDOWN:
			if(commandhistory_itr != commandhistory.begin()) {
				commandhistory_itr--;
				command_buf = *commandhistory_itr;
				if(command_cursor > command_buf.length())
					command_cursor = command_buf.length();
			}
			window::notify_screen_update();
			return;
		case SDLK_LEFT:
			command_cursor = (command_cursor > 0) ? (command_cursor - 4) : 0;
			window::notify_screen_update();
			return;
		case SDLK_RIGHT:
			command_cursor = (command_cursor < command_buf.length()) ? (command_cursor + 4) :
				command_buf.length();
			window::notify_screen_update();
			return;
		case SDLK_HOME:
			command_cursor = 0;
			window::notify_screen_update();
			return;
		case SDLK_UP:
		case SDLK_PAGEUP: {
			auto tmp = commandhistory_itr;
			if(++tmp != commandhistory.end()) {
				commandhistory_itr++;
				command_buf = *commandhistory_itr;
				if(command_cursor > command_buf.length())
					command_cursor = command_buf.length();
			}
			window::notify_screen_update();
			return;
		}
		case SDLK_DELETE:
			if(command_cursor < command_buf.length())
				command_buf = command_buf.substr(0, command_cursor) +
					command_buf.substr(command_cursor + 4);
			window::notify_screen_update();
			*commandhistory_itr = command_buf;
			return;
		case SDLK_BACKSPACE:
			if(command_cursor > 0) {
				command_buf = command_buf.substr(0, command_cursor - 4) +
					command_buf.substr(command_cursor);
				command_cursor -= 4;
			}
			window::notify_screen_update();
			*commandhistory_itr = command_buf;
			return;
		default:
			break;
		};

		//Not a special editing operation, insert/overwrite a character.
		uint32_t code = k.unicode;
		if(!code)
			return;
		uint8_t c1 = 33 + ((code >> 18) & 0x3F);
		uint8_t c2 = 33 + ((code >> 12) & 0x3F);
		uint8_t c3 = 33 + ((code >> 6) & 0x3F);
		uint8_t c4 = 33 + (code & 0x3F);
		if(command_overwrite && command_cursor < command_buf.length()) {
			command_buf[command_cursor] = c1;
			command_buf[command_cursor + 1] = c2;
			command_buf[command_cursor + 2] = c3;
			command_buf[command_cursor + 3] = c4;
			command_cursor += 4;
		} else {
			std::string foo = "    ";
			foo[0] = c1;
			foo[1] = c2;
			foo[2] = c3;
			foo[3] = c4;
			command_buf = command_buf.substr(0, command_cursor) + foo + command_buf.substr(command_cursor);
			command_cursor += 4;
		}
		*commandhistory_itr = command_buf;
		window::notify_screen_update();
	}

	void do_event(SDL_Event& e) throw(std::bad_alloc)
	{
#ifdef SIGALRM
		alarm(WATCHDOG_TIMEOUT);
#endif
		if(e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE && e.key.keysym.mod == (KMOD_LCTRL |
			KMOD_LALT))
			exit(1);
		if(e.type == SDL_USEREVENT && e.user.code == 0) {
			if(screen_is_dirty)
				window::notify_screen_update();
		}
		SDLKey key;
		if(e.type == SDL_ACTIVEEVENT && e.active.gain && e.active.state == SDL_APPACTIVE) {
			window::notify_screen_update();
			return;
		}
		if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
			key = e.key.keysym.sym;

		if(e.type == SDL_QUIT && state == WINSTATE_IDENTIFY)
			return;
		if(e.type == SDL_QUIT && state == WINSTATE_MODAL) {
			delayed_close_flag = true;
			return;
		}
		if(e.type == SDL_QUIT) {
			window_callback::do_close();
			state = WINSTATE_NORMAL;
			return;
		}

		switch(state) {
		case WINSTATE_NORMAL:
			if(e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
				int32_t xc = e.button.x;
				int32_t yc = e.button.y;
				xc -= 6;
				yc -= 6;
				if(e.button.button == SDL_BUTTON_LEFT) {
					if(e.button.state == SDL_PRESSED)
						mouse_mask |= 1;
					else
						mouse_mask &= ~1;
				}
				if(e.button.button == SDL_BUTTON_MIDDLE) {
					if(e.button.state == SDL_PRESSED)
						mouse_mask |= 2;
					else
						mouse_mask &= ~2;
				}
				if(e.button.button == SDL_BUTTON_RIGHT) {
					if(e.button.state == SDL_PRESSED)
						mouse_mask |= 4;
					else
						mouse_mask &= ~4;
				}
				window_callback::do_click(xc, yc, mouse_mask);
			}
			if(e.type == SDL_KEYDOWN && key == SDLK_ESCAPE)
				return;
			if(e.type == SDL_KEYUP && key == SDLK_ESCAPE) {
				state = WINSTATE_COMMAND;
				command_buf = "";
				command_cursor = 0;
				commandhistory.push_front("");
				if(commandhistory.size() > MAXHISTORY)
					commandhistory.pop_back();
				commandhistory_itr = commandhistory.begin();
				window::notify_screen_update();
				poll_inputs_internal();
				return;
			}
			process_input_event(&e);
			break;
		case WINSTATE_MODAL:
			//Send the key and eat it (prevent input from getting confused).
			keygroup::set_exclusive_key_listener(&keyeater);
			process_input_event(&e),
			keygroup::set_exclusive_key_listener(NULL);
			if(e.type == SDL_KEYUP && key == SDLK_ESCAPE) {
				state = WINSTATE_NORMAL;
				modconfirm = false;
				modal_return_flag = true;
				modmsg = "";
				window::notify_screen_update(true);
				return;
			}
			if(e.type == SDL_KEYUP && (key == SDLK_RETURN || key == SDLK_KP_ENTER)) {
				state = WINSTATE_NORMAL;
				modal_return_flag = true;
				modmsg = "";
				window::notify_screen_update(true);
				return;
			}
			break;
		case WINSTATE_COMMAND:
			//Send the key and eat it (prevent input from getting confused).
			keygroup::set_exclusive_key_listener(&keyeater);
			process_input_event(&e),
			keygroup::set_exclusive_key_listener(NULL);
			if(e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE) {
				state = WINSTATE_NORMAL;
				command_buf = "";
				window::notify_screen_update();
				if(commandhistory.front() == "")
					commandhistory.pop_front();
				return;
			}
			if(e.type == SDL_KEYUP && (e.key.keysym.sym == SDLK_RETURN ||
				e.key.keysym.sym == SDLK_KP_ENTER)) {
				state = WINSTATE_NORMAL;
				if(commandhistory.front() == "")
					commandhistory.pop_front();
				command::invokeC(decode_string(command_buf));
				command_buf = "";
				window::notify_screen_update();
				autorepeat_phase = 0;
				return;
			}
			if(e.type == SDL_KEYDOWN) {
				autorepeating_key = e.key.keysym;
				autorepeat_phase = 1;
				autorepeat_timecounter = 0;
				do_keyboard_command_edit(e.key.keysym);
			} else if(e.type == SDL_KEYUP) {
				autorepeat_phase = 0;
			}
			if(e.type == SDL_USEREVENT && e.user.code == 0) {
				autorepeat_timecounter++;
				if(!autorepeat_phase)
					break;
				unsigned timeout = (autorepeat_phase == 1) ? autorepeat_first : autorepeat_subsequent;
				if(autorepeat_timecounter >= timeout) {
					do_keyboard_command_edit(autorepeating_key);
					autorepeat_timecounter = 0;
					autorepeat_phase = 2;
				}
			}
			break;
		case WINSTATE_IDENTIFY:
			process_input_event(&e);
			break;
		};
	}
}

void graphics_init()
{
	SDL_initialized = true;
#ifdef SIGALRM
	signal(SIGALRM, sigalrm_handler);
	alarm(WATCHDOG_TIMEOUT);
#endif
	init_keys();
	if(!sdl_init) {
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER);
		SDL_EnableUNICODE(true);
		sdl_init = true;
		tid = SDL_AddTimer(MIN_UPDATE_TIME / 1000 + 1, timer_cb, NULL);
	}
	state = WINSTATE_NORMAL;
	current_screen = NULL;
	pause_active = false;
	hwsurf = NULL;
	command_overwrite = false;
	old_screen_h = 0;
	old_screen_w = 0;
	modal_return_flag = false;
	delayed_close_flag = false;
	console_mode = false;

	window::notify_screen_update();
	std::string windowname = "lsnes-" + lsnes_version + "[" + bsnes_core_version + "]";
	SDL_WM_SetCaption(windowname.c_str(), "lsnes");

	init_joysticks();
}

void graphics_quit()
{
	if(sdl_init) {
		SDL_RemoveTimer(tid);
		SDL_Quit();
		sdl_init = false;
	}
	SDL_initialized = false;
}

bool window::modal_message(const std::string& msg, bool confirm) throw(std::bad_alloc)
{
	modconfirm = confirm;
	modmsg = msg;
	state = WINSTATE_MODAL;
	notify_screen_update();
	poll_inputs_internal();
	bool ret = modconfirm;
	if(delayed_close_flag) {
		delayed_close_flag = false;
		window_callback::do_close();
	}
	return ret;
}

void window::notify_message() throw(std::bad_alloc, std::runtime_error)
{
	notify_screen_update();
}

void window::set_main_surface(screen& scr) throw()
{
	current_screen = &scr;
	notify_screen_update(true);
}

namespace
{
	bool is_time_for_screen_update(bool full)
	{
		uint64_t curtime = get_utime();
		//Always do full updates.
		if(!full && last_ui_update < curtime && last_ui_update + MIN_UPDATE_TIME > curtime) {
			screen_is_dirty = true;
			return false;
		}
		last_ui_update = curtime;
		screen_is_dirty = false;
		return true;
	}

	std::pair<uint32_t, uint32_t> compute_screen_size(uint32_t width, uint32_t height)
	{
		if(width < 512)
			width = 512;
		if(height < 448)
			height = 448;
		return std::make_pair(width, height);
	}

	std::pair<uint32_t, uint32_t> compute_window_size(uint32_t width, uint32_t height)
	{
		auto g = compute_screen_size(width, height);
		uint32_t win_w = ((g.first + 15) >> 4 << 4) + 278;
		uint32_t win_h = g.second + MAXMESSAGES * 16 + 48;
		return std::make_pair(win_w, win_h);
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

	void redraw_borders(SDL_Surface* swsurf, std::pair<uint32_t, uint32_t> screensize,
		std::pair<uint32_t, uint32_t> windowsize)
	{
		//Blank the screen and draw borders.
		memset(swsurf->pixels, 0, windowsize.second * swsurf->pitch);
		uint32_t bordercolor = 0xFF << (swsurf->format->Gshift);
		uint32_t msgbox_min_x = 2;
		uint32_t msgbox_min_y = 2;
		uint32_t msgbox_max_x = windowsize.first - 2;
		uint32_t msgbox_max_y = windowsize.second - 28;
		uint32_t cmdbox_min_x = 2;
		uint32_t cmdbox_max_x = windowsize.first - 2;
		uint32_t cmdbox_min_y = windowsize.second - 26;
		uint32_t cmdbox_max_y = windowsize.second - 2;
		if(!console_mode) {
			uint32_t scrbox_min_x = 2;
			uint32_t scrbox_max_x = screensize.first + 10;
			uint32_t scrbox_min_y = 2;
			uint32_t scrbox_max_y = screensize.second + 10;
			uint32_t stsbox_min_x = screensize.first + 12;
			uint32_t stsbox_max_x = windowsize.first - 2;
			uint32_t stsbox_min_y = 2;
			uint32_t stsbox_max_y = screensize.second + 10;
			msgbox_min_y = screensize.second + 12;
			draw_rectangle(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, scrbox_min_x,
				scrbox_min_y, scrbox_max_x, scrbox_max_y, bordercolor, 2);
			draw_rectangle(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, stsbox_min_x,
				stsbox_min_y, stsbox_max_x, stsbox_max_y, bordercolor, 2);
		}
		draw_rectangle(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, msgbox_min_x,
			msgbox_min_y, msgbox_max_x, msgbox_max_y, bordercolor, 2);
		draw_rectangle(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, cmdbox_min_x,
			cmdbox_min_y, cmdbox_max_x, cmdbox_max_y, bordercolor, 2);
	}

	void draw_main_screen(SDL_Surface* swsurf, std::pair<uint32_t, uint32_t> screensize)
	{
		uint32_t cw = current_screen ? current_screen->width : 0;
		uint32_t ch = current_screen ? current_screen->height : 0;
		//Blank parts not drawn.
		for(uint32_t i = 6; i < ch + 6; i++)
			memset(reinterpret_cast<uint8_t*>(swsurf->pixels) + i * swsurf->pitch + 24 + 4 * cw, 0,
				4 * (screensize.first - cw));
		for(uint32_t i = ch + 6; i < screensize.second + 6; i++)
			memset(reinterpret_cast<uint8_t*>(swsurf->pixels) + i * swsurf->pitch + 24, 0,
				4 * screensize.first);
		if(current_screen) {
			for(uint32_t i = 0; i < ch; i++)
				memcpy(reinterpret_cast<uint8_t*>(swsurf->pixels) + (i + 6) * swsurf->pitch + 24,
					reinterpret_cast<uint8_t*>(current_screen->memory) + current_screen->pitch * i,
					4 * cw);
		}
	}

	void draw_status_area(SDL_Surface* swsurf, std::pair<uint32_t, uint32_t> screensize)
	{
		uint32_t status_x = screensize.first + 16;
		uint32_t status_y = 6;
		auto& emustatus = window::get_emustatus();
		for(auto i : emustatus) {
			std::string msg = i.first + " " + i.second;
			draw_string(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, msg, status_x, status_y,
				256);
			status_y += 16;
		}
		while(status_y - 6 < screensize.second / 16 * 16) {
			draw_string(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, "", status_x, status_y,
				256);
			status_y += 16;
		}
	}

	void draw_messages(SDL_Surface* swsurf, std::pair<uint32_t, uint32_t> screensize,
		std::pair<uint32_t, uint32_t> windowsize)
	{
		uint32_t message_y;
		if(!console_mode)
			message_y = screensize.second + 16;
		else
			message_y = 6;
		size_t maxmessages = window::msgbuf.get_max_window_size();
		size_t msgnum = window::msgbuf.get_visible_first();
		size_t visible = window::msgbuf.get_visible_count();
		for(size_t j = 0; j < maxmessages; j++)
			try {
				std::ostringstream o;
				if(j < visible)
					o << (msgnum + j + 1) << ": " << window::msgbuf.get_message(msgnum + j);
				draw_string(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, o.str(), 6,
					message_y + 16 * j, windowsize.first - 12);
			} catch(...) {
			}
		if(window::msgbuf.is_more_messages())
			try {
				draw_string(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, "--More--",
					windowsize.first - 76, message_y + 16 * maxmessages - 16, 64);
		} catch(...) {
		}
	}

	void draw_command(SDL_Surface* swsurf, std::pair<uint32_t, uint32_t> windowsize)
	{
		uint32_t command_y = windowsize.second - 22;
		try {
			std::string command_showas = decode_string(command_buf);
			if(state == WINSTATE_COMMAND)
				draw_command(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, command_showas,
					command_cursor / 4, 6, command_y, windowsize.first - 12, command_overwrite);
			else
				draw_string(reinterpret_cast<uint8_t*>(swsurf->pixels), swsurf->pitch, "", 6,
					command_y, windowsize.first - 12);
		} catch(...) {
		}
	}
}

void window::notify_screen_update(bool full) throw()
{
	bool resize_screen = false;
	if(!is_time_for_screen_update(full)) {
		return;
	}
	auto windowsize = compute_window_size(current_screen ? current_screen->width : 0, current_screen ? 
		current_screen->height : 0);
	auto screensize = compute_screen_size(current_screen ? current_screen->width : 0, current_screen ? 
		current_screen->height : 0);
	show_fps();

	
	if(!hwsurf || windowsize != current_windowsize) {
		//Create/Resize the window.
		SDL_Surface* hwsurf2 = SDL_SetVideoMode(windowsize.first, windowsize.second, 32, SDL_SWSURFACE);
		if(!hwsurf2) {
			//We are in too fucked up state to even print error as message.
			std::cout << "PANIC: Can't create/resize window: " << SDL_GetError() << std::endl;
			exit(1);
		}
		hwsurf = hwsurf2;
		full = true;
		current_windowsize = windowsize;
	}
	if(current_screen)
		current_screen->set_palette(hwsurf->format->Rshift, hwsurf->format->Gshift, hwsurf->format->Bshift);
	SDL_LockSurface(hwsurf);
	if(full)
		redraw_borders(hwsurf, screensize, windowsize);
	if(!console_mode) {
		draw_main_screen(hwsurf, screensize);
		draw_status_area(hwsurf, screensize);
	}
	draw_messages(hwsurf, screensize, windowsize);
	draw_command(hwsurf, windowsize);

	//Draw modal dialog.
	if(state == WINSTATE_MODAL)
		try {
			draw_modal_dialog(hwsurf, modmsg, modconfirm);
		} catch(...) {
		}
	SDL_UnlockSurface(hwsurf);
	//SDL_BlitSurface(swsurf, NULL, hwsurf, NULL);
	SDL_UpdateRect(hwsurf, 0, 0, 0, 0);
}

void poll_inputs_internal() throw(std::bad_alloc)
{
	SDL_Event e;
	identify_helper h;
	if(state == WINSTATE_IDENTIFY)
		keygroup::set_exclusive_key_listener(&h);
	while(state != WINSTATE_NORMAL) {
		window::poll_joysticks();
		if(SDL_PollEvent(&e))
			do_event(e);
		::wait_usec(10000);
		if(delayed_close_flag) {
			state = WINSTATE_NORMAL;
			return;
		}
		if(h.got_it()) {
			window::modal_message(h.keys(), false);
			keygroup::set_exclusive_key_listener(NULL);
		}
	}
}

void window::poll_inputs() throw(std::bad_alloc)
{
	SDL_Event e;
	while(1) {
		assert(state == WINSTATE_NORMAL);
		poll_joysticks();
		if(!pause_active && !SDL_PollEvent(&e))
			break;
		else if(!pause_active)
			do_event(e);
		else if(SDL_PollEvent(&e))
			do_event(e);
		else
			::wait_usec(10000);
	}
}

void window::paused(bool enable) throw()
{
	pause_active = enable;
	notify_screen_update();
}

namespace
{
	function_ptr_command<> identify_key("identify-key", "Identify a key",
		"Syntax: identify-key\nIdentifies a (pseudo-)key.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			identify();
		});

	function_ptr_command<> scroll_up("scroll-up", "Scroll messages a page up",
		"Syntax: scroll-up\nScrolls message console backward one page.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::msgbuf.scroll_up_page();
		});

	function_ptr_command<> scroll_fullup("scroll-fullup", "Scroll messages to beginning",
		"Syntax: scroll-fullup\nScrolls message console to its beginning.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::msgbuf.scroll_beginning();
		});

	function_ptr_command<> scroll_fulldown("scroll-fulldown", "Scroll messages to end",
		"Syntax: scroll-fulldown\nScrolls message console to its end.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::msgbuf.scroll_end();
		});

	function_ptr_command<> scrolldown("scroll-down", "Scroll messages a page down",
		"Syntax: scroll-up\nScrolls message console forward one page.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::msgbuf.scroll_down_page();
		});

	function_ptr_command<> toggle_console("toggle-console", "Toggle console between small and full window",
		"Syntax: toggle-console\nToggles console between small and large.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			console_mode = !console_mode;
			if(console_mode)
				window::msgbuf.set_max_window_size(hwsurf ? (hwsurf->h - 38) / 16 : 36);
			else
				window::msgbuf.set_max_window_size(MAXMESSAGES);
			window::notify_screen_update(true);
		});
}

void window::wait_usec(uint64_t usec) throw(std::bad_alloc)
{
	wait_canceled = false;
	uint64_t end_at = get_utime() + usec;
	while(!wait_canceled) {
		SDL_Event e;
		poll_joysticks();
		while(SDL_PollEvent(&e))
			do_event(e);
		uint64_t curtime = get_utime();
		if(curtime > end_at || wait_canceled)
			break;
		if(end_at > curtime + 10000)
			::wait_usec(10000);
		else
			::wait_usec(end_at - curtime);
	}
	wait_canceled = false;
}

void window::fatal_error2() throw()
{
	try {
		message("PANIC: Cannot continue, press ESC or close window to exit.");
		//Force redraw.
		notify_screen_update(true);
	} catch(...) {
		//Just crash.
		exit(1);
	}
	while(true) {
		SDL_Event e;
		if(SDL_WaitEvent(&e)) {
			if(e.type == SDL_QUIT)
				exit(1);
			if(e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE)
				exit(1);
		}
	}
}

void window::cancel_wait() throw()
{
	wait_canceled = true;
}

const char* graphics_plugin_name = "SDL graphics plugin";
