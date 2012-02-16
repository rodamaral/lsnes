#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/moviefile.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"

#include "platform/wxwidgets/platform.hpp"

#include <fstream>
#include <stdexcept>


#include <vector>
#include <string>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

class wxeditor_settings_setting : public wxEvtHandler
{
public:
	wxeditor_settings_setting(wxSizer* sizer, wxWindow* window, const std::string& name);
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

class wxeditor_settings;

class wxeditor_settings_listener : public information_dispatch
{
public:
	wxeditor_settings_listener(wxeditor_settings* _editor);
	~wxeditor_settings_listener() throw();
	void on_setting_change(const std::string& setting, const std::string& value);
	void on_setting_clear(const std::string& setting);
private:
	wxeditor_settings* editor;
};

class wxeditor_settings : public wxDialog
{
public:
	wxeditor_settings(wxWindow* parent);
	~wxeditor_settings();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void change_setting(const std::string& setting, const std::string& value);
	void clear_setting(const std::string& setting);
private:
	wxeditor_settings_listener listener;
	std::vector<wxeditor_settings_setting*> esettings;
	wxScrolledWindow* scrollwin;
	wxButton* close;
};

wxeditor_settings_setting::wxeditor_settings_setting(wxSizer* sizer, wxWindow* window, const std::string& name)
{
	a_name = name;
	parent = window;
	std::string pvalue = "<unknown>";
	runemufn([&pvalue, a_name]() {
		try {
			if(!setting::is_set(a_name))
				pvalue = "<unset>";
			else
				pvalue = setting::get(a_name);
		} catch(...) {
		}
		});
	sizer->Add(new wxStaticText(window, wxID_ANY, towxstring(name)), 0, wxGROW);
	sizer->Add(label = new wxStaticText(window, wxID_ANY, towxstring(pvalue)), 0, wxGROW);
	sizer->Add(edit = new wxButton(window, wxID_ANY, wxT("Edit")), 0, wxGROW);
	sizer->Add(clear = new wxButton(window, wxID_ANY, wxT("Clear")), 0, wxGROW);
	edit->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_settings_setting::on_edit_click), NULL, this);
	clear->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_settings_setting::on_clear_click), NULL, this);
}

void wxeditor_settings_setting::on_clear_click(wxCommandEvent& e)
{
	try {
		bool fault = false;;
		std::string faulttext;
		runemufn([a_name, &fault, &faulttext]() { 
			try {
				setting::blank(a_name);
			} catch(std::exception& e) {
				fault = true;
				faulttext = e.what();
			}
			});
		if(fault) {
			wxMessageDialog* d = new wxMessageDialog(parent,
				towxstring(std::string("Can't clear setting: ") + faulttext), wxT("Error"), wxOK |
				wxICON_EXCLAMATION);
			d->ShowModal();
			d->Destroy();
		}
	}catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(std::string("Can't clear setting: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		d->Destroy();
	}
}

void wxeditor_settings_setting::on_edit_click(wxCommandEvent& e)
{
	try {
		std::string newsetting;
		std::string oldvalue = setting::get(a_name);
		wxTextEntryDialog* d = new wxTextEntryDialog(parent, towxstring("Enter new value for " + a_name),
			wxT("Enter new value for setting"), towxstring(oldvalue));
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		newsetting = tostdstring(d->GetValue());
		bool fault = false;;
		std::string faulttext;
		runemufn([a_name, newsetting, &fault, &faulttext]() { 
			try {
				setting::set(a_name, newsetting);
			} catch(std::exception& e) {
				fault = true;
				faulttext = e.what();
			}
			});
		if(fault) {
			wxMessageDialog* d = new wxMessageDialog(parent,
				towxstring(std::string("Can't set setting: ") + faulttext), wxT("Error"), wxOK |
				wxICON_EXCLAMATION);
			d->ShowModal();
			d->Destroy();
		}
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(std::string("Can't set setting: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		d->Destroy();
	}
}

void wxeditor_settings_setting::change_setting(const std::string& _setting, const std::string& value)
{
	runuifun([_setting, label, a_name, value]() {
		if(_setting != a_name)
			return;
		label->SetLabel(towxstring(value));
		});
}

void wxeditor_settings_setting::clear_setting(const std::string& _setting)
{
	runuifun([_setting, label, a_name]() {
		if(_setting != a_name)
			return;
		label->SetLabel(wxT("<unset>"));
		});
}

wxeditor_settings::wxeditor_settings(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit settings"), wxDefaultPosition, wxSize(-1, -1)), listener(this)
{
	std::set<std::string> settings_set;
	runemufn([&settings_set]() { settings_set = setting::get_settings_set(); });

	scrollwin = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxVSCROLL);
	scrollwin->SetMinSize(wxSize(-1, 500));

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	wxFlexGridSizer* t_s = new wxFlexGridSizer(settings_set.size(), 4, 0, 0);
	for(auto i : settings_set)
		esettings.push_back(new wxeditor_settings_setting(t_s, scrollwin, i));
	scrollwin->SetSizer(t_s);
	top_s->Add(scrollwin);
	scrollwin->SetScrollRate(0, 20);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(close = new wxButton(this, wxID_CANCEL, wxT("Close")), 0, wxGROW);
	close->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_settings::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	t_s->Layout();
	top_s->Layout();
	Fit();
}

wxeditor_settings::~wxeditor_settings()
{
	for(auto i : esettings)
		delete i;
}

bool wxeditor_settings::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_settings::on_close(wxCommandEvent& e)
{
	EndModal(wxID_OK);
}

void wxeditor_settings::change_setting(const std::string& _setting, const std::string& value)
{
	for(auto i : esettings)
		i->change_setting(_setting, value);
}

void wxeditor_settings::clear_setting(const std::string& _setting)
{
	for(auto i : esettings)
		i->clear_setting(_setting);
}

wxeditor_settings_listener::wxeditor_settings_listener(wxeditor_settings* _editor)
	: information_dispatch("wxeditor_settings-listener")
{
	editor = _editor;
}

wxeditor_settings_listener::~wxeditor_settings_listener() throw()
{
}

void wxeditor_settings_listener::on_setting_change(const std::string& _setting, const std::string& value)
{
	editor->change_setting(_setting, value);
}

void wxeditor_settings_listener::on_setting_clear(const std::string& _setting)
{
	editor->clear_setting(_setting);
}

void wxeditor_settings_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_settings(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
