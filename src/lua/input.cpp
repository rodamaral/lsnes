#include "core/lua-int.hpp"

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
}
