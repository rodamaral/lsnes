#ifndef _wxwidgets_settingseditor__hpp__included__
#define _wxwidgets_settingseditor__hpp__included__

#include "core/dispatch.hpp"

#include <vector>
#include <string>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

class wx_settings_editor_setting : public wxEvtHandler
{
public:
	wx_settings_editor_setting(wxSizer* sizer, wxWindow* window, const std::string& name);
	void on_clear_click(wxCommandEvent& e);
	void on_edit_click(wxCommandEvent& e);
	void change_setting(const std::string& setting, const std::string& value);
	void clear_setting(const std::string& setting);
private:
	std::string a_name;
	wxWindow* parent;
	wxStaticText* label;
	wxButton* clear;
	wxButton* edit;
};

class wx_settings_editor;

class wx_settings_editor_listener : public information_dispatch
{
public:
	wx_settings_editor_listener(wx_settings_editor* _editor);
	~wx_settings_editor_listener() throw();
	void on_setting_change(const std::string& setting, const std::string& value);
	void on_setting_clear(const std::string& setting);
private:
	wx_settings_editor* editor;
};

class wx_settings_editor : public wxDialog
{
public:
	wx_settings_editor(wxWindow* parent);
	~wx_settings_editor();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void change_setting(const std::string& setting, const std::string& value);
	void clear_setting(const std::string& setting);
private:
	wx_settings_editor_listener listener;
	std::vector<wx_settings_editor_setting*> esettings;
	wxButton* close;
};

#endif
