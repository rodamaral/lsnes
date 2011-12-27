#include "core/moviedata.hpp"

#include "plat-wxwidgets/authorseditor.hpp"
#include "plat-wxwidgets/common.hpp"

wx_authors_editor::wx_authors_editor(wxWindow* parent)
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
		wxCommandEventHandler(wx_authors_editor::on_authors_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_authors_editor::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_authors_editor::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	c_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	projectname->SetValue(towxstring(our_movie.gamename));
	std::string x;
	for(auto i : our_movie.authors)
		x = x + i.first + "|" + i.second + "\n";
	authors->SetValue(towxstring(x));
}

bool wx_authors_editor::ShouldPreventAppExit() const
{
	return false;
}

void wx_authors_editor::on_authors_change(wxCommandEvent& e)
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

void wx_authors_editor::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wx_authors_editor::on_ok(wxCommandEvent& e)
{
	our_movie.gamename = tostdstring(projectname->GetValue());
	std::vector<std::pair<std::string, std::string>> newauthors;
	size_t lines = authors->GetNumberOfLines();
	for(size_t i = 0; i < lines; i++) {
		std::string l = tostdstring(authors->GetLineText(i));
		if(l != "" && l != "|")
			newauthors.push_back(split_author(l));
	}
	our_movie.authors = newauthors;
	EndModal(wxID_OK);
}
