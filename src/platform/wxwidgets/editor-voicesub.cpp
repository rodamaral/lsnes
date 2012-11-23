#ifdef WITH_OPUS_CODEC
#include "core/inthread.hpp"
#include <stdexcept>

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "library/string.hpp"

#define NOTHING 0xFFFFFFFFFFFFFFFFULL

namespace
{
	bool voicesub_open = false;
}

class wxeditor_voicesub : public wxDialog
{
public:
	wxeditor_voicesub(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_select(wxCommandEvent& e);
	void on_play(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_export_o(wxCommandEvent& e);
	void on_export_p(wxCommandEvent& e);
	void on_export_s(wxCommandEvent& e);
	void on_import_o(wxCommandEvent& e);
	void on_import_p(wxCommandEvent& e);
	void on_change_ts(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
	void on_unload(wxCommandEvent& e);
	void on_refresh(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
private:
	bool closing;
	void refresh();
	uint64_t get_id();
	std::map<int, uint64_t> smap;
	wxListBox* subtitles;
	wxButton* playbutton;
	wxButton* deletebutton;
	wxButton* exportobutton;
	wxButton* exportpbutton;
	wxButton* exportsbutton;
	wxButton* importobutton;
	wxButton* importpbutton;
	wxButton* changetsbutton;
	wxButton* loadbutton;
	wxButton* unloadbutton;
	wxButton* refreshbutton;
	wxButton* closebutton;
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
	pbutton_s->Add(exportpbutton = new wxButton(this, wxID_ANY, wxT("Sox")), 0, wxGROW);
	pbutton_s->Add(exportsbutton = new wxButton(this, wxID_ANY, wxT("Superstream")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Import")), 0, wxGROW);
	pbutton_s->Add(importobutton = new wxButton(this, wxID_ANY, wxT("Opus")), 0, wxGROW);
	pbutton_s->Add(importpbutton = new wxButton(this, wxID_ANY, wxT("Sox")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("Misc.")), 0, wxGROW);
	pbutton_s->Add(changetsbutton = new wxButton(this, wxID_ANY, wxT("Change time")), 0, wxGROW);
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
	exportsbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export_s), NULL, this);
	importobutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_import_o), NULL, this);
	importpbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_import_p), NULL, this);
	changetsbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_change_ts), NULL, this);
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
	refresh();
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
	changetsbutton->Enable(valid);
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
	refresh();
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
	refresh();
}

void wxeditor_voicesub::on_export_o(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		std::string filename;
		try {
			filename = pick_file(this, "Select opus file to export", ".", true);
		} catch(...) {
			return;
		}
		voicesub_export_stream(id, filename, EXTFMT_OPUSDEMO);
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_export_p(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		std::string filename;
		try {
			filename = pick_file(this, "Select sox file to export", ".", true);
		} catch(...) {
			return;
		}
		voicesub_export_stream(id, filename, EXTFMT_SOX);
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_export_s(wxCommandEvent& e)
{
	try {
		std::string filename;
		try {
			filename = pick_file(this, "Select sox file to export (superstream)", ".", true);
		} catch(...) {
			return;
		}
		voicesub_export_superstream(filename);
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting superstream", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_import_o(wxCommandEvent& e)
{
	try {
		std::string filename;
		uint64_t ts;
		try {
			ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter position for newly "
				"imported stream"));
			filename = pick_file(this, "Select opus file to import", ".", false);
		} catch(...) {
			return;
		}
		voicesub_import_stream(ts, filename, EXTFMT_OPUSDEMO);
	} catch(std::exception& e) {
		show_message_ok(this, "Error importing", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_import_p(wxCommandEvent& e)
{
	try {
		std::string filename;
		uint64_t ts;
		try {
			ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter position for newly "
				"imported stream"));
			filename = pick_file(this, "Select sox file to import", ".", false);
		} catch(...) {
			return;
		}
		voicesub_import_stream(ts, filename, EXTFMT_SOX);
	} catch(std::exception& e) {
		show_message_ok(this, "Error importing", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_change_ts(wxCommandEvent& e)
{
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		uint64_t ts;
		try {
			ts = voicesub_parse_timebase(pick_text(this, "Enter timebase", "Enter new position for "
				"stream"));
		} catch(...) {
			return;
		}
		voicesub_alter_timebase(id, ts);
	} catch(std::exception& e) {
		show_message_ok(this, "Error changing timebase", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_load(wxCommandEvent& e)
{
	try {
		std::string filename;
		try {
			filename = pick_file(this, "Select collection to load", ".", false);
		} catch(...) {
			return;
		}
		voicesub_load_collection(filename);
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading collection", e.what(), wxICON_EXCLAMATION);
	}
	refresh();
}

void wxeditor_voicesub::on_unload(wxCommandEvent& e)
{
	voicesub_unload_collection();
	refresh();
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
	bool cflag = voicesub_collection_loaded();
	unloadbutton->Enable(cflag);
	exportsbutton->Enable(cflag);
	importobutton->Enable(cflag);
	importpbutton->Enable(cflag);
	int sel = subtitles->GetSelection();
	subtitles->Clear();
	smap.clear();
	int next = 0;
	for(auto i : voicesub_get_stream_info()) {
		smap[next++] = i.id;
		std::string text = (stringfmt() << "#" << i.id << " " << voicesub_ts_seconds(i.length) << "s@"
			<< voicesub_ts_seconds(i.base) << "s").str();
		subtitles->Append(towxstring(text));
	}
	if(sel != wxNOT_FOUND && sel < subtitles->GetCount())
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
