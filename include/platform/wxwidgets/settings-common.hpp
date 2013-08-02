#ifndef _platform__wxwidgets__settings_common__hpp__included__
#define _platform__wxwidgets__settings_common__hpp__included__

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

struct settings_tab : public wxPanel
{
	settings_tab(wxWindow* parent) : wxPanel(parent, -1), closing_flag(false) {}
	~settings_tab() {}
	void notify_close() { closing_flag = true; }
	bool closing() { return closing_flag; }
	virtual void on_close() {}
	virtual void on_notify() {}
	void set_notify(std::function<void()> _notify) { notify = _notify; }
	void do_notify() { notify(); }
	void call_window_fit();
private:
	bool closing_flag;
	std::function<void()> notify;
};

struct settings_tab_factory
{
	settings_tab_factory(const std::string& tabname, std::function<settings_tab*(wxWindow* parent)> create_fn);
	~settings_tab_factory();
	settings_tab* create(wxWindow* parent) { return _create_fn(parent); }
	std::string get_name() { return _tabname; }
	static std::list<settings_tab_factory*> factories();
private:
	std::string _tabname;
	std::function<settings_tab*(wxWindow* parent)> _create_fn;
};

struct settings_menu : public wxMenu
{
	settings_menu(wxWindow* win, int wxid_low);
	void on_selected(wxCommandEvent& e);
private:
	wxWindow* parent;
	std::map<int, settings_tab_factory*> items;
};

void display_settings_dialog(wxWindow* parent, settings_tab_factory* singletab = NULL);
void settings_activate_keygrab(std::function<void(std::string key)> callback);
void settings_deactivate_keygrab();

#endif
