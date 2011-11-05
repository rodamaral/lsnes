#include "labelcombobox.hpp"
#include "command.hpp"

labeledcombobox::labeledcombobox(wxSizer* sizer, wxWindow* parent, const std::string& label, wxString* choices,
	size_t choice_count, size_t defaultidx, bool start_enabled, wxEvtHandler* dispatch_to,
	wxObjectEventFunction on_fn_change)
{
	sizer->Add(new wxStaticText(parent, wxID_ANY, towxstring(label)), 0, wxGROW);
	sizer->Add(combo = new wxComboBox(parent, wxID_ANY, choices[defaultidx], wxDefaultPosition, wxDefaultSize,
		choice_count, choices, wxCB_READONLY), 0, wxGROW);
	if(dispatch_to)
		combo->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED, on_fn_change, NULL, dispatch_to);
	combo->Enable(start_enabled);
	enabled = start_enabled;
	forced = false;
}

std::string labeledcombobox::get_choice()
{
	return tostdstring(combo->GetValue());
}

void labeledcombobox::enable()
{
	if(forced && !enabled)
		combo->SetValue(remembered);
	combo->Enable();
	enabled = true;
}

void labeledcombobox::disable(const wxString& choice)
{
	combo->Disable();
	if(enabled)
		remembered = combo->GetValue();
	combo->SetValue(choice);
	enabled = false;
	forced = true;
}

void labeledcombobox::disable()
{
	combo->Disable();
	if(enabled)
		remembered = combo->GetValue();
	enabled = false;
	forced = false;
}
