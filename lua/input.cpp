#include "lua-int.hpp"

namespace
{
	class lua_input_set : public lua_function
	{
	public:
		lua_input_set() : lua_function("input.set") {}
		int invoke(lua_State* LS)
		{
			if(!lua_input_controllerdata)
				return 0;
			unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
			unsigned index = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
			short value = get_numeric_argument<short>(LS, 3, fname.c_str());
			if(controller > 7 || index > 11)
				return 0;
			(*lua_input_controllerdata)(controller >> 2, controller & 3, index) = value;
			return 0;
		}
	} input_set;

	class lua_input_get : public lua_function
	{
	public:
		lua_input_get() : lua_function("input.get") {}
		int invoke(lua_State* LS)
		{
			if(!lua_input_controllerdata)
				return 0;
			unsigned controller = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
			unsigned index = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
			if(controller > 7 || index > 11)
				return 0;
			lua_pushnumber(LS, (*lua_input_controllerdata)(controller >> 2, controller & 3, index));
			return 1;
		}
	} input_get;

	class lua_input_reset : public lua_function
	{
	public:
		lua_input_reset() : lua_function("input.reset") {}
		int invoke(lua_State* LS)
		{
			if(!lua_input_controllerdata)
				return 0;
			long cycles = 0;
			get_numeric_argument(LS, 1, cycles, fname.c_str());
			if(cycles < 0)
				return 0;
			short lo = cycles % 10000;
			short hi = cycles / 10000;
			(*lua_input_controllerdata)(CONTROL_SYSTEM_RESET) = 1;
			(*lua_input_controllerdata)(CONTROL_SYSTEM_RESET_CYCLES_HI) = hi;
			(*lua_input_controllerdata)(CONTROL_SYSTEM_RESET_CYCLES_LO) = lo;
			return 0;
		}
	} input_reset;
}