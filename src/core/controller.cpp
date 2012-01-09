#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"

#include <map>
#include <sstream>

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
	controller_frame curcontrols;
	controller_frame autoheld_controls;
	std::vector<controller_frame> autofire_pattern;

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
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor, controller_frame& c)
	{
		enum devicetype_t p = controller_type_by_logical(ui_id);
		int x = controller_index_by_logical(ui_id);
		int bid = -1;
		switch(p) {
		case DT_NONE:
			messages << "No such controller #" << (ui_id + 1) << std::endl;
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
				messages << "Invalid button for gamepad" << std::endl;
				return;
			};
			break;
		case DT_MOUSE:
			switch(button) {
			case BUTTON_L:		bid = SNES_DEVICE_ID_MOUSE_LEFT; break;
			case BUTTON_R:		bid = SNES_DEVICE_ID_MOUSE_RIGHT; break;
			default:
				messages << "Invalid button for mouse" << std::endl;
				return;
			};
			break;
		case DT_JUSTIFIER:
			switch(button) {
			case BUTTON_START:	bid = SNES_DEVICE_ID_JUSTIFIER_START; break;
			case BUTTON_TRIGGER:	bid = SNES_DEVICE_ID_JUSTIFIER_TRIGGER; break;
			default:
				messages << "Invalid button for justifier" << std::endl;
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
				messages << "Invalid button for superscope" << std::endl;
				return;
			};
			break;
		};
		if(do_xor)
			c.axis(x, bid, c.axis(x, bid) ^ newstate);
		else
			c.axis(x, bid, newstate);
	}

	//Do button action.
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool do_xor = false)
	{
		if(do_xor) {
			do_button_action(ui_id, button, newstate, do_xor, autoheld_controls);
			int x = controller_index_by_logical(ui_id);
			enum devicetype_t p = controller_type_by_logical(ui_id);
			int y = get_physcial_id_for_control(p, button);
			if(x >= 0 && y >= 0)
				information_dispatch::do_autohold_update(x, y, get_autohold(x, y));
		} else
			do_button_action(ui_id, button, newstate, do_xor, curcontrols);
	}

	function_ptr_command<tokensplitter&> autofire("autofire", "Set autofire pattern",
		"Syntax: autofire <buttons|->...\nSet autofire pattern\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			if(!t)
				throw std::runtime_error("Need at least one frame for autofire");
			std::vector<controller_frame> new_autofire_pattern;
			init_buttonmap();
			while(t) {
				std::string fpattern = t;
				if(fpattern == "-")
					new_autofire_pattern.push_back(curcontrols.blank_frame());
				else {
					controller_frame c(curcontrols.blank_frame());
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
			information_dispatch::do_status_update();
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
	unsigned p1devs = porttype_info::lookup(porttypes[0]).controllers();
	unsigned p2devs = porttype_info::lookup(porttypes[1]).controllers();
	if(lid >= p1devs + p2devs)
		return -1;
	//Exceptional: If p1 is none, map all to p2.
	if(!p1devs)
		return lid + MAX_CONTROLLERS_PER_PORT;
	//LID 0 Is always PID 0 unless out of range.
	if(lid == 0)
		return 0;
	//LID 1-n are on port 2.
	else if(lid < 1 + p2devs)
		return lid - 1 + MAX_CONTROLLERS_PER_PORT;
	//From there, those are on port 1 (except for the first).
	else
		return lid - p2devs;
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
	enum porttype_t rawtype = porttypes[x / MAX_CONTROLLERS_PER_PORT];
	return porttype_info::lookup(rawtype).devicetype(x % MAX_CONTROLLERS_PER_PORT);
}

void controller_set_port_type(unsigned port, porttype_t ptype, bool set_core) throw()
{
	if(set_core && ptype != PT_INVALID)
		snes_set_controller_port_device(port != 0, porttype_info::lookup(ptype).internal_type());
	porttype_t oldtype = curcontrols.get_port_type(port);
	if(oldtype != ptype) {
		curcontrols.set_port_type(port, ptype);
		autoheld_controls.set_port_type(port, ptype);
		//The old autofire pattern no longer applies.
		autofire_pattern.clear();
	}
	porttypes[port] = ptype;
	update_analog_indices();
	information_dispatch::do_autohold_reconfigure();
}

controller_frame get_current_controls(uint64_t frame)
{
	if(autofire_pattern.size())
		return curcontrols ^ autoheld_controls ^ autofire_pattern[frame % autofire_pattern.size()];
	else
		return curcontrols ^ autoheld_controls;
}

void send_analog_input(int32_t x, int32_t y, unsigned index)
{
	auto g2 = get_framebuffer_size();
	if(controller_ismouse_by_analog(index)) {
		x -= (g2.first / 2);
		y -= (g2.second / 2);
	} else {
		auto g = get_scale_factors(g2.first, g2.second);
		x /= g.first;
		y /= g.second;
	}
	int aindex = controller_index_by_analog(index);
	if(aindex < 0) {
		messages << "No analog controller in slot #" << (index + 1) << std::endl;
		return;
	}
	curcontrols.axis(aindex, 0, x);
	curcontrols.axis(aindex, 1, y);
}

void set_curcontrols_reset(int32_t delay)
{
	if(delay >= 0) {
		curcontrols.reset(true);
		curcontrols.delay(std::make_pair(delay / 10000, delay % 10000));
	} else {
		curcontrols.reset(false);
		curcontrols.delay(std::make_pair(0, 0));
	}
}

void change_autohold(unsigned pid, unsigned idx, bool newstate)
{
	if(pid >= MAX_PORTS * MAX_CONTROLLERS_PER_PORT || idx >= MAX_CONTROLS_PER_CONTROLLER)
		return;
	autoheld_controls.axis(pid, idx, newstate ? 1 : 0);
	information_dispatch::do_autohold_update(pid, idx, newstate);
	update_movie_state();
	information_dispatch::do_status_update();
}

bool get_autohold(unsigned pid, unsigned idx)
{
	if(pid >= MAX_PORTS * MAX_CONTROLLERS_PER_PORT || idx >= MAX_CONTROLS_PER_CONTROLLER)
		return false;
	return (autoheld_controls.axis(pid, idx) != 0);
}

std::string get_button_name(unsigned lidx)
{
	if(lidx < 16)
		return buttonnames[lidx];
	else
		return "";
}

int get_physcial_id_for_control(devicetype_t dtype, unsigned lidx)
{
	switch(dtype) {
	case DT_NONE:	return -1;
	case DT_GAMEPAD:
		switch(lidx) {
		case BUTTON_UP: 	return SNES_DEVICE_ID_JOYPAD_UP;
		case BUTTON_DOWN:	return SNES_DEVICE_ID_JOYPAD_DOWN;
		case BUTTON_LEFT:	return SNES_DEVICE_ID_JOYPAD_LEFT;
		case BUTTON_RIGHT:	return SNES_DEVICE_ID_JOYPAD_RIGHT;
		case BUTTON_A:		return SNES_DEVICE_ID_JOYPAD_A;
		case BUTTON_B:		return SNES_DEVICE_ID_JOYPAD_B;
		case BUTTON_X:		return SNES_DEVICE_ID_JOYPAD_X;
		case BUTTON_Y:		return SNES_DEVICE_ID_JOYPAD_Y;
		case BUTTON_L:		return SNES_DEVICE_ID_JOYPAD_L;
		case BUTTON_R:		return SNES_DEVICE_ID_JOYPAD_R;
		case BUTTON_SELECT:	return SNES_DEVICE_ID_JOYPAD_SELECT;
		case BUTTON_START:	return SNES_DEVICE_ID_JOYPAD_START;
		default:		return -1;
		};
	case DT_MOUSE:
		switch(lidx) {
		case BUTTON_L:		return SNES_DEVICE_ID_MOUSE_LEFT;
		case BUTTON_R:		return SNES_DEVICE_ID_MOUSE_RIGHT;
		default:		return -1;
		};
	case DT_SUPERSCOPE:
		switch(lidx) {
		case BUTTON_TRIGGER:	return SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER;
		case BUTTON_CURSOR:	return SNES_DEVICE_ID_SUPER_SCOPE_CURSOR;
		case BUTTON_PAUSE:	return SNES_DEVICE_ID_SUPER_SCOPE_PAUSE;
		case BUTTON_TURBO:	return SNES_DEVICE_ID_SUPER_SCOPE_TURBO;
		default:		return -1;
		};
	case DT_JUSTIFIER:
		switch(lidx) {
		case BUTTON_START:	return SNES_DEVICE_ID_JUSTIFIER_START;
		case BUTTON_TRIGGER:	return SNES_DEVICE_ID_JUSTIFIER_TRIGGER;
		default:		return -1;
		};
	default:	return -1;
	}
}
