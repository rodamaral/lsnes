#include "platform/wxwidgets/menu_recent.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "library/eatarg.hpp"

template<class T>
recent_menu<T>::recent_menu(wxWindow* win, int wxid_low, int wxid_high, const std::string& cfg,
	void (*cb)(const T& name))
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

template<class T> void recent_menu<T>::on_select(wxCommandEvent& e)
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

template<class T> void recent_menu<T>::update()
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
		items[id] = Append(id, towxstring(i.display()));
		id++;
	}
	if(has_ents)
		items[-1] = AppendSeparator();
	items[wxid_range_high] = Append(wxid_range_high, wxT("Refresh"));
}

template<class T> void recent_menu<T>::add(const T& file)
{
	rfiles.add(file);
}

void _dummy_3642632773273272787237272723()
{
	recent_menu<recentfile_path> x(NULL, 0, 0, "", NULL);
	recent_menu<recentfile_multirom> y(NULL, 0, 0, "", NULL);
	eat_argument(&recent_menu<recentfile_path>::add);
	eat_argument(&recent_menu<recentfile_multirom>::add);
}
