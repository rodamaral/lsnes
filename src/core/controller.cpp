#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "library/string.hpp"

#include <map>
#include <sstream>

namespace
{
	std::map<std::string, std::pair<unsigned, unsigned>> buttonmap;

	void init_buttonmap()
	{
		static int done = 0;
		if(done)
			return;
		for(unsigned i = 0; i < 8; i++)
			for(unsigned j = 0; j < MAX_LOGICAL_BUTTONS; j++) {
				buttonmap[(stringfmt() << (i + 1) << get_logical_button_name(j)).str()] =
					std::make_pair(i, j);
			}
		done = 1;
	}

	//Do button action.
	void do_button_action(unsigned ui_id, unsigned button, short newstate, bool autoh)
	{
		int x = controls.lcid_to_pcid(ui_id);
		if(x < 0) {
			messages << "No such controller #" << (ui_id + 1) << std::endl;
			return;
		}
		int bid = controls.button_id(x, button);
		if(bid < 0) {
			messages << "Invalid button for controller type" << std::endl;
			return;
		}
		if(autoh) {
			controls.autohold(x, bid, controls.autohold(x, bid) ^ newstate);
			information_dispatch::do_autohold_update(x, bid, controls.autohold(x, bid));
		} else
			controls.button(x, bid, newstate);
	}

	void send_analog(unsigned lcid, int32_t x, int32_t y)
	{
		int pcid = controls.lcid_to_pcid(lcid);
		if(pcid < 0) {
			messages << "Controller #" << (lcid + 1) << " not present" << std::endl;
			return;
		}
		if(controls.is_analog(pcid) < 0) {
			messages << "Controller #" << (lcid + 1) << " is not analog" << std::endl;
			return;
		}
		auto g2 = get_framebuffer_size();
		if(controls.is_mouse(pcid)) {
			controls.analog(pcid, x - g2.first / 2, y - g2.second / 2);
		} else
			controls.analog(pcid, x / 2 , y / 2);
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
					new_autofire_pattern.push_back(controls.get_blank());
				else {
					controller_frame c(controls.get_blank());
					while(fpattern != "") {
						size_t split = fpattern.find_first_of(",");
						std::string button = fpattern;
						std::string rest;
						if(split < fpattern.length()) {
							button = fpattern.substr(0, split);
							rest = fpattern.substr(split + 1);
						}
						if(!buttonmap.count(button))
							(stringfmt() << "Invalid button '" << button << "'").throwex();
						auto g = buttonmap[button];
						int x = controls.lcid_to_pcid(g.first);
						if(x < 0)
							(stringfmt() << "No such controller #" << (g.first + 1)).
								throwex();
						int bid = controls.button_id(x, g.second);
						if(bid < 0)
							(stringfmt() << "Invalid button for controller type").
								throwex();
						c.axis(x, bid, true);
						fpattern = rest;
					}
					new_autofire_pattern.push_back(c);
				}
			}
			controls.autofire(new_autofire_pattern);
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

	class analog_action : public command
	{
	public:
		analog_action(const std::string& cmd, unsigned _controller)
			throw(std::bad_alloc)
			: command(cmd)
		{
			controller = _controller;
		}
		~analog_action() throw() {}
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			keygroup* mouse_x = keygroup::lookup_by_name("mouse_x");
			keygroup* mouse_y = keygroup::lookup_by_name("mouse_y");
			if(!mouse_x || !mouse_y) {
				messages << "Controller analog function not available without mouse" << std::endl;
				return;
			}
			send_analog(controller, mouse_x->get_value(), mouse_y->get_value());
		}
	private:
		unsigned controller;
	};

	class button_action_helper
	{
	public:
		button_action_helper()
		{
			for(size_t i = 0; i < MAX_LOGICAL_BUTTONS; ++i)
				for(int j = 0; j < 3; ++j)
					for(unsigned k = 0; k < 8; ++k) {
						stringfmt x, y, expx;
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
						x << (k + 1) << get_logical_button_name(i);
						y << (k + 1) << get_logical_button_name(i);
						expx << "Controller " << (k + 1) << " " << get_logical_button_name(i);
						our_commands.insert(new button_action(x.str(), j, k, y.str()));
						if(j == 0)
							our_icommands.insert(new inverse_key(x.str(), expx.str()));
					}
			for(unsigned k = 0; k < 8; ++k) {
				stringfmt x, expx;
				x << "controller" << (k + 1) << "analog";
				expx << "Controller " << (k + 1) << " analog function";
				our_commands.insert(new analog_action(x.str(), k));
				our_icommands.insert(new inverse_key(x.str(), expx.str()));
			}
		}
		~button_action_helper()
		{
			for(auto i : our_commands)
				delete i;
			for(auto i : our_icommands)
				delete i;
			our_commands.clear();
			our_icommands.clear();
		}
		std::set<command*> our_commands;
		std::set<inverse_key*> our_icommands;
	} bah;
}

controller_state controls;
