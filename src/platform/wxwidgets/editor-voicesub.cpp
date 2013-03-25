#ifdef WITH_OPUS_CODEC
#include "core/inthread.hpp"
#include <stdexcept>

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "core/dispatch.hpp"
#include "library/string.hpp"
#include <sstream>

#define NOTHING 0xFFFFFFFFFFFFFFFFULL

namespace
{
	bool voicesub_open = false;
}

class wxeditor_voicesub : public wxDialog
{
public:
	wxeditor_voicesub(wxWindow* parent);
	~wxeditor_voicesub() throw();
	bool ShouldPreventAppExit() const;
	void on_select(wxCommandEvent& e);
	void on_play(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_export_o(wxCommandEvent& e);
	void on_export_p(wxCommandEvent& e);
	void on_export_q(wxCommandEvent& e);
	void on_export_s(wxCommandEvent& e);
	void on_import_o(wxCommandEvent& e);
	void on_import_p(wxCommandEvent& e);
	void on_import_q(wxCommandEvent& e);
	void on_change_ts(wxCommandEvent& e);
	void on_change_gain(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
	void on_unload(wxCommandEvent& e);
	void on_refresh(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void refresh();
private:
	struct refresh_listener : public information_dispatch
	{
		refresh_listener(wxeditor_voicesub* v)
			: information_dispatch("voicesub-editor-change-listner")
		{
			obj = v;
		}
		void on_voice_stream_change()
		{
			wxeditor_voicesub* _obj = obj;
			runuifun([_obj]() -> void { _obj->refresh(); });
		}
		wxeditor_voicesub* obj;
	};
	bool closing;
	uint64_t get_id();
	std::map<int, uint64_t> smap;
	wxListBox* subtitles;
	wxButton* playbutton;
	wxButton* deletebutton;
	wxButton* exportobutton;
	wxButton* exportpbutton;
	wxButton* exportqbutton;
	wxButton* exportsbutton;
	wxButton* importobutton;
	wxButton* importpbutton;
	wxButton* importqbutton;
	wxButton* changetsbutton;
	wxButton* changegainbutton;
	wxButton* loadbutton;
	wxButton* unloadbutton;
	wxButton* refreshbutton;
	wxButton* closebutton;
	refresh_listener* rlistener;
	
};

wxeditor_voicesub::wxeditor_voicesub(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit commentary track"), wxDefaultPosition, wxSize(-1, -1))
{
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(7, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(subtitles = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(300, 400), 0, NULL,
		wxLB_SINGLE), 1, wxGROW);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Stream")), 0, wxGROW);
	pbutton_s->Add(playbutton = new wxButton(this, wxID_ANY, wxT("Play")), 0, wxGROW);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Export")), 0, wxGROW);
	pbutton_s->Add(exportobutton = new wxButton(this, wxID_ANY, wxT("Opus")), 0, wxGROW);
	pbutton_s->Add(exportqbutton = new wxButton(this, wxID_ANY, wxT("Ogg")), 0, wxGROW);
	pbutton_s->Add(exportpbutton = new wxButton(this, wxID_ANY, wxT("Sox")), 0, wxGROW);
	pbutton_s->Add(exportsbutton = new wxButton(this, wxID_ANY, wxT("Superstream")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Import")), 0, wxGROW);
	pbutton_s->Add(importobutton = new wxButton(this, wxID_ANY, wxT("Opus")), 0, wxGROW);
	pbutton_s->Add(importqbutton = new wxButton(this, wxID_ANY, wxT("Ogg")), 0, wxGROW);
	pbutton_s->Add(importpbutton = new wxButton(this, wxID_ANY, wxT("Sox")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Misc.")), 0, wxGROW);
	pbutton_s->Add(changetsbutton = new wxButton(this, wxID_ANY, wxT("Change time")), 0, wxGROW);
	pbutton_s->Add(changegainbutton = new wxButton(this, wxID_ANY, wxT("Change gain")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Collection")), 0, wxGROW);
	pbutton_s->Add(loadbutton = new wxButton(this, wxID_ANY, wxT("Load")), 0, wxGROW);
	pbutton_s->Add(unloadbutton = new wxButton(this, wxID_ANY, wxT("Unload")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(refreshbutton = new wxButton(this, wxID_OK, wxT("Refresh")), 0, wxGROW);
	pbutton_s->Add(closebutton = new wxButton(this, wxID_OK, wxT("Close")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	playbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_play), NULL, this);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_delete), NULL, this);
	exportobutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export_o), NULL, this);
	exportpbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export_p), NULL, this);
	exportqbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export_q), NULL, this);
	exportsbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export_s), NULL, this);
	importobutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_import_o), NULL, this);
	importpbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_import_p), NULL, this);
	importqbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_import_q), NULL, this);
	changetsbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_change_ts), NULL, this);
	changegainbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_change_gain), NULL, this);
	loadbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_load), NULL, this);
	unloadbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_unload), NULL, this);
	refreshbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_refresh), NULL, this);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_close), NULL, this);
	subtitles->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_voicesub::on_select), NULL, this);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_voicesub::on_wclose));

	top_s->SetSizeHints(this);
	Fit();
	rlistener = new refresh_listener(this);
	refresh();
}

wxeditor_voicesub::~wxeditor_voicesub() throw()
{
	delete rlistener;
}

void wxeditor_voicesub::on_select(wxCommandEvent& e)
{
	if(closing)
		return;
	uint64_t id = get_id();
	bool valid = (id != NOTHING);
	playbutton->Enable(valid);
	deletebutton->Enable(valid);
	exportobutton->Enable(valid);
	exportpbutton->Enable(valid);
	exportqbutton->Enable(valid);
	changetsbutton->Enable(valid);
	changegainbutton->Enable(valid);
}

void wxeditor_voicesub::on_play(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		voicesub_play_stream(id);
	} catch(std::exception& e) {
		show_message_ok(this, "Error playing", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_delete(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		voicesub_delete_stream(id);
	} catch(std::exception& e) {
		show_message_ok(this, "Error deleting", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_export_o(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		std::string filename;
		filename = pick_file(this, "Select opus file to export", ".", true, "odm");
		voicesub_export_stream(id, filename, EXTFMT_OPUSDEMO);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_export_p(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		std::string filename;
		filename = pick_file(this, "Select sox file to export", ".", true, "sox");
		voicesub_export_stream(id, filename, EXTFMT_SOX);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_export_q(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		std::string filename;
			filename = pick_file(this, "Select Ogg (Opus) file to export", ".", true, "opus");
		voicesub_export_stream(id, filename, EXTFMT_OGGOPUS);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_export_s(wxCommandEvent& e)
{
	try {
		std::string filename;
		filename = pick_file(this, "Select sox file to export (superstream)", ".", true, "sox");
		voicesub_export_superstream(filename);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting superstream", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_import_o(wxCommandEvent& e)
{
	try {
		std::string filename;
		uint64_t ts;
		ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter position for newly "
			"imported stream"));
		filename = pick_file(this, "Select opus file to import", ".", false, "odm");
		voicesub_import_stream(ts, filename, EXTFMT_OPUSDEMO);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error importing", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_import_p(wxCommandEvent& e)
{
	try {
		std::string filename;
		uint64_t ts;
		ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter position for newly "
			"imported stream"));
		filename = pick_file(this, "Select sox file to import", ".", false, "sox");
		voicesub_import_stream(ts, filename, EXTFMT_SOX);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error importing", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_import_q(wxCommandEvent& e)
{
	try {
		std::string filename;
		uint64_t ts;
		ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter position for newly "
			"imported stream"));
		filename = pick_file(this, "Select Ogg (Opus) file to import", ".", false, "opus");
		voicesub_import_stream(ts, filename, EXTFMT_OGGOPUS);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error importing", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_change_ts(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		uint64_t ts;
		ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter new position for "
			"stream"));
		voicesub_alter_timebase(id, ts);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error changing timebase", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_change_gain(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		float gain;
		std::string old = (stringfmt() << voicesub_get_gain(id)).str();
		gain = parse_value<float>(pick_text(this, "Enter gain", "Enter new gain (dB) for stream", old));
		voicesub_set_gain(id, gain);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error changing gain", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_load(wxCommandEvent& e)
{
	try {
		std::string filename;
		try {
			filename = pick_file(this, "Select collection to load", ".", false, "lsvs");
		} catch(...) {
			return;
		}
		voicesub_load_collection(filename);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading collection", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_unload(wxCommandEvent& e)
{
	voicesub_unload_collection();
}

void wxeditor_voicesub::on_refresh(wxCommandEvent& e)
{
	refresh();
}

void wxeditor_voicesub::on_close(wxCommandEvent& e)
{
	Destroy();
	voicesub_open = false;
}

void wxeditor_voicesub::refresh()
{
	if(closing)
		return;
	bool cflag = voicesub_collection_loaded();
	unloadbutton->Enable(cflag);
	exportsbutton->Enable(cflag);
	importobutton->Enable(cflag);
	importpbutton->Enable(cflag);
	importqbutton->Enable(cflag);
	int sel = subtitles->GetSelection();
	subtitles->Clear();
	smap.clear();
	int next = 0;
	for(auto i : voicesub_get_stream_info()) {
		smap[next++] = i.id;
		std::ostringstream tmp;
		tmp << "#" << i.id << " " << voicesub_ts_seconds(i.length) << "s@" << voicesub_ts_seconds(i.base)
			<< "s";
		float gain = voicesub_get_gain(i.id);
		if(gain < -1e-5 || gain > 1e-5)
			tmp << " (gain " << gain << "dB)";
		std::string text = tmp.str();
		subtitles->Append(towxstring(text));
	}
	if(sel != wxNOT_FOUND && sel < (ssize_t)subtitles->GetCount())
		subtitles->SetSelection(sel);
	else if(subtitles->GetCount())
		subtitles->SetSelection(0);
	wxCommandEvent e;
	on_select(e);
}

uint64_t wxeditor_voicesub::get_id()
{
	int id = subtitles->GetSelection();
	return smap.count(id) ? smap[id] : NOTHING;
}

void wxeditor_voicesub::on_wclose(wxCloseEvent& e)
{
	bool wasc = closing;
	closing = true;
	if(!wasc)
		Destroy();
	voicesub_open = false;
}

bool wxeditor_voicesub::ShouldPreventAppExit() const { return false; }

void show_wxeditor_voicesub(wxWindow* parent)
{
	if(voicesub_open)
		return;
	wxeditor_voicesub* v = new wxeditor_voicesub(parent);
	v->Show();
	voicesub_open = true;
}
#endif
