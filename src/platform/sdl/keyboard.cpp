#include "core/window.hpp"
#include "platform/sdl/platform.hpp"

#include <map>

#include <SDL.h>

namespace
{
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
		const char* clazz;
		unsigned symbol;
	} keys_table[] = {
		{"backspace",		"editing",	SDLK_BACKSPACE		},
		{"tab",			"editing",	SDLK_TAB		},
		{"clear",		"editing",	SDLK_CLEAR		},
		{"return",		"editing",	SDLK_RETURN		},
		{"pause",		"special",	SDLK_PAUSE		},
		{"escape",		"editing",	SDLK_ESCAPE		},
		{"space",		"characters",	SDLK_SPACE		},
		{"exclaim",		"characters",	SDLK_EXCLAIM		},
		{"quotedbl",		"characters",	SDLK_QUOTEDBL		},
		{"hash",		"characters",	SDLK_HASH		},
		{"dollar",		"characters",	SDLK_DOLLAR		},
		{"ampersand",		"characters",	SDLK_AMPERSAND		},
		{"quote",		"characters",	SDLK_QUOTE		},
		{"leftparen",		"characters",	SDLK_LEFTPAREN		},
		{"rightparen",		"characters",	SDLK_RIGHTPAREN		},
		{"asterisk",		"characters",	SDLK_ASTERISK		},
		{"plus",		"characters",	SDLK_PLUS		},
		{"comma",		"characters",	SDLK_COMMA		},
		{"minus",		"characters",	SDLK_MINUS		},
		{"period",		"characters",	SDLK_PERIOD		},
		{"slash",		"characters",	SDLK_SLASH		},
		{"0",			"numeric",	SDLK_0			},
		{"1",			"numeric",	SDLK_1			},
		{"2",			"numeric",	SDLK_2			},
		{"3",			"numeric",	SDLK_3			},
		{"4",			"numeric",	SDLK_4			},
		{"5",			"numeric",	SDLK_5			},
		{"6",			"numeric",	SDLK_6			},
		{"7",			"numeric",	SDLK_7			},
		{"8",			"numeric",	SDLK_8			},
		{"9",			"numeric",	SDLK_9			},
		{"colon",		"characters",	SDLK_COLON		},
		{"semicolon",		"characters",	SDLK_SEMICOLON		},
		{"less",		"characters",	SDLK_LESS		},
		{"equals",		"characters",	SDLK_EQUALS		},
		{"greater",		"characters",	SDLK_GREATER		},
		{"question",		"characters",	SDLK_QUESTION		},
		{"at",			"characters",	SDLK_AT			},
		{"leftbracket",		"characters",	SDLK_LEFTBRACKET	},
		{"backslash",		"characters",	SDLK_BACKSLASH		},
		{"rightbracket",	"characters",	SDLK_RIGHTBRACKET	},
		{"caret",		"characters",	SDLK_CARET		},
		{"underscore",		"characters",	SDLK_UNDERSCORE		},
		{"backquote",		"characters",	SDLK_BACKQUOTE		},
		{"a",			"alphabetic",	SDLK_a			},
		{"b",			"alphabetic",	SDLK_b			},
		{"c",			"alphabetic",	SDLK_c			},
		{"d",			"alphabetic",	SDLK_d			},
		{"e",			"alphabetic",	SDLK_e			},
		{"f",			"alphabetic",	SDLK_f			},
		{"g",			"alphabetic",	SDLK_g			},
		{"h",			"alphabetic",	SDLK_h			},
		{"i",			"alphabetic",	SDLK_i			},
		{"j",			"alphabetic",	SDLK_j			},
		{"k",			"alphabetic",	SDLK_k			},
		{"l",			"alphabetic",	SDLK_l			},
		{"m",			"alphabetic",	SDLK_m			},
		{"n",			"alphabetic",	SDLK_n			},
		{"o",			"alphabetic",	SDLK_o			},
		{"p",			"alphabetic",	SDLK_p			},
		{"q",			"alphabetic",	SDLK_q			},
		{"r",			"alphabetic",	SDLK_r			},
		{"s",			"alphabetic",	SDLK_s			},
		{"t",			"alphabetic",	SDLK_t			},
		{"u",			"alphabetic",	SDLK_u			},
		{"v",			"alphabetic",	SDLK_v			},
		{"w",			"alphabetic",	SDLK_w			},
		{"x",			"alphabetic",	SDLK_x			},
		{"y",			"alphabetic",	SDLK_y			},
		{"z",			"alphabetic",	SDLK_z			},
		{"delete",		"editing",	SDLK_DELETE		},
		{"world_0",		"international",SDLK_WORLD_0		},
		{"world_1",		"international",SDLK_WORLD_1		},
		{"world_2",		"international",SDLK_WORLD_2		},
		{"world_3",		"international",SDLK_WORLD_3		},
		{"world_4",		"international",SDLK_WORLD_4		},
		{"world_5",		"international",SDLK_WORLD_5		},
		{"world_6",		"international",SDLK_WORLD_6		},
		{"world_7",		"international",SDLK_WORLD_7		},
		{"world_8",		"international",SDLK_WORLD_8		},
		{"world_9",		"international",SDLK_WORLD_9		},
		{"world_10",		"international",SDLK_WORLD_10		},
		{"world_11",		"international",SDLK_WORLD_11		},
		{"world_12",		"international",SDLK_WORLD_12		},
		{"world_13",		"international",SDLK_WORLD_13		},
		{"world_14",		"international",SDLK_WORLD_14		},
		{"world_15",		"international",SDLK_WORLD_15		},
		{"world_16",		"international",SDLK_WORLD_16		},
		{"world_17",		"international",SDLK_WORLD_17		},
		{"world_18",		"international",SDLK_WORLD_18		},
		{"world_19",		"international",SDLK_WORLD_19		},
		{"world_20",		"international",SDLK_WORLD_20		},
		{"world_21",		"international",SDLK_WORLD_21		},
		{"world_22",		"international",SDLK_WORLD_22		},
		{"world_23",		"international",SDLK_WORLD_23		},
		{"world_24",		"international",SDLK_WORLD_24		},
		{"world_25",		"international",SDLK_WORLD_25		},
		{"world_26",		"international",SDLK_WORLD_26		},
		{"world_27",		"international",SDLK_WORLD_27		},
		{"world_28",		"international",SDLK_WORLD_28		},
		{"world_29",		"international",SDLK_WORLD_29		},
		{"world_30",		"international",SDLK_WORLD_30		},
		{"world_31",		"international",SDLK_WORLD_31		},
		{"world_32",		"international",SDLK_WORLD_32		},
		{"world_33",		"international",SDLK_WORLD_33		},
		{"world_34",		"international",SDLK_WORLD_34		},
		{"world_35",		"international",SDLK_WORLD_35		},
		{"world_36",		"international",SDLK_WORLD_36		},
		{"world_37",		"international",SDLK_WORLD_37		},
		{"world_38",		"international",SDLK_WORLD_38		},
		{"world_39",		"international",SDLK_WORLD_39		},
		{"world_40",		"international",SDLK_WORLD_40		},
		{"world_41",		"international",SDLK_WORLD_41		},
		{"world_42",		"international",SDLK_WORLD_42		},
		{"world_43",		"international",SDLK_WORLD_43		},
		{"world_44",		"international",SDLK_WORLD_44		},
		{"world_45",		"international",SDLK_WORLD_45		},
		{"world_46",		"international",SDLK_WORLD_46		},
		{"world_47",		"international",SDLK_WORLD_47		},
		{"world_48",		"international",SDLK_WORLD_48		},
		{"world_49",		"international",SDLK_WORLD_49		},
		{"world_50",		"international",SDLK_WORLD_50		},
		{"world_51",		"international",SDLK_WORLD_51		},
		{"world_52",		"international",SDLK_WORLD_52		},
		{"world_53",		"international",SDLK_WORLD_53		},
		{"world_54",		"international",SDLK_WORLD_54		},
		{"world_55",		"international",SDLK_WORLD_55		},
		{"world_56",		"international",SDLK_WORLD_56		},
		{"world_57",		"international",SDLK_WORLD_57		},
		{"world_58",		"international",SDLK_WORLD_58		},
		{"world_59",		"international",SDLK_WORLD_59		},
		{"world_60",		"international",SDLK_WORLD_60		},
		{"world_61",		"international",SDLK_WORLD_61		},
		{"world_62",		"international",SDLK_WORLD_62		},
		{"world_63",		"international",SDLK_WORLD_63		},
		{"world_64",		"international",SDLK_WORLD_64		},
		{"world_65",		"international",SDLK_WORLD_65		},
		{"world_66",		"international",SDLK_WORLD_66		},
		{"world_67",		"international",SDLK_WORLD_67		},
		{"world_68",		"international",SDLK_WORLD_68		},
		{"world_69",		"international",SDLK_WORLD_69		},
		{"world_70",		"international",SDLK_WORLD_70		},
		{"world_71",		"international",SDLK_WORLD_71		},
		{"world_72",		"international",SDLK_WORLD_72		},
		{"world_73",		"international",SDLK_WORLD_73		},
		{"world_74",		"international",SDLK_WORLD_74		},
		{"world_75",		"international",SDLK_WORLD_75		},
		{"world_76",		"international",SDLK_WORLD_76		},
		{"world_77",		"international",SDLK_WORLD_77		},
		{"world_78",		"international",SDLK_WORLD_78		},
		{"world_79",		"international",SDLK_WORLD_79		},
		{"world_80",		"international",SDLK_WORLD_80		},
		{"world_81",		"international",SDLK_WORLD_81		},
		{"world_82",		"international",SDLK_WORLD_82		},
		{"world_83",		"international",SDLK_WORLD_83		},
		{"world_84",		"international",SDLK_WORLD_84		},
		{"world_85",		"international",SDLK_WORLD_85		},
		{"world_86",		"international",SDLK_WORLD_86		},
		{"world_87",		"international",SDLK_WORLD_87		},
		{"world_88",		"international",SDLK_WORLD_88		},
		{"world_89",		"international",SDLK_WORLD_89		},
		{"world_90",		"international",SDLK_WORLD_90		},
		{"world_91",		"international",SDLK_WORLD_91		},
		{"world_92",		"international",SDLK_WORLD_92		},
		{"world_93",		"international",SDLK_WORLD_93		},
		{"world_94",		"international",SDLK_WORLD_94		},
		{"world_95",		"international",SDLK_WORLD_95		},
		{"kp0",			"numeric",	SDLK_KP0		},
		{"kp1",			"numeric",	SDLK_KP1		},
		{"kp2",			"numeric",	SDLK_KP2		},
		{"kp3",			"numeric",	SDLK_KP3		},
		{"kp4",			"numeric",	SDLK_KP4		},
		{"kp5",			"numeric",	SDLK_KP5		},
		{"kp6",			"numeric",	SDLK_KP6		},
		{"kp7",			"numeric",	SDLK_KP7		},
		{"kp8",			"numeric",	SDLK_KP8		},
		{"kp9",			"numeric",	SDLK_KP9		},
		{"kp_period",		"characters",	SDLK_KP_PERIOD		},
		{"kp_divide",		"characters",	SDLK_KP_DIVIDE		},
		{"kp_multiply",		"characters",	SDLK_KP_MULTIPLY	},
		{"kp_minus",		"characters",	SDLK_KP_MINUS		},
		{"kp_plus",		"characters",	SDLK_KP_PLUS		},
		{"kp_enter",		"characters",	SDLK_KP_ENTER		},
		{"kp_equals",		"characters",	SDLK_KP_EQUALS		},
		{"up",			"editing",	SDLK_UP			},
		{"down",		"editing",	SDLK_DOWN		},
		{"right",		"editing",	SDLK_RIGHT		},
		{"left",		"editing",	SDLK_LEFT		},
		{"insert",		"editing",	SDLK_INSERT		},
		{"home",		"editing",	SDLK_HOME		},
		{"end",			"editing",	SDLK_END		},
		{"pageup",		"editing",	SDLK_PAGEUP		},
		{"pagedown",		"editing",	SDLK_PAGEDOWN		},
		{"f1",			"F-keys",	SDLK_F1			},
		{"f2",			"F-keys",	SDLK_F2			},
		{"f3",			"F-keys",	SDLK_F3			},
		{"f4",			"F-keys",	SDLK_F4			},
		{"f5",			"F-keys",	SDLK_F5			},
		{"f6",			"F-keys",	SDLK_F6			},
		{"f7",			"F-keys",	SDLK_F7			},
		{"f8",			"F-keys",	SDLK_F8			},
		{"f9",			"F-keys",	SDLK_F9			},
		{"f10",			"F-keys",	SDLK_F10		},
		{"f11",			"F-keys",	SDLK_F11		},
		{"f12",			"F-keys",	SDLK_F12		},
		{"f13",			"F-keys",	SDLK_F13		},
		{"f14",			"F-keys",	SDLK_F14		},
		{"f15",			"F-keys",	SDLK_F15		},
		{"numlock",		"locks",	SDLK_NUMLOCK		},
		{"capslock",		"locks",	SDLK_CAPSLOCK		},
		{"scrollock",		"locks",	SDLK_SCROLLOCK		},
		{"rshift",		"modifiers",	SDLK_RSHIFT		},
		{"lshift",		"modifiers",	SDLK_LSHIFT		},
		{"rctrl",		"modifiers",	SDLK_RCTRL		},
		{"lctrl",		"modifiers",	SDLK_LCTRL		},
		{"ralt",		"modifiers",	SDLK_RALT		},
		{"lalt",		"modifiers",	SDLK_LALT		},
		{"rmeta",		"modifiers",	SDLK_RMETA		},
		{"lmeta",		"modifiers",	SDLK_LMETA		},
		{"lsuper",		"modifiers",	SDLK_LSUPER		},
		{"rsuper",		"modifiers",	SDLK_RSUPER		},
		{"mode",		"modifiers",	SDLK_MODE		},
		{"compose",		"modifiers",	SDLK_COMPOSE		},
		{"help",		"special",	SDLK_HELP		},
		{"print",		"special",	SDLK_PRINT		},
		{"sysreq",		"special",	SDLK_SYSREQ		},
		{"break",		"special",	SDLK_BREAK		},
		{"menu",		"special",	SDLK_MENU		},
		{"power",		"special",	SDLK_POWER		},
		{"euro",		"characters",	SDLK_EURO		},
		{"undo",		"special",	SDLK_UNDO		},
		{NULL,			0			}
	};

	std::map<unsigned, keyboard_modifier*> supported_modifiers;
	std::map<unsigned, keyboard_key_key*> scancodekeys;
	std::map<unsigned, keyboard_key_key*> symbolkeys;
}

unsigned translate_sdl_key(SDL_Event& e, keypress& k)
{
	if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
		keyboard_modifier_set modifiers;
		short value = (e.type == SDL_KEYDOWN) ? 1 : 0;
		SDL_KeyboardEvent& ke = e.key;
		SDL_keysym sym = ke.keysym;
		uint8_t scancode = sym.scancode;
		unsigned symbol = sym.sym;
		for(auto l : supported_modifiers)
			if(sym.mod & l.first)
				modifiers.add(*l.second);
		if(symbolkeys.count(symbol))
			k = keypress(modifiers, *scancodekeys[scancode], *symbolkeys[symbol], value);
		else
			k = keypress(modifiers, *scancodekeys[scancode], value);
		return 1;
	}
	return 0;
}


uint32_t get_command_edit_operation(SDL_Event& e, bool enable)
{
	if(!enable)
		return SPECIAL_NOOP;
	uint32_t press = (e.type == SDL_KEYDOWN) ? PRESSED_MASK : 0;
	//Everything except keyboard is no-op.
	if(e.type != SDL_KEYDOWN && e.type != SDL_KEYUP)
		return SPECIAL_NOOP;
	//Keys with CTRL held.
	if(e.key.keysym.mod & KMOD_CTRL) {
		switch(e.key.keysym.sym) {
		case 'A':	case 'a':
			return SPECIAL_HOME | press;
		case 'B':	case 'b':
			return SPECIAL_LEFT | press;
		case 'D':	case 'd':
			return SPECIAL_DELETE | press;
		case 'E':	case 'e':
			return SPECIAL_END | press;
		case 'F':	case 'f':
			return SPECIAL_RIGHT | press;
		case 'P':	case 'p':
			return SPECIAL_UP | press;
		case 'N':	case 'n':
			return SPECIAL_DOWN | press;
		case SDLK_LEFT:
			return SPECIAL_LEFT_WORD | press;
		case SDLK_RIGHT:
			return SPECIAL_RIGHT_WORD | press;
		case 'W':	case 'w':
			return SPECIAL_DELETE_WORD | press;
		default:
			return SPECIAL_NOOP;
		};
	}
	//Keys with ALT held.
	if(e.key.keysym.mod & KMOD_ALT) {
		switch(e.key.keysym.sym) {
		case 'B':	case 'b':
			return SPECIAL_LEFT_WORD | press;
		case 'D':	case 'd':
			return SPECIAL_DELETE_WORD | press;
		case 'F':	case 'f':
			return SPECIAL_RIGHT_WORD | press;
		default:
			return SPECIAL_NOOP;
		};
	}
	//Escape is special.
	if(e.key.keysym.sym == SDLK_ESCAPE)
		return (e.type == SDL_KEYUP) ? SPECIAL_NAK : SPECIAL_NOOP;
	//Return/Enter is special.
	if(e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER)
		return (e.type == SDL_KEYUP) ? SPECIAL_ACK : SPECIAL_NOOP;
	//Other special keys.
	switch(e.key.keysym.sym) {
	case SDLK_BACKSPACE:
		return SPECIAL_BACKSPACE | press;
	case SDLK_INSERT:
	case SDLK_KP0:
		return SPECIAL_INSERT | press;
	case SDLK_DELETE:
	case SDLK_KP_PERIOD:
		return SPECIAL_DELETE | press;
	case SDLK_HOME:
	case SDLK_KP7:
		return SPECIAL_HOME | press;
	case SDLK_END:
	case SDLK_KP1:
		return SPECIAL_END | press;
	case SDLK_PAGEUP:
	case SDLK_KP9:
		return SPECIAL_PGUP | press;
	case SDLK_PAGEDOWN:
	case SDLK_KP3:
		return SPECIAL_PGDN | press;
	case SDLK_UP:
	case SDLK_KP8:
		return SPECIAL_UP | press;
	case SDLK_DOWN:
	case SDLK_KP2:
		return SPECIAL_DOWN | press;
	case SDLK_LEFT:
	case SDLK_KP4:
		return SPECIAL_LEFT | press;
	case SDLK_RIGHT:
	case SDLK_KP6:
		return SPECIAL_RIGHT | press;
	case SDLK_KP5:
		return SPECIAL_NOOP;
	};
	return e.key.keysym.unicode | press;
}

void init_sdl_keys()
{
	struct sdl_modifier* m = modifiers_table;
	while(m->name) {
		keyboard_modifier* m2;
		if(m->linkname)
			m2 = new keyboard_modifier(lsnes_kbd, m->name, m->linkname);
		else
			m2 = new keyboard_modifier(lsnes_kbd, m->name);
		if(m->sdlvalue)
			supported_modifiers[m->sdlvalue] = m2;
		m++;
	}
	struct sdl_key* k = keys_table;
	while(k->name) {
		symbolkeys[k->symbol] = new keyboard_key_key(lsnes_kbd, k->name, k->clazz);
		k++;
	}
	for(unsigned i = 0; i < 256; i++) {
		std::ostringstream x;
		x << "key" << i;
		scancodekeys[i] = new keyboard_key_key(lsnes_kbd, x.str(), "scancode");
	}
}

void deinit_sdl_keys()
{
	for(auto i : supported_modifiers)
		delete i.second;
	for(auto i : scancodekeys)
		delete i.second;
	for(auto i : symbolkeys)
		delete i.second;
	supported_modifiers.clear();
	scancodekeys.clear();
	symbolkeys.clear();
}
