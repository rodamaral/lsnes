#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/emucore.hpp"
#include "core/moviedata.hpp"
#include "core/controller.hpp"
#include <iostream>

namespace
{
	int input_set(lua_state& L, unsigned port, unsigned controller, unsigned index, short value)
	{
		if(!lua_input_controllerdata)
			return 0;
		lua_input_controllerdata->axis3(port, controller, index, value);
		return 0;
	}

	int input_get(lua_state& L, unsigned port, unsigned controller, unsigned index)
	{
		if(!lua_input_controllerdata)
			return 0;
		L.pushnumber(lua_input_controllerdata->axis3(port, controller, index));
		return 1;
	}

	int input_controllertype(lua_state& L, unsigned port, unsigned controller)
	{
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		const port_type& p = f.get_port_type(port);
		if(p.controllers <= controller)
			L.pushnil();
		else if(p.ctrlname == "")
			L.pushnil();
		else
			L.pushstring(p.ctrlname.c_str());
		return 1;
	}

	int input_seta(lua_state& L, unsigned port, unsigned controller, int base, const char* fname)
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
			L.get_numeric_argument<short>(i + base, val, fname);
			lua_input_controllerdata->axis3(port, controller, i, val);
		}
		return 0;
	}

	int input_geta(lua_state& L, unsigned port, unsigned controller)
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
		L.pushnumber(fret);
		for(unsigned i = 0; i < pt.controller_indices[controller]; i++)
			L.pushnumber(lua_input_controllerdata->axis3(port, controller, i));
		return pt.controller_indices[controller] + 1;
	}

	function_ptr_luafun iset(LS, "input.set", [](lua_state& L, const std::string& fname) -> int {
		unsigned controller = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned index = L.get_numeric_argument<unsigned>(2, fname.c_str());
		short value = L.get_numeric_argument<short>(3, fname.c_str());
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_set(L, _controller.first, _controller.second, index, value);
	});

	function_ptr_luafun iset2(LS, "input.set2", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		unsigned index = L.get_numeric_argument<unsigned>(3, fname.c_str());
		short value = L.get_numeric_argument<short>(4, fname.c_str());
		return input_set(L, port, controller, index, value);
	});

	function_ptr_luafun iget(LS, "input.get", [](lua_state& L, const std::string& fname) -> int {
		unsigned controller = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned index = L.get_numeric_argument<unsigned>(2, fname.c_str());
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_get(L, _controller.first, _controller.second, index);
	});

	function_ptr_luafun iget2(LS, "input.get2", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		unsigned index = L.get_numeric_argument<unsigned>(3, fname.c_str());
		return input_get(L, port, controller, index);
	});

	function_ptr_luafun iseta(LS, "input.seta", [](lua_state& L, const std::string& fname) -> int {
		unsigned controller = L.get_numeric_argument<unsigned>(1, fname.c_str());
		uint64_t base = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_seta(L, _controller.first, _controller.second, 3, fname.c_str());
	});

	function_ptr_luafun iseta2(LS, "input.seta2", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		uint64_t base = L.get_numeric_argument<uint64_t>(3, fname.c_str());
		return input_seta(L, port, controller, 4, fname.c_str());
	});

	function_ptr_luafun igeta(LS, "input.geta", [](lua_state& L, const std::string& fname) -> int {
		unsigned controller = L.get_numeric_argument<unsigned>(1, fname.c_str());
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_geta(L, _controller.first, _controller.second);
	});

	function_ptr_luafun igeta2(LS, "input.geta2", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		return input_geta(L, port, controller);
	});

	function_ptr_luafun igett(LS, "input.controllertype", [](lua_state& L, const std::string& fname) -> int {
		unsigned controller = L.get_numeric_argument<unsigned>(1, fname.c_str());
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_controllertype(L, _controller.first, _controller.second);
	});

	function_ptr_luafun igett2(LS, "input.controllertype2", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		return input_controllertype(L, port, controller);
	});

	function_ptr_luafun ireset(LS, "input.reset", [](lua_state& L, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		long cycles = 0;
		L.get_numeric_argument(1, cycles, fname.c_str());
		if(cycles < 0)
			return 0;
		short lo = cycles % 10000;
		short hi = cycles / 10000;
		lua_input_controllerdata->axis3(0, 0, 1, 1);
		lua_input_controllerdata->axis3(0, 0, 2, hi);
		lua_input_controllerdata->axis3(0, 0, 3, lo);
		return 0;
	});

	function_ptr_luafun iraw(LS, "input.raw", [](lua_state& L, const std::string& fname) -> int {
		auto s = keygroup::get_all_parameters();
		L.newtable();
		for(auto i : s) {
			L.pushstring(i.first.c_str());
			push_keygroup_parameters(L, i.second);
			L.settable(-3);
		}
		return 1;
	});

	function_ptr_luafun ireq(LS, "input.keyhook", [](lua_state& L, const std::string& fname) -> int {
		struct keygroup* k;
		bool state;
		std::string x = L.get_string(1, fname.c_str());
		state = L.get_bool(2, fname.c_str());
		k = keygroup::lookup_by_name(x);
		if(!k) {
			L.pushstring("Invalid key name");
			L.error();
			return 0;
		}
		k->request_hook_callback(state);
		return 0;
	});

	function_ptr_luafun ijget(LS, "input.joyget", [](lua_state& L, const std::string& fname) -> int {
		unsigned lcid = L.get_numeric_argument<unsigned>(1, fname.c_str());
		if(!lua_input_controllerdata)
			return 0;
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0) {
			L.pushstring("Invalid controller for input.joyget");
			L.error();
			return 0;
		}
		L.newtable();
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid.first, pcid.second, i);
			if(y < 0)
				continue;
			L.pushstring(n.c_str());
			L.pushboolean(lua_input_controllerdata->axis3(pcid.first, pcid.second, y) != 0);
			L.settable(-3);
		}
		if(lua_input_controllerdata->is_analog(pcid.first, pcid.second)) {
			L.pushstring("xaxis");
			L.pushnumber(lua_input_controllerdata->axis3(pcid.first, pcid.second, 0));
			L.settable(-3);
			L.pushstring("yaxis");
			L.pushnumber(lua_input_controllerdata->axis3(pcid.first, pcid.second, 1));
			L.settable(-3);
		}
		return 1;
	});

	function_ptr_luafun ijset(LS, "input.joyset", [](lua_state& L, const std::string& fname) -> int {
		unsigned lcid = L.get_numeric_argument<unsigned>(1, fname.c_str());
		if(L.type(2) != LUA_TTABLE) {
			L.pushstring("Invalid type for input.joyset");
			L.error();
			return 0;
		}
		if(!lua_input_controllerdata)
			return 0;
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0) {
			L.pushstring("Invalid controller for input.joyset");
			L.error();
			return 0;
		}
		unsigned lcnt = get_core_logical_controller_limits().second;
		for(unsigned i = 0; i < lcnt; i++) {
			std::string n = get_logical_button_name(i);
			int y = lua_input_controllerdata->button_id(pcid.first, pcid.second, i);
			if(y < 0)
				continue;
			L.pushstring(n.c_str());
			L.gettable(2);
			int s = L.toboolean(-1) ? 1 : 0;
			lua_input_controllerdata->axis3(pcid.first, pcid.second, y, s);
			L.pop(1);
		}
		if(lua_input_controllerdata->is_analog(pcid.first, pcid.second)) {
			L.pushstring("xaxis");
			L.gettable(2);
			int s = L.tonumber(-1);
			lua_input_controllerdata->axis3(pcid.first, pcid.second, 0, s);
			L.pop(1);
			L.pushstring("yaxis");
			L.gettable(2);
			s = L.tonumber(-1);
			lua_input_controllerdata->axis3(pcid.first, pcid.second, 1, s);
			L.pop(1);
		}
		return 0;
	});

	function_ptr_luafun ijlcid_to_pcid(LS, "input.lcid_to_pcid", [](lua_state& L, const std::string& fname) ->
		int {
		unsigned lcid = L.get_numeric_argument<unsigned>(1, fname.c_str());
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			return 0;
		int legacy_pcid = -1;
		for(unsigned i = 0;; i++)
			try {
				auto p = controls.legacy_pcid_to_pair(i);
				if(p.first == pcid.first && p.second == pcid.second) {
					legacy_pcid = i;
					break;
				}
			} catch(...) {
				break;
			}
		if(legacy_pcid >= 0)
			L.pushnumber(legacy_pcid);
		else
			L.pushboolean(false);
		L.pushnumber(pcid.first);
		L.pushnumber(pcid.second);
		return 3;
	});
}
