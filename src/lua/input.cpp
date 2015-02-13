#include "core/keymapper.hpp"
#include "lua/internal.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/controller.hpp"
#include "interface/romtype.hpp"
#include <iostream>

namespace
{
	int input_set(lua::state& L, unsigned port, unsigned controller, unsigned index, short value)
	{
		auto& core = CORE();
		if(!core.lua2->input_controllerdata)
			return 0;
		core.lua2->input_controllerdata->axis3(port, controller, index, value);
		return 0;
	}

	int input_get(lua::state& L, unsigned port, unsigned controller, unsigned index)
	{
		auto& core = CORE();
		if(!core.lua2->input_controllerdata)
			return 0;
		L.pushnumber(core.lua2->input_controllerdata->axis3(port, controller, index));
		return 1;
	}

	int input_controllertype(lua::state& L, unsigned port, unsigned controller)
	{
		auto& core = CORE();
		auto& m = core.mlogic->get_movie();
		portctrl::frame f = m.read_subframe(m.get_current_frame(), 0);
		if(port >= f.get_port_count()) {
			L.pushnil();
			return 1;
		}
		const portctrl::type& p = f.get_port_type(port);
		if(controller >= p.controller_info->controllers.size())
			L.pushnil();
		else
			L.pushlstring(p.controller_info->controllers[controller].type);
		return 1;
	}

	int input_seta(lua::state& L, unsigned port, unsigned controller, uint64_t base, lua::parameters& P)
	{
		auto& core = CORE();
		if(!core.lua2->input_controllerdata)
			return 0;
		short val;
		if(port >= core.lua2->input_controllerdata->get_port_count())
			return 0;
		const portctrl::type& pt = core.lua2->input_controllerdata->get_port_type(port);
		if(controller >= pt.controller_info->controllers.size())
			return 0;
		for(unsigned i = 0; i < pt.controller_info->controllers[controller].buttons.size(); i++) {
			val = (base >> i) & 1;
			P(P.optional(val, val));
			core.lua2->input_controllerdata->axis3(port, controller, i, val);
		}
		return 0;
	}

	int input_geta(lua::state& L, unsigned port, unsigned controller)
	{
		auto& core = CORE();
		if(!core.lua2->input_controllerdata)
			return 0;
		if(port >= core.lua2->input_controllerdata->get_port_count())
			return 0;
		const portctrl::type& pt = core.lua2->input_controllerdata->get_port_type(port);
		if(controller >= pt.controller_info->controllers.size())
			return 0;
		uint64_t fret = 0;
		for(unsigned i = 0; i < pt.controller_info->controllers[controller].buttons.size(); i++)
			if(core.lua2->input_controllerdata->axis3(port, controller, i))
				fret |= (1ULL << i);
		L.pushnumber(fret);
		for(unsigned i = 0; i < pt.controller_info->controllers[controller].buttons.size(); i++)
			L.pushnumber(core.lua2->input_controllerdata->axis3(port, controller, i));
		return pt.controller_info->controllers[controller].buttons.size() + 1;
	}

	class _keyhook_listener : public keyboard::event_listener
	{
		void on_key_event(keyboard::modifier_set& modifiers, keyboard::key& key, keyboard::event& event)
		{
			auto& core = CORE();
			core.lua2->callback_keyhook(key.get_name(), key);
		}
	} S_keyhook_listener;

	const portctrl::controller_set* lookup_ps(unsigned port)
	{
		auto& core = CORE();
		auto& m = core.mlogic->get_movie();
		portctrl::frame f = m.read_subframe(m.get_current_frame(), 0);
		if(port >= f.get_port_count())
			return NULL;	//Port does not exist.
		const portctrl::type& p = f.get_port_type(port);
		return p.controller_info;
	}

	int set(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned controller, index, value;

		if(!core.lua2->input_controllerdata) return 0;

		P(controller, index, value);

		auto _controller = core.lua2->input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_set(L, _controller.first, _controller.second, index, value);
	}

	int set2(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller, index;
		short value;

		P(port, controller, index, value);

		return input_set(L, port, controller, index, value);
	}

	int get(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned controller, index;

		if(!core.lua2->input_controllerdata) return 0;

		P(controller, index);

		auto _controller = core.lua2->input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_get(L, _controller.first, _controller.second, index);
	}

	int get2(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller, index;

		P(port, controller, index);

		return input_get(L, port, controller, index);
	}

	int seta(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned controller;
		uint64_t base;

		if(!core.lua2->input_controllerdata) return 0;

		P(controller, base);

		auto _controller = core.lua2->input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_seta(L, _controller.first, _controller.second, base, P);
	}

	int seta2(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller;
		uint64_t base;

		P(port, controller, base);

		return input_seta(L, port, controller, base, P);
	}

	int geta(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned controller;

		if(!core.lua2->input_controllerdata) return 0;

		P(controller);

		auto _controller = core.lua2->input_controllerdata->porttypes().legacy_pcid_to_pair(controller);
		return input_geta(L, _controller.first, _controller.second);
	}

	int geta2(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller;

		P(port, controller);

		return input_geta(L, port, controller);
	}

	int controllertype(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned controller;

		P(controller);

		auto& m = core.mlogic->get_movie();
		const portctrl::type_set& s = m.read_subframe(m.get_current_frame(), 0).porttypes();
		auto _controller = s.legacy_pcid_to_pair(controller);
		return input_controllertype(L, _controller.first, _controller.second);
	}

	int controllertype2(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller;

		P(port, controller);

		return input_controllertype(L, port, controller);
	}

	int reset(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		long cycles;

		if(!core.lua2->input_controllerdata) return 0;

		P(P.optional(cycles, 0));

		if(cycles < 0)
			return 0;
		short lo = cycles % 10000;
		short hi = cycles / 10000;
		core.lua2->input_controllerdata->axis3(0, 0, 1, 1);
		core.lua2->input_controllerdata->axis3(0, 0, 2, hi);
		core.lua2->input_controllerdata->axis3(0, 0, 3, lo);
		return 0;
	}

	int raw(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		L.newtable();
		for(auto i : core.keyboard->all_keys()) {
			L.pushlstring(i->get_name());
			push_keygroup_parameters(L, *i);
			L.settable(-3);
		}
		return 1;
	}

	int keyhook(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		std::string x;
		bool state;

		P(x, state);

		keyboard::key* key = core.keyboard->try_lookup_key(x);
		if(!key)
			throw std::runtime_error("Invalid key name");
		bool ostate = core.lua2->hooked_keys.count(x) > 0;
		if(ostate == state)
			return 0;
		if(state) {
			core.lua2->hooked_keys.insert(x);
			key->add_listener(S_keyhook_listener, true);
		} else {
			core.lua2->hooked_keys.erase(x);
			key->remove_listener(S_keyhook_listener);
		}
		return 0;
	}

	int joyget(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned lcid;

		P(lcid);

		if(!core.lua2->input_controllerdata)
			return 0;
		auto pcid = core.controls->lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			throw std::runtime_error("Invalid controller for input.joyget");
		L.newtable();
		const portctrl::type& pt = core.lua2->input_controllerdata->get_port_type(pcid.first);
		const portctrl::controller& ctrl = pt.controller_info->controllers[pcid.second];
		unsigned lcnt = ctrl.buttons.size();
		for(unsigned i = 0; i < lcnt; i++) {
			if(ctrl.buttons[i].type == portctrl::button::TYPE_NULL)
				continue;
			L.pushlstring(ctrl.buttons[i].name);
			if(ctrl.buttons[i].is_analog())
				L.pushnumber(core.lua2->input_controllerdata->axis3(pcid.first, pcid.second, i));
			else if(ctrl.buttons[i].type == portctrl::button::TYPE_BUTTON)
				L.pushboolean(core.lua2->input_controllerdata->axis3(pcid.first, pcid.second, i) !=
					0);
			L.settable(-3);
		}
		return 1;
	}

	int joyset(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned lcid;
		int ltbl;

		P(lcid, P.table(ltbl));

		if(!core.lua2->input_controllerdata)
			return 0;
		auto pcid = core.controls->lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			throw std::runtime_error("Invalid controller for input.joyset");
		const portctrl::type& pt = core.lua2->input_controllerdata->get_port_type(pcid.first);
		const portctrl::controller& ctrl = pt.controller_info->controllers[pcid.second];
		unsigned lcnt = ctrl.buttons.size();
		for(unsigned i = 0; i < lcnt; i++) {
			if(ctrl.buttons[i].type == portctrl::button::TYPE_NULL)
				continue;
			L.pushlstring(ctrl.buttons[i].name);
			L.gettable(ltbl);
			int s;
			if(ctrl.buttons[i].is_analog()) {
				if(L.type(-1) == LUA_TNIL)
					s = core.lua2->input_controllerdata->axis3(pcid.first, pcid.second, i);
				else
					s = L.tonumber(-1);
			} else {
				if(L.type(-1) == LUA_TNIL)
					s = core.lua2->input_controllerdata->axis3(pcid.first, pcid.second, i);
				else if(L.type(-1) == LUA_TSTRING)
					s = core.lua2->input_controllerdata->axis3(pcid.first, pcid.second, i) ^ 1;
				else
					s = L.toboolean(-1) ? 1 : 0;
			}
			core.lua2->input_controllerdata->axis3(pcid.first, pcid.second, i, s);
			L.pop(1);
		}
		return 0;
	}

	int lcid_to_pcid(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned lcid;

		P(lcid);

		auto pcid = core.controls->lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			return 0;
		int legacy_pcid = -1;
		for(unsigned i = 0;; i++)
			try {
				auto p = core.controls->legacy_pcid_to_pair(i);
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
	}

	int lcid_to_pcid2(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned lcid;

		P(lcid);

		auto pcid = core.controls->lcid_to_pcid(lcid - 1);
		if(pcid.first < 0)
			return 0;
		L.pushnumber(pcid.first);
		L.pushnumber(pcid.second);
		return 2;
	}

	int _port_type(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned port;

		P(port);

		auto& m = core.mlogic->get_movie();
		const portctrl::type_set& s = m.read_subframe(m.get_current_frame(), 0).porttypes();
		try {
			const portctrl::type& p = s.port_type(port);
			L.pushlstring(p.name);
		} catch(...) {
			return 0;
		}
		return 1;
	}

	int veto_button(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		if(core.lua2->veto_flag) *core.lua2->veto_flag = true;
		return 0;
	}

	int controller_info(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		unsigned port, controller;

		P(port, controller);

		const portctrl::controller_set* ps;
		unsigned lcid = 0;
		unsigned classnum = 1;
		ps = lookup_ps(port);
		if(!ps || ps->controllers.size() <= controller)
			return 0;
		for(unsigned i = 0; i < 8; i++) {
			auto pcid = core.controls->lcid_to_pcid(i);
			if(pcid.first < 0)
				continue;
			if(pcid.first == (int)port && pcid.second == (int)controller) {
				lcid = i + 1;
				break;
			}
			const portctrl::controller_set* ps2 = lookup_ps(pcid.first);
			if(ps->controllers[controller].cclass == ps2->controllers[pcid.second].cclass)
				classnum++;
		}
		const portctrl::controller& cs = ps->controllers[controller];
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
				case portctrl::button::TYPE_NULL: L.pushstring("null"); break;
				case portctrl::button::TYPE_BUTTON: L.pushstring("button"); break;
				case portctrl::button::TYPE_AXIS: L.pushstring("axis"); break;
				case portctrl::button::TYPE_RAXIS: L.pushstring("raxis"); break;
				case portctrl::button::TYPE_TAXIS: L.pushstring("axis"); break;
				case portctrl::button::TYPE_LIGHTGUN: L.pushstring("lightgun"); break;
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
	}

	lua::functions LUA_input_fns(lua_func_misc, "input", {
		{"set", set},
		{"set2", set2},
		{"get", get},
		{"get2", get2},
		{"seta", seta},
		{"seta2", seta2},
		{"geta", geta},
		{"geta2", geta2},
		{"controllertype", controllertype},
		{"controllertype2", controllertype2},
		{"reset", reset},
		{"raw", raw},
		{"keyhook", keyhook},
		{"joyget", joyget},
		{"joyset", joyset},
		{"lcid_to_pcid", lcid_to_pcid},
		{"lcid_to_pcid2", lcid_to_pcid2},
		{"port_type", _port_type},
		{"veto_button", veto_button},
		{"controller_info", controller_info},
	});
}
