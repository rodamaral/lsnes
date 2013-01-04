#include "lsnes.hpp"
#include "core/emucore.hpp"

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
#include <string>

namespace
{
	struct controller_bind
	{
		std::string cclass;
		unsigned number;
		std::string name;
		bool is_axis;
		bool xrel;
		bool yrel;
		unsigned control1;
		unsigned control2;	//Axis only, UINT_MAX if not valid.
	};

	struct active_bind
	{
		unsigned port;
		unsigned controller;
		struct controller_bind bind;
	};

	struct controller_triple
	{
		std::string cclass;
		unsigned port;
		unsigned controller;
		bool operator<(const struct controller_triple& t) const throw()
		{
			if(cclass < t.cclass) return true;
			if(cclass > t.cclass) return false;
			if(port < t.port) return true;
			if(port > t.port) return false;
			if(controller < t.controller) return true;
			if(controller > t.controller) return false;
			return false;
		}
		bool operator==(const struct controller_triple& t) const throw()
		{
			return (cclass == t.cclass && port == t.port && controller == t.controller);
		}
	};

	std::map<std::string, unsigned> allocated_controllers;
	std::map<std::string, controller_bind> all_buttons;
	std::map<controller_triple, unsigned> assignments;
	std::map<std::string, active_bind> active_buttons;

	void process_controller(port_controller& controller, unsigned number)
	{
		unsigned analog_num = 1;
		bool multi_analog = (controller.analog_actions() > 1);
		//This controller might be processed already, but perhaps only partially.
		for(unsigned i = 0; i < controller.button_count; i++) {
			if(controller.buttons[i]->is_analog())
				continue;
			std::string name = (stringfmt() << controller.cclass << "-" << number << "-"
				<< controller.buttons[i]->name).str();
			controller_bind b;
			b.cclass = controller.cclass;
			b.number = number;
			b.name = controller.buttons[i]->name;
			b.is_axis = false;
			b.xrel = b.yrel = false;
			b.control1 = i;
			b.control2 = std::numeric_limits<unsigned>::max();
			if(!all_buttons.count(name))
				all_buttons[name] = b;
		}
		for(unsigned i = 0; i < controller.analog_actions(); i++) {
			auto g = controller.analog_action(i);
			auto raxis = port_controller_button::TYPE_RAXIS;
			std::string name;
			controller_bind b;
			if(multi_analog)
				b.name = (stringfmt() << "analog" << analog_num).str();
			else
				b.name = (stringfmt() << "analog").str();
			name = (stringfmt() << controller.cclass << "-" << number << "-" << b.name).str();
			analog_num++;
			b.cclass = controller.cclass;
			b.number = number;
			b.is_axis = true;
			b.xrel = (g.first < controller.button_count) &&
				(controller.buttons[g.first]->type == raxis);
			b.yrel = (g.second < controller.button_count) &&
				(controller.buttons[g.second]->type == raxis);
			b.control1 = g.first;
			b.control2 = g.second;
			if(!all_buttons.count(name) ||
				(all_buttons[name].control2 == std::numeric_limits<unsigned>::max()) &&
				(b.control2 < std::numeric_limits<unsigned>::max()))
				all_buttons[name] = b;
		}
	}

	void process_controller(port_controller& controller, unsigned port, unsigned number_in_port)
	{
		controller_triple key;
		key.cclass = controller.cclass;
		key.port = port;
		key.controller = number_in_port;
		unsigned n;
		if(!assignments.count(key)) {
			if(allocated_controllers.count(controller.cclass)) 
				++(allocated_controllers[controller.cclass]);
			else
				allocated_controllers[controller.cclass] = 1;
			assignments[key] = n = allocated_controllers[controller.cclass];
		} else
			n = assignments[key];
		process_controller(controller, n);
	}

	void process_port(unsigned port, port_type& ptype)
	{
		//What makes this nasty: Separate ports are always processed, but the same controllers can come
		//multiple times, including with partial reprocessing.
		std::map<std::string, unsigned> counts;
		for(unsigned i = 0; i < ptype.controller_info->controller_count; i++) {
			unsigned n;
			if(!counts.count(ptype.controller_info->controllers[i]->cclass))
				counts[ptype.controller_info->controllers[i]->cclass] = 1;
			else
				counts[ptype.controller_info->controllers[i]->cclass]++;
			n = counts[ptype.controller_info->controllers[i]->cclass];
			process_controller(*ptype.controller_info->controllers[i], port, n);
		}
	}

	void init_buttonmap()
	{
		static int done = 0;
		if(done)
			return;
		process_port(0, core_portgroup.get_default_type(0));
		for(unsigned i = 0; i < core_userports; i++) {
			for(auto j : core_portgroup.get_types()) {
				if(!j->legal(i))
					continue;
				process_port(i + 1, *j);
			}
		}
		done = 1;
	}

	//Do button action.
	void do_button_action(const std::string& name, short newstate, int mode)
	{
		if(!active_buttons.count(name)) {
			messages << "No such button " << name << std::endl;
			return;
		}
		auto x = active_buttons[name];
		if(x.bind.is_axis)
			return;
		if(mode == 1) {
			//Autohold.
			controls.autohold2(x.port, x.controller, x.bind.control1, controls.autohold2(
				x.port, x.controller, x.bind.control1) ^ newstate);
			information_dispatch::do_autohold_update(x.port, x.controller, x.bind.control1,
				controls.autohold2(x.port, x.controller, x.bind.control1));
		} else if(mode == 2) {
			//Framehold.
			bool nstate = controls.framehold2(x.port, x.controller, x.bind.control1) ^ newstate;
			controls.framehold2(x.port, x.controller, x.bind.control1, nstate);
			if(nstate)
				messages << "Holding " << name << " for the next frame" << std::endl;
			else
				messages << "Not holding " << name << " for the next frame" << std::endl;
		} else
			controls.button2(x.port, x.controller, x.bind.control1, newstate);
	}

	void send_analog(const std::string& name, int32_t x, int32_t y)
	{
		if(!active_buttons.count(name)) {
			messages << "No such action " << name << std::endl;
			return;
		}
		auto z = active_buttons[name];
		if(!z.bind.is_axis) {
			std::cerr << name << " is not a axis." << std::endl;
			return;
		}
		auto g2 = get_framebuffer_size();
		x = z.bind.xrel ? (x - g2.first / 2) : (x / 2);
		y = z.bind.yrel ? (y - g2.second / 2) : (y / 2);
		if(z.bind.control1 < std::numeric_limits<unsigned>::max())
			controls.analog(z.port, z.controller, z.bind.control1, x);
		if(z.bind.control2 < std::numeric_limits<unsigned>::max())
			controls.analog(z.port, z.controller, z.bind.control2, y);
	}

	function_ptr_command<const std::string&> autofire(lsnes_cmd, "autofire", "Set autofire pattern",
		"Syntax: autofire <buttons|->...\nSet autofire pattern\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex(".*[^ \t].*", a, "Need at least one frame for autofire");
			std::vector<controller_frame> new_autofire_pattern;
			init_buttonmap();
			std::string pattern = a;
			while(pattern != "") {
				std::string fpattern;
				extract_token(pattern, fpattern, " \t", true);
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
						if(!active_buttons.count(button))
							(stringfmt() << "Invalid button '" << button << "'").throwex();
						auto g = active_buttons[button];
						if(g.bind.is_axis)
							(stringfmt() << "Invalid button '" << button << "'").throwex();
						c.axis3(g.port, g.controller, g.bind.control1, true);
						fpattern = rest;
					}
					new_autofire_pattern.push_back(c);
				}
			}
			controls.autofire(new_autofire_pattern);
		});

	void do_action(const std::string& name, short state, int mode)
	{
		if(mode < 3)
			do_button_action(name, state, mode);
		else if(mode == 3) {
			keyboard_key* mouse_x = lsnes_kbd.try_lookup_key("mouse_x");
			keyboard_key* mouse_y = lsnes_kbd.try_lookup_key("mouse_y");
			if(!mouse_x || !mouse_y) {
				messages << "Controller analog function not available without mouse" << std::endl;
				return;
			}
			send_analog(name, mouse_x->get_state(), mouse_y->get_state());
		}
	}

	function_ptr_command<const std::string&> button_p(lsnes_cmd, "+controller", "Press a button",
		"Syntax: +button <button>...\nPress a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 1, 0);
		});

	function_ptr_command<const std::string&> button_r(lsnes_cmd, "-controller", "Release a button",
		"Syntax: -button <button>...\nRelease a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 0, 0);
		});

	function_ptr_command<const std::string&> button_h(lsnes_cmd, "hold-controller", "Autohold a button",
		"Syntax: hold-button <button>...\nAutohold a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 1, 1);
		});

	function_ptr_command<const std::string&> button_t(lsnes_cmd, "type-controller", "Type a button",
		"Syntax: type-button <button>...\nType a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 1, 2);
		});

	function_ptr_command<const std::string&> button_d(lsnes_cmd, "designate-position", "Set postion",
		"Syntax: designate-position <button>...\nDesignate position for an axis\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 0, 3);
		});

	class button_action_helper
	{
	public:
		button_action_helper()
		{
			init_buttonmap();
			for(auto i : all_buttons) {
				if(!i.second.is_axis) {
					our.insert(new controller_key(lsnes_mapper, (stringfmt()
						<< "+controller " << i.first).str(), (stringfmt()
						<< "Controller‣" << i.second.cclass
						<< "-" << i.second.number << "‣" << i.second.name).str()));
					our.insert(new controller_key(lsnes_mapper, (stringfmt()
						<< "hold-controller " << i.first).str(), (stringfmt()
						<< "Controller‣" << i.second.cclass
						<< "-" << i.second.number << "‣" << i.second.name
						<< " (hold)").str()));
					our.insert(new controller_key(lsnes_mapper, (stringfmt()
						<< "type-controller " << i.first).str(), (stringfmt()
						<< "Controller‣" << i.second.cclass
						<< "-" << i.second.number << "‣" << i.second.name
						<< " (type)").str()));
				} else
					our.insert(new controller_key(lsnes_mapper, (stringfmt()
						<< "designate-position " << i.first).str(), (stringfmt()
						<< "Controller‣" << i.second.cclass
						<< "-" << i.second.number << "‣" << i.second.name).str()));
			}
		}
		~button_action_helper()
		{
			for(auto i : our)
				delete i;
			our.clear();
		}
		std::set<controller_key*> our;
	} bah;
}

void reread_active_buttons()
{
	std::map<std::string, unsigned> classnum;
	active_buttons.clear();
	for(unsigned i = 0;; i++) {
		auto x = controls.lcid_to_pcid(i);
		if(x.first < 0)
			break;
		const port_type& pt = controls.get_blank().get_port_type(x.first);
		const port_controller& ctrl = *pt.controller_info->controllers[x.second];
		if(!classnum.count(ctrl.cclass))
			classnum[ctrl.cclass] = 1;
		else
			classnum[ctrl.cclass]++;
		for(unsigned j = 0; j < ctrl.button_count; j++) {
			std::string name = (stringfmt() << ctrl.cclass << "-" << classnum[ctrl.cclass] << "-"
				<< ctrl.buttons[j]->name).str();
			if(all_buttons.count(name)) {
				active_bind a;
				a.port = x.first;
				a.controller = x.second;
				a.bind = all_buttons[name];
				active_buttons[name] = a;
			}
		}
		bool multi = (ctrl.analog_actions() > 1);
		for(unsigned j = 0; j < ctrl.analog_actions(); j++) {
			std::string name;
			if(multi)
				name = (stringfmt() << "analog" << (j + 1)).str();
			else
				name = "analog";
			std::string cname = (stringfmt() << ctrl.cclass << "-" << classnum[ctrl.cclass] << "-"
				<< name).str();
			if(all_buttons.count(cname)) {
				active_bind a;
				a.port = x.first;
				a.controller = x.second;
				a.bind = all_buttons[cname];
				active_buttons[cname] = a;
			}
		}
	}
}

controller_state controls;
