#ifndef _plat_wxwidgets__menu_loadrom__hpp__included__
#define _plat_wxwidgets__menu_loadrom__hpp__included__

#include <functional>
#include <wx/string.h>
#include <wx/wx.h>
#include "interface/romtype.hpp"
#include "core/dispatch.hpp"
#include <map>

class loadrom_menu : public wxMenu
{
public:
	loadrom_menu(wxWindow* win, int wxid_low, int wxid_high, std::function<void(core_type* name)> cb);
	void on_select(wxCommandEvent& e);
	void update();
private:
	wxWindow* pwin;
	std::map<int, core_type*> entries;
	std::map<int, wxMenuItem*> items;
	int wxid_range_low;
	int wxid_range_high;
	std::function<void(core_type* name)> callback;
	struct dispatch::target<> corelistener;
};

#endif
