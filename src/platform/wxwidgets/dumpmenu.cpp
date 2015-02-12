#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/project.hpp"
#include "core/ui-services.hpp"

#include "platform/wxwidgets/menu_dump.hpp"
#include "platform/wxwidgets/platform.hpp"

struct dumper_info
{
	dumper_factory_base* instance;
	std::string name;
	bool active;
	std::map<std::string, std::string> modes;
};

namespace
{
	struct dumper_menu_struct
	{
		int end_wxid;
		wxMenuItem* end_item;
		std::map<int, std::string> start_wxids;
		std::map<int, wxMenuItem*> start_items;
		wxMenuItem* sep;
	};
	std::map<std::string, dumper_menu_struct> menustructure;
	std::string last_processed;
	bool first;

}

class dumper_menu_monitor : public master_dumper::notifier
{
public:
	dumper_menu_monitor(dumper_menu* dmenu)
	{
		linked = dmenu;
	}

	~dumper_menu_monitor() throw()
	{
	}

	void dumpers_updated() throw()
	{
		runuifun([this]() { if(this->linked) this->linked->update(); });
	}
	void dump_status_change() throw()
	{
		dumpers_updated();
	}
private:
	dumper_menu* linked;
};

dumper_menu::dumper_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high)
	: inst(_inst)
{
	CHECK_UI_THREAD;
	pwin = win;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(dumper_menu::on_select), NULL, this);
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	monitor = new dumper_menu_monitor(this);
	inst.mdumper->add_notifier(*monitor);
	update();
}

dumper_menu::~dumper_menu()
{
	inst.mdumper->drop_notifier(*monitor);
	delete monitor;
}

void dumper_menu::on_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId();
	if(id < wxid_range_low || id > wxid_range_high)
		return;
	for(auto i : menustructure) {
		dumper_factory_base* t = existing_dumpers[i.first].factory;
		if(i.second.end_wxid == id) {
			UI_end_dump(inst, *t);
			return;
		}
		if(i.second.start_wxids.count(id)) {
			//Execute start of dump operation.
			std::string mode = i.second.start_wxids[id];
			unsigned d = t->mode_details(mode);
			std::string prefix;
			if((d & dumper_factory_base::target_type_mask) == dumper_factory_base::target_type_file) {
				wxFileDialog* d = new wxFileDialog(pwin, wxT("Choose file"),
					towxstring(UI_get_project_otherpath(inst)), wxT(""), wxT("*.*"),
					wxFD_SAVE);
				std::string modext = t->mode_extension(mode);
					d->SetWildcard(towxstring(modext + " files|*." + modext));
				auto p = inst.project->get();
				if(p)
					d->SetFilename(towxstring(p->prefix + "." + modext));
				if(d->ShowModal() == wxID_OK)
					prefix = tostdstring(d->GetPath());
				d->Destroy();
			} else if((d & dumper_factory_base::target_type_mask) ==
				dumper_factory_base::target_type_prefix) {
				wxFileDialog* d = new wxFileDialog(pwin, wxT("Choose prefix"),
					towxstring(UI_get_project_otherpath(inst)), wxT(""), wxT("*.*"),
					wxFD_SAVE);
				auto p = inst.project->get();
				if(p)
					d->SetFilename(towxstring(p->prefix));
				if(d->ShowModal() == wxID_OK)
					prefix = tostdstring(d->GetPath());
				d->Destroy();
			} else if((d & dumper_factory_base::target_type_mask) ==
				dumper_factory_base::target_type_special) {
				try {
					prefix = pick_text(pwin, "Choose target", "Enter target to dump to", "");
				} catch(...) {
					return;
				}
			} else {
				wxMessageBox(wxT("Unsupported target type"), _T("Dumper error"), wxICON_EXCLAMATION |
					wxOK, pwin);
				return;
			}
			if(prefix == "")
				return;
			try {
				UI_start_dump(inst, *t, mode, prefix);
			} catch(std::exception& e) {
				show_exception(this->pwin, "Error starting dump", "", e);
			}
			return;
		}
	}
}

void dumper_menu::update()
{
	CHECK_UI_THREAD;
	dumper_information dinfo = UI_get_dumpers(inst);
	//Destroy all old entries.
	for(auto i : menustructure) {
		struct dumper_menu_struct& m = i.second;
		if(m.end_item)
			Remove(m.end_item);
		for(auto mi : m.start_items)
			Remove(mi.second);
		if(m.sep)
			Remove(m.sep);
	}
	//And create new ones.
	int id = wxid_range_low;
	first = true;
	menustructure.clear();
	for(auto i : dinfo.dumpers) {
		//Skip dumper called "NULL" unless actually active, since it doesn't really work.
		if(i.second.hidden && !i.second.active)
			continue;
		if(!first)
			menustructure[last_processed].sep = AppendSeparator();
		last_processed = i.first;
		first = false;
		menustructure[i.first].end_item = NULL;
		menustructure[i.first].end_wxid = wxID_ANY;
		if(!i.second.active) {
			if(i.second.modes.empty()) {
				menustructure[i.first].start_items[id] = Append(id, towxstring("Dump " +
					i.second.name + "..."));
				menustructure[i.first].start_wxids[id++] = "";
			}
			for(auto j : i.second.modes) {
				menustructure[i.first].start_items[id] = Append(id, towxstring("Dump " +
					i.second.name + " (" + j.second + ")..."));
				menustructure[i.first].start_wxids[id++] = j.first;
			}
		} else {
			menustructure[i.first].end_item = Append(id, towxstring("End " + i.second.name));
			menustructure[i.first].end_wxid = id++;
		}
	}
	existing_dumpers = dinfo.dumpers;
}
