#include "core/window.hpp"
#include "core/audioapi.hpp"

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

class wxeditor_sounddev : public wxDialog
{
public:
	wxeditor_sounddev(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
private:
	wxComboBox* rdev;
	wxComboBox* pdev;
	wxButton* okbutton;
	wxButton* cancel;
};

wxeditor_sounddev::wxeditor_sounddev(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Choose sound device"), wxDefaultPosition, wxSize(-1, -1))
{
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	auto pdev_map = audioapi_driver_get_devices(false);
	auto rdev_map = audioapi_driver_get_devices(true);
	std::string current_pdev = pdev_map[audioapi_driver_get_device(false)];
	std::string current_rdev = rdev_map[audioapi_driver_get_device(true)];
	std::vector<wxString> available_pdevs;
	std::vector<wxString> available_rdevs;
	for(auto i : pdev_map)
		available_pdevs.push_back(towxstring(i.second));
	for(auto i : rdev_map)
		available_rdevs.push_back(towxstring(i.second));

	wxFlexGridSizer* c_s = new wxFlexGridSizer(2, 2, 0, 0);
	c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Input device:")), 0, wxGROW);
	c_s->Add(rdev = new wxComboBox(this, wxID_ANY, towxstring(current_rdev), wxDefaultPosition,
		wxSize(400, -1), available_rdevs.size(), &available_rdevs[0], wxCB_READONLY), 1, wxGROW);
	c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Output device:")), 0, wxGROW);
	c_s->Add(pdev = new wxComboBox(this, wxID_ANY, towxstring(current_pdev), wxDefaultPosition,
		wxSize(400, -1), available_pdevs.size(), &available_pdevs[0], wxCB_READONLY), 1, wxGROW);
	top_s->Add(c_s);

	if(available_pdevs.size() < 2)
		pdev->Disable();
	if(available_rdevs.size() < 2)
		rdev->Disable();

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_sounddev::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_sounddev::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	c_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

bool wxeditor_sounddev::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_sounddev::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_sounddev::on_ok(wxCommandEvent& e)
{
	std::string newpdev = tostdstring(pdev->GetValue());
	std::string newrdev = tostdstring(rdev->GetValue());
	std::string _newpdev = "null";
	std::string _newrdev = "null";
	for(auto i : audioapi_driver_get_devices(false))
		if(i.second == newpdev)
			_newpdev = i.first;
	for(auto i : audioapi_driver_get_devices(true))
		if(i.second == newrdev)
			_newrdev = i.first;
	platform::set_sound_device(_newpdev, _newrdev);
	EndModal(wxID_OK);
}

void wxeditor_sounddev_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_sounddev(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
