#include "core/keymapper.hpp"
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

	function_ptr_luafun iraw("input.raw", [](lua_State* LS, const std::string& fname) -> int {
		auto s = keygroup::get_all_parameters();
		lua_newtable(LS);
		for(auto i : s) {
			lua_pushstring(LS, i.first.c_str());
			lua_newtable(LS);
			lua_pushstring(LS, "last_rawval");
			lua_pushnumber(LS, i.second.last_rawval);
			lua_settable(LS, -3);
			lua_pushstring(LS, "cal_left");
			lua_pushnumber(LS, i.second.cal_left);
			lua_settable(LS, -3);
			lua_pushstring(LS, "cal_center");
			lua_pushnumber(LS, i.second.cal_center);
			lua_settable(LS, -3);
			lua_pushstring(LS, "cal_right");
			lua_pushnumber(LS, i.second.cal_right);
			lua_settable(LS, -3);
			lua_pushstring(LS, "cal_tolerance");
			lua_pushnumber(LS, i.second.cal_tolerance);
			lua_settable(LS, -3);
			lua_pushstring(LS, "ktype");
			switch(i.second.ktype) {
			case keygroup::KT_DISABLED:		lua_pushstring(LS, "disabled");		break;
			case keygroup::KT_KEY:			lua_pushstring(LS, "key");		break;
			case keygroup::KT_AXIS_PAIR:		lua_pushstring(LS, "axis");		break;
			case keygroup::KT_AXIS_PAIR_INVERSE:	lua_pushstring(LS, "axis-inverse");	break;
			case keygroup::KT_HAT:			lua_pushstring(LS, "hat");		break;
			case keygroup::KT_MOUSE:		lua_pushstring(LS, "mouse");		break;
			case keygroup::KT_PRESSURE_PM:		lua_pushstring(LS, "pressure-pm");	break;
			case keygroup::KT_PRESSURE_P0:		lua_pushstring(LS, "pressure-p0");	break;
			case keygroup::KT_PRESSURE_0M:		lua_pushstring(LS, "pressure-0m");	break;
			case keygroup::KT_PRESSURE_0P:		lua_pushstring(LS, "pressure-0p");	break;
			case keygroup::KT_PRESSURE_M0:		lua_pushstring(LS, "pressure-m0");	break;
			case keygroup::KT_PRESSURE_MP:		lua_pushstring(LS, "pressure-mp");	break;
			};
			lua_settable(LS, -3);
			lua_settable(LS, -3);
		}
		return 1;
	});
}
