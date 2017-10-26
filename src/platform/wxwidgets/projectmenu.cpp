#include <functional>
#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "platform/wxwidgets/menu_projects.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "library/eatarg.hpp"
#include "core/instance.hpp"
#include "core/project.hpp"

projects_menu::projects_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high,
	const std::string& cfg, std::function<void(const std::string& id)> cb)
	: inst(_inst), hook(*this), rfiles(cfg, wxid_high - wxid_low - 1)	//Reserve wxid_low and wxid_high.
{
	CHECK_UI_THREAD;
	pwin = win;
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	selected_cb = cb;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(projects_menu::on_select), NULL, this);
	rfiles.add_hook(hook);
	update();
}

projects_menu::~projects_menu()
{
}

void projects_menu::on_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId();
	if(id < wxid_range_low || id > wxid_range_high)
		return;
	if(id == wxid_range_low) {
		//Other.
		auto projects = inst.project->enumerate();
		std::vector<std::string> a;
		std::vector<wxString> b;
		for(auto i : projects) {
			a.push_back(i.first);
			b.push_back(towxstring(i.second));
		}
		if(a.empty()) {
			show_message_ok(pwin, "Load project", "No projects available", wxICON_EXCLAMATION);
			return;
		}
		wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(pwin, wxT("Select project to switch to:"),
			wxT("Load project"), b.size(), &b[0]);
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			return;
		}
		std::string _id = a[d2->GetSelection()];
		selected_cb(_id);
	} else if(id == wxid_range_high) {
		//Refresh.
		update();
	} else {
		//Select.
		if(entries.count(id))
			selected_cb(entries[id]._id);
	}
}

void projects_menu::update()
{
	CHECK_UI_THREAD;
	auto ents = rfiles.get();
	int id = wxid_range_low + 1; //wxid_range_low is reserved for other.
	for(auto i : items)
		Delete(i.second);
	items.clear();
	bool has_ents = false;
	for(auto i : ents) {
		has_ents = true;
		if(id >= wxid_range_high)
			break;
		entries[id] = i;
		items[id] = Append(id, towxstring(i.display()));
		id++;
	}
	if(has_ents)
		items[-1] = AppendSeparator();
	items[wxid_range_high] = Append(wxid_range_high, wxT("Refresh"));
	items[-2] = AppendSeparator();
	items[wxid_range_low] = Append(wxid_range_low, wxT("Select other..."));
}
