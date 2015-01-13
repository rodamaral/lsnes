#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/radiobut.h>

#include "core/subtitles.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/dispatch.hpp"
#include "library/string.hpp"

#include "platform/wxwidgets/platform.hpp"
#include <cstdint>


namespace
{
	struct subdata
	{
		uint64_t first;
		uint64_t last;
		std::string text;
	};
}

class wxeditor_subtitles : public wxFrame
{
public:
	wxeditor_subtitles(wxWindow* parent, emulator_instance& _inst);
	~wxeditor_subtitles() throw();
	bool ShouldPreventAppExit() const;
	void on_change(wxCommandEvent& e);
	void on_add(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void refresh();
private:
	emulator_instance& inst;
	bool closing;
	wxListBox* subs;
	wxTextCtrl* subtext;
	wxButton* add;
	wxButton* edit;
	wxButton* _delete;
	wxButton* close;
	std::map<int, subdata> subtexts;
	struct dispatch::target<> subchange;
};

namespace
{

	class wxeditor_subtitles_subtitle : public wxDialog
	{
	public:
		wxeditor_subtitles_subtitle(wxWindow* parent, subdata d);
		void on_change(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e);
		void on_ok(wxCommandEvent& e);
		subdata get_result();
	private:
		wxTextCtrl* first;
		wxTextCtrl* last;
		wxTextCtrl* text;
		wxButton* ok;
		wxButton* cancel;
	};

	wxeditor_subtitles_subtitle::wxeditor_subtitles_subtitle(wxWindow* parent, subdata d)
		: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit subtitle"), wxDefaultPosition, wxSize(-1, -1))
	{
		CHECK_UI_THREAD;
		Centre();
		wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
		SetSizer(top_s);

		wxFlexGridSizer* data_s = new wxFlexGridSizer(3, 2, 0, 0);
		data_s->Add(new wxStaticText(this, wxID_ANY, wxT("First frame:")));
		data_s->Add(first = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(200, -1)));
		data_s->Add(new wxStaticText(this, wxID_ANY, wxT("Last frame:")));
		data_s->Add(last = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(200, -1)));
		data_s->Add(new wxStaticText(this, wxID_ANY, wxT("Text:")));
		data_s->Add(text = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)));
		top_s->Add(data_s, 1, wxGROW);

		first->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxeditor_subtitles_subtitle::on_change), NULL, this);
		last->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxeditor_subtitles_subtitle::on_change), NULL, this);
		text->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxeditor_subtitles_subtitle::on_change), NULL, this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_ANY, wxT("OK")), 0, wxGROW);
		pbutton_s->Add(cancel = new wxButton(this, wxID_ANY, wxT("Cancel")), 0, wxGROW);
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles_subtitle::on_ok),
			NULL, this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_subtitles_subtitle::on_cancel), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		pbutton_s->SetSizeHints(this);
		top_s->SetSizeHints(this);

		first->SetValue(towxstring((stringfmt() << d.first).str()));
		last->SetValue(towxstring((stringfmt() << d.last).str()));
		text->SetValue(towxstring(d.text));
		Fit();
	}

	void wxeditor_subtitles_subtitle::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		bool valid = true;
		std::string _first = tostdstring(first->GetValue());
		std::string _last = tostdstring(last->GetValue());
		std::string _text = tostdstring(text->GetValue());
		valid = valid && regex_match("[0-9]{1,19}", _first);
		valid = valid && regex_match("[0-9]{1,19}", _last);
		valid = valid && (_text != "");
		ok->Enable(valid);
	}

	void wxeditor_subtitles_subtitle::on_cancel(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		EndModal(wxID_CANCEL);
	}

	void wxeditor_subtitles_subtitle::on_ok(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		EndModal(wxID_OK);
	}

	subdata wxeditor_subtitles_subtitle::get_result()
	{
		CHECK_UI_THREAD;
		subdata d;
		d.first = parse_value<uint64_t>(tostdstring(first->GetValue()));
		d.last = parse_value<uint64_t>(tostdstring(last->GetValue()));
		d.text = tostdstring(text->GetValue());
		return d;
	}

	bool edit_subtext(wxWindow* w, struct subdata& d)
	{
		CHECK_UI_THREAD;
		bool res = false;
		wxeditor_subtitles_subtitle* editor = NULL;
		try {
			editor = new wxeditor_subtitles_subtitle(w, d);
			int ret = editor->ShowModal();
			if(ret == wxID_OK) {
				d = editor->get_result();
				res = true;
			}
		} catch(...) {
			return false;
		}
		if(editor)
			editor->Destroy();
		return res;
	}
}


wxeditor_subtitles::wxeditor_subtitles(wxWindow* parent, emulator_instance& _inst)
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Edit subtitles"), wxDefaultPosition, wxSize(-1, -1)), inst(_inst)
{
	CHECK_UI_THREAD;
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	//TODO: Apppropriate controls.
	top_s->Add(subs = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(300, 400), 0, NULL,
		wxLB_SINGLE), 1, wxGROW);
	subs->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_subtitles::on_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(add = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
	pbutton_s->Add(edit = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
	pbutton_s->Add(_delete = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	pbutton_s->Add(close = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
	add->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles::on_add), NULL, this);
	edit->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles::on_edit), NULL, this);
	_delete->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles::on_delete), NULL,
		this);
	close->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_subtitles::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
	subchange.set(inst.dispatch->subtitle_change, [this]() { runuifun([this]() -> void {
		this->refresh(); }); });
	refresh();
}

wxeditor_subtitles::~wxeditor_subtitles() throw()
{
}

bool wxeditor_subtitles::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_subtitles::on_close(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	closing = true;
	Destroy();
}

void wxeditor_subtitles::on_wclose(wxCloseEvent& e)
{
	closing = true;
}

void wxeditor_subtitles::refresh()
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	std::map<std::pair<uint64_t, uint64_t>, std::string> _subtitles;
	inst.iqueue->run([&_subtitles]() -> void {
		auto keys = CORE().subtitles->get_all();
		for(auto i : keys)
			_subtitles[i] = CORE().subtitles->get(i.first, i.second);
	});
	int sel = subs->GetSelection();
	bool found = (subtexts.count(sel) != 0);
	subdata matching = subtexts[sel];
	subs->Clear();
	unsigned num = 0;
	subtexts.clear();
	for(auto i : _subtitles) {
		subdata newdata;
		newdata.first = i.first.first;
		newdata.last = i.first.second;
		newdata.text = i.second;
		subtexts[num++] = newdata;
		std::string s = (stringfmt() << i.first.first << "-" << i.first.second << ": " << i.second).str();
		subs->Append(towxstring(s));
	}
	for(size_t i = 0; i < subs->GetCount(); i++)
		if(subtexts[i].first == matching.first && subtexts[i].last == matching.last)
			subs->SetSelection(i);
	if(subs->GetSelection() == wxNOT_FOUND && sel < (ssize_t)subs->GetCount())
		subs->SetSelection(sel);
	sel = subs->GetSelection();
	found = (subtexts.count(sel) != 0);
	edit->Enable(found);
	_delete->Enable(found);
}

void wxeditor_subtitles::on_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	int sel = subs->GetSelection();
	bool found = (subtexts.count(sel) != 0);
	edit->Enable(found);
	_delete->Enable(found);
}

void wxeditor_subtitles::on_add(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	subdata t;
	t.first = 0;
	t.last = 0;
	t.text = "";
	if(edit_subtext(this, t))
		inst.subtitles->set(t.first, t.last, t.text);
}

void wxeditor_subtitles::on_edit(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	int sel = subs->GetSelection();
	if(!subtexts.count(sel))
		return;
	auto t = subtexts[sel];
	auto old = t;
	if(edit_subtext(this, t)) {
		inst.subtitles->set(old.first, old.last, "");
		inst.subtitles->set(t.first, t.last, t.text);
	}
}

void wxeditor_subtitles::on_delete(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	int sel = subs->GetSelection();
	if(!subtexts.count(sel))
		return;
	auto t = subtexts[sel];
	inst.subtitles->set(t.first, t.last, "");
}

void wxeditor_subtitles_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	wxFrame* editor;
	if(!inst.mlogic) {
		show_message_ok(parent, "No movie", "Can't edit subtitles of nonexistent movie", wxICON_EXCLAMATION);
		return;
	}
	try {
		editor = new wxeditor_subtitles(parent, inst);
		editor->Show();
	} catch(...) {
	}
}
