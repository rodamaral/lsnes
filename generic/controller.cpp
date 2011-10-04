#include "controller.hpp"
#include "lsnes.hpp"
#include "mainloop.hpp"
#include <map>
#include <sstream>
#include "window.hpp"
#include "command.hpp"
#include "framebuffer.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#define BUTTON_LEFT 0		//Gamepad
#define BUTTON_RIGHT 1		//Gamepad
#define BUTTON_UP 2		//Gamepad
#define BUTTON_DOWN 3		//Gamepad
#define BUTTON_A 4		//Gamepad
#define BUTTON_B 5		//Gamepad
#define BUTTON_X 6		//Gamepad
#define BUTTON_Y 7		//Gamepad
#define BUTTON_L 8		//Gamepad & Mouse
#define BUTTON_R 9		//Gamepad & Mouse
#define BUTTON_SELECT 10	//Gamepad
#define BUTTON_START 11		//Gamepad & Justifier
#define BUTTON_TRIGGER 12	//Superscope.
#define BUTTON_CURSOR 13	//Superscope & Justifier
#define BUTTON_PAUSE 14		//Superscope
#define BUTTON_TURBO 15		//Superscope

namespace
{
	porttype_t porttypes[2];
	int analog_indices[3] = {-1, -1, -1};
	bool analog_is_mouse[3];
	//Current controls.
	controls_t curcontrols;
	controls_t autoheld_controls;
	std::vector<controls_t> autofire_pattern;

	void update_analog_indices() throw()
	{
		int i = 0;
		for(unsigned j = 0; j < sizeof(analog_indices) / sizeof(analog_indices[0]); j++)
			analog_indices[j] = -1;
		for(unsigned j = 0; j < 8; j++) {
			devicetype_t d = controller_type_by_logical(j);
			switch(d) {
			case DT_NONE:
			case DT_GAMEPAD:
				break;
			case DT_MOUSE:
				analog_is_mouse[i] = true;
				analog_indices[i++] = j;
				break;
			case DT_SUPERSCOPE:
			case DT_JUSTIFIER:
				analog_is_mouse[i] = false;
				analog_indices[i++] = j;
				break;
			}
		}
	}

	std::map<std::string, std::pair<unsigned, unsigned>> buttonmap;

	const char* buttonnames[] = {
		"left", "right", "up", "down", "A", "B", "X", "Y", "L", "R", "select", "start", "trigger", "cursor",
		"pause", "turbo"
	};

	void init_buttonmap()
	{
		static int done = 0;
		if(done)
			return;
		for(unsigned i = 0; i < 8; i++)
			for(unsigned j = 0; j < sizeof(buttonnames) / sizeof(buttonnames[0]); j++) {
				std::ostringstream x;
				x << (i + 1) << buttonnames[j];
				buttonmap[x.str()] = std::make_pair(i, j);
			}
		done = 1;
	}

	//Do button action.
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor, controls_t& c)
	{
		enum devicetype_t p = controller_type_by_logical(ui_id);
		int x = controller_index_by_logical(ui_id);
		int bid = -1;
		switch(p) {
		case DT_NONE:
			window::out() << "No such controller #" << (ui_id + 1) << std::endl;
			return;
		case DT_GAMEPAD:
			switch(button) {
			case BUTTON_UP: 	bid = SNES_DEVICE_ID_JOYPAD_UP; break;
			case BUTTON_DOWN:	bid = SNES_DEVICE_ID_JOYPAD_DOWN; break;
			case BUTTON_LEFT:	bid = SNES_DEVICE_ID_JOYPAD_LEFT; break;
			case BUTTON_RIGHT:	bid = SNES_DEVICE_ID_JOYPAD_RIGHT; break;
			case BUTTON_A:		bid = SNES_DEVICE_ID_JOYPAD_A; break;
			case BUTTON_B:		bid = SNES_DEVICE_ID_JOYPAD_B; break;
			case BUTTON_X:		bid = SNES_DEVICE_ID_JOYPAD_X; break;
			case BUTTON_Y:		bid = SNES_DEVICE_ID_JOYPAD_Y; break;
			case BUTTON_L:		bid = SNES_DEVICE_ID_JOYPAD_L; break;
			case BUTTON_R:		bid = SNES_DEVICE_ID_JOYPAD_R; break;
			case BUTTON_SELECT:	bid = SNES_DEVICE_ID_JOYPAD_SELECT; break;
			case BUTTON_START:	bid = SNES_DEVICE_ID_JOYPAD_START; break;
			default:
				window::out() << "Invalid button for gamepad" << std::endl;
				return;
			};
			break;
		case DT_MOUSE:
			switch(button) {
			case BUTTON_L:		bid = SNES_DEVICE_ID_MOUSE_LEFT; break;
			case BUTTON_R:		bid = SNES_DEVICE_ID_MOUSE_RIGHT; break;
			default:
				window::out() << "Invalid button for mouse" << std::endl;
				return;
			};
			break;
		case DT_JUSTIFIER:
			switch(button) {
			case BUTTON_START:	bid = SNES_DEVICE_ID_JUSTIFIER_START; break;
			case BUTTON_TRIGGER:	bid = SNES_DEVICE_ID_JUSTIFIER_TRIGGER; break;
			default:
				window::out() << "Invalid button for justifier" << std::endl;
				return;
			};
			break;
		case DT_SUPERSCOPE:
			switch(button) {
			case BUTTON_TRIGGER:	bid = SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER; break;
			case BUTTON_CURSOR:	bid = SNES_DEVICE_ID_SUPER_SCOPE_CURSOR; break;
			case BUTTON_PAUSE:	bid = SNES_DEVICE_ID_SUPER_SCOPE_PAUSE; break;
			case BUTTON_TURBO:	bid = SNES_DEVICE_ID_SUPER_SCOPE_TURBO; break;
			default:
				window::out() << "Invalid button for superscope" << std::endl;
				return;
			};
			break;
		};
		if(do_xor)
			c((x & 4) ? 1 : 0, x & 3, bid) ^= newstate;
		else
			c((x & 4) ? 1 : 0, x & 3, bid) = newstate;
	}

	//Do button action.
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor = false)
	{
		if(do_xor)
			do_button_action(ui_id, button, newstate, do_xor, autoheld_controls);
		else
			do_button_action(ui_id, button, newstate, do_xor, curcontrols);
	}

	function_ptr_command<tokensplitter&> autofire("autofire", "Set autofire pattern",
		"Syntax: autofire <buttons|->...\nSet autofire pattern\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			if(!t)
				throw std::runtime_error("Need at least one frame for autofire");
			std::vector<controls_t> new_autofire_pattern;
			init_buttonmap();
			while(t) {
				std::string fpattern = t;
				if(fpattern == "-")
					new_autofire_pattern.push_back(controls_t());
				else {
					controls_t c;
					while(fpattern != "") {
						size_t split = fpattern.find_first_of(",");
						std::string button = fpattern;
						std::string rest;
						if(split < fpattern.length()) {
							button = fpattern.substr(0, split);
							rest = fpattern.substr(split + 1);
						}
						if(!buttonmap.count(button)) {
							std::ostringstream x;
							x << "Invalid button '" << button << "'";
							throw std::runtime_error(x.str());
						}
						auto g = buttonmap[button];
						do_button_action(g.first, g.second, 1, false, c);
						fpattern = rest;
					}
					new_autofire_pattern.push_back(c);
				}
			}
			autofire_pattern = new_autofire_pattern;
		});

	class button_action : public command
	{
	public:
		button_action(const std::string& cmd, int _type, unsigned _controller, std::string _button)
			throw(std::bad_alloc)
			: command(cmd)
		{
			commandn = cmd;
			type = _type;
			controller = _controller;
			button = _button;
		}
		~button_action() throw() {}
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			init_buttonmap();
			if(!buttonmap.count(button))
				return;
			auto i = buttonmap[button];
			do_button_action(i.first, i.second, (type != 1) ? 1 : 0, (type == 2));
			update_movie_state();
			window::notify_screen_update();
		}
		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Press/Unpress button";
		}
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + commandn + "\n"
				"Presses/Unpresses button\n";
		}
		std::string commandn;
		unsigned controller;
		int type;
		std::string button;
	};

	class button_action_helper
	{
	public:
		button_action_helper()
		{
			for(size_t i = 0; i < sizeof(buttonnames) / sizeof(buttonnames[0]); ++i)
				for(int j = 0; j < 3; ++j)
					for(unsigned k = 0; k < 8; ++k) {
						std::ostringstream x, y;
						switch(j) {
						case 0:
							x << "+controller";
							break;
						case 1:
							x << "-controller";
							break;
						case 2:
							x << "controllerh";
							break;
						};
						x << (k + 1);
						x << buttonnames[i];
						y << (k + 1);
						y << buttonnames[i];
						new button_action(x.str(), j, k, y.str());
					}
		}
	} bah;

}

int controller_index_by_logical(unsigned lid) throw()
{
	bool p1multitap = (porttypes[0] == PT_MULTITAP);
	unsigned p1devs = port_types[porttypes[0]].devices;
	unsigned p2devs = port_types[porttypes[1]].devices;
	if(lid >= p1devs + p2devs)
		return -1;
	if(!p1multitap)
		if(lid < p1devs)
			return lid;
		else
			return 4 + lid - p1devs;
	else
		if(lid == 0)
			return 0;
		else if(lid < 5)
			return lid + 3;
		else
			return lid - 4;
}

int controller_index_by_analog(unsigned aid) throw()
{
	if(aid > 2)
		return -1;
	return analog_indices[aid];
}

bool controller_ismouse_by_analog(unsigned aid) throw()
{
	if(aid > 2)
		return false;
	return analog_is_mouse[aid];
}

devicetype_t controller_type_by_logical(unsigned lid) throw()
{
	int x = controller_index_by_logical(lid);
	if(x < 0)
		return DT_NONE;
	enum porttype_t rawtype = porttypes[x >> 2];
	if((x & 3) < port_types[rawtype].devices)
		return port_types[rawtype].dtype;
	else
		return DT_NONE;
}

void controller_set_port_type(unsigned port, porttype_t ptype, bool set_core) throw()
{
	if(set_core && ptype != PT_INVALID)
		snes_set_controller_port_device(port != 0, port_types[ptype].bsnes_type);
	porttypes[port] = ptype;
	update_analog_indices();
}

controls_t get_current_controls(uint64_t frame)
{
	if(autofire_pattern.size())
		return curcontrols ^ autoheld_controls ^ autofire_pattern[frame % autofire_pattern.size()];
	else
		return curcontrols ^ autoheld_controls;
}

void send_analog_input(int32_t x, int32_t y, unsigned index)
{
	if(controller_ismouse_by_analog(index)) {
		x -= 256;
		y -= (framebuffer.height / 2);
	} else {
		x /= (framebuffer.width / 256);
		y /= (framebuffer.height / 224);
	}
	int aindex = controller_index_by_analog(index);
	if(aindex < 0) {
		window::out() << "No analog controller in slot #" << (index + 1) << std::endl;
		return;
	}
	curcontrols(aindex >> 2, aindex & 3, 0) = x;
	curcontrols(aindex >> 2, aindex & 3, 1) = y;
}

void set_curcontrols_reset(int32_t delay)
{
	if(delay >= 0) {
		curcontrols(CONTROL_SYSTEM_RESET) = 1;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_HI) = delay / 10000;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_LO) = delay % 10000;
	} else {
		curcontrols(CONTROL_SYSTEM_RESET) = 0;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_HI) = 0;
		curcontrols(CONTROL_SYSTEM_RESET_CYCLES_LO) = 0;
	}

}
