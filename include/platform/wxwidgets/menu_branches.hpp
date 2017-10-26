#ifndef _plat_wxwidgets__menu_branches__hpp__included__
#define _plat_wxwidgets__menu_branches__hpp__included__

#include "core/dispatch.hpp"
#include <functional>
#include <wx/string.h>
#include <wx/wx.h>
#include <map>
#include <set>
#include <vector>

class emulator_instance;

class branches_menu : public wxMenu
{
public:
	branches_menu(wxWindow* win, emulator_instance& inst, int wxid_low, int wxid_high);
	~branches_menu();
	void on_select(wxCommandEvent& e);
	void update();
	bool any_enabled();
	struct miteminfo
	{
		miteminfo(wxMenuItem* it, bool ismenu, wxMenu* p)
			: item(it), is_menu(ismenu), parent(p)
		{
		}
		wxMenuItem* item;
		bool is_menu;
		wxMenu* parent;
	};
	void set_disabler(std::function<void(bool enabled)> fn) { disabler_fn = fn; }
private:
	struct dispatch::target<> branchchange;
	wxWindow* pwin;
	int wxid_range_low;
	int wxid_range_high;
	std::map<int, uint64_t> branch_ids;
	std::list<wxMenu*> menus;
	std::list<miteminfo> otheritems;
	std::function<void(bool enabled)> disabler_fn;
	emulator_instance& inst;
};

#endif
