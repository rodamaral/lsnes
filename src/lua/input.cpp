#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/emucore.hpp"
#include "core/moviedata.hpp"
#include "core/controller.hpp"
#include <iostream>

extern bool* lua_veto_flag;

namespace
{
	function_ptr_luafun iset("input.set", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		short value = get_numeric_argument<short>(LS, 3, fname.c_str());
		if(controller >= MAX_PORTS * MAX_CONTROLLERS_PER_PORT || index > MAX_CONTROLS_PER_CONTROLLER)
			return 0;
		lua_input_controllerdata->axis(controller, index, value);
		return 0;
	});

	function_ptr_luafun iget("input.get", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		if(controller >= MAX_PORTS * MAX_CONTROLLERS_PER_PORT || index > MAX_CONTROLS_PER_CONTROLLER)
			return 0;
		lua_pushnumber(LS, lua_input_controllerdata->axis(controller, index));
		return 1;
	});

	function_ptr_luafun iseta("input.seta", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		short val;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		if(controller >= MAX_PORTS * MAX_CONTROLLERS_PER_PORT)
			return 0;
		uint64_t base = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		for(unsigned i = 0; i < MAX_CONTROLS_PER_CONTROLLER; i++) {
			val = (base >> i) & 1;
			get_numeric_argument<short>(LS, i + 3, val, fname.c_str());
			lua_input_controllerdata->axis(controller, i, val);
		}
		return 0;
	});

	function_ptr_luafun igeta("input.geta", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		if(controller >= MAX_PORTS * MAX_CONTROLLERS_PER_PORT)
			return 0;
		uint64_t fret = 0;
		for(unsigned i = 0; i < MAX_CONTROLS_PER_CONTROLLER; i++)
			if(lua_input_controllerdata->axis(controller, i))
				fret |= (1ULL << i);
		lua_pushnumber(LS, fret);
		for(unsigned i = 0; i < MAX_CONTROLS_PER_CONTROLLER; i++)
			lua_pushnumber(LS, lua_input_controllerdata->axis(controller, i));
		return MAX_CONTROLS_PER_CONTROLLER + 1;
	});

	function_ptr_luafun igett("input.controllertype", [](lua_State* LS, const std::string& fname) -> int {
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		porttype_info& p = f.get_port_type(controller / MAX_CONTROLLERS_PER_PORT);
		if(p.controllers <= controller % MAX_CONTROLLERS_PER_PORT)
			lua_pushnil(LS);
		else if(p.ctrlname == "")
			lua_pushnil(LS);
		else
			lua_pushstring(LS, p.ctrlname.c_str());
		return 1;
	});

	function_ptr_luafun ireset("input.reset", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		long cycles = 0;
		get_numeric_argument(LS, 1, cycles, fname.c_str());
		if(cycles < 0)
			return 0;
		short lo = cycles % 10000;
		short hi = cycles / 10000;
		lua_input_controllerdata->reset(true);
		lua_input_controllerdata->delay(std::make_pair(hi, lo));
		return 0;
	});

	function_ptr_luafun iraw("input.raw", [](lua_State* LS, const std::string& fname) -> int {
		auto s = keygroup::get_all_parameters();
		lua_newtable(LS);
		for(auto i : s) {
			lua_pushstring(LS, i.first.c_str());
			push_keygroup_parameters(LS, i.second);
			lua_settable(LS, -3);
		}
		return 1;
	});

	function_ptr_luafun ireq("input.keyhook", [](lua_State* LS, const std::string& fname) -> int {
		struct keygroup* k;
		bool state;
		std::string x = get_string_argument(LS, 1, fname.c_str());
		state = get_boolean_argument(LS, 2, fname.c_str());
		k = keygroup::lookup_by_name(x);
		if(!k)
			throw std::runtime_error("Invalid key name");
		k->request_hook_callback(state);
		return 0;
	});

	function_ptr_luafun ijget("input.joyget", [](lua_State* LS, const std::string& fname) -> int {
		unsigned lcid = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		if(!lua_input_controllerdata)
			return 0;
		int pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid < 0)
			throw std::runtime_error("Invalid controller for input.joyget");
		lua_newtable(LS);
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid, i);
			if(y < 0)
				continue;
			lua_pushstring(LS, n.c_str());
			lua_pushboolean(LS, lua_input_controllerdata->axis(pcid, y) != 0);
			lua_settable(LS, -3);
		}
		if(lua_input_controllerdata->is_analog(pcid)) {
			lua_pushstring(LS, "xaxis");
			lua_pushnumber(LS, lua_input_controllerdata->axis(pcid, 0));
			lua_settable(LS, -3);
			lua_pushstring(LS, "yaxis");
			lua_pushnumber(LS, lua_input_controllerdata->axis(pcid, 1));
			lua_settable(LS, -3);
		}
		return 1;
	});

	function_ptr_luafun ijset("input.joyset", [](lua_State* LS, const std::string& fname) -> int {
		unsigned lcid = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		if(lua_type(LS, 2) != LUA_TTABLE)
			throw std::runtime_error("Invalid type for input.joyset");
		if(!lua_input_controllerdata)
			return 0;
		int pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid < 0)
			throw std::runtime_error("Invalid controller for input.joyset");
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid, i);
			if(y < 0)
				continue;
			lua_pushstring(LS, n.c_str());
			lua_gettable(LS, 2);
			if(lua_type(LS, -1) == LUA_TBOOLEAN) {
				int s = lua_toboolean(LS, -1) ? 1 : 0;
				lua_input_controllerdata->axis(pcid, y, s);
			} else if(lua_type(LS, -1) == LUA_TSTRING) {
				lua_input_controllerdata->axis(pcid, y, lua_input_controllerdata->axis(pcid, y) ^ 1);
			}
			lua_pop(LS, 1);
		}
		if(lua_input_controllerdata->is_analog(pcid)) {
			lua_pushstring(LS, "xaxis");
			lua_gettable(LS, 2);
			if(lua_type(LS, -1) != LUA_TNIL) {
				int s = lua_tonumber(LS, -1);
				lua_input_controllerdata->axis(pcid, 0, s);
			}
			lua_pop(LS, 1);
			lua_pushstring(LS, "yaxis");
			lua_gettable(LS, 2);
			if(lua_type(LS, -1) != LUA_TNIL) {
				int s = lua_tonumber(LS, -1);
				lua_input_controllerdata->axis(pcid, 1, s);
			}
			lua_pop(LS, 1);
		}
		return 0;
	});

	function_ptr_luafun ijlcid_to_pcid("input.lcid_to_pcid", [](lua_State* LS, const std::string& fname) -> int {
		unsigned lcid = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		int pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid < 0)
			return 0;
		lua_pushnumber(LS, pcid);
		lua_pushnumber(LS, pcid / 4);
		lua_pushnumber(LS, pcid % 4);
		return 3;
	});

	//THE NEW API.

	function_ptr_luafun i2set("input.set2", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 3, fname.c_str());
		short value = get_numeric_argument<short>(LS, 4, fname.c_str());
		if(port > 2 || controller > 3 || (port == 0 && controller > 0))
			return 0;
		if(port > 0) {
			unsigned ctn = port * 4 + controller - 4;
			lua_input_controllerdata->axis(ctn, index, value);
		} else if(index == 0) {
			lua_input_controllerdata->sync(value);
		} else if(index == 1) {
			lua_input_controllerdata->reset(value);
		} else if(index == 3) {
			auto d = lua_input_controllerdata->delay();
			d.first = value;
			lua_input_controllerdata->delay(d);
		} else if(index == 2) {
			auto d = lua_input_controllerdata->delay();
			d.second = value;
			lua_input_controllerdata->delay(d);
		}
		return 0;
	});

	function_ptr_luafun i2get("input.get2", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 3, fname.c_str());
		if(port > 2 || controller > 3 || (port == 0 && controller > 0))
			return 0;
		if(port > 0) {
			unsigned ctn = port * 4 + controller - 4;
			lua_pushnumber(LS, lua_input_controllerdata->axis(ctn, index));
			return 1;
		} else if(index == 0) {
			lua_pushnumber(LS, lua_input_controllerdata->sync());
			return 1;
		} else if(index == 1) {
			lua_pushnumber(LS, lua_input_controllerdata->reset());
			return 1;
		} else if(index == 3) {
			lua_pushnumber(LS, lua_input_controllerdata->delay().first);
			return 1;
		} else if(index == 2) {
			lua_pushnumber(LS, lua_input_controllerdata->delay().second);
			return 1;
		}
		return 0;
	});

	function_ptr_luafun ijlcid_to_pcid2("input.lcid_to_pcid2", [](lua_State* LS, const std::string& fname) ->
		int {
		unsigned lcid = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		int pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid < 0)
			return 0;
		lua_pushnumber(LS, pcid / 4 + 1);
		lua_pushnumber(LS, pcid % 4);
		return 2;
	});

	function_ptr_luafun iporttype("input.port_type", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		if(port == 0) {
			lua_pushstring(LS, "system");
		} else if(port < 3) {
			auto& m = get_movie();
			controller_frame f = m.read_subframe(m.get_current_frame(), 0);
			porttype_info& p = f.get_port_type(port - 1);
			lua_pushstring(LS, p.name.c_str());
		} else
			lua_pushnil(LS);
		return 1;
	});

	struct button_struct
	{
		const char* type;
		const char symbol;
		const char* name;
		bool hidden;
	};

	button_struct gamepad_buttons[] = {
		{"button", 'B', "B", false},
		{"button", 'Y', "Y", false},
		{"button", 's', "select", false},
		{"button", 'S', "start", false},
		{"button", 'u', "up", false},
		{"button", 'd', "down", false},
		{"button", 'l', "left", false},
		{"button", 'r', "right", false},
		{"button", 'A', "A", false},
		{"button", 'X', "X", false},
		{"button", 'L', "L", false},
		{"button", 'R', "R", false},
		{"button", '0', "ext0", false},
		{"button", '1', "ext1", false},
		{"button", '2', "ext2", false},
		{"button", '3', "ext3", false}
	};

	button_struct gbgamepad_buttons[] = {
		{"button", 'A', "A", false},
		{"button", 'B', "B", false},
		{"button", 's', "select", false},
		{"button", 'S', "start", false},
		{"button", 'r', "right", false},
		{"button", 'l', "left", false},
		{"button", 'u', "up", false},
		{"button", 'd', "down", false}
	};

	button_struct mouse_buttons[] = {
		{"raxis", '\0', "xaxis", false},
		{"raxis", '\0', "yaxis", false},
		{"button", 'L', "L", false},
		{"button", 'R', "R", false}
	};

	button_struct superscope_buttons[] = {
		{"axis", '\0', "xaxis", false},
		{"axis", '\0', "yaxis", false},
		{"button", 'T', "trigger", false},
		{"button", 'C', "cursor", false},
		{"button", 'U', "turbo", false},
		{"button", 'P', "pause", false}
	};

	button_struct justifier_buttons[] = {
		{"axis", '\0', "xaxis", false},
		{"axis", '\0', "yaxis", false},
		{"button", 'T', "trigger", false},
		{"button", 'S', "start", false}
	};

	button_struct system_buttons[] = {
		{"button", 'F', "framesync", true},
		{"button", 'R', "reset", true},
		{"axis", '\0', "rhigh", true},
		{"axis", '\0', "rlow", true}
	};

	struct controller_struct
	{
		const char* cclass;
		const char* type;
		unsigned button_count;
		button_struct* buttons;
	};

	controller_struct system_controller = { "(system)", "(system)", 4, system_buttons};
	controller_struct gbgamepad_controller = { "gb", "gamepad", 8, gbgamepad_buttons};
	controller_struct gamepad_controller = { "gamepad", "gamepad", 12, gamepad_buttons};
	controller_struct gamepad16_controller = { "gamepad", "gamepad16", 16, gamepad_buttons};
	controller_struct mouse_controller = { "mouse", "mouse", 4, mouse_buttons};
	controller_struct justifier_controller = { "justifier", "justifier", 4, justifier_buttons};
	controller_struct superscope_controller = { "superscope", "superscope", 6, superscope_buttons};

	struct port_struct
	{
		unsigned count;
		controller_struct* ctrls;
	};

	port_struct system_port = {1, &system_controller};
	port_struct none_port = {0, NULL};
	port_struct gbgamepad_port = {1, &gbgamepad_controller};
	port_struct gamepad_port = {1, &gamepad_controller};
	port_struct gamepad16_port = {1, &gamepad16_controller};
	port_struct multitap_port = {4, &gamepad_controller};
	port_struct multitap16_port = {4, &gamepad16_controller};
	port_struct mouse_port = {1, &mouse_controller};
	port_struct justifier_port = {1, &justifier_controller};
	port_struct justifiers_port = {2, &justifier_controller};
	port_struct superscope_port = {1, &superscope_controller};

	port_struct* lookup_ps(unsigned port)
	{
		if(port == 0)
			return &system_port;
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		porttype_info& p = f.get_port_type(port - 1);
		if(p.name == "none") return &none_port;
		if(p.name == "gamepad" && p.storage_size == 1) return &gbgamepad_port;
		if(p.name == "gamepad" && p.storage_size > 1) return &gamepad_port;
		if(p.name == "gamepad16") return &gamepad16_port;
		if(p.name == "multitap") return &multitap_port;
		if(p.name == "multitap16") return &multitap16_port;
		if(p.name == "justifier") return &justifier_port;
		if(p.name == "justifiers") return &justifiers_port;
		if(p.name == "superscope") return &superscope_port;
		if(p.name == "mouse") return &mouse_port;
		return NULL;
	}

	function_ptr_luafun iveto("input.veto_button", [](lua_State* LS, const std::string& fname) -> int {
		if(lua_veto_flag) *lua_veto_flag = true;
		return 0;
	});

	function_ptr_luafun ictrlinfo("input.controller_info", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		port_struct* ps;
		unsigned lcid = 0;
		unsigned classnum = 1;
		ps = lookup_ps(port);
		if(!ps || ps->count <= controller)
			return 0;
		for(unsigned i = 0; i < 8; i++) {
			int pcid = controls.lcid_to_pcid(i);
			if(pcid < 0)
				continue;
			if(pcid == 4 * port + controller - 4) {
				lcid = i + 1;
				break;
			}
			port_struct* ps2 = lookup_ps(pcid / 4 + 1);
			if(!strcmp(ps->ctrls->cclass, ps2->ctrls->cclass))
				classnum++;
		}
		controller_struct* cs = ps->ctrls;
		lua_newtable(LS);
		lua_pushstring(LS, "type");
		lua_pushstring(LS, cs->type);
		lua_rawset(LS, -3);
		lua_pushstring(LS, "class");
		lua_pushstring(LS, cs->cclass);
		lua_rawset(LS, -3);
		lua_pushstring(LS, "classnum");
		lua_pushnumber(LS, classnum);
		lua_rawset(LS, -3);
		lua_pushstring(LS, "lcid");
		lua_pushnumber(LS, lcid);
		lua_rawset(LS, -3);
		lua_pushstring(LS, "button_count");
		lua_pushnumber(LS, cs->button_count);
		lua_rawset(LS, -3);
		lua_pushstring(LS, "buttons");
		lua_newtable(LS);
		//Push the buttons.
		for(unsigned i = 0; i < cs->button_count; i++) {
			lua_pushnumber(LS, i + 1);
			lua_newtable(LS);
			lua_pushstring(LS, "type");
			lua_pushstring(LS, cs->buttons[i].type);
			lua_rawset(LS, -3);
			if(cs->buttons[i].symbol) {
				lua_pushstring(LS, "symbol");
				lua_pushlstring(LS, &cs->buttons[i].symbol, 1);
				lua_rawset(LS, -3);
			}
			lua_pushstring(LS, "name");
			lua_pushstring(LS, cs->buttons[i].name);
			lua_rawset(LS, -3);
			lua_pushstring(LS, "hidden");
			lua_pushboolean(LS, cs->buttons[i].hidden);
			lua_rawset(LS, -3);
			lua_rawset(LS, -3);
		}
		lua_rawset(LS, -3);
		return 1;
	});
}
