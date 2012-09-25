#ifndef _plat_wxwidgets__menu_recent__hpp__included__
#define _plat_wxwidgets__menu_recent__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>
#include "library/recentfiles.hpp"
#include <map>

class recent_menu : public wxMenu
{
public:
	recent_menu(wxWindow* win, int wxid_low, int wxid_high, const std::string& cfg,
		void (*cb)(const std::string& name));
	void on_select(wxCommandEvent& e);
	void update();
	void add(const std::string& file);
private:
	class rhook : public recent_files::hook
	{
	public:
		rhook(recent_menu& _pmenu) : pmenu(_pmenu) {}
		~rhook() {}
		void operator()() { pmenu.update(); }
	private:
		recent_menu& pmenu;
	} hook;
	recent_files rfiles;
	wxWindow* pwin;
	std::map<int, std::string> entries;
	std::map<int, wxMenuItem*> items;
	int wxid_range_low;
	int wxid_range_high;
	void (*callback)(const std::string& name);
};

#endif
