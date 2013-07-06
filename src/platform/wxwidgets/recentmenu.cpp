#include "platform/wxwidgets/menu_recent.hpp"
#include "platform/wxwidgets/platform.hpp"

recent_menu::recent_menu(wxWindow* win, int wxid_low, int wxid_high, const std::string& cfg,
	void (*cb)(const std::string& name))
	: hook(*this), rfiles(cfg, wxid_high - wxid_low)	//Reserve wxid_high for refresh.
{
	pwin = win;
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	callback = cb;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(recent_menu::on_select), NULL, this);
	rfiles.add_hook(hook);
	update();
}

void recent_menu::on_select(wxCommandEvent& e)
{
	int id = e.GetId();
	if(id < wxid_range_low || id > wxid_range_high)
		return;
	if(id == wxid_range_high) {
		//Refresh.
		update();
	} else {
		//Select.
		if(entries.count(id)) {
			callback(entries[id]);
			rfiles.add(entries[id]);
		}
	}
}

void recent_menu::update()
{
	auto ents = rfiles.get();
	int id = wxid_range_low;
	for(auto i : items)
		Delete(i.second);
	items.clear();
	bool has_ents = false;
	for(auto i : ents) {
		has_ents = true;
		if(id >= wxid_range_high)
			break;
		entries[id] = i;
		items[id] = Append(id, towxstring(i));
		id++;
	}
	if(has_ents)
		items[-1] = AppendSeparator();
	items[wxid_range_high] = Append(wxid_range_high, wxT("Refresh"));
}

void recent_menu::add(const std::string& file)
{
	rfiles.add(file);
}
