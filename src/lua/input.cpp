#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"

namespace
{
	function_ptr_luafun iset("input.set", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 3, fname.c_str());
		short value = get_numeric_argument<short>(LS, 4, fname.c_str());
		lua_input_controllerdata->axis(port, controller, index, value);
		return 0;
	});

	function_ptr_luafun iget("input.get", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		unsigned index = get_numeric_argument<unsigned>(LS, 3, fname.c_str());
		lua_pushnumber(LS, lua_input_controllerdata->axis(port, controller, index));
		return 1;
	});

	function_ptr_luafun iseta("input.seta", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		short val;
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		uint64_t base = get_numeric_argument<uint64_t>(LS, 3, fname.c_str());
		for(unsigned i = 0; i < lua_input_controllerdata->control_count(); i++) {
			val = (base >> i) & 1;
			get_numeric_argument<short>(LS, i + 4, val, fname.c_str());
			lua_input_controllerdata->axis(port, controller, i, val);
		}
		return 0;
	});

	function_ptr_luafun igeta("input.geta", [](lua_State* LS, const std::string& fname) -> int {
		if(!lua_input_controllerdata)
			return 0;
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		uint64_t fret = 0;
		for(unsigned i = 0; i < lua_input_controllerdata->control_count(); i++)
			if(lua_input_controllerdata->axis(port, controller, i))
				fret |= (1ULL << i);
		lua_pushnumber(LS, fret);
		for(unsigned i = 0; i < lua_input_controllerdata->control_count(); i++)
			lua_pushnumber(LS, lua_input_controllerdata->axis(port, controller, i));
		return lua_input_controllerdata->control_count() + 1;
	});

	function_ptr_luafun igett("input.controllertype", [](lua_State* LS, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(LS, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(LS, 2, fname.c_str());
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		porttype_t p = f.get_port_type(port);
		const porttype_info& i = porttype_info::lookup(p);
		if(i.controllers <= controller)
			lua_pushnil(LS);
		else if(p == PT_NONE)
			lua_pushnil(LS);
		else if(p == PT_GAMEPAD)
			lua_pushstring(LS, "gamepad");
		else if(p == PT_MULTITAP)
			lua_pushstring(LS, "gamepad");
		else if(p == PT_MOUSE)
			lua_pushstring(LS, "mouse");
		else if(p == PT_SUPERSCOPE)
			lua_pushstring(LS, "superscope");
		else if(p == PT_JUSTIFIER)
			lua_pushstring(LS, "justifier");
		else if(p == PT_JUSTIFIERS)
			lua_pushstring(LS, "justifier");
		else
			lua_pushstring(LS, "unknown");
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
