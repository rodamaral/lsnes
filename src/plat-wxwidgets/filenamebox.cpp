#include "core/zip.hpp"

#include "plat-wxwidgets/common.hpp"
#include "plat-wxwidgets/filenamebox.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

filenamebox::filenamebox(wxSizer* sizer, wxWindow* parent, const std::string& initial_label, int flags,
	wxEvtHandler* dispatch_to, wxObjectEventFunction on_fn_change)
{
	wxSizer* inner = sizer;
	if(flags & FNBF_OI)
		inner = new wxFlexGridSizer(1, 3, 0, 0);
	given_flags = flags;
	last_label = initial_label;
	label = new wxStaticText(parent, wxID_ANY, towxstring(last_label));
	filename = new wxTextCtrl(parent, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, -1));
	file_select = new wxButton(parent, wxID_ANY, wxT("..."));
	inner->Add(label, 0, wxGROW);
	inner->Add(filename, 1, wxGROW);
	inner->Add(file_select, 0, wxGROW);
	if((flags & FNBF_NN) == 0)
		filename->Connect(wxEVT_COMMAND_TEXT_UPDATED, on_fn_change, NULL, dispatch_to);
	file_select->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(filenamebox::on_file_select), NULL,
		this);
	if(flags & FNBF_SD)
		disable();
	if(flags & FNBF_OI) {
		inner->SetSizeHints(parent);
		sizer->Add(inner, 0, wxGROW);
	}
	pwindow = parent;
	enabled = ((flags & FNBF_SD) == 0);
}

filenamebox::~filenamebox()
{
	//Wxwidgets destroys the subwidgets.
}


void filenamebox::on_file_select(wxCommandEvent& e)
{
	std::string fname;
	wxFileDialog* d = new wxFileDialog(pwindow, towxstring("Choose " + last_label), wxT("."));
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		return;
	}
	fname = tostdstring(d->GetPath());
	d->Destroy();
	if(given_flags & FNBF_PZ) {
		//Did we pick a .zip file?
		try {
			zip_reader zr(fname);
			std::vector<wxString> files;
			for(auto i : zr)
				files.push_back(towxstring(i));
			wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(pwindow, wxT("Select file within .zip"),
				wxT("Select member"), files.size(), &files[0]);
			if(d2->ShowModal() == wxID_CANCEL) {
				d2->Destroy();
				return;
			}
			fname = fname + "/" + tostdstring(d2->GetStringSelection());
			d2->Destroy();
		} catch(...) {
			//Ignore error.
		}
	}
	filename->SetValue(towxstring(fname));
}

std::string filenamebox::get_file()
{
	if(!enabled)
		return "";
	else
		return tostdstring(filename->GetValue());
}

bool filenamebox::is_enabled()
{
	return enabled;
}

void filenamebox::enable(const std::string& new_label)
{
	change_label(new_label);
	enable();
}

void filenamebox::change_label(const std::string& new_label)
{
	last_label = new_label;
	if(enabled || (given_flags & FNBF_PL))
		label->SetLabel(towxstring(last_label));
}

void filenamebox::disable()
{
	label->Disable();
	filename->Disable();
	file_select->Disable();
	if((given_flags & FNBF_PL) == 0)
		label->SetLabel(wxT(""));
	enabled = false;
}

void filenamebox::enable()
{
	if((given_flags & FNBF_PL) == 0)
		label->SetLabel(towxstring(last_label));
	label->Enable();
	filename->Enable();
	file_select->Enable();
	enabled = true;
}

bool filenamebox::is_nonblank()
{
	if(!enabled)
		return false;
	return (filename->GetValue().Length() != 0);
}

bool filenamebox::is_nonblank_or_disabled()
{
	if(!enabled)
		return true;
	return (filename->GetValue().Length() != 0);
}

void filenamebox::clear()
{
	filename->SetValue(wxT(""));
}
