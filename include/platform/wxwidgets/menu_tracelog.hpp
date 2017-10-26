#ifndef _plat_wxwidgets__menu_tracelog__hpp__included__
#define _plat_wxwidgets__menu_tracelog__hpp__included__

#include "core/dispatch.hpp"
#include <functional>
#include <wx/string.h>
#include <wx/wx.h>
#include <map>
#include <set>
#include <vector>

class emulator_instance;

class tracelog_menu : public wxMenu
{
public:
	tracelog_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high);
	~tracelog_menu();
	void on_select(wxCommandEvent& e);
	void update();
	bool any_enabled();
	void set_disabler(std::function<void(bool enabled)> fn) { disabler_fn = fn; }
private:
	struct dispatch::target<> corechange;
	emulator_instance& inst;
	wxWindow* pwin;
	int wxid_range_low;
	int wxid_range_high;
	std::vector<wxMenuItem*> items;
	std::map<int, std::string> cpunames;
	std::function<void(bool enabled)> disabler_fn;
};

#endif
