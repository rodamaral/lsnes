#ifndef _plat_wxwidgets__menu_recent__hpp__included__
#define _plat_wxwidgets__menu_recent__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>
#include "library/recentfiles.hpp"
#include <map>

template<class T>
class recent_menu : public wxMenu
{
public:
	recent_menu(wxWindow* win, int wxid_low, int wxid_high, const std::string& cfg,
		void (*cb)(const T& name))  __attribute__((noinline));
	void on_select(wxCommandEvent& e);
	void update();
	void add(const T& file);
private:
	class rhook : public recentfiles::hook
	{
	public:
		rhook(recent_menu& _pmenu) : pmenu(_pmenu) {}
		~rhook() {}
		void operator()() { pmenu.update(); }
	private:
		recent_menu& pmenu;
	} hook;
	recentfiles::set<T> rfiles;
	wxWindow* pwin;
	std::map<int, T> entries;
	std::map<int, wxMenuItem*> items;
	int wxid_range_low;
	int wxid_range_high;
	void (*callback)(const T& name);
};

#endif
