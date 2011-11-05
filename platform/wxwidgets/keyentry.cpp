#include "keyentry.hpp"
#include "common.hpp"
#include "keymapper.hpp"

#define S_DONT_CARE "Ignore"
#define S_RELEASED "Released"
#define S_PRESSED "Pressed"

wx_key_entry::wx_key_entry(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("Specify key"), wxDefaultPosition, wxSize(-1, -1))
{
	std::vector<wxString> keych;

	std::set<std::string> mods = modifier::get_set();
	std::set<std::string> keys = keygroup::get_keys();
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);
	
	wxFlexGridSizer* t_s = new wxFlexGridSizer(mods.size() + 1, 3, 0, 0);
	for(auto i : mods) {
		t_s->Add(new wxStaticText(this, wxID_ANY, towxstring(i)), 0, wxGROW);
		keyentry_mod_data m;
		t_s->Add(m.pressed = new wxCheckBox(this, wxID_ANY, wxT("Pressed")), 0, wxGROW);
		t_s->Add(m.unmasked = new wxCheckBox(this, wxID_ANY, wxT("Unmasked")), 0, wxGROW);
		m.pressed->Disable();
		modifiers[i] = m;
		m.pressed->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wx_key_entry::on_change_setting), NULL, this);
		m.unmasked->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wx_key_entry::on_change_setting), NULL, this);
	}
	for(auto i : keys)
		keych.push_back(towxstring(i));
	mainkey = new labeledcombobox(t_s, this, "Key", &keych[0], keych.size(), 0, true, this,
			(wxObjectEventFunction)(&wx_key_entry::on_change_setting));
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("")), 0, wxGROW);
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_key_entry::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_key_entry::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

#define TMPFLAG_UNMASKED 65
#define TMPFLAG_UNMASKED_LINK_CHILD 2
#define TMPFLAG_UNMASKED_LINK_PARENT 68
#define TMPFLAG_PRESSED 8
#define TMPFLAG_PRESSED_LINK_CHILD 16
#define TMPFLAG_PRESSED_LINK_PARENT 32

void wx_key_entry::on_change_setting(wxCommandEvent& e)
{
	for(auto& i : modifiers)
		i.second.tmpflags = 0;
	for(auto& i : modifiers) {
		modifier* m = NULL;
		try {
			m = &modifier::lookup(i.first);
		} catch(...) {
			i.second.pressed->Disable();
			i.second.unmasked->Disable();
			continue;
		}
		std::string j = m->linked_name();
		if(i.second.unmasked->GetValue())
			i.second.tmpflags |= TMPFLAG_UNMASKED;
		if(j != "") {
			if(modifiers[j].unmasked->GetValue())
				i.second.tmpflags |= TMPFLAG_UNMASKED_LINK_PARENT;
			if(i.second.unmasked->GetValue())
				modifiers[j].tmpflags |= TMPFLAG_UNMASKED_LINK_CHILD;
		}
		if(i.second.pressed->GetValue())
			i.second.tmpflags |= TMPFLAG_PRESSED;
		if(j != "") {
			if(modifiers[j].pressed->GetValue())
				i.second.tmpflags |= TMPFLAG_PRESSED_LINK_PARENT;
			if(i.second.pressed->GetValue())
				modifiers[j].tmpflags |= TMPFLAG_PRESSED_LINK_CHILD;
		}
	}
	for(auto& i : modifiers) {
		//Unmasked is to be enabled if neither unmasked link flag is set.
		if(i.second.tmpflags & ((TMPFLAG_UNMASKED_LINK_CHILD | TMPFLAG_UNMASKED_LINK_PARENT) & ~64)) {
			i.second.unmasked->SetValue(false);
			i.second.unmasked->Disable();
		} else
			i.second.unmasked->Enable();
		//Pressed is to be enabled if:
		//- This modifier is unmasked or parent is unmasked.
		//- Parent nor child is not pressed.
		if(((i.second.tmpflags & (TMPFLAG_UNMASKED | TMPFLAG_UNMASKED_LINK_PARENT | TMPFLAG_PRESSED_LINK_CHILD |
			TMPFLAG_PRESSED_LINK_PARENT)) & 112) == 64)
			i.second.pressed->Enable();
		else {
			i.second.pressed->SetValue(false);
			i.second.pressed->Disable();
		}
	}
}

void wx_key_entry::on_ok(wxCommandEvent& e)
{
	EndModal(wxID_OK);
}

void wx_key_entry::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

std::string wx_key_entry::getkey()
{
	std::string x;
	bool f;
	f = true;
	for(auto i : modifiers) {
		if(i.second.pressed->GetValue()) {
			if(!f)
				x = x + ",";
			f = false;
			x = x + i.first;
		}
	}
	x = x + "/";
	f = true;
	for(auto i : modifiers) {
		if(i.second.unmasked->GetValue()) {
			if(!f)
				x = x + ",";
			f = false;
			x = x + i.first;
		}
	}
	x = x + "|" + mainkey->get_choice();
	return x;
}
