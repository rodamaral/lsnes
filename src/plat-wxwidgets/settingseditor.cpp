#include "core/settings.hpp"
#include "core/window.hpp"

#include "plat-wxwidgets/settingseditor.hpp"
#include "plat-wxwidgets/common.hpp"

#include <stdexcept>

wx_settings_editor_setting::wx_settings_editor_setting(wxSizer* sizer, wxWindow* window, const std::string& name)
{
	a_name = name;
	parent = window;
	std::string pvalue = "<unknown>";
	try {
		if(!setting::is_set(a_name))
			pvalue = "<unset>";
		else 
			pvalue = setting::get(a_name);
	} catch(...) {
	}
	sizer->Add(new wxStaticText(window, wxID_ANY, towxstring(name)), 0, wxGROW);
	sizer->Add(label = new wxStaticText(window, wxID_ANY, towxstring(pvalue)), 0, wxGROW);
	sizer->Add(edit = new wxButton(window, wxID_ANY, wxT("Edit")), 0, wxGROW);
	sizer->Add(clear = new wxButton(window, wxID_ANY, wxT("Clear")), 0, wxGROW);
	edit->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_settings_editor_setting::on_edit_click), NULL, this);
	clear->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_settings_editor_setting::on_clear_click), NULL, this);
}

void wx_settings_editor_setting::on_clear_click(wxCommandEvent& e)
{
	try {
		setting::blank(a_name);
	}catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(std::string("Can't clear setting: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		d->Destroy();
	}
}

void wx_settings_editor_setting::on_edit_click(wxCommandEvent& e)
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
		setting::set(a_name, newsetting);
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(parent, towxstring(std::string("Can't set setting: ") +
			e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		d->Destroy();
	}
}

void wx_settings_editor_setting::change_setting(const std::string& _setting, const std::string& value)
{
	if(_setting != a_name)
		return;
	label->SetLabel(towxstring(value));
}

void wx_settings_editor_setting::clear_setting(const std::string& _setting)
{
	if(_setting != a_name)
		return;
	label->SetLabel(wxT("<unset>"));
}

wx_settings_editor::wx_settings_editor(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit settings"), wxDefaultPosition, wxSize(-1, -1)), listener(this)
{
	std::set<std::string> settings_set = setting::get_settings_set();

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);
	
	wxFlexGridSizer* t_s = new wxFlexGridSizer(settings_set.size(), 4, 0, 0);
	for(auto i : settings_set)
		esettings.push_back(new wx_settings_editor_setting(t_s, this, i));
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(close = new wxButton(this, wxID_CANCEL, wxT("Close")), 0, wxGROW);
	close->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_settings_editor::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wx_settings_editor::~wx_settings_editor()
{
	for(auto i : esettings)
		delete i;
}

bool wx_settings_editor::ShouldPreventAppExit() const
{
	return false;
}

void wx_settings_editor::on_close(wxCommandEvent& e)
{
	EndModal(wxID_OK);
}

void wx_settings_editor::change_setting(const std::string& _setting, const std::string& value)
{
	for(auto i : esettings)
		i->change_setting(_setting, value);
}

void wx_settings_editor::clear_setting(const std::string& _setting)
{
	for(auto i : esettings)
		i->clear_setting(_setting);
}

wx_settings_editor_listener::wx_settings_editor_listener(wx_settings_editor* _editor)
{
	editor = _editor;
}

wx_settings_editor_listener::~wx_settings_editor_listener() throw()
{
}

void wx_settings_editor_listener::on_setting_change(const std::string& _setting, const std::string& value)
{
	editor->change_setting(_setting, value);
}

void wx_settings_editor_listener::on_setting_clear(const std::string& _setting)
{
	editor->clear_setting(_setting);
}
