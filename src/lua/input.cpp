#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/emucore.hpp"
#include "core/moviedata.hpp"
#include "core/controller.hpp"
#include <iostream>

namespace
{
	int input_set(lua_State* LS, unsigned port, unsigned controller, unsigned index, short value)
	{
		if(!lua_input_controllerdata)
			return 0;
		lua_input_controllerdata->axis3(port, controller, index, value);
		return 0;
	}

	int input_get(lua_State* LS, unsigned port, unsigned controller, unsigned index)
	{
		if(!lua_input_controllerdata)
			return 0;
		lua_pushnumber(LS, lua_input_controllerdata->axis3(port, controller, index));
		return 1;
	}

	int input_controllertype(lua_State* LS, unsigned port, unsigned controller)
	{
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		const port_type& p = f.get_port_type(port);
		if(p.controllers <= controller)
			lua_pushnil(LS);
		else if(p.ctrlname == "")
			lua_pushnil(LS);
		else
			lua_pushstring(LS, p.ctrlname.c_str());
		return 1;
	}

	int input_seta(lua_State* LS, unsigned port, unsigned controller, int base, const char* fname)
	{
		if(!lua_input_controllerdata)
			return 0;
		short val;
		if(port >= lua_input_controllerdata->get_port_count())
			return 0;
		const port_type& pt = lua_input_controllerdata->get_port_type(port);
		if(controller >= pt.controllers)
			return 0;
		for(unsigned i = 0; i < pt.controller_indices[controller]; i++) {
			val = (base >> i) & 1;
			get_numeric_argument<short>(LS, i + base, val, fname);
			lua_input_controllerdata->axis3(port, controller, i, val);
		}
		return 0;
	}

	int input_geta(lua_State* LS, unsigned port, unsigned controller)
	{
		if(!lua_input_controllerdata)
			return 0;
		if(port >= lua_input_controllerdata->get_port_count())
			return 0;
		const port_type& pt = lua_input_controllerdata->get_port_type(port);
		if(controller >= pt.controllers)
			return 0;
		uint64_t fret = 0;
		for(unsigned i = 0; i < pt.controller_indices[controller]; i++)
			if(lua_input_controllerdata->axis3(port, controller, i))
				fret |= (1ULL << i);
		lua_pushnumber(LS, fret);
		for(unsigned i = 0; i < pt.controller_indices[controller]; i++)
			lua_pushnumber(LS, lua_input_controllerdata->axis3(port, controller, i));
		return pt.controller_indices[controller] + 1;
	}

	function_ptr_luafun iset("input.set", [](lua_State* LS, const std::string& fname) -> int {
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		short value = get_numeric_argument<short>(LS, 3, fname.c_str());
		return input_set(LS, controller / 4 + 1, controller % 4, index, value);
	});

	function_ptr_luafun iset2("input.set2", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 3, fname.c_str());
		short value = get_numeric_argument<short>(LS, 4, fname.c_str());
		return input_set(LS, port, controller, index, value);
	});

	function_ptr_luafun iget("input.get", [](lua_State* LS, const std::string& fname) -> int {
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		return input_get(LS, controller / 4 + 1, controller % 4, index);
	});

	function_ptr_luafun iget2("input.get2", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 3, fname.c_str());
		return input_get(LS, port, controller, index);
	});

	function_ptr_luafun iseta("input.seta", [](lua_State* LS, const std::string& fname) -> int {
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		uint64_t base = get_numeric_argument<uint64_t>(LS, 2, fname.c_str());
		return input_seta(LS, controller / 4 + 1, controller % 4, 3, fname.c_str());
	});

	function_ptr_luafun iseta2("input.seta2", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		uint64_t base = get_numeric_argument<uint64_t>(LS, 3, fname.c_str());
		return input_seta(LS, port, controller, 4, fname.c_str());
	});

	function_ptr_luafun igeta("input.geta", [](lua_State* LS, const std::string& fname) -> int {
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		return input_geta(LS, controller / 4 + 1, controller % 4);
	});

	function_ptr_luafun igeta2("input.geta2", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		return input_geta(LS, port, controller);
	});

	function_ptr_luafun igett("input.controllertype", [](lua_State* LS, const std::string& fname) -> int {
		unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		return input_controllertype(LS, controller / 4 + 1, controller % 4);
	});

	function_ptr_luafun igett2("input.controllertype2", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		return input_controllertype(LS, port, controller);
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
		lua_input_controllerdata->axis3(0, 0, 1, 1);
		lua_input_controllerdata->axis3(0, 0, 2, hi);
		lua_input_controllerdata->axis3(0, 0, 3, lo);
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
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0) {
			lua_pushstring(LS, "Invalid controller for input.joyget");
			lua_error(LS);
			return 0;
		}
		lua_newtable(LS);
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid.first, pcid.second, i);
			if(y < 0)
				continue;
			lua_pushstring(LS, n.c_str());
			lua_pushboolean(LS, lua_input_controllerdata->axis3(pcid.first, pcid.second, y) != 0);
			lua_settable(LS, -3);
		}
		if(lua_input_controllerdata->is_analog(pcid.first, pcid.second)) {
			lua_pushstring(LS, "xaxis");
			lua_pushnumber(LS, lua_input_controllerdata->axis3(pcid.first, pcid.second, 0));
			lua_settable(LS, -3);
			lua_pushstring(LS, "yaxis");
			lua_pushnumber(LS, lua_input_controllerdata->axis3(pcid.first, pcid.second, 1));
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
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0) {
			lua_pushstring(LS, "Invalid controller for input.joyset");
			lua_error(LS);
			return 0;
		}
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid.first, pcid.second, i);
			if(y < 0)
				continue;
			lua_pushstring(LS, n.c_str());
			lua_gettable(LS, 2);
			int s = lua_toboolean(LS, -1) ? 1 : 0;
			lua_input_controllerdata->axis3(pcid.first, pcid.second, y, s);
			lua_pop(LS, 1);
		}
		if(lua_input_controllerdata->is_analog(pcid.first, pcid.second)) {
			lua_pushstring(LS, "xaxis");
			lua_gettable(LS, 2);
			int s = lua_tonumber(LS, -1);
			lua_input_controllerdata->axis3(pcid.first, pcid.second, 0, s);
			lua_pop(LS, 1);
			lua_pushstring(LS, "yaxis");
			lua_gettable(LS, 2);
			s = lua_tonumber(LS, -1);
			lua_input_controllerdata->axis3(pcid.first, pcid.second, 1, s);
			lua_pop(LS, 1);
		}
		return 0;
	});

	function_ptr_luafun ijlcid_to_pcid("input.lcid_to_pcid", [](lua_State* LS, const std::string& fname) -> int {
		unsigned lcid = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			return 0;
		lua_pushnumber(LS, (pcid.first - 1) * 4 + pcid.second);
		lua_pushnumber(LS, pcid.first);
		lua_pushnumber(LS, pcid.second);
		return 3;
	});
}
