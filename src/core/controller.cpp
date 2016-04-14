#include "lsnes.hpp"

#include "cmdhelp/button.hpp"
#include "cmdhelp/macro.hpp"
#include "cmdhelp/buttonmode.hpp"
#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "core/mainloop.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include "lua/lua.hpp"

#include <map>
#include <sstream>
#include <string>

namespace
{
	unsigned next_id_from_map(std::map<std::string, unsigned>& map, const std::string& key, unsigned base)
	{
		if(!map.count(key))
			return (map[key] = base);
		else
			return ++map[key];
	}
}

button_mapping::button_mapping(controller_state& _controls, keyboard::mapper& _mapper, keyboard::keyboard& _keyboard,
	emu_framebuffer& _fbuf, emulator_dispatch& _dispatch, lua_state& _lua2, command::group& _cmd)
	: controls(_controls), mapper(_mapper), keyboard(_keyboard), fbuf(_fbuf), edispatch(_dispatch),
	lua2(_lua2), cmd(_cmd),
	button_p(cmd, CBUTTON::p, [this](const std::string& a) { this->do_action(a, 1, 0); }),
	button_r(cmd, CBUTTON::r, [this](const std::string& a) { this->do_action(a, 0, 0); }),
	button_h(cmd, CBUTTON::h, [this](const std::string& a) { this->do_action(a, 1, 1); }),
	button_t(cmd, CBUTTON::t, [this](const std::string& a) { this->do_action(a, 1, 2); }),
	button_d(cmd, CBUTTON::d, [this](const std::string& a) { this->do_action(a, 0, 3); }),
	button_ap(cmd, CBUTTON::ap, [this](const std::string& a) { this->do_autofire_action(a, 1); }),
	button_ar(cmd, CBUTTON::ar, [this](const std::string& a) { this->do_autofire_action(a, 0); }),
	button_at(cmd, CBUTTON::at, [this](const std::string& a) { this->do_autofire_action(a, -1); }),
	button_a(cmd, CBUTTON::a, [this](const std::string& a) { this->do_analog_action(a); }),
	afire_p(cmd, BMODE::afp, [this]() { this->promote_autofire = true; }),
	afire_n(cmd, BMODE::afn, [this]() { this->promote_autofire = false; }),
	ahold_p(cmd, BMODE::ahp, [this]() { this->promote_autohold = true; }),
	ahold_n(cmd, BMODE::ahn, [this]() { this->promote_autohold = false; }),
	typed_p(cmd, BMODE::atp, [this]() { this->promote_typed = true; }),
	typed_n(cmd, BMODE::atn, [this]() { this->promote_typed = false; })
{
	ncore.set(notify_new_core, [this]() { this->init(); });
}

button_mapping::~button_mapping()
{
	cleanup();
}

void button_mapping::reinit()
{
	init();
}

void button_mapping::reread()
{
	std::map<std::string, unsigned> classnum;
	active_buttons.clear();
	for(unsigned i = 0;; i++) {
		auto x = controls.lcid_to_pcid(i);
		if(x.first < 0)
			break;
		const portctrl::type& pt = controls.get_blank().get_port_type(x.first);
		const portctrl::controller& ctrl = pt.controller_info->controllers[x.second];
		if(!classnum.count(ctrl.cclass))
			classnum[ctrl.cclass] = 1;
		else
			classnum[ctrl.cclass]++;
		for(unsigned j = 0; j < ctrl.buttons.size(); j++) {
			std::string name = (stringfmt() << ctrl.cclass << "-" << classnum[ctrl.cclass] << "-"
				<< ctrl.buttons[j].name).str();
			if(all_buttons.count(name)) {
				button_mapping::active_bind a;
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
				button_mapping::active_bind a;
				a.port = x.first;
				a.controller = x.second;
				a.bind = all_buttons[cname];
				active_buttons[cname] = a;
			}
		}
	}
}

void button_mapping::load(controller_state& ctrlstate)
{
	auto s = ctrlstate.enumerate_macro();
	for(auto i : s) {
		if(!macro_binds.count(i)) {
			//New macro, create inverse bind.
			macro_binds[i] = new keyboard::invbind(mapper, CMACRO::t.name + (" " + i) , "Macro‣" + i +
				" (toggle)");
			macro_binds2[i] = new keyboard::invbind(mapper, CMACRO::p.name + (" " + i) , "Macro‣" +
				i + " (hold)");
		}
	}
	for(auto i : macro_binds) {
		if(!s.count(i.first)) {
			//Removed macro, delete inverse bind.
			delete macro_binds[i.first];
			delete macro_binds2[i.first];
			macro_binds.erase(i.first);
			macro_binds2.erase(i.first);
		}
	}
}

void button_mapping::load(controller_state& ctrlstate, project_info& pinfo)
{
	for(auto i : pinfo.macros)
		try {
			ctrlstate.set_macro(i.first, portctrl::macro(i.second));
		} catch(std::exception& e) {
			messages << "Unable to load macro " << i.first << ": " << e.what() << std::endl;
		}
	for(auto i : ctrlstate.enumerate_macro())
		if(!pinfo.macros.count(i))
			ctrlstate.erase_macro(i);
	load(ctrlstate);
}

void button_mapping::cleanup()
{
	for(auto i : added_keys)
		delete i.second;
	for(auto i : macro_binds)
		delete i.second;
	for(auto i : macro_binds2)
		delete i.second;
	added_keys.clear();
	macro_binds.clear();
	macro_binds2.clear();
}

std::pair<int, int> button_mapping::byname(const std::string& name)
{
	for(auto i : active_buttons) {
		std::string _name = (stringfmt() << i.second.bind.cclass << "-" << i.second.bind.number).str();
		if(name != _name)
			continue;
		return std::make_pair(i.second.port, i.second.controller);
	}
	return std::make_pair(-1, -1);
}

void button_mapping::promote_key(keyboard::ctrlrkey& k)
{
	std::string name = k.get_command();
	if(added_keys.count(name)) {
		//Already exists.
		delete &k;
		return;
	}
	added_keys[name] = &k;
	if(!button_keys.count(name))
		return;
	k.append(button_keys[name]);
	messages << button_keys[name] << " bound (button) to " << name << std::endl;
	button_keys.erase(name);
}

void button_mapping::add_button(const std::string& name, const button_mapping::controller_bind& binding)
{
	keyboard::ctrlrkey* k;
	if(binding.mode == 0) {
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::p.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name).str());
		promote_key(*k);
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::h.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name << "‣hold").str());
		promote_key(*k);
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::t.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name << "‣type").str());
		promote_key(*k);
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::ap.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name << "‣autofire").str());
		promote_key(*k);
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::at.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name << "‣autofire toggle").str());
		promote_key(*k);
	} else if(binding.mode == 1) {
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::d.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name).str());
		promote_key(*k);
	} else if(binding.mode == 2) {
		k = new keyboard::ctrlrkey(mapper, (stringfmt() << CBUTTON::a.name << " "
			<< name).str(), (stringfmt() << "Controller‣" << binding.cclass << "‣#"
			<< binding.number << "‣" << binding.name << " (axis)").str(), true);
		promote_key(*k);
	}
}

//Take specified controller info and process it as specified controller of its class.
void button_mapping::process_controller(portctrl::controller& controller, unsigned number)
{
	unsigned analog_num = 1;
	bool multi_analog = (controller.analog_actions() > 1);
	//This controller might be processed already, but perhaps only partially.
	for(unsigned i = 0; i < controller.buttons.size(); i++) {
		if(controller.buttons[i].shadow)
			continue;
		if(controller.buttons[i].type != portctrl::button::TYPE_BUTTON)
			continue;
		std::string name = (stringfmt() << controller.cclass << "-" << number << "-"
			<< controller.buttons[i].name).str();
		button_mapping::controller_bind b;
		b.cclass = controller.cclass;
		b.number = number;
		b.name = controller.buttons[i].name;
		b.mode = 0;
		b.xrel = b.yrel = false;
		b.control1 = i;
		b.control2 = std::numeric_limits<unsigned>::max();
		if(!all_buttons.count(name)) {
			all_buttons[name] = b;
			add_button(name, b);
		}
	}
	for(unsigned i = 0; i < controller.buttons.size(); i++) {
		if(controller.buttons[i].shadow)
			continue;
		if(!controller.buttons[i].is_analog())
			continue;
		std::string name = (stringfmt() << controller.cclass << "-" << number << "-"
			<< controller.buttons[i].name).str();
		button_mapping::controller_bind b;
		b.cclass = controller.cclass;
		b.number = number;
		b.name = controller.buttons[i].name;
		b.rmin = controller.buttons[i].rmin;
		b.rmax = controller.buttons[i].rmax;
		b.centered = controller.buttons[i].centers;
		b.mode = 2;
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
		auto raxis = portctrl::button::TYPE_RAXIS;
		std::string name;
		button_mapping::controller_bind b;
		if(multi_analog)
			b.name = (stringfmt() << "analog" << analog_num).str();
		else
			b.name = (stringfmt() << "analog").str();
		name = (stringfmt() << controller.cclass << "-" << number << "-" << b.name).str();
		analog_num++;
		b.cclass = controller.cclass;
		b.number = number;
		b.mode = 1;
		b.xrel = (g.first < controller.buttons.size()) &&
			(controller.buttons[g.first].type == raxis);
		b.yrel = (g.second < controller.buttons.size()) &&
			(controller.buttons[g.second].type == raxis);
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

//The rules for class number allocations are:
//- Different cores allocate numbers independently.
//- Within the same core, the allocations for higher port start after the previous port ends.
void button_mapping::process_controller(std::map<std::string, unsigned>& allocated,
	std::map<controller_triple, unsigned>& assigned,  portctrl::controller& controller, unsigned port,
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

void button_mapping::process_port(std::map<std::string, unsigned>& allocated,
	std::map<controller_triple, unsigned>& assigned, unsigned port, portctrl::type& ptype)
{
	//What makes this nasty: Separate ports are always processed, but the same controllers can come
	//multiple times, including with partial reprocessing.
	std::map<std::string, unsigned> counts;
	for(unsigned i = 0; i < ptype.controller_info->controllers.size(); i++) {
		//No, n might not equal i + 1, since some ports have heterogenous controllers (e.g.
		//gameboy-gambatte system port).
		unsigned n = next_id_from_map(counts, ptype.controller_info->controllers[i].cclass, 1);
		process_controller(allocated, assigned, ptype.controller_info->controllers[i], port, n);
	}
}

void button_mapping::init()
{
	std::vector<portctrl::type*> ptypes;
	for(auto k : core_core::all_cores()) {
		if(cores_done.count(k))
			continue;
		std::map<std::string, unsigned> allocated;
		std::map<button_mapping::controller_triple, unsigned> assigned;
		auto ptypes = k->get_port_types();
		for(unsigned i = 0; i < ptypes.size(); i++) {
			bool any = false;
			for(unsigned j = 0; j < ptypes.size(); j++) {
				if(!ptypes[j]->legal(i))
					continue;
				any = true;
				process_port(allocated, assigned, i, *ptypes[j]);
			}
			if(!any)
				break;
		}
		cores_done.insert(k);
	}
}

//Check that button is active.
bool button_mapping::check_button_active(const std::string& name)
{
	if(active_buttons.count(name))
		return true;
	//This isn't active. check if there are any other active buttons on the thingy and don't complain
	//if there are.
	bool complain = true;
	auto ckey = keyboard.get_current_key();
	if(ckey) {
		auto cb = mapper.get_controllerkeys_kbdkey(ckey);
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
void button_mapping::do_button_action(const std::string& name, short newstate, int mode)
{
	if(!all_buttons.count(name)) {
		messages << "No such button " << name << std::endl;
		return;
	}
	if(!check_button_active(name))
		return;
	auto x = active_buttons[name];
	if(x.bind.mode != 0)
		return;
	if(mode == 0 && newstate == 1 && promote_autofire) {
		this->do_autofire_action(name, -1);
	}
	if(mode == 1 || (mode == 0 && promote_autohold && newstate == 1)) {
		//Autohold.
		int16_t nstate = controls.autohold2(x.port, x.controller, x.bind.control1) ^ newstate;
		if(lua2.callback_do_button(x.port, x.controller, x.bind.control1, nstate ? "hold" : "unhold"))
			return;
		controls.autohold2(x.port, x.controller, x.bind.control1, nstate);
		edispatch.autohold_update(x.port, x.controller, x.bind.control1, nstate);
		if(nstate)
			messages << "Holding " << name << std::endl;
		else
			messages << "Not holding " << name << std::endl;
	}
	if(mode == 2 || (mode == 0 && promote_typed && newstate == 1)) {
		//Framehold.
		bool nstate = controls.framehold2(x.port, x.controller, x.bind.control1) ^ newstate;
		if(lua2.callback_do_button(x.port, x.controller, x.bind.control1, nstate ? "type" : "untype"))
			return;
		controls.framehold2(x.port, x.controller, x.bind.control1, nstate);
		if(nstate)
			messages << "Holding " << name << " for the next frame" << std::endl;
		else
			messages << "Not holding " << name << " for the next frame" << std::endl;
	}
	if(mode == 0 && !promote_autohold && !promote_autofire && !promote_typed) {
		if(lua2.callback_do_button(x.port, x.controller, x.bind.control1, newstate ? "press" :
			"release"))
			return;
		controls.button2(x.port, x.controller, x.bind.control1, newstate);
	}
}

void button_mapping::send_analog(const std::string& name, int32_t x, int32_t y)
{
	if(!all_buttons.count(name)) {
		messages << "No such action " << name << std::endl;
		return;
	}
	if(!check_button_active(name))
		return;
	auto z = active_buttons[name];
	if(z.bind.mode != 1) {
		std::cerr << name << " is not a axis." << std::endl;
		return;
	}
	if(lua2.callback_do_button(z.port, z.controller, z.bind.control1, "analog"))
		return;
	if(lua2.callback_do_button(z.port, z.controller, z.bind.control2, "analog"))
		return;
	auto g2 = fbuf.get_framebuffer_size();
	x = z.bind.xrel ? (x - g2.first / 2) : (x / 2);
	y = z.bind.yrel ? (y - g2.second / 2) : (y / 2);
	if(z.bind.control1 < std::numeric_limits<unsigned>::max())
		controls.analog(z.port, z.controller, z.bind.control1, x);
	if(z.bind.control2 < std::numeric_limits<unsigned>::max())
		controls.analog(z.port, z.controller, z.bind.control2, y);
}

void button_mapping::do_action(const std::string& name, short state, int mode)
{
	if(mode < 3)
		do_button_action(name, state, mode);
	else if(mode == 3) {
		keyboard::key* mouse_x = keyboard.try_lookup_key("mouse_x");
		keyboard::key* mouse_y = keyboard.try_lookup_key("mouse_y");
		if(!mouse_x || !mouse_y) {
			messages << "Controller analog function not available without mouse" << std::endl;
			return;
		}
		send_analog(name, mouse_x->get_state(), mouse_y->get_state());
	}
}

void button_mapping::do_analog_action(const std::string& a)
{
	int _value;
	regex_results r = regex("([^ \t]+)[ \t]+(-?[0-9]+)[ \t]*", a, "Invalid analog action");
	std::string name = r[1];
	int value = parse_value<int>(r[2]);
	if(!all_buttons.count(name)) {
		messages << "No such button " << name << std::endl;
		return;
	}
	if(!check_button_active(name))
		return;
	auto x = active_buttons[name];
	if(x.bind.mode != 2)
		return;
	if(lua2.callback_do_button(x.port, x.controller, x.bind.control1, "analog"))
		return;
	int rmin = x.bind.rmin;
	int rmax = x.bind.rmax;
	//FIXME: Do something with this?
	//bool centered = x.bind.centered;
	int64_t pvalue = value + 32768;
	_value = pvalue * (rmax - rmin) / 65535 + rmin;
	controls.analog(x.port, x.controller, x.bind.control1, _value);
}

void button_mapping::do_autofire_action(const std::string& a, int mode)
{
	regex_results r = regex("([^ \t]+)(([ \t]+([0-9]+))?[ \t]+([0-9]+))?[ \t]*", a,
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
	if(z.bind.mode != 0) {
		std::cerr << name << " is not a button." << std::endl;
		return;
	}
	auto afire = controls.autofire2(z.port, z.controller, z.bind.control1);
	if(mode == 1 || (mode == -1 && afire.first == 0)) {
		//Turn on.
		if(lua2.callback_do_button(z.port, z.controller, z.bind.control1, (stringfmt() << "autofire "
			<< duty << " " << cyclelen).str().c_str()))
			return;
		controls.autofire2(z.port, z.controller, z.bind.control1, duty, cyclelen);
		edispatch.autofire_update(z.port, z.controller, z.bind.control1, duty, cyclelen);
		messages << "Autofiring " << name << " (duty " << duty << "/" << cyclelen << ")" << std::endl;
	} else if(mode == 0 || (mode == -1 && afire.first != 0)) {
		//Turn off.
		if(lua2.callback_do_button(z.port, z.controller, z.bind.control1, "autofire"))
			return;
		controls.autofire2(z.port, z.controller, z.bind.control1, 0, 1);
		edispatch.autofire_update(z.port, z.controller, z.bind.control1, 0, 1);
		messages << "Not autofiring " << name << std::endl;
	}
}
