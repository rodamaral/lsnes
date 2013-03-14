#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
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

	std::map<std::string, controller_bind> all_buttons;
	std::map<std::string, active_bind> active_buttons;

	//Promote stored key to active key.
	void promote_key(controller_key& k)
	{
		std::string name = k.get_command();
		if(!button_keys.count(name))
			return;
		k.set(button_keys[name]);
		messages << button_keys[name] << " bound (button) to " << name << std::endl;
		button_keys.erase(name);
	}

	//Allocate controller keys for specified button.
	void add_button(const std::string& name, const controller_bind& binding)
	{
		controller_key* k;
		if(!binding.is_axis) {
			k = new controller_key(lsnes_mapper, (stringfmt() << "+controller " << name).str(),
				(stringfmt() << "Controller‣" << binding.cclass << "-" << binding.number << "‣"
				<< binding.name).str());
			promote_key(*k);
			k = new controller_key(lsnes_mapper, (stringfmt() << "hold-controller " << name).str(),
				(stringfmt() << "Controller‣" << binding.cclass << "-" << binding.number << "‣"
				<< binding.name << " (hold)").str());
			promote_key(*k);
			k = new controller_key(lsnes_mapper, (stringfmt() << "type-controller " << name).str(),
				(stringfmt() << "Controller‣" << binding.cclass << "-" << binding.number << "‣"
				<< binding.name << " (type)").str());
			promote_key(*k);
			k = new controller_key(lsnes_mapper, (stringfmt() << "+autofire-controller " << name).str(),
				(stringfmt() << "Controller‣" << binding.cclass << "-" << binding.number << "‣"
				<< binding.name << " (autofire)").str());
			promote_key(*k);
			k = new controller_key(lsnes_mapper, (stringfmt() << "autofire-controller " << name).str(),
				(stringfmt() << "Controller‣" << binding.cclass << "-" << binding.number << "‣"
				<< binding.name << " (autofire toggle)").str());
			promote_key(*k);
		} else {
			k = new controller_key(lsnes_mapper, (stringfmt() << "designate-position " << name).str(),
				(stringfmt() << "Controller‣" << binding.cclass << "-" << binding.number << "‣"
				<< binding.name).str());
			promote_key(*k);
		}
	}

	//Take specified controller info and process it as specified controller of its class.
	void process_controller(port_controller& controller, unsigned number)
	{
		unsigned analog_num = 1;
		bool multi_analog = (controller.analog_actions() > 1);
		//This controller might be processed already, but perhaps only partially.
		for(unsigned i = 0; i < controller.button_count; i++) {
			if(controller.buttons[i]->shadow)
				continue;
			if(controller.buttons[i]->type != port_controller_button::TYPE_BUTTON)
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
			if(!all_buttons.count(name)) {
				all_buttons[name] = b;
				add_button(name, b);
			}
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
			if(!all_buttons.count(name))
				add_button(name, b);
			if(!all_buttons.count(name) ||
				((all_buttons[name].control2 == std::numeric_limits<unsigned>::max()) &&
				(b.control2 < std::numeric_limits<unsigned>::max()))) {
				all_buttons[name] = b;
			}
		}
	}

	unsigned next_id_from_map(std::map<std::string, unsigned>& map, const std::string& key, unsigned base)
	{
		if(!map.count(key))
			return (map[key] = base);
		else
			return ++map[key];
	}

	//The rules for class number allocations are:
	//- Different cores allocate numbers independently.
	//- Within the same core, the allocations for higher port start after the previous port ends.
	

	void process_controller(std::map<std::string, unsigned>& allocated,
		std::map<controller_triple, unsigned>& assigned,  port_controller& controller, unsigned port,
		unsigned number_in_port)
	{
		controller_triple key;
		key.cclass = controller.cclass;
		key.port = port;
		key.controller = number_in_port;
		unsigned n;
		if(!assigned.count(key))
			assigned[key] = next_id_from_map(allocated, controller.cclass, 1);
		n = assigned[key];
		process_controller(controller, n);
	}

	void process_port(std::map<std::string, unsigned>& allocated,
		std::map<controller_triple, unsigned>& assigned, unsigned port, port_type& ptype)
	{
		//What makes this nasty: Separate ports are always processed, but the same controllers can come
		//multiple times, including with partial reprocessing.
		std::map<std::string, unsigned> counts;
		for(unsigned i = 0; i < ptype.controller_info->controller_count; i++) {
			//No, n might not equal i + 1, since some ports have heterogenous controllers (e.g.
			//gameboy-gambatte system port).
			unsigned n = next_id_from_map(counts, ptype.controller_info->controllers[i]->cclass, 1);
			process_controller(allocated, assigned, *ptype.controller_info->controllers[i], port, n);
		}
	}

	void init_buttonmap()
	{
		static std::set<core_core*> done;
		std::vector<port_type*> ptypes;
		for(auto k : core_core::all_cores()) {
			if(done.count(k))
				continue;
			std::map<std::string, unsigned> allocated;
			std::map<controller_triple, unsigned> assigned;
			auto ptypes = k->get_port_types();
			for(unsigned i = 0;; i++) {
				bool any = false;
				for(unsigned j = 0; ptypes[j]; j++) {
					if(!ptypes[j]->legal(i))
						continue;
					any = true;
					process_port(allocated, assigned, i, *ptypes[j]);
				}
				if(!any)
					break;
			}
			done.insert(k);
		}
	}

	//Check that button is active.
	bool check_button_active(const std::string& name)
	{
		if(active_buttons.count(name))
			return true;
		//This isn't active. check if there are any other active buttons on the thingy and don't complain
		//if there are.
		bool complain = true;
		auto ckey = lsnes_kbd.get_current_key();
		if(ckey) {
			auto cb = lsnes_mapper.get_controllerkeys_kbdkey(ckey);
			for(auto i : cb) {
				regex_results r = regex("[^ \t]+[ \t]+([^ \t]+)([ \t]+.*)?", i->get_command());
				if(!r)
					continue;
				if(active_buttons.count(r[1]))
					complain = false;
			}
		}
		if(complain)
			messages << "No such button " << name << std::endl;
		return false;
	}

	//Do button action.
	void do_button_action(const std::string& name, short newstate, int mode)
	{
		if(!all_buttons.count(name)) {
			messages << "No such button " << name << std::endl;
			return;
		}
		if(!check_button_active(name))
			return;
		auto x = active_buttons[name];
		if(x.bind.is_axis)
			return;
		if(mode == 1) {
			//Autohold.
			int16_t nstate = controls.autohold2(x.port, x.controller, x.bind.control1) ^ newstate;
			if(lua_callback_do_button(x.port, x.controller, x.bind.control1, nstate ? "hold" : "unhold"))
				return;
			controls.autohold2(x.port, x.controller, x.bind.control1, nstate);
			information_dispatch::do_autohold_update(x.port, x.controller, x.bind.control1, nstate);
		} else if(mode == 2) {
			//Framehold.
			bool nstate = controls.framehold2(x.port, x.controller, x.bind.control1) ^ newstate;
			if(lua_callback_do_button(x.port, x.controller, x.bind.control1, nstate ? "type" : "untype"))
				return;
			controls.framehold2(x.port, x.controller, x.bind.control1, nstate);
			if(nstate)
				messages << "Holding " << name << " for the next frame" << std::endl;
			else
				messages << "Not holding " << name << " for the next frame" << std::endl;
		} else {
			if(lua_callback_do_button(x.port, x.controller, x.bind.control1, newstate ? "press" :
				"release"))
				return;
			controls.button2(x.port, x.controller, x.bind.control1, newstate);
		}
	}

	void send_analog(const std::string& name, int32_t x, int32_t y)
	{
		if(!all_buttons.count(name)) {
			messages << "No such action " << name << std::endl;
			return;
		}
		if(!check_button_active(name))
			return;
		auto z = active_buttons[name];
		if(!z.bind.is_axis) {
			std::cerr << name << " is not a axis." << std::endl;
			return;
		}
		if(lua_callback_do_button(z.port, z.controller, z.bind.control1, "analog"))
			return;
		if(lua_callback_do_button(z.port, z.controller, z.bind.control2, "analog"))
			return;
		auto g2 = get_framebuffer_size();
		x = z.bind.xrel ? (x - g2.first / 2) : (x / 2);
		y = z.bind.yrel ? (y - g2.second / 2) : (y / 2);
		if(z.bind.control1 < std::numeric_limits<unsigned>::max())
			controls.analog(z.port, z.controller, z.bind.control1, x);
		if(z.bind.control2 < std::numeric_limits<unsigned>::max())
			controls.analog(z.port, z.controller, z.bind.control2, y);
	}

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

	void do_autofire_action(const std::string& a, int mode)
	{
		regex_results r = regex("([^ ]+)(([ \t]+([0-9]+))?[ \t]+([0-9]+))?[ \t]*", a,
			"Invalid autofire parameters");
		std::string name = r[1];
		std::string _duty = r[4];
		std::string _cyclelen = r[5];
		if(_duty == "") _duty = "1";
		if(_cyclelen == "") _cyclelen = "2";
		uint32_t duty = parse_value<uint32_t>(_duty);
		uint32_t cyclelen = parse_value<uint32_t>(_cyclelen);
		if(duty >= cyclelen)
			throw std::runtime_error("Invalid autofire parameters");
		if(!all_buttons.count(name)) {
			messages << "No such button " << name << std::endl;
			return;
		}
		if(!check_button_active(name))
			return;
		auto z = active_buttons[name];
		if(z.bind.is_axis) {
			std::cerr << name << " is not a button." << std::endl;
			return;
		}
		auto afire = controls.autofire2(z.port, z.controller, z.bind.control1);
		if(mode == 1 || (mode == -1 && afire.first == 0)) {
			//Turn on.
			if(lua_callback_do_button(z.port, z.controller, z.bind.control1, (stringfmt() << "autofire "
				<< duty << " " << cyclelen).str().c_str()))
				return;
			controls.autofire2(z.port, z.controller, z.bind.control1, duty, cyclelen);
			information_dispatch::do_autofire_update(z.port, z.controller, z.bind.control1, duty,
				cyclelen);
		} else if(mode == 0 || (mode == -1 && afire.first != 0)) {
			//Turn off.
			if(lua_callback_do_button(z.port, z.controller, z.bind.control1, "autofire"))
				return;
			controls.autofire2(z.port, z.controller, z.bind.control1, 0, 1);
			information_dispatch::do_autofire_update(z.port, z.controller, z.bind.control1, 0, 1);
		}
	}

	function_ptr_command<const std::string&> button_p(lsnes_cmd, "+controller", "Press a button",
		"Syntax: +controller<button>...\nPress a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 1, 0);
		});

	function_ptr_command<const std::string&> button_r(lsnes_cmd, "-controller", "Release a button",
		"Syntax: -controller<button>...\nRelease a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 0, 0);
		});

	function_ptr_command<const std::string&> button_h(lsnes_cmd, "hold-controller", "Autohold a button",
		"Syntax: hold-controller<button>...\nAutohold a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 1, 1);
		});

	function_ptr_command<const std::string&> button_t(lsnes_cmd, "type-controller", "Type a button",
		"Syntax: type-controller<button>...\nType a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 1, 2);
		});

	function_ptr_command<const std::string&> button_d(lsnes_cmd, "designate-position", "Set postion",
		"Syntax: designate-position <button>...\nDesignate position for an axis\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_action(a, 0, 3);
		});

	function_ptr_command<const std::string&> button_ap(lsnes_cmd, "+autofire-controller", "Start autofire",
		"Syntax: +autofire-controller<button> [[<duty> ]<cyclelen>]...\nAutofire a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_autofire_action(a, 1);
		});

	function_ptr_command<const std::string&> button_an(lsnes_cmd, "-autofire-controller", "End autofire",
		"Syntax: -autofire-controller<button> [[<duty> ]<cyclelen>]...\nEnd Autofire on a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_autofire_action(a, 0);
		});

	function_ptr_command<const std::string&> button_at(lsnes_cmd, "autofire-controller", "Toggle autofire",
		"Syntax: autofire-controller<button> [[<duty> ]<cyclelen>]...\nToggle Autofire on a button\n",
		[](const std::string& a) throw(std::bad_alloc, std::runtime_error) {
			do_autofire_action(a, -1);
		});

	class new_core_snoop : public information_dispatch
	{
	public:
		new_core_snoop() : information_dispatch("controller-newcore")
		{
		}
		void on_new_core()
		{
			init_buttonmap();
		}
	} coresnoop;
}

void reinitialize_buttonmap()
{
	init_buttonmap();
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
std::map<std::string, std::string> button_keys;
