#include "window.hpp"
#include "render.hpp"
#include "command.hpp"
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

#define WATCHDOG_TIMEOUT 15
#define MAXMESSAGES 6
#define MSGHISTORY 1000
#define MAXHISTORY 1000
#define JOYTHRESHOLD 3200

#include <SDL.h>
#include <string>
#include <map>
#include <stdexcept>

#define SDL_DEV_NONE 0
#define SDL_DEV_KEYBOARD 1
#define SDL_DEV_JOYAXIS 2
#define SDL_DEV_JOYBUTTON 3
#define SDL_DEV_JOYHAT 4
// Limit the emulator to ~30fps.
#define MIN_UPDATE_TIME 33

class keymapper_helper_sdl
{
public:
	typedef struct {
		int device;
		SDL_keysym keyboard;
		uint32_t joyaxis;
		uint32_t joybutton;
		uint32_t joyhat;
	} keysymbol;
	struct internal_keysymbol
	{
		int mode;
		unsigned scan;
		unsigned symbolic;
		unsigned joyaxis;
		unsigned joyhat;
		unsigned joybutton;
		bool operator==(const internal_keysymbol& k) const;
	};
	struct translated_event
	{
		translated_event(keysymbol _symbol, bool _polarity, bool _more = false);
		keysymbol symbol;
		bool polarity;
		bool more;
	};
	static translated_event translate_event(SDL_Event& e) throw(std::bad_alloc, std::runtime_error);
	static unsigned mod_str(std::string mod) throw(std::bad_alloc, std::runtime_error);
	static unsigned mod_key(keysymbol key) throw(std::bad_alloc);
	static internal_keysymbol key_str(std::string key) throw(std::bad_alloc, std::runtime_error);
	static internal_keysymbol key_key(keysymbol key) throw(std::bad_alloc);
	static std::string name_key(unsigned mod, unsigned modmask, struct internal_keysymbol key)
		throw(std::bad_alloc);
	static std::string print_key_info(keysymbol key) throw(std::bad_alloc);
};

#define KEYTYPE_SCAN_SYMBOL 0
#define KEYTYPE_SCAN 1
#define KEYTYPE_SYMBOL 2
#define KEYTYPE_JOYAXIS 3
#define KEYTYPE_JOYHAT 4
#define KEYTYPE_JOYBUTTON 5
#define KEYTYPE_NONE 6

#define BARE_CTRL	0x0001
#define BARE_LCTRL	0x0002
#define BARE_RCTRL	0x0004
#define BARE_NUM	0x0008
#define BARE_SHIFT	0x0010
#define BARE_LSHIFT	0x0020
#define BARE_RSHIFT	0x0040
#define BARE_CAPS	0x0080
#define BARE_ALT	0x0100
#define BARE_LALT	0x0200
#define BARE_RALT	0x0400
#define BARE_MODE	0x0800
#define BARE_META	0x1000
#define BARE_LMETA	0x2000
#define BARE_RMETA	0x4000

#define LCTRL_VALUE	(BARE_CTRL | BARE_LCTRL)
#define RCTRL_VALUE	(BARE_CTRL | BARE_RCTRL)
#define LALT_VALUE	(BARE_ALT | BARE_LALT)
#define RALT_VALUE	(BARE_ALT | BARE_RALT)
#define LSHIFT_VALUE	(BARE_SHIFT | BARE_LSHIFT)
#define RSHIFT_VALUE	(BARE_SHIFT | BARE_RSHIFT)
#define LMETA_VALUE	(BARE_META | BARE_LMETA)
#define RMETA_VALUE	(BARE_META | BARE_RMETA)

struct calibration
{
	calibration();
	short left;
	short center;
	short right;
	short temp;
	bool known;
	void update(short val) throw();
	void centered() throw();
	short analog(short val) throw();
	short digital(short val) throw();
};

calibration::calibration()
{
	known = false;
}

void calibration::update(short val) throw()
{
	if(!known) {
		temp = val;
		left = val;
		center = val;
		right = val;
		known = true;
		return;
	}
	temp = val;
	if(temp > right)
		right = temp;
	if(temp < left)
		left = temp;
}

void calibration::centered() throw()
{
	center = temp;
}

short calibration::analog(short val) throw()
{
	if(!known)
		return 0;
	if(val <= left)
		return -32768;
	if(val >= right)
		return 32767;
	if(val < center)
		return static_cast<short>(-32768.0 * (val - left) / (center - left));
	if(val > center)
		return static_cast<short>(32767.0 * (val - center) / (right - center));
	return 0;
}

short calibration::digital(short val) throw()
{
	short a = analog(val);
	if(a < -JOYTHRESHOLD)
		return -1;
	if(a > JOYTHRESHOLD)
		return 1;
	return 0;
}

std::map<unsigned short, calibration> calibrations;

void handle_calibration_event(SDL_JoyAxisEvent& e)
{
	unsigned short num = static_cast<unsigned short>(e.which) * 256 + static_cast<unsigned short>(e.axis);
	calibrations[num].update(e.value);
}

void exit_calibration()
{
	for(auto i = calibrations.begin(); i != calibrations.end(); i++)
		i->second.centered();
}

namespace
{
	bool wait_canceled;
	SDL_TimerID tid;
	std::map<unsigned short, int> axis_current_state;	//-1, 0 or 1.
	std::map<unsigned short, unsigned> hat_current_state;	//1 for up, 2 for right, 4 for down, 8 for left.

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

	struct modifier
	{
		const char* name;
		bool synthethic;
		unsigned sdlvalue;
		unsigned asrealkey;
		unsigned asmodifier;
	} modifiers_table[] = {
		{ "ctrl",	true,	0,		0,		0x0001	},
		{ "lctrl",	false,	KMOD_LCTRL,	0x0003,		0x0002	},
		{ "rctrl",	false,	KMOD_RCTRL,	0x0005,		0x0004	},
		{ "alt",	true,	0,		0,		0x0008	},
		{ "lalt",	false,	KMOD_LALT,	0x0018,		0x0010	},
		{ "ralt",	false,	KMOD_RALT,	0x0028,		0x0020	},
		{ "shift",	true,	0,		0,		0x0040	},
		{ "lshift",	false,	KMOD_LSHIFT,	0x00C0,		0x0080	},
		{ "rshift",	false,	KMOD_RSHIFT,	0x0140,		0x0100	},
		{ "meta",	true,	0,		0,		0x0200	},
		{ "lmeta",	false,	KMOD_LMETA,	0x0600,		0x0400	},
		{ "rmeta",	false,	KMOD_RMETA,	0x0A00,		0x0800	},
		{ "num",	false,	KMOD_NUM,	0x1000,		0x1000	},
		{ "caps",	false,	KMOD_CAPS,	0x2000,		0x2000	},
		{ "mode",	false,	KMOD_MODE,	0x4000,		0x4000	},
		{ NULL,		false,	0,		0,		0	}
	};

	struct key
	{
		const char* name;
		unsigned symbol;
	} keys_table[] = {
		{"backspace",		SDLK_BACKSPACE},
		{"tab",			SDLK_TAB},
		{"clear",		SDLK_CLEAR},
		{"return",		SDLK_RETURN},
		{"pause",		SDLK_PAUSE},
		{"escape",		SDLK_ESCAPE},
		{"space",		SDLK_SPACE},
		{"exclaim",		SDLK_EXCLAIM},
		{"quotedbl",		SDLK_QUOTEDBL},
		{"hash",		SDLK_HASH},
		{"dollar",		SDLK_DOLLAR},
		{"ampersand",		SDLK_AMPERSAND},
		{"quote",		SDLK_QUOTE},
		{"leftparen",		SDLK_LEFTPAREN},
		{"rightparen",		SDLK_RIGHTPAREN},
		{"asterisk",		SDLK_ASTERISK},
		{"plus",		SDLK_PLUS},
		{"comma",		SDLK_COMMA},
		{"minus",		SDLK_MINUS},
		{"period",		SDLK_PERIOD},
		{"slash",		SDLK_SLASH},
		{"0",			SDLK_0},
		{"1",			SDLK_1},
		{"2",			SDLK_2},
		{"3",			SDLK_3},
		{"4",			SDLK_4},
		{"5",			SDLK_5},
		{"6",			SDLK_6},
		{"7",			SDLK_7},
		{"8",			SDLK_8},
		{"9",			SDLK_9},
		{"colon",		SDLK_COLON},
		{"semicolon",		SDLK_SEMICOLON},
		{"less",		SDLK_LESS},
		{"equals",		SDLK_EQUALS},
		{"greater",		SDLK_GREATER},
		{"question",		SDLK_QUESTION},
		{"at",			SDLK_AT},
		{"leftbracket",		SDLK_LEFTBRACKET},
		{"backslash",		SDLK_BACKSLASH},
		{"rightbracket",	SDLK_RIGHTBRACKET},
		{"caret",		SDLK_CARET},
		{"underscore",		SDLK_UNDERSCORE},
		{"backquote",		SDLK_BACKQUOTE},
		{"a",			SDLK_a},
		{"b",			SDLK_b},
		{"c",			SDLK_c},
		{"d",			SDLK_d},
		{"e",			SDLK_e},
		{"f",			SDLK_f},
		{"g",			SDLK_g},
		{"h",			SDLK_h},
		{"i",			SDLK_i},
		{"j",			SDLK_j},
		{"k",			SDLK_k},
		{"l",			SDLK_l},
		{"m",			SDLK_m},
		{"n",			SDLK_n},
		{"o",			SDLK_o},
		{"p",			SDLK_p},
		{"q",			SDLK_q},
		{"r",			SDLK_r},
		{"s",			SDLK_s},
		{"t",			SDLK_t},
		{"u",			SDLK_u},
		{"v",			SDLK_v},
		{"w",			SDLK_w},
		{"x",			SDLK_x},
		{"y",			SDLK_y},
		{"z",			SDLK_z},
		{"delete",		SDLK_DELETE},
		{"world_0",		SDLK_WORLD_0},
		{"world_1",		SDLK_WORLD_1},
		{"world_2",		SDLK_WORLD_2},
		{"world_3",		SDLK_WORLD_3},
		{"world_4",		SDLK_WORLD_4},
		{"world_5",		SDLK_WORLD_5},
		{"world_6",		SDLK_WORLD_6},
		{"world_7",		SDLK_WORLD_7},
		{"world_8",		SDLK_WORLD_8},
		{"world_9",		SDLK_WORLD_9},
		{"world_10",		SDLK_WORLD_10},
		{"world_11",		SDLK_WORLD_11},
		{"world_12",		SDLK_WORLD_12},
		{"world_13",		SDLK_WORLD_13},
		{"world_14",		SDLK_WORLD_14},
		{"world_15",		SDLK_WORLD_15},
		{"world_16",		SDLK_WORLD_16},
		{"world_17",		SDLK_WORLD_17},
		{"world_18",		SDLK_WORLD_18},
		{"world_19",		SDLK_WORLD_19},
		{"world_20",		SDLK_WORLD_20},
		{"world_21",		SDLK_WORLD_21},
		{"world_22",		SDLK_WORLD_22},
		{"world_23",		SDLK_WORLD_23},
		{"world_24",		SDLK_WORLD_24},
		{"world_25",		SDLK_WORLD_25},
		{"world_26",		SDLK_WORLD_26},
		{"world_27",		SDLK_WORLD_27},
		{"world_28",		SDLK_WORLD_28},
		{"world_29",		SDLK_WORLD_29},
		{"world_30",		SDLK_WORLD_30},
		{"world_31",		SDLK_WORLD_31},
		{"world_32",		SDLK_WORLD_32},
		{"world_33",		SDLK_WORLD_33},
		{"world_34",		SDLK_WORLD_34},
		{"world_35",		SDLK_WORLD_35},
		{"world_36",		SDLK_WORLD_36},
		{"world_37",		SDLK_WORLD_37},
		{"world_38",		SDLK_WORLD_38},
		{"world_39",		SDLK_WORLD_39},
		{"world_40",		SDLK_WORLD_40},
		{"world_41",		SDLK_WORLD_41},
		{"world_42",		SDLK_WORLD_42},
		{"world_43",		SDLK_WORLD_43},
		{"world_44",		SDLK_WORLD_44},
		{"world_45",		SDLK_WORLD_45},
		{"world_46",		SDLK_WORLD_46},
		{"world_47",		SDLK_WORLD_47},
		{"world_48",		SDLK_WORLD_48},
		{"world_49",		SDLK_WORLD_49},
		{"world_50",		SDLK_WORLD_50},
		{"world_51",		SDLK_WORLD_51},
		{"world_52",		SDLK_WORLD_52},
		{"world_53",		SDLK_WORLD_53},
		{"world_54",		SDLK_WORLD_54},
		{"world_55",		SDLK_WORLD_55},
		{"world_56",		SDLK_WORLD_56},
		{"world_57",		SDLK_WORLD_57},
		{"world_58",		SDLK_WORLD_58},
		{"world_59",		SDLK_WORLD_59},
		{"world_60",		SDLK_WORLD_60},
		{"world_61",		SDLK_WORLD_61},
		{"world_62",		SDLK_WORLD_62},
		{"world_63",		SDLK_WORLD_63},
		{"world_64",		SDLK_WORLD_64},
		{"world_65",		SDLK_WORLD_65},
		{"world_66",		SDLK_WORLD_66},
		{"world_67",		SDLK_WORLD_67},
		{"world_68",		SDLK_WORLD_68},
		{"world_69",		SDLK_WORLD_69},
		{"world_70",		SDLK_WORLD_70},
		{"world_71",		SDLK_WORLD_71},
		{"world_72",		SDLK_WORLD_72},
		{"world_73",		SDLK_WORLD_73},
		{"world_74",		SDLK_WORLD_74},
		{"world_75",		SDLK_WORLD_75},
		{"world_76",		SDLK_WORLD_76},
		{"world_77",		SDLK_WORLD_77},
		{"world_78",		SDLK_WORLD_78},
		{"world_79",		SDLK_WORLD_79},
		{"world_80",		SDLK_WORLD_80},
		{"world_81",		SDLK_WORLD_81},
		{"world_82",		SDLK_WORLD_82},
		{"world_83",		SDLK_WORLD_83},
		{"world_84",		SDLK_WORLD_84},
		{"world_85",		SDLK_WORLD_85},
		{"world_86",		SDLK_WORLD_86},
		{"world_87",		SDLK_WORLD_87},
		{"world_88",		SDLK_WORLD_88},
		{"world_89",		SDLK_WORLD_89},
		{"world_90",		SDLK_WORLD_90},
		{"world_91",		SDLK_WORLD_91},
		{"world_92",		SDLK_WORLD_92},
		{"world_93",		SDLK_WORLD_93},
		{"world_94",		SDLK_WORLD_94},
		{"world_95",		SDLK_WORLD_95},
		{"kp0",			SDLK_KP0},
		{"kp1",			SDLK_KP1},
		{"kp2",			SDLK_KP2},
		{"kp3",			SDLK_KP3},
		{"kp4",			SDLK_KP4},
		{"kp5",			SDLK_KP5},
		{"kp6",			SDLK_KP6},
		{"kp7",			SDLK_KP7},
		{"kp8",			SDLK_KP8},
		{"kp9",			SDLK_KP9},
		{"kp_period",		SDLK_KP_PERIOD},
		{"kp_divide",		SDLK_KP_DIVIDE},
		{"kp_multiply",		SDLK_KP_MULTIPLY},
		{"kp_minus",		SDLK_KP_MINUS},
		{"kp_plus",		SDLK_KP_PLUS},
		{"kp_enter",		SDLK_KP_ENTER},
		{"kp_equals",		SDLK_KP_EQUALS},
		{"up",			SDLK_UP},
		{"down",		SDLK_DOWN},
		{"right",		SDLK_RIGHT},
		{"left",		SDLK_LEFT},
		{"insert",		SDLK_INSERT},
		{"home",		SDLK_HOME},
		{"end",			SDLK_END},
		{"pageup",		SDLK_PAGEUP},
		{"pagedown",		SDLK_PAGEDOWN},
		{"f1",			SDLK_F1},
		{"f2",			SDLK_F2},
		{"f3",			SDLK_F3},
		{"f4",			SDLK_F4},
		{"f5",			SDLK_F5},
		{"f6",			SDLK_F6},
		{"f7",			SDLK_F7},
		{"f8",			SDLK_F8},
		{"f9",			SDLK_F9},
		{"f10",			SDLK_F10},
		{"f11",			SDLK_F11},
		{"f12",			SDLK_F12},
		{"f13",			SDLK_F13},
		{"f14",			SDLK_F14},
		{"f15",			SDLK_F15},
		{"numlock",		SDLK_NUMLOCK},
		{"capslock",		SDLK_CAPSLOCK},
		{"scrollock",		SDLK_SCROLLOCK},
		{"rshift",		SDLK_RSHIFT},
		{"lshift",		SDLK_LSHIFT},
		{"rctrl",		SDLK_RCTRL},
		{"lctrl",		SDLK_LCTRL},
		{"ralt",		SDLK_RALT},
		{"lalt",		SDLK_LALT},
		{"rmeta",		SDLK_RMETA},
		{"lmeta",		SDLK_LMETA},
		{"lsuper",		SDLK_LSUPER},
		{"rsuper",		SDLK_RSUPER},
		{"mode",		SDLK_MODE},
		{"compose",		SDLK_COMPOSE},
		{"help",		SDLK_HELP},
		{"print",		SDLK_PRINT},
		{"sysreq",		SDLK_SYSREQ},
		{"break",		SDLK_BREAK},
		{"menu",		SDLK_MENU},
		{"power",		SDLK_POWER},
		{"euro",		SDLK_EURO},
		{"undo",		SDLK_UNDO},
		{NULL,			0}
	};

	unsigned recognize_one_modifier(std::string mod)
	{
		struct modifier* m = modifiers_table;
		while(m->name) {
			if(mod == std::string(m->name))
				return m->asmodifier;
			m++;
		}
		throw std::runtime_error("Unknown modifier '" + mod + "'");
	}

	uint32_t transpack[16] = {0, 1, 3, 2, 5, 4, 0, 7, 8, 0, 0, 6, 0, 0, 0};
	uint32_t polarities[9] = {1, 1, 1, 1, 0, 0, 0, 0, 0};
	uint32_t directions[9] = {1, 2, 4, 8, 1, 2, 4, 8, 0};

	uint64_t X[] = {
		0x032221008ULL,		//From center.
		0x344444184ULL,		//From up.
		0x555544855ULL,		//From up&right.
		0x555528055ULL,		//From right.
		0x555586655ULL,		//From down&right.
		0x663816666ULL,		//From down.
		0x668777777ULL,		//From down&left.
		0x082777777ULL,		//From left.
		0x844777777ULL		//From up&left.
	};

	void hat_transition(uint32_t& ov, uint32_t& v, bool& polarity, bool& more)
	{
		uint32_t t1 = transpack[ov];
		uint32_t t2 = transpack[v];
		more = (v != ((X[t1] >> 4 * t2) & 0xF));
		polarity = polarities[(X[t1] >> 4 * t2) & 0xF];
		ov ^= directions[(X[t1] >> 4 * t2) & 0xF];
		v = directions[(X[t1] >> 4 * t2) & 0xF];
	}
}

keymapper_helper_sdl::translated_event::translated_event(keysymbol _symbol, bool _polarity, bool _more)
{
	symbol = _symbol;
	polarity = _polarity;
	more = _more;
}

keymapper_helper_sdl::translated_event keymapper_helper_sdl::translate_event(SDL_Event& e)
	throw(std::bad_alloc, std::runtime_error)
{
	if(e.type == SDL_JOYHATMOTION) {
		uint32_t axis = static_cast<uint32_t>(e.jhat.which) * 256 + static_cast<uint32_t>(e.jhat.hat);
		uint32_t v = 0;
		if(e.jhat.value & SDL_HAT_UP)		v |= 1;
		if(e.jhat.value & SDL_HAT_RIGHT)	v |= 2;
		if(e.jhat.value & SDL_HAT_DOWN)		v |= 4;
		if(e.jhat.value & SDL_HAT_LEFT)		v |= 8;
		unsigned ov = hat_current_state[axis];
		bool more = false;
		bool polarity = false;
		if(ov == v) {
			keysymbol k;
			k.device = SDL_DEV_NONE;
			return translated_event(k, false);
		}
		hat_transition(ov, v, polarity, more);
		hat_current_state[axis] = ov;
		axis |= (v << 16);
		keysymbol k;
		k.device = SDL_DEV_JOYHAT;
		k.joyhat = axis;
		return translated_event(k, polarity, more);
	} else if(e.type == SDL_JOYAXISMOTION) {
		uint32_t axis = static_cast<uint32_t>(e.jaxis.which) * 256 + static_cast<uint32_t>(e.jaxis.axis);
		int v = calibrations[axis].digital(e.jaxis.value);
		int ov = axis_current_state[axis];
		bool more = false;
		bool polarity = false;
		if(ov == v) {
			keysymbol k;
			k.device = SDL_DEV_NONE;
			return translated_event(k, false);
		} else if(ov == -1) {
			more = (v == 1);
			polarity = false;
		} else if(ov == 0) {
			polarity = true;
			axis |= ((v == 1) ? 0x10000 : 0);
		} else if(ov == 1) {
			more = (v == -1);
			polarity = false;
			axis |= 0x10000;
		}
		keysymbol k;
		k.device = SDL_DEV_JOYAXIS;
		k.joyaxis = axis;
		return translated_event(k, polarity, more);
	} else if(e.type == SDL_JOYBUTTONUP || e.type == SDL_JOYBUTTONDOWN) {
		keysymbol k;
		k.device = SDL_DEV_JOYBUTTON;
		k.joybutton = static_cast<uint32_t>(e.jbutton.which) * 256 + static_cast<uint32_t>(e.jbutton.button);
		return translated_event(k, (e.type == SDL_JOYBUTTONDOWN));

	} else if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
		keysymbol k;
		k.device = SDL_DEV_KEYBOARD;
		k.keyboard = e.key.keysym;
		return translated_event(k, (e.type == SDL_KEYDOWN));
	} else {
		keysymbol k;
		k.device = SDL_DEV_NONE;
		return translated_event(k, false);
	}
}

bool keymapper_helper_sdl::internal_keysymbol::operator==(const internal_keysymbol& k) const
{
	if(mode == KEYTYPE_SCAN_SYMBOL && k.mode == KEYTYPE_SCAN_SYMBOL)
		return (scan == k.scan || symbolic == k.symbolic);
	if((mode == KEYTYPE_SCAN_SYMBOL && k.mode == KEYTYPE_SCAN) || (k.mode == KEYTYPE_SCAN_SYMBOL &&
		mode == KEYTYPE_SCAN) || (k.mode == KEYTYPE_SCAN && mode == KEYTYPE_SCAN))
		return (scan == k.scan);
	if((mode == KEYTYPE_SCAN_SYMBOL && k.mode == KEYTYPE_SYMBOL) || (k.mode == KEYTYPE_SCAN_SYMBOL &&
		mode == KEYTYPE_SYMBOL) || (k.mode == KEYTYPE_SYMBOL && mode == KEYTYPE_SYMBOL))
		return (symbolic == k.symbolic);
	if(mode == KEYTYPE_JOYAXIS && k.mode == KEYTYPE_JOYAXIS)
		return (joyaxis == k.joyaxis);
	if(mode == KEYTYPE_JOYHAT && k.mode == KEYTYPE_JOYHAT) {
		//Joystick hats are handled specially. Diffrent hats never compare equal.
		if((joyhat & 0xFFFF) != (k.joyhat & 0xFFFF))
			return false;
		//For the same hat, if there is overlap, then they compare equal.
		return (((joyhat & k.joyhat) >> 16) != 0);
	}
	return false;
}

unsigned keymapper_helper_sdl::mod_str(std::string mod) throw(std::bad_alloc, std::runtime_error)
{
	unsigned ret = 0;
	while(mod != "") {
		size_t split = mod.find_first_of(",");
		std::string omod;
		if(split < mod.length()) {
			omod = mod.substr(0, split);
			mod = mod.substr(split + 1);
			if(mod == "")
				throw std::runtime_error("Invalid modifier specification (trailing comma)");
		} else {
			omod = mod;
			mod = "";
		}
		ret |= recognize_one_modifier(omod);
	}
	return ret;
}

unsigned keymapper_helper_sdl::mod_key(keymapper_helper_sdl::keysymbol key) throw(std::bad_alloc)
{
	if(key.device != SDL_DEV_KEYBOARD)
		return 0;
	struct modifier* m = modifiers_table;
	unsigned ret = 0;
	while(m->name) {
		if(!m->synthethic && (key.keyboard.mod & m->sdlvalue))
			ret |= m->asrealkey;
		m++;
	}
	return ret;
}

keymapper_helper_sdl::internal_keysymbol keymapper_helper_sdl::key_str(std::string key) throw(std::bad_alloc,
	std::runtime_error)
{
	if(key.length() > 8 && key.substr(0, 8) == "joystick") {
		const char* rest = key.c_str() + 8;
		char* end;
		unsigned subtype = 0;
		unsigned x;
		unsigned long value = strtoul(rest, &end, 10);
		if(value > 255)
			throw std::runtime_error("Bad joystick number '" + key + "'");
		x = value * 256;
		if(!strncmp(end, "button", 6)) {
			subtype = 1;
			rest = end + 6;
		} else if(!strncmp(end, "axis", 4)) {
			subtype = 2;
			rest = end + 4;
		} else if(!strncmp(end, "hat", 3)) {
			subtype = 3;
			rest = end + 3;
		} else
			throw std::runtime_error("Bad joystick subtype '" + key + "'");
		value = strtoul(rest, &end, 10);
		x += value;
		if(subtype == 1) {
			if(value > 255 || *end)
				throw std::runtime_error("Bad joystick button '" + key + "'");
			internal_keysymbol k;
			k.mode = KEYTYPE_JOYBUTTON;
			k.joybutton = x;
			return k;
		} else if(subtype == 2) {
			if(!strncmp(end, "+", 1) && value <= 255)
				x |= 0x10000;
			else if(!strncmp(end, "-", 1) && value <= 255)
				;
			else
				throw std::runtime_error("Bad joystick axis '" + key + "'");
			internal_keysymbol k;
			k.mode = KEYTYPE_JOYAXIS;
			k.joyaxis = x;
			return k;
		}  else if(subtype == 3) {
			if(!strncmp(end, "n", 1) && value <= 255)
				x |= 0x10000;
			else if(!strncmp(end, "e", 1) && value <= 255)
				x |= 0x20000;
			else if(!strncmp(end, "s", 1) && value <= 255)
				x |= 0x40000;
			else if(!strncmp(end, "w", 1) && value <= 255)
				x |= 0x80000;
			else
				throw std::runtime_error("Bad joystick hat '" + key + "'");
			internal_keysymbol k;
			k.mode = KEYTYPE_JOYHAT;
			k.joyhat = x;
			return k;
		}
	} if(key.length() > 3 && key.substr(0, 3) == "key") {
		const char* rest = key.c_str() + 3;
		char* end;
		unsigned long value = strtoul(rest, &end, 10);
		if(*end || value > 255)
			throw std::runtime_error("Bad scancode keysymbol '" + key + "'");
		internal_keysymbol k;
		k.mode = KEYTYPE_SCAN;
		k.scan = value;
		k.symbolic = 0;
		return k;
	}
	struct key* k = keys_table;
	while(k->name) {
		if(key == std::string(k->name)) {
			internal_keysymbol k2;
			k2.mode = KEYTYPE_SYMBOL;
			k2.scan = 0;
			k2.symbolic = k->symbol;
			return k2;
		}
		k++;
	}
	throw std::runtime_error("Unknown key '" + key + "'");
}

keymapper_helper_sdl::internal_keysymbol keymapper_helper_sdl::key_key(keymapper_helper_sdl::keysymbol key)
	throw(std::bad_alloc)
{
	internal_keysymbol k;
	if(key.device == SDL_DEV_KEYBOARD) {
		k.mode = KEYTYPE_SCAN_SYMBOL;
		k.scan = key.keyboard.scancode;
		k.symbolic = key.keyboard.sym;
	} else if(key.device == SDL_DEV_JOYAXIS) {
		k.mode = KEYTYPE_JOYAXIS;
		k.joyaxis = key.joyaxis;
	} else if(key.device == SDL_DEV_JOYBUTTON) {
		k.mode = KEYTYPE_JOYBUTTON;
		k.joybutton = key.joybutton;
	} else if(key.device == SDL_DEV_JOYHAT) {
		k.mode = KEYTYPE_JOYHAT;
		k.joyhat = key.joyhat;
	} else {
		k.mode = KEYTYPE_NONE;
	}
	return k;
}

std::string keymapper_helper_sdl::name_key(unsigned mod, unsigned modmask, struct internal_keysymbol key)
	throw(std::bad_alloc)
{
	std::string ret = "";
	if(mod != 0 || modmask != 0) {
		struct modifier* m = modifiers_table;
		while(m->name) {
			if((mod & m->asmodifier) == m->asmodifier) {
				ret = ret + m->name + ",";
			}
			m++;
		}
		ret = ret.substr(0, ret.length() - 1);
		ret = ret + "/";
		m = modifiers_table;
		while(m->name) {
			if((modmask & m->asmodifier) == m->asmodifier) {
				ret = ret + m->name + ",";
			}
			m++;
		}
		ret = ret.substr(0, ret.length() - 1);
		ret = ret + " ";
	}
	if(key.mode == KEYTYPE_SCAN_SYMBOL || key.mode == KEYTYPE_SYMBOL) {
		struct key* k = keys_table;
		while(k->name) {
			if(key.symbolic == k->symbol) {
				ret = ret + k->name;
				break;
			}
			k++;
		}
	} else if(key.mode == KEYTYPE_SCAN) {
		std::ostringstream x;
		x << key.scan;
		ret = ret + "key" + x.str();
	} else if(key.mode == KEYTYPE_JOYAXIS) {
		std::ostringstream x;
		x << "joystick" << ((key.joyaxis >> 8) & 0xFF) << "axis" << (key.joyaxis & 0xFF);
		x << ((key.joyaxis & 0x10000) ? '+' : '-');
		ret = ret + x.str();
	} else if(key.mode == KEYTYPE_JOYBUTTON) {
		std::ostringstream x;
		x << "joystick" << ((key.joybutton >> 8) & 0xFF) << "button" << (key.joybutton & 0xFF);
		ret = ret + x.str();
	} else if(key.mode == KEYTYPE_JOYHAT) {
		std::ostringstream x;
		x << "joystick" << ((key.joyhat >> 8) & 0xFF) << "hat" << (key.joyhat & 0xFF);
		if(key.joyhat & 0x10000)
			x << "n";
		if(key.joyhat & 0x40000)
			x << "s";
		if(key.joyhat & 0x20000)
			x << "e";
		if(key.joyhat & 0x80000)
			x << "w";
		ret = ret + x.str();
	} else {
		ret = ret + "<invalid>";
	}
	return ret;
}

std::string keymapper_helper_sdl::print_key_info(keymapper_helper_sdl::keysymbol key) throw(std::bad_alloc)
{
	std::string ret;
	if(key.device == SDL_DEV_KEYBOARD && key.keyboard.mod) {
		ret = ret + "Modifiers: ";
		unsigned mod = mod_key(key);
		struct modifier* m = modifiers_table;
		while(m->name) {
			if((mod & m->asmodifier) == m->asmodifier) {
				ret = ret + m->name + " ";
			}
			m++;
		}
		ret = ret + "\n";
	}
	if(key.device == SDL_DEV_KEYBOARD) {
		{
			std::ostringstream x;
			x << static_cast<unsigned>(key.keyboard.scancode);
			ret = ret + "Name: key" + x.str() + "\n";
		}
		struct key* k = keys_table;
		while(k->name) {
			if(key.keyboard.sym == k->symbol) {
				ret = ret + "Name: " + k->name + "\n";
				break;
			}
			k++;
		}
	} else if(key.device == SDL_DEV_JOYAXIS) {
		std::ostringstream x;
		x << "joystick" << ((key.joyaxis >> 8) & 0xFF) << "axis" << (key.joyaxis & 0xFF);
		x << ((key.joyaxis & 0x10000) ? '+' : '-');
		ret + ret + "Name: " + x.str() + "\n";
	} else if(key.device == SDL_DEV_JOYBUTTON) {
		std::ostringstream x;
		x << "joystick" << ((key.joybutton >> 8) & 0xFF) << "button" << (key.joybutton & 0xFF);
		ret + ret + "Name: " + x.str() + "\n";
	} else if(key.device == SDL_DEV_JOYHAT) {
		std::ostringstream x;
		x << "joystick" << ((key.joyhat >> 8) & 0xFF) << "hat" << (key.joyhat & 0xFF);
		if(key.joyhat & 0x10000)
			x << "n";
		if(key.joyhat & 0x20000)
			x << "e";
		if(key.joyhat & 0x40000)
			x << "s";
		if(key.joyhat & 0x80000)
			x << "w";
		ret + ret + "Name: " + x.str() + "\n";
	}
	return ret;
}


extern uint32_t fontdata[];


namespace
{
	uint32_t mouse_mask = 0;
	uint32_t vc_xoffset;
	uint32_t vc_yoffset;
	uint32_t vc_hscl = 1;
	uint32_t vc_vscl = 1;
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
	std::map<std::string, std::string> emustatus;
	std::map<uint64_t, std::string> messagebuffer;
	uint64_t messagebuffer_next_seq;
	uint64_t messagebuffer_first_seq;
	uint64_t messagebuffer_first_show;
	bool console_mode;
	uint32_t maxmessages;
	std::list<std::string> commandhistory;
	std::list<std::string>::iterator commandhistory_itr;
	screen* current_screen;
	keymapper<keymapper_helper_sdl> mapper;
	SDL_Surface* hwsurf;
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


namespace
{
	const size_t audiobuf_size = 8192;
	uint16_t audiobuf[audiobuf_size];
	volatile size_t audiobuf_get = 0;
	volatile size_t audiobuf_put = 0;
	uint64_t sampledup_ctr = 0;
	uint64_t sampledup_inc = 0;
	uint64_t sampledup_mod = 1;
	Uint16 format = AUDIO_S16SYS;
	bool stereo = true;
	bool sound_enabled = true;

	void calculate_sampledup(uint32_t real_rate)
	{
		sampledup_ctr = 0;
		sampledup_inc = 64081;
		sampledup_mod = 2 * real_rate + 64081;
	}

	void audiocb(void* dummy, Uint8* stream, int len)
	{
		static uint16_t lprev = 32768;
		static uint16_t rprev = 32768;
		if(!sound_enabled)
			lprev = rprev = 32768;
		uint16_t bias = (format == AUDIO_S8 || format == AUDIO_S16LSB || format == AUDIO_S16MSB || format ==
			AUDIO_S16SYS) ? 32768 : 0;
		while(len > 0) {
			uint16_t l, r;
			if(audiobuf_get == audiobuf_put) {
				l = lprev;
				r = rprev;
			} else {
				l = lprev = audiobuf[audiobuf_get++];
				r = rprev = audiobuf[audiobuf_get++];
				if(audiobuf_get == audiobuf_size)
					audiobuf_get = 0;
			}
			if(!stereo)
				l = l / 2 + r / 2;
			if(format == AUDIO_U8 || format == AUDIO_S8) {
				stream[0] = (l - bias) >> 8;
				if(stereo)
					stream[1] = (r - bias) >> 8;
				stream += (stereo ? 2 : 1);
				len -= (stereo ? 2 : 1);
			} else if(format == AUDIO_S16SYS || format == AUDIO_U16SYS) {
				reinterpret_cast<uint16_t*>(stream)[0] = (l - bias);
				if(stereo)
					reinterpret_cast<int16_t*>(stream)[1] = (r - bias);
				stream += (stereo ? 4 : 2);
				len -= (stereo ? 4 : 2);
			} else if(format == AUDIO_S16LSB || format == AUDIO_U16LSB) {
				stream[0] = (l - bias);
				stream[1] = (l - bias) >> 8;
				if(stereo) {
					stream[2] = (r - bias);
					stream[3] = (r - bias) >> 8;
				}
				stream += (stereo ? 4 : 2);
				len -= (stereo ? 4 : 2);
			} else if(format == AUDIO_S16MSB || format == AUDIO_U16MSB) {
				stream[1] = (l - bias);
				stream[0] = (l - bias) >> 8;
				if(stereo) {
					stream[3] = (r - bias);
					stream[2] = (r - bias) >> 8;
				}
				stream += (stereo ? 4 : 2);
				len -= (stereo ? 4 : 2);
			}
		}
	}

	void identify()
	{
		state = WINSTATE_IDENTIFY;
		window::message("Press key to identify.");
		window::notify_screen_update();
		window::poll_inputs();
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
		for(auto si = s.begin(); si != s.end(); si++) {
			uint32_t old_x = pos_x;
			uint32_t curstart = 16;
			if(c == hilite_pos && hilite_mode == 1)
				curstart = 14;
			if(c == hilite_pos && hilite_mode == 2)
				curstart = 0;
			auto g = find_glyph(*si, pos_x, pos_y, 0, pos_x, pos_y);
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
			for(uint32_t i = pos_x; i < maxwidth; i++) {
				ptr[i] = ((i - pos_x) < 8 && j >= curstart) ? 0xFFFFFFFFU : 0;
			}
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
		for(auto i = s2.begin(); i != s2.end(); i++) {
			auto g = find_glyph(*i, pos_x, pos_y, 0, pos_x, pos_y);
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
		SDL_LockSurface(surf);
		for(uint32_t j = y1 - 6; j < y2 + 6; j++)
			memset(reinterpret_cast<uint8_t*>(surf->pixels) + j * surf->pitch + 4 * (x1 - 6), 0,
				4 * (x2 - x1 + 12));
		uint32_t bordercolor = (128 << surf->format->Gshift) | (255 << surf->format->Rshift);
		draw_rectangle(reinterpret_cast<uint8_t*>(surf->pixels), surf->pitch, x1 - 4, y1 - 4, x2 + 4, y2 + 4,
			bordercolor, 2);

		pos_x = 0;
		pos_y = 0;
		for(auto i = s2.begin(); i != s2.end(); i++) {
			uint32_t ox = pos_x;
			uint32_t oy = pos_y;
			auto g = find_glyph(*i, pos_x, pos_y, 0, pos_x, pos_y);
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
		alarm(WATCHDOG_TIMEOUT);
		if(e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE && e.key.keysym.mod == (KMOD_LCTRL |
			KMOD_LALT))
			exit(1);
		if(e.type == SDL_USEREVENT && e.user.code == 0) {
			if(screen_is_dirty)
				window::notify_screen_update();
		}
		SDLKey key;
		get_ticks_msec();
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
			command::invokeC("quit-emulator");
			state = WINSTATE_NORMAL;
			return;
		}

		switch(state) {
		case WINSTATE_NORMAL:
			if(e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
				int32_t xc = e.button.x;
				int32_t yc = e.button.y;
				xc = (xc - 6 - vc_xoffset) / vc_hscl;
				yc = (yc - 6 - vc_yoffset) / vc_vscl;
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
				{
					std::ostringstream x;
					x << "mouse_button " << xc << " " << yc << " " << mouse_mask;
					command::invokeC(x.str());
				}
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
				return;
			}
			{
				bool more = true;
				{
					auto i = keymapper_helper_sdl::translate_event(e);
					std::string cmd;
					if(i.symbol.device != SDL_DEV_NONE)
						cmd = mapper.map(i.symbol, i.polarity);
					if(cmd != "")
						command::invokeC(cmd);
					more = i.more;
				}
				return;
			}
			break;
		case WINSTATE_MODAL:
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
			auto i = keymapper_helper_sdl::translate_event(e);
			if(!i.polarity && i.symbol.device != SDL_DEV_NONE)
				window::modal_message(keymapper_helper_sdl::print_key_info(i.symbol), false);
			break;
		};
	}
}

void window::init()
{
	signal(SIGALRM, sigalrm_handler);
	alarm(WATCHDOG_TIMEOUT);
	system_log.open("lsnes.log", std::ios_base::out | std::ios_base::app);
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes started at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	if(!sdl_init) {
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER);
		SDL_EnableUNICODE(true);
		sdl_init = true;
		tid = SDL_AddTimer(MIN_UPDATE_TIME, timer_cb, NULL);
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
	messagebuffer_next_seq = 0;
	messagebuffer_first_seq = 0;
	messagebuffer_first_show = 0;
	console_mode = false;
	maxmessages = MAXMESSAGES;

	notify_screen_update();
	std::string windowname = "lsnes-" + lsnes_version + "[" + bsnes_core_version + "]";
	SDL_WM_SetCaption(windowname.c_str(), "lsnes");

	SDL_AudioSpec* desired = new SDL_AudioSpec();
	SDL_AudioSpec* obtained = new SDL_AudioSpec();

	desired->freq = 44100;
	desired->format = AUDIO_S16SYS;
	desired->channels = 2;
	desired->samples = 8192;
	desired->callback = audiocb;
	desired->userdata = NULL;

	if(SDL_OpenAudio(desired, obtained) < 0) {
		message("Audio can't be initialized, audio playback disabled");
		return;
	}

	//Fill the parameters.
	calculate_sampledup(obtained->freq);
	format = obtained->format;
	stereo = (obtained->channels == 2);
	//GO!!!
	SDL_PauseAudio(0);
}

void window::quit()
{
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes shutting down at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log.close();
	if(sdl_init) {
		SDL_RemoveTimer(tid);
		SDL_Quit();
		sdl_init = false;
	}
}

bool window::modal_message(const std::string& msg, bool confirm) throw(std::bad_alloc)
{
	modconfirm = confirm;
	modmsg = msg;
	state = WINSTATE_MODAL;
	notify_screen_update();
	poll_inputs();
	bool ret = modconfirm;
	if(delayed_close_flag) {
		delayed_close_flag = false;
		command::invokeC("quit-emulator");
	}
	return ret;
}

void window::message(const std::string& msg) throw(std::bad_alloc)
{
	std::string msg2 = msg;
	bool locked_mode = (messagebuffer_next_seq - messagebuffer_first_show <= maxmessages) ;
	while(msg2 != "") {
		size_t s = msg2.find_first_of("\n");
		std::string forlog;
		if(s >= msg2.length()) {
			messagebuffer[messagebuffer_next_seq++] = (forlog = msg2);
			system_log << forlog << std::endl;
			break;
		} else {
			messagebuffer[messagebuffer_next_seq++] = (forlog = msg2.substr(0, s));
			system_log << forlog << std::endl;
			msg2 = msg2.substr(s + 1);
		}

	}
	if(locked_mode && messagebuffer_first_show + maxmessages < messagebuffer_next_seq)
		messagebuffer_first_show = messagebuffer_next_seq - maxmessages;

	while(messagebuffer.size() > MSGHISTORY) {
		messagebuffer.erase(messagebuffer_first_seq++);
		if(messagebuffer_first_show < messagebuffer_first_seq)
			messagebuffer_first_show = messagebuffer_first_seq;
	}
	notify_screen_update();
}

void window::set_main_surface(screen& scr) throw()
{
	current_screen = &scr;
	notify_screen_update(true);
}

void window::notify_screen_update(bool full) throw()
{
	uint64_t curtime = get_ticks_msec();
	if(!full && last_ui_update < curtime && last_ui_update + MIN_UPDATE_TIME > curtime) {
		screen_is_dirty = true;
		return;
	}
	last_ui_update = curtime;
	screen_is_dirty = false;

	try {
		std::ostringstream y;
		y << get_framerate();
		emustatus["FPS"] = y.str();
	} catch(...) {
	}

	std::string command_showas = decode_string(command_buf);
	uint32_t screen_w = 512;
	uint32_t screen_h = 448;
	if(current_screen && current_screen->width >= 512 && current_screen->height >= 448) {
		screen_w = current_screen->width;
		screen_h = current_screen->height;
	}
	uint32_t win_w = ((screen_w < 512) ? 512 : ((screen_w + 15) / 16 * 16)) + 278;
	uint32_t win_h = screen_h + MAXMESSAGES * 16 + 48;
	if(!hwsurf || static_cast<uint32_t>(hwsurf->w) != win_w || static_cast<uint32_t>(hwsurf->h) != win_h ||
		old_screen_w != screen_w || old_screen_h != screen_h || full) {
		//Create/Resize the window.
		if(!hwsurf || static_cast<uint32_t>(hwsurf->w) != win_w || static_cast<uint32_t>(hwsurf->h) != win_h) {
			SDL_Surface* hwsurf2 = SDL_SetVideoMode(win_w, win_h, 32, SDL_SWSURFACE | SDL_DOUBLEBUF);
			if(!hwsurf2) {
				//We are in too fucked up state to even print error as message.
				std::cout << "PANIC: Can't create/resize window: " << SDL_GetError() << std::endl;
				exit(1);
			}
			hwsurf = hwsurf2;
		}
		if(current_screen)
			current_screen->set_palette(hwsurf->format->Rshift, hwsurf->format->Gshift,
				hwsurf->format->Bshift);
		//Blank the screen and draw borders.
		SDL_LockSurface(hwsurf);
		memset(hwsurf->pixels, 0, win_h * hwsurf->pitch);
		uint32_t bordercolor = 255 << hwsurf->format->Gshift;
		if(console_mode) {
			draw_rectangle(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, 2, 2, win_w - 2,
				win_h - 28, bordercolor, 2);
			draw_rectangle(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, 2, win_h - 26,
				win_w - 2, win_h - 2, bordercolor, 2);
		} else {
			draw_rectangle(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, 2, 2, screen_w + 10,
				screen_h + 10, bordercolor, 2);
			draw_rectangle(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, screen_w + 12, 2,
				screen_w + 276, screen_h + 10, bordercolor, 2);
			draw_rectangle(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, 2, screen_h + 12,
				win_w - 2, screen_h + MAXMESSAGES * 16 + 20, bordercolor, 2);
			draw_rectangle(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, 2,
				screen_h + MAXMESSAGES * 16 + 22, win_w - 2, screen_h + MAXMESSAGES * 16 + 46,
				bordercolor, 2);
		}
		SDL_UnlockSurface(hwsurf);
		old_screen_w = screen_w;
		old_screen_h = screen_h;
	}
	SDL_LockSurface(hwsurf);
	if(!console_mode) {
		if(current_screen) {
			//Draw main screen (blanking background if needed).
			if(screen_w < current_screen->width || screen_h < current_screen->height)
				for(uint32_t i = 6; i < screen_h + 6; i++)
					memset(reinterpret_cast<uint8_t*>(hwsurf->pixels) + i * hwsurf->pitch + 24, 0,
					       4 * screen_w);
			for(uint32_t i = 0; i < current_screen->height; i++)
				memcpy(reinterpret_cast<uint8_t*>(hwsurf->pixels) + (i + 6) * hwsurf->pitch + 24,
					reinterpret_cast<uint8_t*>(current_screen->memory) + current_screen->pitch * i,
					4 * current_screen->width);
		} else {
			//Draw blank.
			for(uint32_t i = 6; i < screen_h + 6; i++)
				memset(reinterpret_cast<uint8_t*>(hwsurf->pixels) + i * hwsurf->pitch + 24, 0,
					4 * screen_w);
		}
		//Draw status.
		uint32_t status_x = screen_w + 16;
		uint32_t status_y = 6;
		for(auto i = emustatus.begin(); i != emustatus.end(); i++) {
			std::string msg = i->first + " " + i->second;
			draw_string(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, msg, status_x, status_y,
				256);
			status_y += 16;
		}
		while(status_y - 6 < screen_h / 16 * 16) {
			draw_string(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, "", status_x, status_y,
				256);
			status_y += 16;
		}
	}
	//Draw messages.
	uint32_t message_y;
	if(!console_mode)
		message_y = screen_h + 16;
	else
		message_y = 6;
	for(size_t j = 0; j < maxmessages; j++)
		try {
			std::ostringstream o;
			if(messagebuffer_first_show + j < messagebuffer_next_seq)
				o << (messagebuffer_first_show + j + 1) << ": "
					<< messagebuffer[messagebuffer_first_show + j];
			draw_string(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, o.str(), 6,
				message_y + 16 * j, win_w - 12);
		} catch(...) {
		}
	if(messagebuffer_next_seq - messagebuffer_first_show > maxmessages)
		try {
			draw_string(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, "--More--", win_w - 76,
				message_y + 16 * maxmessages - 16, 64);
		} catch(...) {
		}

	//Draw command_buf.
	uint32_t command_y = win_h - 22;
	try {
		if(state == WINSTATE_COMMAND)
			draw_command(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, command_showas,
				command_cursor / 4, 6, command_y, win_w - 12, command_overwrite);
		else
			draw_string(reinterpret_cast<uint8_t*>(hwsurf->pixels), hwsurf->pitch, "", 6, command_y,
				win_w - 12);
	} catch(...) {
	}
	//Draw modal dialog.
	if(state == WINSTATE_MODAL)
		try {
			draw_modal_dialog(hwsurf, modmsg, modconfirm);
		} catch(...) {
		}
	SDL_UnlockSurface(hwsurf);
	SDL_Flip(hwsurf);
}

void window::poll_inputs() throw(std::bad_alloc)
{
	SDL_Event e;
	while(1) {
		if(modal_return_flag) {
			modal_return_flag = false;
			return;
		}
		if(state == WINSTATE_NORMAL && !pause_active && !SDL_PollEvent(&e))
			break;
		else if(state == WINSTATE_NORMAL && !pause_active)
			do_event(e);
		else if(SDL_WaitEvent(&e))
			do_event(e);
	}
}

void window::bind(std::string mod, std::string modmask, std::string keyname, std::string cmd) throw(std::bad_alloc,
	std::runtime_error)
{
	mapper.bind(mod, modmask, keyname, cmd);
	if(modmask == "")
		message("Key " + keyname + " bound to '" + cmd + "'");
	else
		message("Key " + mod + "/" + modmask + " " + keyname + " bound to '" + cmd + "'");
}

void window::unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
	std::runtime_error)
{
	mapper.unbind(mod, modmask, keyname);
	if(modmask == "")
		message("Key " + keyname + " unbound");
	else
		message("Key " + mod + "/" + modmask + " " + keyname + " unbound");
}

std::map<std::string, std::string>& window::get_emustatus() throw()
{
	return emustatus;
}

void window::dumpbindings() throw(std::bad_alloc)
{
	mapper.dumpbindings();
}

void window::paused(bool enable) throw()
{
	pause_active = enable;
	notify_screen_update();
}

void window::sound_enable(bool enable) throw()
{
	sound_enabled = enable;
	SDL_PauseAudio(enable ? 0 : 1);
}

namespace
{
	function_ptr_command<const std::string&> enable_sound("enable-sound", "Enable/Disable sound",
		"Syntax: enable-sound <on/off>\nEnable or disable sound.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			std::string s = args;
			if(s == "on" || s == "true" || s == "1" || s == "enable" || s == "enabled")
				window::sound_enable(true);
			else if(s == "off" || s == "false" || s == "0" || s == "disable" || s == "disabled")
				window::sound_enable(false);
			else
				throw std::runtime_error("Bad sound setting");
		});

	function_ptr_command<> identify_key("identify-key", "Identify a key",
		"Syntax: identify-key\nIdentifies a (pseudo-)key.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			identify();
		});

	function_ptr_command<> scroll_up("scroll-up", "Scroll messages a page up",
		"Syntax: scroll-up\nScrolls message console backward one page.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(messagebuffer_first_show > maxmessages)
				messagebuffer_first_show -= maxmessages;
			else
				messagebuffer_first_show = 0;
			if(messagebuffer_first_show < messagebuffer_first_seq)
				messagebuffer_first_show = messagebuffer_first_seq;
			window::notify_screen_update();
		});

	function_ptr_command<> scroll_fullup("scroll-fullup", "Scroll messages to beginning",
		"Syntax: scroll-fullup\nScrolls message console to its beginning.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			messagebuffer_first_show = messagebuffer_first_seq;
			window::notify_screen_update();
		});

	function_ptr_command<> scroll_fulldown("scroll-fulldown", "Scroll messages to end",
		"Syntax: scroll-fulldown\nScrolls message console to its end.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(messagebuffer_next_seq < maxmessages)
				messagebuffer_first_show = 0;
			else
				messagebuffer_first_show = messagebuffer_next_seq - maxmessages;
			window::notify_screen_update();
		});

	function_ptr_command<> scrolldown("scroll-down", "Scroll messages a page down",
		"Syntax: scroll-up\nScrolls message console forward one page.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			messagebuffer_first_show += maxmessages;
			if(messagebuffer_next_seq < maxmessages)
				messagebuffer_first_show = 0;
			else if(messagebuffer_next_seq < messagebuffer_first_show + maxmessages)
				messagebuffer_first_show = messagebuffer_next_seq - maxmessages;
			window::notify_screen_update();
		});

	function_ptr_command<> toggle_console("toggle-console", "Toggle console between small and full window",
		"Syntax: toggle-console\nToggles console between small and large.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			console_mode = !console_mode;
			if(console_mode)
				maxmessages = hwsurf ? (hwsurf->h - 38) / 16 : 36;
			else
				maxmessages = MAXMESSAGES;
			if(messagebuffer_next_seq < maxmessages)
				messagebuffer_first_show = 0;
			else
				messagebuffer_first_show = messagebuffer_next_seq - maxmessages;
			window::notify_screen_update(true);
		});
}

void window::wait_msec(uint64_t msec) throw(std::bad_alloc)
{
	wait_canceled = false;
	uint64_t basetime =  get_ticks_msec();
	while(!wait_canceled) {
		if(msec > 10)
			SDL_Delay(10);
		else
			SDL_Delay(msec);
		SDL_Event e;
		while(SDL_PollEvent(&e))
			do_event(e);
		uint64_t passed = get_ticks_msec() - basetime;
		if(passed > msec)
			break;
	}
}

void window::fatal_error() throw()
{
	try {
		message("PANIC: Cannot continue, press ESC or close window to exit.");
		//Force redraw.
		notify_screen_update(true);
	} catch(...) {
		//Just crash.
		exit(1);
	}
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes paniced at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log.close();
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

uint64_t get_ticks_msec() throw()
{
	static uint64_t tickbase = 0;
	static Uint32 last_ticks = 0;
	Uint32 cur_ticks = SDL_GetTicks();
	if(last_ticks > cur_ticks)
		tickbase += 0x100000000ULL;
	last_ticks = cur_ticks;
	return tickbase + cur_ticks;
}

void window::cancel_wait() throw()
{
	wait_canceled = true;
}

void window::play_audio_sample(uint16_t left, uint16_t right) throw()
{
	sampledup_ctr += sampledup_inc;
	while(sampledup_ctr < sampledup_mod) {
		audiobuf[audiobuf_put++] = left;
		audiobuf[audiobuf_put++] = right;
		if(audiobuf_put == audiobuf_size)
			audiobuf_put = 0;
		sampledup_ctr += sampledup_inc;
	}
	sampledup_ctr -= sampledup_mod;
}

void window::set_window_compensation(uint32_t xoffset, uint32_t yoffset, uint32_t hscl, uint32_t vscl)
{
	vc_xoffset = xoffset;
	vc_yoffset = yoffset;
	vc_hscl = hscl;
	vc_vscl = vscl;
}
