#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/inthread.hpp"
#include "core/project.hpp"
#include "core/ui-services.hpp"
#include "library/string.hpp"

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"

#include <stdexcept>
#include <sstream>
#include <set>

#define NOTHING 0xFFFFFFFFFFFFFFFFULL

namespace
{
	std::set<emulator_instance*> voicesub_open;
}

class wxeditor_voicesub : public wxDialog
{
public:
	wxeditor_voicesub(wxWindow* parent, emulator_instance& _inst);
	~wxeditor_voicesub() throw();
	bool ShouldPreventAppExit() const;
	void on_select(wxCommandEvent& e);
	void on_play(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_export(wxCommandEvent& e);
	void on_export_s(wxCommandEvent& e);
	void on_import(wxCommandEvent& e);
	void on_change_ts(wxCommandEvent& e);
	void on_change_gain(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
	void on_unload(wxCommandEvent& e);
	void on_refresh(wxCommandEvent& e);
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void refresh();
private:
	emulator_instance& inst;
	bool closing;
	uint64_t get_id();
	std::map<int, uint64_t> smap;
	wxListBox* subtitles;
	wxButton* playbutton;
	wxButton* deletebutton;
	wxButton* exportpbutton;
	wxButton* exportsbutton;
	wxButton* importpbutton;
	wxButton* changetsbutton;
	wxButton* changegainbutton;
	wxButton* loadbutton;
	wxButton* unloadbutton;
	wxButton* refreshbutton;
	wxButton* closebutton;
	struct dispatch::target<> corechange;
	struct dispatch::target<> vstreamchange;
};

wxeditor_voicesub::wxeditor_voicesub(wxWindow* parent, emulator_instance& _inst)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit commentary track"), wxDefaultPosition, wxSize(-1, -1)),
	inst(_inst)
{
	CHECK_UI_THREAD;
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(6, 1, 0, 0);
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
	pbutton_s->Add(new wxStaticText(this, wxID_ANY, wxT("I/O")), 0, wxGROW);
	pbutton_s->Add(importpbutton = new wxButton(this, wxID_ANY, wxT("Import")), 0, wxGROW);
	pbutton_s->Add(exportpbutton = new wxButton(this, wxID_ANY, wxT("Export")), 0, wxGROW);
	pbutton_s->Add(exportsbutton = new wxButton(this, wxID_ANY, wxT("Export all")), 0, wxGROW);
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
	exportpbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export), NULL, this);
	exportsbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_export_s), NULL, this);
	importpbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_voicesub::on_import), NULL, this);
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
	vstreamchange.set(inst.dispatch->voice_stream_change, [this]() {
		runuifun([this]() -> void { this->refresh(); });
	});
	corechange.set(inst.dispatch->core_change, [this]() {
		runuifun([this]() -> void { this->refresh(); });
	});
	refresh();
}

wxeditor_voicesub::~wxeditor_voicesub() throw()
{
}

void wxeditor_voicesub::on_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	uint64_t id = get_id();
	bool valid = (id != NOTHING);
	playbutton->Enable(valid);
	deletebutton->Enable(valid);
	exportpbutton->Enable(valid);
	changetsbutton->Enable(valid);
	changegainbutton->Enable(valid);
}

void wxeditor_voicesub::on_play(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		inst.commentary->play_stream(id);
	} catch(std::exception& e) {
		show_message_ok(this, "Error playing", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_delete(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		inst.commentary->delete_stream(id);
	} catch(std::exception& e) {
		show_message_ok(this, "Error deleting", e.what(), wxICON_EXCLAMATION);
	}
}

namespace
{
	class _opus_or_sox
	{
	public:
		typedef std::pair<std::string, enum voice_commentary::external_stream_format> returntype;
		_opus_or_sox() {}
		filedialog_input_params input(bool save) const
		{
			filedialog_input_params p;
			p.types.push_back(filedialog_type_entry("Opus streams", "*.opus", "opus"));
			p.types.push_back(filedialog_type_entry("SoX files", "*.sox", "sox"));
			p.default_type = 0;
			return p;
		}
		std::pair<std::string, enum voice_commentary::external_stream_format> output(
			const filedialog_output_params& p, bool save) const
		{
			return std::make_pair(p.path, (p.typechoice == 1) ? voice_commentary::EXTFMT_SOX :
				voice_commentary::EXTFMT_OGGOPUS);
		}
	} filetype_opus_sox;
}

void wxeditor_voicesub::on_export(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		auto filename = choose_file_save(this, "Select file to epxort", UI_get_project_otherpath(inst),
			filetype_opus_sox);
		inst.commentary->export_stream(id, filename.first, filename.second);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_export_s(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string filename;
		filename = choose_file_save(this, "Select file to export superstream",
			UI_get_project_otherpath(inst), filetype_sox);
		inst.commentary->export_superstream(filename);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error exporting superstream", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_import(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		uint64_t ts;
		ts = inst.commentary->parse_timebase(pick_text(this, "Enter timebase",
			"Enter position for newly imported stream"));
		auto filename = choose_file_save(this, "Select file to import", UI_get_project_otherpath(inst),
			filetype_opus_sox);
		inst.commentary->import_stream(ts, filename.first, filename.second);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error importing", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_change_ts(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		uint64_t ts;
		ts = inst.commentary->parse_timebase(pick_text(this, "Enter timebase",
			"Enter new position for stream"));
		inst.commentary->alter_timebase(id, ts);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error changing timebase", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_change_gain(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	uint64_t id = get_id();
	if(id == NOTHING)
		return;
	try {
		float gain;
		std::string old = (stringfmt() << inst.commentary->get_gain(id)).str();
		gain = parse_value<float>(pick_text(this, "Enter gain", "Enter new gain (dB) for stream", old));
		inst.commentary->set_gain(id, gain);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error changing gain", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_load(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(UI_in_project_context(inst))
		return;
	try {
		std::string filename;
		try {
			//Use "." here because there can't be active project.
			filename = choose_file_load(this, "Select collection to load", ".", filetype_commentary);
		} catch(...) {
			return;
		}
		inst.commentary->load_collection(filename);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading collection", e.what(), wxICON_EXCLAMATION);
	}
}

void wxeditor_voicesub::on_unload(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	if(UI_in_project_context(inst))
		return;
	inst.commentary->unload_collection();
}

void wxeditor_voicesub::on_refresh(wxCommandEvent& e)
{
	refresh();
}

void wxeditor_voicesub::on_close(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	voicesub_open.erase(&inst);
	Destroy();
}

void wxeditor_voicesub::refresh()
{
	CHECK_UI_THREAD;
	if(closing)
		return;
	bool cflag = inst.commentary->collection_loaded();
	bool pflag = UI_in_project_context(inst);
	unloadbutton->Enable(cflag && !pflag);
	loadbutton->Enable(!pflag);
	exportsbutton->Enable(cflag);
	importpbutton->Enable(cflag);
	int sel = subtitles->GetSelection();
	subtitles->Clear();
	smap.clear();
	int next = 0;
	for(auto i : inst.commentary->get_stream_info()) {
		smap[next++] = i.id;
		std::ostringstream tmp;
		tmp << "#" << i.id << " " << inst.commentary->ts_seconds(i.length) << "s@"
			<< inst.commentary->ts_seconds(i.base) << "s";
		float gain = inst.commentary->get_gain(i.id);
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
	CHECK_UI_THREAD;
	int id = subtitles->GetSelection();
	return smap.count(id) ? smap[id] : NOTHING;
}

void wxeditor_voicesub::on_wclose(wxCloseEvent& e)
{
	CHECK_UI_THREAD;
	bool wasc = closing;
	closing = true;
	if(!wasc)
		Destroy();
	voicesub_open.erase(&inst);
}

bool wxeditor_voicesub::ShouldPreventAppExit() const { return false; }

void show_wxeditor_voicesub(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	if(voicesub_open.count(&inst))
		return;
	wxeditor_voicesub* v = new wxeditor_voicesub(parent, inst);
	v->Show();
	voicesub_open.insert(&inst);
}
