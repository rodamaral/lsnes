#include "core/keymapper.hpp"
#include "lua/internal.hpp"

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
}
