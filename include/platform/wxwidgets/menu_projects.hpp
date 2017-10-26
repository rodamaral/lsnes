#ifndef _plat_wxwidgets__menu_projects__hpp__included__
#define _plat_wxwidgets__menu_projects__hpp__included__

#include "core/dispatch.hpp"
#include "library/recentfiles.hpp"
#include <functional>
#include <wx/string.h>
#include <wx/wx.h>
#include <map>
#include <set>
#include <vector>

class emulator_instance;

class projects_menu : public wxMenu
{
public:
	projects_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high, const std::string& cfg,
		std::function<void(const std::string& id)> cb);
	~projects_menu();
	void on_select(wxCommandEvent& e);
	void update();
	void add(recentfiles::namedobj obj) { rfiles.add(obj); }
private:
	emulator_instance& inst;
	class rhook : public recentfiles::hook
	{
	public:
		rhook(projects_menu& _pmenu) : pmenu(_pmenu) {}
		~rhook() {}
		void operator()() { pmenu.update(); }
	private:
		projects_menu& pmenu;
	} hook;
	wxWindow* pwin;
	int wxid_range_low;
	int wxid_range_high;
	std::map<int, wxMenuItem*> items;
	std::map<int, recentfiles::namedobj> entries;
	std::function<void(std::string id)> selected_cb;
	recentfiles::set<recentfiles::namedobj> rfiles;
};

#endif
