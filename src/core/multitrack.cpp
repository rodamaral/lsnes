#include <string>
#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/moviedata.hpp"
#include "core/multitrack.hpp"
#include "lua/internal.hpp"

void update_movie_state();

bool multitrack_edit::is_enabled()
{
	return enabled;
}

void multitrack_edit::enable(bool state)
{
	{
		umutex_class h(mutex);
		enabled = state;
		controllerstate.clear();
	}
	update_movie_state();
}

void multitrack_edit::set(unsigned port, unsigned controller, state s)
{
	{
		umutex_class h(mutex);
		controllerstate[std::make_pair(port, controller)] = s;
	}
	update_movie_state();
}

void multitrack_edit::set_and_notify(unsigned port, unsigned controller, state s)
{
	if(!movb.get_movie().readonly_mode())
		return;
	set(port, controller, s);
	notify_multitrack_change(port, controller, (int)s);
}

void multitrack_edit::rotate(bool forward)
{
	if(!movb.get_movie().readonly_mode())
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
		notify_multitrack_change(x[i2].first, x[i2].second, s);
	}
	update_movie_state();
}

multitrack_edit::state multitrack_edit::get(unsigned port, unsigned controller)
{
	umutex_class h(mutex);
	auto key = std::make_pair(port, controller);
	if(controllerstate.count(key))
		return controllerstate[key];
	return MT_PRESERVE;
}

void multitrack_edit::config_altered()
{
	umutex_class h(mutex);
	controllerstate.clear();
}

void multitrack_edit::process_frame(controller_frame& input)
{
	if(!movb.get_movie().readonly_mode())
		return;
	umutex_class h(mutex);
	bool any_need = false;
	if(!enabled)
		return;
	for(auto i : controllerstate)
		any_need = any_need || (i.second != MT_PRESERVE);
	if(!any_need)
		return;	//No need to twiddle.
	unsigned indices = input.get_index_count();
	const port_type_set& portset = input.porttypes();
	pollcounter_vector& p = movb.get_movie().get_pollcounters();
	for(unsigned i = 0; i < indices; i++) {
		port_index_triple t = portset.index_to_triple(i);
		if(!t.valid)
			continue;
		auto key = std::make_pair(t.port, t.controller);
		uint32_t pc = p.get_polls(i);
		if(!controllerstate.count(key) || controllerstate[key] == MT_PRESERVE || (!t.port && !t.controller)) {
			int16_t v = movb.get_movie().read_subframe_at_index(pc, t.port, t.controller, t.control);
			input.axis3(t.port, t.controller, t.control, v);
		} else {
			int16_t v = movb.get_movie().read_subframe_at_index(pc, t.port, t.controller, t.control);
			auto m = controllerstate[key];
			const port_type& pt = portset.port_type(t.port);
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
			movb.get_movie().write_subframe_at_index(pc, t.port, t.controller, t.control, v);
			v = movb.get_movie().read_subframe_at_index(pc, t.port, t.controller, t.control);
			input.axis3(t.port, t.controller, t.control, v);
		}
	}
}

bool multitrack_edit::any_records()
{
	if(!movb.get_movie().readonly_mode())
		return true;
	umutex_class h(mutex);
	bool any_need = false;
	for(auto i : controllerstate)
		any_need = any_need || (i.second != MT_PRESERVE);
	return any_need;
}

namespace
{
	function_ptr_command<> rotate_forward(lsnes_cmd, "rotate-multitrack", "Rotate multitrack",
		"Syntax: rotate-multitrack\nRotate multitrack\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			multitrack_editor.rotate(true);
			update_movie_state();
		});

	function_ptr_command<> rotate_backward(lsnes_cmd, "rotate-multitrack-backwards", "Rotate multitrack backwards",
		"Syntax: rotate-multitrack-backwards\nRotate multitrack backwards\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			multitrack_editor.rotate(false);
			update_movie_state();
		});

	function_ptr_command<const std::string&> set_mt(lsnes_cmd, "set-multitrack", "Set multitrack mode",
		"Syntax: set-multitrack <controller> <mode>\nSet multitrack mode\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			regex_results r = regex("(.*)[ \t]+(.*)", args);
			if(!r)
				throw std::runtime_error("Bad arguments");
			auto c = controller_by_name(r[1]);
			if(c.first < 0)
				throw std::runtime_error("No such controller");
			if(r[2] == "keep")
				multitrack_editor.set_and_notify(c.first, c.second, multitrack_edit::MT_PRESERVE);
			else if(r[2] == "rewrite")
				multitrack_editor.set_and_notify(c.first, c.second, multitrack_edit::MT_OVERWRITE);
			else if(r[2] == "or")
				multitrack_editor.set_and_notify(c.first, c.second, multitrack_edit::MT_OR);
			else if(r[2] == "xor")
				multitrack_editor.set_and_notify(c.first, c.second, multitrack_edit::MT_XOR);
			else
				throw std::runtime_error("Invalid mode (keep, rewrite, or, xor)");
			update_movie_state();
		});

	inverse_bind _mtback(lsnes_mapper, "rotate-multitrack-backwards", "Multitrack‣Rotate backwards");
	inverse_bind _mtfwd(lsnes_mapper, "rotate-multitrack", "Multitrack‣Rotate forward");

	function_ptr_luafun mtlua(lua_func_misc, "input.multitrack_state", [](lua_state& L, const std::string& fname)
		-> int {
			unsigned port = L.get_numeric_argument<unsigned>(1, fname.c_str());
			unsigned controller = L.get_numeric_argument<unsigned>(2, fname.c_str());
			auto s = multitrack_editor.get(port, controller);
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
		});
}

multitrack_edit multitrack_editor;
