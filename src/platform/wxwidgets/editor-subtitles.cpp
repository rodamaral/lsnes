#include "core/subtitles.hpp"
#include "core/moviedata.hpp"

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>

#include "library/string.hpp"
#include "core/emucore.hpp"

class wxeditor_subtitles : public wxDialog
{
public:
	wxeditor_subtitles(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_subtitles_change(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
private:
	wxTextCtrl* subs;
	wxButton* ok;
	wxButton* cancel;
};


wxeditor_subtitles::wxeditor_subtitles(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit subtitles"), wxDefaultPosition, wxSize(-1, -1))
{
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(subs = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, 300),
		wxTE_MULTILINE), 1, wxGROW);
	subs->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxeditor_subtitles::on_subtitles_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")), 0, wxGROW);
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles::on_cancel), NULL,
		this);
	top_s->Add(pbutton_s, 0, wxGROW);

	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();

	std::string txt = "";
	runemufn([&txt]() {
		for(auto i : get_subtitles()) {
			std::ostringstream line;
			line << i.first << " " << i.second << " " << get_subtitle_for(i.first, i.second) << std::endl;
			txt = txt + line.str();
		}
	});
	subs->SetValue(towxstring(txt));
}

bool wxeditor_subtitles::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_subtitles::on_subtitles_change(wxCommandEvent& e)
{
	std::string txt = tostdstring(subs->GetValue());
	std::string line;
	while(txt != "") {
		extract_token(txt, line, "\n");
		istrip_CR(line);
		if(line == "")
			continue;
		auto r = regex("([0-9]+)[ \t]+([0-9]+)[ \t]+(.+)", line);
		if(!r) {
			ok->Disable();
			return;
		}
		try {
			parse_value<uint64_t>(r[1]);
			parse_value<uint64_t>(r[2]);
		} catch(...) {
			ok->Disable();
			return;
		}
	}
	ok->Enable();
}

void wxeditor_subtitles::on_ok(wxCommandEvent& e)
{
	std::map<std::pair<uint64_t, uint64_t>, std::string> data;
	runemufn([&data]() {
		for(auto i : get_subtitles())
			data[std::make_pair(i.first, i.second)] = "";
	});
	std::string txt = tostdstring(subs->GetValue());
	std::string line;
	while(txt != "") {
		extract_token(txt, line, "\n");
		istrip_CR(line);
		if(line == "")
			continue;
		auto r = regex("([0-9]+)[ \t]+([0-9]+)[ \t]+(.+)", line);
		if(!r)
			return;
		try {
			uint64_t f = parse_value<uint64_t>(r[1]);
			uint64_t l = parse_value<uint64_t>(r[2]);
			data[std::make_pair(f, l)] = r[3];
		} catch(...) {
			return;
		}
	}
	runemufn([&data]() {
		for(auto i : data)
			set_subtitle_for(i.first.first, i.first.second, i.second);
	});
	EndModal(wxID_OK);
}

void wxeditor_subtitles::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_subtitles_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_subtitles(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
