#ifndef _plat_wxwidgets__menu_dump__hpp__included__
#define _plat_wxwidgets__menu_dump__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>
#include <map>

class dumper_menu_monitor;
class dumper_info;
class dumper_information_1;

class dumper_menu : public wxMenu
{
public:
	dumper_menu(wxWindow* win, int wxid_low, int wxid_high);
	~dumper_menu();
	void on_select(wxCommandEvent& e);
	void update();
private:
	dumper_menu_monitor* monitor;
	wxWindow* pwin;
	int wxid_range_low;
	int wxid_range_high;
	std::map<std::string, dumper_information_1> existing_dumpers;
};

#endif
