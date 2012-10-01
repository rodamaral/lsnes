#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/emucore.hpp"
#include "core/moviedata.hpp"
#include "core/controller.hpp"
#include <iostream>

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
		if(!k) {
			lua_pushstring(LS, "Invalid key name");
			lua_error(LS);
			return 0;
		}
		k->request_hook_callback(state);
		return 0;
	});

	function_ptr_luafun ijget("input.joyget", [](lua_State* LS, const std::string& fname) -> int {
		unsigned lcid = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		if(!lua_input_controllerdata)
			return 0;
		int pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid < 0) {
			lua_pushstring(LS, "Invalid controller for input.joyget");
			lua_error(LS);
			return 0;
		}
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
		if(lua_type(LS, 2) != LUA_TTABLE) {
			lua_pushstring(LS, "Invalid type for input.joyset");
			lua_error(LS);
			return 0;
		}
		if(!lua_input_controllerdata)
			return 0;
		int pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid < 0) {
			lua_pushstring(LS, "Invalid controller for input.joyset");
			lua_error(LS);
			return 0;
		}
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid, i);
			if(y < 0)
				continue;
			lua_pushstring(LS, n.c_str());
			lua_gettable(LS, 2);
			int s = lua_toboolean(LS, -1) ? 1 : 0;
			lua_input_controllerdata->axis(pcid, y, s);
			lua_pop(LS, 1);
		}
		if(lua_input_controllerdata->is_analog(pcid)) {
			lua_pushstring(LS, "xaxis");
			lua_gettable(LS, 2);
			int s = lua_tonumber(LS, -1);
			lua_input_controllerdata->axis(pcid, 0, s);
			lua_pop(LS, 1);
			lua_pushstring(LS, "yaxis");
			lua_gettable(LS, 2);
			s = lua_tonumber(LS, -1);
			lua_input_controllerdata->axis(pcid, 1, s);
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
}
