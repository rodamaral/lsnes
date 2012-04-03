#include "platform/wxwidgets/platform.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <vector>
#include <string>

#include <boost/lexical_cast.hpp>
#include <sstream>

#define FIRMWAREPATH "firmwarepath"
#define ROMPATH "rompath"
#define MOVIEPATH "moviepath"

class wxeditor_paths : public wxDialog
{
public:
	wxeditor_paths(wxWindow* parent);
	~wxeditor_paths();
	bool ShouldPreventAppExit() const;
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
private:
	wxButton* okbutton;
	wxButton* cancel;
	wxTextCtrl* rompath;
	wxTextCtrl* moviepath;
	wxTextCtrl* firmwarepath;
};

wxeditor_paths::wxeditor_paths(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Paths"), wxDefaultPosition, wxSize(-1, -1))
{
	std::string cur_rompath, cur_moviepath, cur_firmwarepath;
	runemufn([&cur_firmwarepath, &cur_moviepath, &cur_rompath]() {
		cur_firmwarepath = setting::get(FIRMWAREPATH);
		cur_rompath = setting::get(ROMPATH);
		cur_moviepath = setting::get(MOVIEPATH);
	});

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	wxFlexGridSizer* t_s = new wxFlexGridSizer(3, 2, 0, 0);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("ROMs:")), 0, wxGROW);
	t_s->Add(rompath = new wxTextCtrl(this, wxID_ANY, towxstring(cur_rompath), wxDefaultPosition, wxSize(400, -1)), 
		1, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Movies:")), 0, wxGROW);
	t_s->Add(moviepath = new wxTextCtrl(this, wxID_ANY, towxstring(cur_moviepath), wxDefaultPosition,
		wxSize(400, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Firmware:")), 0, wxGROW);
	t_s->Add(firmwarepath = new wxTextCtrl(this, wxID_ANY, towxstring(cur_firmwarepath), wxDefaultPosition,
		wxSize(400, -1)), 1, wxGROW);
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_paths::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_paths::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wxeditor_paths::~wxeditor_paths()
{
}

bool wxeditor_paths::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_paths::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_paths::on_ok(wxCommandEvent& e)
{
	std::string cur_rompath = tostdstring(rompath->GetValue());
	std::string cur_moviepath = tostdstring(moviepath->GetValue());
	std::string cur_firmwarepath = tostdstring(firmwarepath->GetValue());
	runemufn([cur_firmwarepath, cur_moviepath, cur_rompath]() {
		setting::set(ROMPATH, cur_rompath);
		setting::set(MOVIEPATH, cur_moviepath);
		setting::set(FIRMWAREPATH, cur_firmwarepath);
	});
	EndModal(wxID_OK);
}

void wxeditor_paths_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_paths(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
