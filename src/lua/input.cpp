#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/controller.hpp"
#include "interface/romtype.hpp"
#include <iostream>

extern bool* lua_veto_flag;

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
		if(port >= f.get_port_count()) {
			L.pushnil();
			return 1;
		}
		const port_type& p = f.get_port_type(port);
		if(controller >= p.controller_info->controller_count)
			L.pushnil();
		else
			L.pushstring(p.controller_info->controllers[controller]->type);
		return 1;
	}

	int input_seta(lua_state& L, unsigned port, unsigned controller, uint64_t base, const char* fname)
	{
		if(!lua_input_controllerdata)
			return 0;
		short val;
		if(port >= lua_input_controllerdata->get_port_count())
			return 0;
		const port_type& pt = lua_input_controllerdata->get_port_type(port);
		if(controller >= pt.controller_info->controller_count)
			return 0;
		for(unsigned i = 0; i < pt.controller_info->controllers[controller]->button_count; i++) {
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
		if(controller >= pt.controller_info->controller_count)
			return 0;
		uint64_t fret = 0;
		for(unsigned i = 0; i < pt.controller_info->controllers[controller]->button_count; i++)
			if(lua_input_controllerdata->axis3(port, controller, i))
				fret |= (1ULL << i);
		L.pushnumber(fret);
		for(unsigned i = 0; i < pt.controller_info->controllers[controller]->button_count; i++)
			L.pushnumber(lua_input_controllerdata->axis3(port, controller, i));
		return pt.controller_info->controllers[controller]->button_count + 1;
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
		return input_seta(L, _controller.first, _controller.second, base, fname.c_str());
	});

	function_ptr_luafun iseta2(LS, "input.seta2", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		uint64_t base = L.get_numeric_argument<uint64_t>(3, fname.c_str());
		return input_seta(L, port, controller, base, fname.c_str());
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
		auto& m = get_movie();
		const port_type_set& s = m.read_subframe(m.get_current_frame(), 0).porttypes();
		auto _controller = s.legacy_pcid_to_pair(controller);
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
		L.newtable();
		for(auto i : lsnes_kbd.all_keys()) {
			L.pushstring(i->get_name().c_str());
			push_keygroup_parameters(L, *i);
			L.settable(-3);
		}
		return 1;
	});

	class _keyhook_listener : public keyboard_event_listener
	{
		void on_key_event(keyboard_modifier_set& modifiers, keyboard_key& key, keyboard_event& event)
		{
			lua_callback_keyhook(key.get_name(), key);
		}
	} keyhook_listener;
	std::set<std::string> hooked;

	function_ptr_luafun ireq(LS, "input.keyhook", [](lua_state& L, const std::string& fname) -> int {
		bool state;
		std::string x = L.get_string(1, fname.c_str());
		state = L.get_bool(2, fname.c_str());
		keyboard_key* key = lsnes_kbd.try_lookup_key(x);
		if(!key) {
			L.pushstring("Invalid key name");
			L.error();
			return 0;
		}
		bool ostate = hooked.count(x) > 0;
		if(ostate == state)
			return 0;
		if(state) {
			hooked.insert(x);
			key->add_listener(keyhook_listener, true);
		} else {
			hooked.erase(x);
			key->remove_listener(keyhook_listener);
		}
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
		const port_type& pt = lua_input_controllerdata->get_port_type(pcid.first);
		const port_controller& ctrl = *pt.controller_info->controllers[pcid.second];
		unsigned lcnt = ctrl.button_count;
		for(unsigned i = 0; i < lcnt; i++) {
			if(ctrl.buttons[i]->type == port_controller_button::TYPE_NULL)
				continue;
			L.pushstring(ctrl.buttons[i]->name);
			if(ctrl.buttons[i]->is_analog())
				L.pushnumber(lua_input_controllerdata->axis3(pcid.first, pcid.second, i));
			else if(ctrl.buttons[i]->type == port_controller_button::TYPE_BUTTON)
				L.pushboolean(lua_input_controllerdata->axis3(pcid.first, pcid.second, i) != 0);
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
		const port_type& pt = lua_input_controllerdata->get_port_type(pcid.first);
		const port_controller& ctrl = *pt.controller_info->controllers[pcid.second];
		unsigned lcnt = ctrl.button_count;
		for(unsigned i = 0; i < lcnt; i++) {
			if(ctrl.buttons[i]->type == port_controller_button::TYPE_NULL)
				continue;
			L.pushstring(ctrl.buttons[i]->name);
			L.gettable(2);
			int s;
			if(ctrl.buttons[i]->is_analog())
				s = L.tonumber(-1);
			else
				s = L.toboolean(-1) ? 1 : 0;
			lua_input_controllerdata->axis3(pcid.first, pcid.second, i, s);
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

	//THE NEW API.

	function_ptr_luafun ijlcid_to_pcid2(LS, "input.lcid_to_pcid2", [](lua_state& L, const std::string& fname) ->
		int {
		unsigned lcid = L.get_numeric_argument<unsigned>(1, fname.c_str());
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			return 0;
		L.pushnumber(pcid.first);
		L.pushnumber(pcid.second);
		return 2;
	});

	function_ptr_luafun iporttype(LS, "input.port_type", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		auto& m = get_movie();
		const port_type_set& s = m.read_subframe(m.get_current_frame(), 0).porttypes();
		try {
			const port_type& p = s.port_type(port);
			L.pushstring(p.hname.c_str());
		} catch(...) {
			return 0;
		}
		return 1;
	});

	const port_controller_set* lookup_ps(unsigned port)
	{
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		const port_type& p = f.get_port_type(port);
		return p.controller_info;
	}

	function_ptr_luafun iveto(LS, "input.veto_button", [](lua_state& LS, const std::string& fname) -> int {
		if(lua_veto_flag) *lua_veto_flag = true;
		return 0;
	});

	function_ptr_luafun ictrlinfo(LS, "input.controller_info", [](lua_state& L, const std::string& fname) -> int {
		unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
		unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
		const port_controller_set* ps;
		unsigned lcid = 0;
		unsigned classnum = 1;
		ps = lookup_ps(port);
		if(!ps || ps->controller_count <= controller)
			return 0;
		for(unsigned i = 0; i < 8; i++) {
			auto pcid = controls.lcid_to_pcid(i);
			if(pcid.first < 0)
				continue;
			if(pcid.first == port && pcid.second == controller) {
				lcid = i + 1;
				break;
			}
			const port_controller_set* ps2 = lookup_ps(pcid.first);
			if(!strcmp(ps->controllers[controller]->cclass, ps2->controllers[pcid.second]->cclass))
				classnum++;
		}
		port_controller* cs = ps->controllers[controller];
		L.newtable();
		L.pushstring("type");
		L.pushstring(cs->type);
		L.rawset(-3);
		L.pushstring("class");
		L.pushstring(cs->cclass);
		L.rawset(-3);
		L.pushstring("classnum");
		L.pushnumber(classnum);
		L.rawset(-3);
		L.pushstring("lcid");
		L.pushnumber(lcid);
		L.rawset(-3);
		L.pushstring("button_count");
		L.pushnumber(cs->button_count);
		L.rawset(-3);
		L.pushstring("buttons");
		L.newtable();
		//Push the buttons.
		for(unsigned i = 0; i < cs->button_count; i++) {
			L.pushnumber(i + 1);
			L.newtable();
			L.pushstring("type");
			switch(cs->buttons[i]->type) {
				case port_controller_button::TYPE_NULL: L.pushstring("null"); break;
				case port_controller_button::TYPE_BUTTON: L.pushstring("button"); break;
				case port_controller_button::TYPE_AXIS: L.pushstring("axis"); break;
				case port_controller_button::TYPE_RAXIS: L.pushstring("raxis"); break;
			};
			L.rawset(-3);
			if(cs->buttons[i]->symbol) {
				L.pushstring("symbol");
				L.pushlstring(&cs->buttons[i]->symbol, 1);
				L.rawset(-3);
			}
			L.pushstring("name");
			L.pushstring(cs->buttons[i]->name);
			L.rawset(-3);
			L.pushstring("hidden");
			L.pushboolean(cs->buttons[i]->shadow);
			L.rawset(-3);
			L.rawset(-3);
		}
		L.rawset(-3);
		return 1;
	});
}
