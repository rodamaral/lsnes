#include <functional>
#include "platform/wxwidgets/menu_loadrom.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"

loadrom_menu::loadrom_menu(wxWindow* win, int wxid_low, int wxid_high, std::function<void(core_type* name)> cb)
{
	CHECK_UI_THREAD;
	pwin = win;
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	callback = cb;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(loadrom_menu::on_select), NULL, this);
	corelistener.set(notify_new_core, [this]() { this->update(); });
	update();
}

void loadrom_menu::on_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId();
	if(id < wxid_range_low || id > wxid_range_high)
		return;
	if(entries.count(id))
		callback(entries[id]);
}

void loadrom_menu::update()
{
	CHECK_UI_THREAD;
	auto ents = core_type::get_core_types();
	int id = wxid_range_low;
	for(auto i : items)
		Delete(i.second);
	items.clear();
	std::map<std::string, core_type*> ents2;
	for(auto i : ents)
		ents2[i->get_hname() + " [" + i->get_core_identifier() + "]..."] = i;

	for(auto i : ents2) {
		if(id >= wxid_range_high)
			break;
		if(i.second->is_hidden())
			continue;
		if(i.second->isnull())
			continue;
		entries[id] = i.second;
		items[id] = Append(id, towxstring(i.first));
		id++;
	}
}
