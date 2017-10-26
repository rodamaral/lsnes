#ifndef _platform__wxwidgets__settings_common__hpp__included__
#define _platform__wxwidgets__settings_common__hpp__included__

#include <functional>
#include <string>
#include "platform/wxwidgets/platform.hpp"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <vector>
#include <string>
#include <list>

class emulator_instance;

struct settings_tab : public wxPanel
{
	settings_tab(wxWindow* parent, emulator_instance& _inst) : wxPanel(parent, -1), inst(_inst),
		closing_flag(false) {}
	~settings_tab() {}
	void notify_close() { closing_flag = true; }
	bool closing() { return closing_flag; }
	virtual void on_close() {}
	virtual void on_notify() {}
	void set_notify(std::function<void()> _notify) { notify = _notify; }
	void do_notify() { notify(); }
protected:
	emulator_instance& inst;
private:
	bool closing_flag;
	std::function<void()> notify;
};

struct settings_tab_factory
{
	settings_tab_factory(const std::string& tabname, std::function<settings_tab*(wxWindow* parent,
		emulator_instance& inst)> create_fn);
	~settings_tab_factory();
	settings_tab* create(wxWindow* parent, emulator_instance& inst) { return _create_fn(parent, inst); }
	std::string get_name() { return _tabname; }
	static std::list<settings_tab_factory*> factories();
private:
	std::string _tabname;
	std::function<settings_tab*(wxWindow* parent, emulator_instance& inst)> _create_fn;
};

struct settings_menu : public wxMenu
{
	settings_menu(wxWindow* win, emulator_instance& _inst, int wxid_low);
	void on_selected(wxCommandEvent& e);
private:
	emulator_instance& inst;
	wxWindow* parent;
	std::map<int, settings_tab_factory*> items;
};

void display_settings_dialog(wxWindow* parent, emulator_instance& inst, settings_tab_factory* singletab = NULL);
void settings_activate_keygrab(emulator_instance& inst, std::function<void(std::string key)> callback);
void settings_deactivate_keygrab(emulator_instance& inst);

#endif
