#include "core/moviedata.hpp"

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

class wxeditor_authors : public wxDialog
{
public:
	wxeditor_authors(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_authors_change(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
private:
	wxTextCtrl* projectname;
	wxTextCtrl* authors;
	wxButton* okbutton;
	wxButton* cancel;
};


wxeditor_authors::wxeditor_authors(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit game name & authors"), wxDefaultPosition, wxSize(-1, -1))
{
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(4, 1, 0, 0);
	SetSizer(top_s);

	wxFlexGridSizer* c_s = new wxFlexGridSizer(1, 2, 0, 0);
	c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Game name:")), 0, wxGROW);
	c_s->Add(projectname = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1, wxGROW);
	top_s->Add(c_s);

	top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Authors (one per line):")), 0, wxGROW);
	top_s->Add(authors = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE), 0, wxGROW);
	authors->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxeditor_authors::on_authors_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_authors::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_authors::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	c_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	std::string gamename;
	runemufn([&gamename]() { gamename = our_movie.gamename; });
	projectname->SetValue(towxstring(gamename));
	std::string x;
	runemufn([&x]() {
			for(auto i : our_movie.authors)
				x = x + i.first + "|" + i.second + "\n";
		});
	authors->SetValue(towxstring(x));
}

bool wxeditor_authors::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_authors::on_authors_change(wxCommandEvent& e)
{
	try {
		size_t lines = authors->GetNumberOfLines();
		for(size_t i = 0; i < lines; i++) {
			std::string l = tostdstring(authors->GetLineText(i));
			if(l == "|")
				throw 43;
		}
		okbutton->Enable();
	} catch(...) {
		okbutton->Disable();
	}
}

void wxeditor_authors::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_authors::on_ok(wxCommandEvent& e)
{
	std::string gamename = tostdstring(projectname->GetValue());
	runemufn([gamename]() { our_movie.gamename = gamename; });
	std::vector<std::pair<std::string, std::string>> newauthors;
	size_t lines = authors->GetNumberOfLines();
	for(size_t i = 0; i < lines; i++) {
		std::string l = tostdstring(authors->GetLineText(i));
		if(l != "" && l != "|")
			newauthors.push_back(split_author(l));
	}
	runemufn([newauthors]() { our_movie.authors = newauthors; });
	EndModal(wxID_OK);
}

void wxeditor_authors_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_authors(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
