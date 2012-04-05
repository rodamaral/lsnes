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
}

unsigned translate_sdl_key(SDL_Event& e, keypress& k)
{
	if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
		modifier_set modifiers;
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
