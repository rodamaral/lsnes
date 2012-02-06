#include "core/advdumper.hpp"
#include "core/dispatch.hpp"

#include "plat-wxwidgets/menu_dump.hpp"
#include "plat-wxwidgets/platform.hpp"

struct dumper_info
{
	adv_dumper* instance;
	std::string name;
	bool active;
	std::map<std::string, std::string> modes;
};

namespace
{
	std::map<std::string, dumper_info> existing_dumpers;

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

	void update_dumperinfo(std::map<std::string, dumper_info>& new_dumpers, adv_dumper* d)
	{
		struct dumper_info inf;
		inf.instance = d;
		inf.name = d->name();
		std::set<std::string> mset = d->list_submodes();
		for(auto i : mset)
			inf.modes[i] = d->modename(i);
		inf.active = d->busy();
		new_dumpers[d->id()] = inf;
	}
}

class dumper_menu_monitor : public information_dispatch
{
public:
	dumper_menu_monitor(dumper_menu* dmenu)
		: information_dispatch("wxwidgets-dumpmenu")
	{
		linked = dmenu;
	}

	~dumper_menu_monitor()
	{
	}

	void on_dumper_update()
	{
		std::map<std::string, dumper_info> new_dumpers;
		std::set<adv_dumper*> dset = adv_dumper::get_dumper_set();
		for(auto i : dset)
			update_dumperinfo(new_dumpers, i);
		runuifun([linked, new_dumpers]() { if(linked) linked->update(new_dumpers); });
	}
private:
	dumper_menu* linked;
};



dumper_menu::dumper_menu(wxWindow* win, int wxid_low, int wxid_high)
{
	pwin = win;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(dumper_menu::on_select), NULL, this);
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	monitor = new dumper_menu_monitor(this);
	std::map<std::string, dumper_info> new_dumpers;
	runemufn([&new_dumpers]() {
		std::set<adv_dumper*> dset = adv_dumper::get_dumper_set();
		for(auto i : dset)
			update_dumperinfo(new_dumpers, i);
		});
	update(new_dumpers);
}

dumper_menu::~dumper_menu()
{
	delete monitor;
}

void dumper_menu::on_select(wxCommandEvent& e)
{
	int id = e.GetId();
	if(id < wxid_range_low || id > wxid_range_high)
		return;
	for(auto i : menustructure) {
		adv_dumper* t = existing_dumpers[i.first].instance;
		if(i.second.end_wxid == id) {
			//Execute end of dump operation.
			runemufn([t]() { t->end(); });
			return;
		}
		if(i.second.start_wxids.count(id)) {
			//Execute start of dump operation.
			std::string mode = i.second.start_wxids[id];
			bool prefixed = t->wants_prefix(mode);
			std::string prefix;
			wxFileDialog* d = new wxFileDialog(pwin, towxstring(prefixed ? std::string("Choose prefix") :
				std::string("Choose file")), wxT("."));
			if(d->ShowModal() == wxID_OK)
				prefix = tostdstring(d->GetPath());
			d->Destroy();
			if(prefix == "")
				return;
			runemufn([t, mode, prefix]() { t->start(mode, prefix); });
			return;
		}
	}
}

void dumper_menu::update(const std::map<std::string, dumper_info>& new_dumpers)
{
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
	for(auto i : new_dumpers) {
		if(!first)
			menustructure[last_processed].sep = AppendSeparator();
		last_processed = i.first;
		first = false;
		menustructure[i.first].end_item = NULL;
		menustructure[i.first].end_wxid = wxID_ANY;
		if(!i.second.active) {
			if(i.second.modes.empty()) {
				menustructure[i.first].start_items[id] = Append(id, towxstring("Dump " +
					i.second.name));
				menustructure[i.first].start_wxids[id++] = "";
			}
			for(auto j : i.second.modes) {
				menustructure[i.first].start_items[id] = Append(id, towxstring("Dump " +
					i.second.name + " (" + j.second + ")"));
				menustructure[i.first].start_wxids[id++] = j.first;
			}
		} else {
			menustructure[i.first].end_item = Append(id, towxstring("End " + i.second.name));
			menustructure[i.first].end_wxid = id++;
		}
	}
	existing_dumpers = new_dumpers;
}
