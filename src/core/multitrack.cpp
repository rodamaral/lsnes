#include "cmdhelp/multitrack.hpp"
#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/emustatus.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "core/movie.hpp"
#include "core/multitrack.hpp"
#include "lua/internal.hpp"

#include <string>

multitrack_edit::multitrack_edit(movie_logic& _mlogic, controller_state& _controls, emulator_dispatch& _dispatch,
	status_updater& _supdater, button_mapping& _buttons, command::group& _cmd)
	: mlogic(_mlogic), controls(_controls), edispatch(_dispatch), supdater(_supdater), buttons(_buttons),
	cmd(_cmd),
	mt_f(cmd, CMULTITRACK::f, [this]() { this->do_mt_fwd(); }),
	mt_b(cmd, CMULTITRACK::b, [this]() { this->do_mt_bw(); }),
	mt_s(cmd, CMULTITRACK::s, [this](const std::string& a) { this->do_mt_set(a); })
{
}

bool multitrack_edit::is_enabled()
{
	return enabled;
}

void multitrack_edit::enable(bool state)
{
	{
		threads::alock h(mlock);
		enabled = state;
		controllerstate.clear();
	}
	supdater.update();
}

void multitrack_edit::set(unsigned port, unsigned controller, state s)
{
	{
		threads::alock h(mlock);
		controllerstate[std::make_pair(port, controller)] = s;
	}
	supdater.update();
}

void multitrack_edit::set_and_notify(unsigned port, unsigned controller, state s)
{
	if(!mlogic || !mlogic.get_movie().readonly_mode())
		return;
	set(port, controller, s);
	edispatch.multitrack_change(port, controller, (int)s);
}

void multitrack_edit::rotate(bool forward)
{
	if(!mlogic || !mlogic.get_movie().readonly_mode())
		return;
	std::vector<std::pair<unsigned, unsigned>> x;
	for(unsigned i = 0;; i++) {
		auto pcid = controls.lcid_to_pcid(i);
		if(pcid.first < 0)
			break;
		x.push_back(std::make_pair(pcid.first, pcid.second));
	}
	auto old_controllerstate = controllerstate;
	for(unsigned i = 0; i < x.size(); i++) {
		state s = MT_PRESERVE;
		if(old_controllerstate.count(x[i]))
			s = old_controllerstate[x[i]];
		unsigned i2;
		if(forward) {
			i2 = i + 1;
			if(i2 >= x.size())
				i2 = 0;
		} else {
			i2 = i - 1;
			if(i2 >= x.size())
				i2 = x.size() - 1;
		}
		controllerstate[x[i2]] = s;
		edispatch.multitrack_change(x[i2].first, x[i2].second, s);
	}
	supdater.update();
}

multitrack_edit::state multitrack_edit::get(unsigned port, unsigned controller)
{
	threads::alock h(mlock);
	auto key = std::make_pair(port, controller);
	if(controllerstate.count(key))
		return controllerstate[key];
	return MT_PRESERVE;
}

void multitrack_edit::config_altered()
{
	threads::alock h(mlock);
	controllerstate.clear();
}

void multitrack_edit::process_frame(portctrl::frame& input)
{
	if(!mlogic || !mlogic.get_movie().readonly_mode())
		return;
	threads::alock h(mlock);
	bool any_need = false;
	if(!enabled)
		return;
	for(auto i : controllerstate)
		any_need = any_need || (i.second != MT_PRESERVE);
	if(!any_need)
		return;	//No need to twiddle.
	unsigned indices = input.get_index_count();
	const portctrl::type_set& portset = input.porttypes();
	portctrl::counters& p = mlogic.get_movie().get_pollcounters();
	for(unsigned i = 0; i < indices; i++) {
		portctrl::index_triple t = portset.index_to_triple(i);
		if(!t.valid)
			continue;
		auto key = std::make_pair(t.port, t.controller);
		uint32_t pc = p.get_polls(i);
		if(!controllerstate.count(key) || controllerstate[key] == MT_PRESERVE || (!t.port && !t.controller)) {
			int16_t v = mlogic.get_movie().read_subframe_at_index(pc, t.port, t.controller,
				t.control);
			input.axis3(t.port, t.controller, t.control, v);
		} else {
			int16_t v = mlogic.get_movie().read_subframe_at_index(pc, t.port, t.controller,
				t.control);
			controllerstate[key];
			const portctrl::type& pt = portset.port_type(t.port);
			auto pci = pt.controller_info->get(t.controller);
			auto pb = pci ? pci->get(t.control) : NULL;
			bool is_axis = (pb && pb->is_analog());
			switch(controllerstate[key]) {
			case MT_OR:
				if(is_axis) {
					int16_t v2 = input.axis3(t.port, t.controller, t.control);
					if(v2)
						v = v2;
				} else
					v |= input.axis3(t.port, t.controller, t.control);
				break;
			case MT_OVERWRITE:
				v = input.axis3(t.port, t.controller, t.control);
				break;
			case MT_PRESERVE:
				//No-op.
				break;
			case MT_XOR:
				if(is_axis) {
					int16_t v2 = input.axis3(t.port, t.controller, t.control);
					if(v2)
						v = v2;
				} else
					v ^= input.axis3(t.port, t.controller, t.control);
				break;
			}
			mlogic.get_movie().write_subframe_at_index(pc, t.port, t.controller, t.control,
				v);
			v = mlogic.get_movie().read_subframe_at_index(pc, t.port, t.controller,
				t.control);
			input.axis3(t.port, t.controller, t.control, v);
		}
	}
}

bool multitrack_edit::any_records()
{
	if(!mlogic || !mlogic.get_movie().readonly_mode())
		return true;
	threads::alock h(mlock);
	bool any_need = false;
	for(auto i : controllerstate)
		any_need = any_need || (i.second != MT_PRESERVE);
	return any_need;
}

void multitrack_edit::do_mt_set(const std::string& args)
{
	regex_results r = regex("(.*)[ \t]+(.*)", args);
	if(!r)
		throw std::runtime_error("Bad arguments");
	auto c = buttons.byname(r[1]);
	if(c.first < 0)
		throw std::runtime_error("No such controller");
	if(r[2] == "keep")
		set_and_notify(c.first, c.second, multitrack_edit::MT_PRESERVE);
	else if(r[2] == "rewrite")
		set_and_notify(c.first, c.second, multitrack_edit::MT_OVERWRITE);
	else if(r[2] == "or")
		set_and_notify(c.first, c.second, multitrack_edit::MT_OR);
	else if(r[2] == "xor")
		set_and_notify(c.first, c.second, multitrack_edit::MT_XOR);
	else
		throw std::runtime_error("Invalid mode (keep, rewrite, or, xor)");
	supdater.update();
}

void multitrack_edit::do_mt_fwd()
{
	rotate(true);
	supdater.update();
}

void multitrack_edit::do_mt_bw()
{
	rotate(false);
	supdater.update();
}

namespace
{
	int multitrack_state(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller;

		P(port, controller);

		auto s = CORE().mteditor->get(port, controller);
		switch(s) {
		case multitrack_edit::MT_OR:
			L.pushstring("or");
			return 1;
		case multitrack_edit::MT_OVERWRITE:
			L.pushstring("rewrite");
			return 1;
		case multitrack_edit::MT_PRESERVE:
			L.pushstring("keep");
			return 1;
		case multitrack_edit::MT_XOR:
			L.pushstring("xor");
			return 1;
		default:
			return 0;
		}
	}

	lua::functions LUA_mtfn(lua_func_misc, "input", {
		{"multitrack_state", multitrack_state},
	});
}
