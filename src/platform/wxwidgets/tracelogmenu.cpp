#include <wx/wx.h>
#include <wx/statline.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>

#include "platform/wxwidgets/menu_tracelog.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include "core/debug.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/project.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/ui-services.hpp"


tracelog_menu::tracelog_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high)
	: inst(_inst)
{
	CHECK_UI_THREAD;
	pwin = win;
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(tracelog_menu::on_select), NULL, this);
	inst.dbg->set_tracelog_change_cb([this]() { runuifun([this]() { this->update(); }); });
	corechange.set(inst.dispatch->core_change, [this]() { runuifun([this]() { this->update(); }); });
}

tracelog_menu::~tracelog_menu()
{
}

void tracelog_menu::on_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId() - wxid_range_low;
	if(id < 0 || id > wxid_range_high - wxid_range_low) return;
	int rid = id / 2;
	if(!cpunames.count(rid))
		return;
	if(id % 2) {
		wxeditor_tracelog_display(pwin, inst, rid, cpunames[rid]);
	} else {
		bool ch = items[rid]->IsChecked();
		if(ch) {
			try {
				std::string filename = choose_file_save(pwin, "Save " + cpunames[rid] + " Trace",
					UI_get_project_moviepath(inst), filetype_trace, "");
				inst.dbg->tracelog(rid, filename);
			} catch(canceled_exception& e) {
			}
		} else {
			inst.dbg->tracelog(rid, "");
		}
	}
	update();
}

void tracelog_menu::update()
{
	CHECK_UI_THREAD;
	auto _items = inst.rom->get_trace_cpus();
	for(auto i : items)
		Delete(i);
	items.clear();
	unsigned id = 0;
	for(auto i : _items) {
		items.push_back(AppendCheckItem(wxid_range_low + 2 * id, towxstring(i + " (to file)...")));
		cpunames[id] = i;
		items[id]->Check(inst.dbg->is_tracelogging(id));
		id++;
	}
	items.push_back(AppendSeparator());
	id = 0;
	for(auto i : _items) {
		items.push_back(Append(wxid_range_low + 2 * id + 1, towxstring(i + " (to window)...")));
		id++;
	}
	if(disabler_fn) disabler_fn(!_items.empty());
}

bool tracelog_menu::any_enabled()
{
	return !items.empty();
}
