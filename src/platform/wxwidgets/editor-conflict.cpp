#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/scrolwin.h>

#include "interface/romtype.hpp"
#include "library/string.hpp"
#include "core/romimage.hpp"

#include "platform/wxwidgets/platform.hpp"

class wxeditor_conflict : public wxDialog
{
public:
	wxeditor_conflict(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
private:
	void add_options(wxWindow* win, wxSizer* sizer);
	void add_option(wxWindow* win, wxSizer* sizer, const std::string& key, const std::string& name);
	std::map<std::string, wxComboBox*> choices;
	std::map<std::pair<std::string, std::string>, core_type*> rchoices;
	std::set<std::string> bad_defaults;
	wxScrolledWindow* scrollwin;
	wxButton* btnok;
	wxButton* btncancel;
};

wxeditor_conflict::wxeditor_conflict(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit conflicts"), wxDefaultPosition, wxSize(-1, -1))
{
	CHECK_UI_THREAD;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	scrollwin = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxVSCROLL);
	scrollwin->SetMinSize(wxSize(-1, 500));

	wxFlexGridSizer* t_s = new wxFlexGridSizer(0, 2, 0, 0);
	add_options(scrollwin, t_s);
	scrollwin->SetSizer(t_s);
	top_s->Add(scrollwin);
	scrollwin->SetScrollRate(0, 20);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(btnok = new wxButton(this, wxID_CANCEL, wxT("OK")), 0, wxGROW);
	btnok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_conflict::on_ok), NULL, this);
	pbutton_s->Add(btncancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	btncancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_conflict::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	t_s->Layout();
	top_s->Layout();
	Fit();
}

void wxeditor_conflict::add_options(wxWindow* win, wxSizer* sizer)
{
	std::map<std::string, std::string> classes;
	for(auto i : core_type::get_core_types()) {
		if(i->is_hidden())
			continue;
		classes["type:" + i->get_iname()] = "Type " + i->get_iname();
		for(auto j : i->get_extensions())
			classes["ext:" + j] = "Extension " + j;
	}
	for(auto i : classes)
		add_option(win, sizer, i.first, i.second);
}

void wxeditor_conflict::add_option(wxWindow* win, wxSizer* sizer, const std::string& key, const std::string& name)
{
	CHECK_UI_THREAD;
	sizer->Add(new wxStaticText(win, wxID_ANY, towxstring(name)));
	std::vector<wxString> v;
	size_t dfltidx = 0;
	v.push_back(wxT("(unspecified)"));
	regex_results r;
	if(r = regex("ext:(.*)", key)) {
		for(auto i : core_type::get_core_types()) {
			if(i->is_hidden())
				continue;
			for(auto j : i->get_extensions())
				if(j == r[1]) {
					std::string val = i->get_hname() + " / " + i->get_core_identifier();
					rchoices[std::make_pair(key, val)] = i;
					if(core_selections.count(key) && i == preferred_core[key])
						dfltidx = v.size();
					v.push_back(towxstring(val));
				}
		}
	} else if(r = regex("type:(.*)", key)) {
		for(auto i : core_type::get_core_types()) {
			if(i->is_hidden())
				continue;
			if(i->get_iname() == r[1]) {
				std::string val = i->get_hname() + " / " + i->get_core_identifier();
				rchoices[std::make_pair(key, val)] = i;
				if(core_selections.count(key) && i == preferred_core[key])
					dfltidx = v.size();
				v.push_back(towxstring(val));
			}
		}
	}
	if(core_selections.count(key) && !dfltidx) {
		bad_defaults.insert(key);
		dfltidx = v.size();
		v.push_back(towxstring("bad default [" + core_selections[key] + "]"));
	}

	sizer->Add(choices[key] = new wxComboBox(win, wxID_ANY, v[dfltidx], wxDefaultPosition, wxDefaultSize,
		v.size(), &v[0], wxCB_READONLY), 1, wxGROW);
}

bool wxeditor_conflict::ShouldPreventAppExit() const { return false; }

void wxeditor_conflict::on_ok(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	for(auto i : choices) {
		if(i.second->GetSelection() == 0) {
			//Reset to unspecified.
			preferred_core.erase(i.first);
			core_selections.erase(i.first);
		} else if(bad_defaults.count(i.first) && i.second->GetSelection() ==
			(int)(i.second->GetCount() - 1)) {
			//Preserve the previous, reset real setting to unspecified.
			preferred_core.erase(i.first);
		} else {
			//Chose a setting.
			std::string val = tostdstring(i.second->GetStringSelection());
			core_selections[i.first] = val;
			preferred_core[i.first] = rchoices[std::make_pair(i.first, val)];
		}
	}
	EndModal(wxID_OK);
}

void wxeditor_conflict::on_cancel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}

void show_conflictwindow(wxWindow* parent)
{
	CHECK_UI_THREAD;
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_conflict(parent);
		editor->ShowModal();
	} catch(...) {
		return;
	}
	editor->Destroy();
	do_save_configuration();
}

std::map<std::string, std::string> core_selections;
