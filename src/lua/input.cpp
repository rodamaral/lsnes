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
	int input_set(lua::state& L, unsigned port, unsigned controller, unsigned index, short value)
	{
		if(!lua_input_controllerdata)
			return 0;
		lua_input_controllerdata->axis3(port, controller, index, value);
		return 0;
	}

	int input_get(lua::state& L, unsigned port, unsigned controller, unsigned index)
	{
		if(!lua_input_controllerdata)
			return 0;
		L.pushnumber(lua_input_controllerdata->axis3(port, controller, index));
		return 1;
	}

	int input_controllertype(lua::state& L, unsigned port, unsigned controller)
	{
		auto& m = get_movie();
		controller_frame f = m.read_subframe(m.get_current_frame(), 0);
		if(port >= f.get_port_count()) {
			L.pushnil();
			return 1;
		}
		const port_type& p = f.get_port_type(port);
		if(controller >= p.controller_info->controllers.size())
			L.pushnil();
		else
			L.pushlstring(p.controller_info->controllers[controller].type);
		return 1;
	}

	int input_seta(lua::state& L, unsigned port, unsigned controller, uint64_t base, lua::parameters& P)
	{
		if(!lua_input_controllerdata)
			return 0;
		short val;
		if(port >= lua_input_controllerdata->get_port_count())
			return 0;
		const port_type& pt = lua_input_controllerdata->get_port_type(port);
		if(controller >= pt.controller_info->controllers.size())
			return 0;
		for(unsigned i = 0; i < pt.controller_info->controllers[controller].buttons.size(); i++) {
			val = (base >> i) & 1;
			val = P.arg_opt<short>(val);
			lua_input_controllerdata->axis3(port, controller, i, val);
		}
		return 0;
	}

	int input_geta(lua::state& L, unsigned port, unsigned controller)
	{
		if(!lua_input_controllerdata)
			return 0;
		if(port >= lua_input_controllerdata->get_port_count())
			return 0;
		const port_type& pt = lua_input_controllerdata->get_port_type(port);
		if(controller >= pt.controller_info->controllers.size())
			return 0;
		uint64_t fret = 0;
		for(unsigned i = 0; i < pt.controller_info->controllers[controller].buttons.size(); i++)
			if(lua_input_controllerdata->axis3(port, controller, i))
				fret |= (1ULL << i);
		L.pushnumber(fret);
		for(unsigned i = 0; i < pt.controller_info->controllers[controller].buttons.size(); i++)
			L.pushnumber(lua_input_controllerdata->axis3(port, controller, i));
		return pt.controller_info->controllers[controller].buttons.size() + 1;
	}

	lua::fnptr2 iset(lua_func_misc, "input.set", [](lua::state& L, lua::parameters& P) -> int {
		if(!lua_input_controllerdata)
			return 0;

		auto controller = P.arg<unsigned>();
		auto index = P.arg<unsigned>();
		auto value = P.arg<short>();

		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_set(L, _controller.first, _controller.second, index, value);
	});

	lua::fnptr2 iset2(lua_func_misc, "input.set2", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto controller = P.arg<unsigned>();
		auto index = P.arg<unsigned>();
		auto value = P.arg<short>();

		return input_set(L, port, controller, index, value);
	});

	lua::fnptr2 iget(lua_func_misc, "input.get", [](lua::state& L, lua::parameters& P) -> int {
		if(!lua_input_controllerdata)
			return 0;

		auto controller = P.arg<unsigned>();
		auto index = P.arg<unsigned>();
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_get(L, _controller.first, _controller.second, index);
	});

	lua::fnptr2 iget2(lua_func_misc, "input.get2", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto controller = P.arg<unsigned>();
		auto index = P.arg<unsigned>();
		return input_get(L, port, controller, index);
	});

	lua::fnptr2 iseta(lua_func_misc, "input.seta", [](lua::state& L, lua::parameters& P) -> int {
		if(!lua_input_controllerdata)
			return 0;
		auto controller = P.arg<unsigned>();
		auto base = P.arg<uint64_t>();
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_seta(L, _controller.first, _controller.second, base, P);
	});

	lua::fnptr2 iseta2(lua_func_misc, "input.seta2", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto controller = P.arg<unsigned>();
		auto base = P.arg<uint64_t>();
		return input_seta(L, port, controller, base, P);
	});

	lua::fnptr2 igeta(lua_func_misc, "input.geta", [](lua::state& L, lua::parameters& P) -> int {
		if(!lua_input_controllerdata)
			return 0;
		auto controller = P.arg<unsigned>();
		auto _controller = lua_input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_geta(L, _controller.first, _controller.second);
	});

	lua::fnptr2 igeta2(lua_func_misc, "input.geta2", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto controller = P.arg<unsigned>();
		return input_geta(L, port, controller);
	});

	lua::fnptr2 igett(lua_func_misc, "input.controllertype", [](lua::state& L, lua::parameters& P) -> int {
		auto controller = P.arg<unsigned>();
		auto& m = get_movie();
		const port_type_set& s = m.read_subframe(m.get_current_frame(), 0).porttypes();
		auto _controller = s.legacy_pcid_to_pair(controller);
		return input_controllertype(L, _controller.first, _controller.second);
	});

	lua::fnptr2 igett2(lua_func_misc, "input.controllertype2", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto controller = P.arg<unsigned>();
		return input_controllertype(L, port, controller);
	});

	lua::fnptr2 ireset(lua_func_misc, "input.reset", [](lua::state& L, lua::parameters& P) -> int {
		if(!lua_input_controllerdata)
			return 0;
		auto cycles = P.arg<long>(0);
		if(cycles < 0)
			return 0;
		short lo = cycles % 10000;
		short hi = cycles / 10000;
		lua_input_controllerdata->axis3(0, 0, 1, 1);
		lua_input_controllerdata->axis3(0, 0, 2, hi);
		lua_input_controllerdata->axis3(0, 0, 3, lo);
		return 0;
	});

	lua::fnptr2 iraw(lua_func_misc, "input.raw", [](lua::state& L, lua::parameters& P) -> int {
		L.newtable();
		for(auto i : lsnes_kbd.all_keys()) {
			L.pushlstring(i->get_name());
			push_keygroup_parameters(L, *i);
			L.settable(-3);
		}
		return 1;
	});

	class _keyhook_listener : public keyboard::event_listener
	{
		void on_key_event(keyboard::modifier_set& modifiers, keyboard::key& key, keyboard::event& event)
		{
			lua_callback_keyhook(key.get_name(), key);
		}
	} keyhook_listener;
	std::set<std::string> hooked;

	lua::fnptr2 ireq(lua_func_misc, "input.keyhook", [](lua::state& L, lua::parameters& P) -> int {
		auto x = P.arg<std::string>();
		auto state = P.arg<bool>();

		keyboard::key* key = lsnes_kbd.try_lookup_key(x);
		if(!key)
			throw std::runtime_error("Invalid key name");
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

	lua::fnptr2 ijget(lua_func_misc, "input.joyget", [](lua::state& L, lua::parameters& P) -> int {
		auto lcid = P.arg<unsigned>();
		if(!lua_input_controllerdata)
			return 0;
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			throw std::runtime_error("Invalid controller for input.joyget");
		L.newtable();
		const port_type& pt = lua_input_controllerdata->get_port_type(pcid.first);
		const port_controller& ctrl = pt.controller_info->controllers[pcid.second];
		unsigned lcnt = ctrl.buttons.size();
		for(unsigned i = 0; i < lcnt; i++) {
			if(ctrl.buttons[i].type == port_controller_button::TYPE_NULL)
				continue;
			L.pushlstring(ctrl.buttons[i].name);
			if(ctrl.buttons[i].is_analog())
				L.pushnumber(lua_input_controllerdata->axis3(pcid.first, pcid.second, i));
			else if(ctrl.buttons[i].type == port_controller_button::TYPE_BUTTON)
				L.pushboolean(lua_input_controllerdata->axis3(pcid.first, pcid.second, i) != 0);
			L.settable(-3);
		}
		return 1;
	});

	lua::fnptr2 ijset(lua_func_misc, "input.joyset", [](lua::state& L, lua::parameters& P) -> int {
		auto lcid = P.arg<unsigned>();
		if(L.type(2) != LUA_TTABLE)
			throw std::runtime_error("Invalid type for input.joyset");
		if(!lua_input_controllerdata)
			return 0;
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			throw std::runtime_error("Invalid controller for input.joyset");
		const port_type& pt = lua_input_controllerdata->get_port_type(pcid.first);
		const port_controller& ctrl = pt.controller_info->controllers[pcid.second];
		unsigned lcnt = ctrl.buttons.size();
		for(unsigned i = 0; i < lcnt; i++) {
			if(ctrl.buttons[i].type == port_controller_button::TYPE_NULL)
				continue;
			L.pushlstring(ctrl.buttons[i].name);
			L.gettable(2);
			int s;
			if(ctrl.buttons[i].is_analog()) {
				if(L.type(-1) == LUA_TNIL)
					s = lua_input_controllerdata->axis3(pcid.first, pcid.second, i);
				else
					s = L.tonumber(-1);
			} else {
				if(L.type(-1) == LUA_TNIL)
					s = lua_input_controllerdata->axis3(pcid.first, pcid.second, i);
				else if(L.type(-1) == LUA_TSTRING)
					s = lua_input_controllerdata->axis3(pcid.first, pcid.second, i) ^ 1;
				else
					s = L.toboolean(-1) ? 1 : 0;
			}
			lua_input_controllerdata->axis3(pcid.first, pcid.second, i, s);
			L.pop(1);
		}
		return 0;
	});

	lua::fnptr2 ijlcid_to_pcid(lua_func_misc, "input.lcid_to_pcid", [](lua::state& L, lua::parameters& P) -> int {
		auto lcid = P.arg<unsigned>();
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

	lua::fnptr2 ijlcid_to_pcid2(lua_func_misc, "input.lcid_to_pcid2", [](lua::state& L, lua::parameters& P)
		-> int {
		auto lcid = P.arg<unsigned>();
		auto pcid = controls.lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			return 0;
		L.pushnumber(pcid.first);
		L.pushnumber(pcid.second);
		return 2;
	});

	lua::fnptr2 iporttype(lua_func_misc, "input.port_type", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto& m = get_movie();
		const port_type_set& s = m.read_subframe(m.get_current_frame(), 0).porttypes();
		try {
			const port_type& p = s.port_type(port);
			L.pushlstring(p.name);
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

	lua::fnptr2 iveto(lua_func_misc, "input.veto_button", [](lua::state& L, lua::parameters& P) -> int {
		if(lua_veto_flag) *lua_veto_flag = true;
		return 0;
	});

	lua::fnptr2 ictrlinfo(lua_func_misc, "input.controller_info", [](lua::state& L, lua::parameters& P) -> int {
		auto port = P.arg<unsigned>();
		auto controller = P.arg<unsigned>();
		const port_controller_set* ps;
		unsigned lcid = 0;
		unsigned classnum = 1;
		ps = lookup_ps(port);
		if(!ps || ps->controllers.size() <= controller)
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
			if(ps->controllers[controller].cclass == ps2->controllers[pcid.second].cclass)
				classnum++;
		}
		const port_controller& cs = ps->controllers[controller];
		L.newtable();
		L.pushstring("type");
		L.pushlstring(cs.type);
		L.rawset(-3);
		L.pushstring("class");
		L.pushlstring(cs.cclass);
		L.rawset(-3);
		L.pushstring("classnum");
		L.pushnumber(classnum);
		L.rawset(-3);
		L.pushstring("lcid");
		L.pushnumber(lcid);
		L.rawset(-3);
		L.pushstring("button_count");
		L.pushnumber(cs.buttons.size());
		L.rawset(-3);
		L.pushstring("buttons");
		L.newtable();
		//Push the buttons.
		for(unsigned i = 0; i < cs.buttons.size(); i++) {
			L.pushnumber(i + 1);
			L.newtable();
			L.pushstring("type");
			switch(cs.buttons[i].type) {
				case port_controller_button::TYPE_NULL: L.pushstring("null"); break;
				case port_controller_button::TYPE_BUTTON: L.pushstring("button"); break;
				case port_controller_button::TYPE_AXIS: L.pushstring("axis"); break;
				case port_controller_button::TYPE_RAXIS: L.pushstring("raxis"); break;
				case port_controller_button::TYPE_TAXIS: L.pushstring("axis"); break;
				case port_controller_button::TYPE_LIGHTGUN: L.pushstring("lightgun"); break;
			};
			L.rawset(-3);
			if(cs.buttons[i].symbol) {
				L.pushstring("symbol");
				L.pushlstring(&cs.buttons[i].symbol, 1);
				L.rawset(-3);
			}
			if(cs.buttons[i].macro != "") {
				L.pushstring("macro");
				L.pushlstring(cs.buttons[i].macro);
				L.rawset(-3);
			}
			if(cs.buttons[i].is_analog()) {
				L.pushstring("rmin");
				L.pushnumber(cs.buttons[i].rmin);
				L.rawset(-3);
				L.pushstring("rmax");
				L.pushnumber(cs.buttons[i].rmax);
				L.rawset(-3);
			}
			L.pushstring("name");
			L.pushlstring(cs.buttons[i].name);
			L.rawset(-3);
			L.pushstring("hidden");
			L.pushboolean(cs.buttons[i].shadow);
			L.rawset(-3);
			L.rawset(-3);
		}
		L.rawset(-3);
		return 1;
	});
}
