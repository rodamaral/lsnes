//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/statbox.h>
#include <wx/notebook.h>

#include "lsnes.hpp"

#include "core/moviedata.hpp"
#include "core/framerate.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "interface/setting.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include <boost/lexical_cast.hpp>

#include "platform/wxwidgets/platform.hpp"

#define ROM_SELECTS_BASE	(wxID_HIGHEST + 0)
#define ROM_SELECTS_LAST	(wxID_HIGHEST + 127)
#define ASK_FILENAME_BUTTON	(wxID_HIGHEST + 128)
#define ASK_SRAMS_BASE		(wxID_HIGHEST + 129)
#define ASK_SRAMS_LAST		(wxID_HIGHEST + 255)

#define MARKUP_POSTFIX " Markup"


void patching_done(struct loaded_rom& rom, wxWindow* modwin);

#define ROMSELECT_ROM_COUNT 27

namespace
{
	class textboxloadfilename : public wxFileDropTarget
	{
	public:
		textboxloadfilename(wxTextCtrl* _ctrl)
		{
			ctrl = _ctrl;
		}

		bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
		{
			if(filenames.Count() != 1)
				return false;
			ctrl->SetValue(filenames[0]);
			return true;
		}
	private:
		wxTextCtrl* ctrl;
	};

	struct setting_select
	{
		setting_select() {}
		setting_select(wxWindow* parent, const core_setting& s);
		wxWindow* get_label() { return label; }
		wxWindow* get_control();
		const core_setting* get_setting() { return setting; }
		std::string read();
	private:
		wxStaticText* label;
		wxTextCtrl* text;
		wxCheckBox* check;
		wxComboBox* combo;
		const core_setting* setting;
	};

	setting_select::setting_select(wxWindow* parent, const core_setting& s)
		: setting(&s)
	{
		label = new wxStaticText(parent, wxID_ANY, towxstring(setting->hname));
		text = NULL;
		combo = NULL;
		check = NULL;
		if(setting->is_boolean()) {
			check = new wxCheckBox(parent, wxID_ANY, towxstring(""));
			check->SetValue(s.dflt != "0");
		} else if(setting->is_freetext()) {
			text = new wxTextCtrl(parent, wxID_ANY, towxstring(setting->dflt), wxDefaultPosition,
				wxSize(400, -1));
		} else {
			std::vector<wxString> _hvalues;
			std::string dflt = "";
			for(auto i : setting->values) {
				_hvalues.push_back(towxstring(i->hname));
				if(i->iname == setting->dflt)
					dflt = i->hname;
			}
			combo = new wxComboBox(parent, wxID_ANY, towxstring(dflt), wxDefaultPosition,
				wxDefaultSize, _hvalues.size(), &_hvalues[0], wxCB_READONLY);
		}
	}

	wxWindow* setting_select::get_control()
	{
		if(text) return text;
		if(check) return check;
		if(combo) return combo;
		return NULL;
	}

	std::string setting_select::read()
	{
		if(text) return tostdstring(text->GetValue());
		if(check) return check->GetValue() ? "1" : "0";
		if(combo) return setting->hvalue_to_ivalue(tostdstring(combo->GetValue()));
		return "";
	}
}

class wxwin_project : public wxDialog
{
public:
	wxwin_project();
	~wxwin_project();
	void on_file_select(wxCommandEvent& e);
	void on_new_select(wxCommandEvent& e);
	void on_filename_change(wxCommandEvent& e);
	void on_ask_filename(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
private:
	struct moviefile make_movie();
	std::map<std::string, wxTextCtrl*> sram_files;
	std::map<std::string, wxButton*> sram_choosers;
	std::map<std::string, setting_select> settings;
	wxTextCtrl* projectname;
	wxTextCtrl* prefix;
	wxTextCtrl* rtc_sec;
	wxTextCtrl* rtc_subsec;
	wxTextCtrl* authors;
	wxButton* load;
	wxButton* quit;
	std::map<unsigned, std::string> sram_names;
};



void show_projectwindow(wxWindow* modwin)
{
	if(!our_rom->rtype) {
		show_message_ok(modwin, "Can't start new movie", "No ROM loaded", wxICON_EXCLAMATION);
		return;
	}
	wxwin_project* projwin = new wxwin_project();
	projwin->ShowModal();
	projwin->Destroy();
}

//------------------------------------------------------------

wxwin_project::wxwin_project()
	: wxDialog(NULL, wxID_ANY, wxT("Project settings"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	std::vector<wxString> cchoices;
	unsigned dfltidx;

	std::set<std::string> sram_set = our_rom->rtype->srams();

	Centre();
	//2 Top-level block.
	//- Notebook
	//- Button bar.
	wxBoxSizer* toplevel = new wxBoxSizer(wxVERTICAL);
	SetSizer(toplevel);
	wxPanel* new_panel = new wxPanel(this);

	//The new page.
	//3 Page-level blocks.
	//- Controllertypes/initRTC/Gamename/SRAMs.
	//- Authors explanation.
	//- Authors
	wxFlexGridSizer* new_sizer = new wxFlexGridSizer(3, 1, 0, 0);
	new_panel->SetSizer(new_sizer);
	//Controllertypes/Gamename/initRTC/SRAMs.
	wxFlexGridSizer* mainblock = new wxFlexGridSizer(4 + our_rom->rtype->get_settings().settings.size() +
		sram_set.size(), 2, 0, 0);
	for(auto i : our_rom->rtype->get_settings().settings) {
		settings[i.second->iname] = setting_select(new_panel, *i.second);
		mainblock->Add(settings[i.second->iname].get_label());
		mainblock->Add(settings[i.second->iname].get_control());
	}
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Initial RTC value:")), 0, wxGROW);
	wxFlexGridSizer* initrtc = new wxFlexGridSizer(1, 3, 0, 0);
	initrtc->Add(rtc_sec = new wxTextCtrl(new_panel, wxID_ANY, wxT("1000000000"), wxDefaultPosition,
		wxSize(150, -1)), 1, wxGROW);
	initrtc->Add(new wxStaticText(new_panel, wxID_ANY, wxT(":")), 0, wxGROW);
	initrtc->Add(rtc_subsec = new wxTextCtrl(new_panel, wxID_ANY, wxT("0"), wxDefaultPosition,
		wxSize(150, -1)), 1, wxGROW);
	mainblock->Add(initrtc, 0, wxGROW);
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Game name:")), 0, wxGROW);
	mainblock->Add(projectname = new wxTextCtrl(new_panel, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)),
		1, wxGROW);
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Save prefix:")), 0, wxGROW);
	mainblock->Add(prefix = new wxTextCtrl(new_panel, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1,
		wxGROW);
	unsigned idx = 0;
	sram_set.insert("");
	for(auto i : sram_set) {
		std::string name = "SRAM " + i;
		if(i == "")
			name = "Anchor savestate";
		mainblock->Add(new wxStaticText(new_panel, wxID_ANY, towxstring(name)), 0, wxGROW);
		wxFlexGridSizer* fileblock2 = new wxFlexGridSizer(1, 2, 0, 0);
		fileblock2->Add(sram_files[i] = new wxTextCtrl(new_panel, wxID_ANY, wxT(""), wxDefaultPosition,
			wxSize(500, -1)), 1, wxGROW);
		sram_files[i]->SetDropTarget(new textboxloadfilename(sram_files[i]));
		fileblock2->Add(sram_choosers[i] = new wxButton(new_panel, ASK_SRAMS_BASE + idx, wxT("Pick")), 0,
			wxGROW);
		sram_files[i]->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxwin_project::on_filename_change), NULL, this);
		sram_files[i]->SetDropTarget(new textboxloadfilename(sram_files[i]));
		sram_choosers[i]->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_project::on_ask_filename), NULL, this);
		mainblock->Add(fileblock2, 0, wxGROW);
		sram_names[idx] = i;
		idx++;
	}
	new_sizer->Add(mainblock, 0, wxGROW);

	//Authors
	new_sizer->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Authors (one per line):")), 0, wxGROW);
	new_sizer->Add(authors = new wxTextCtrl(new_panel, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE), 0, wxGROW);
	authors->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wxwin_project::on_filename_change), NULL,
		this);

	toplevel->Add(new_panel, 1, wxGROW);

	//Button bar.
	wxBoxSizer* buttonbar = new wxBoxSizer(wxHORIZONTAL);
	buttonbar->Add(load = new wxButton(this, wxID_ANY, wxT("Load")), 0, wxGROW);
	buttonbar->AddStretchSpacer();
	buttonbar->Add(quit = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxGROW);
	load->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_project::on_load), NULL, this);
	quit->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_project::on_quit), NULL, this);
	toplevel->Add(buttonbar, 0, wxGROW);

	//This gets re-enabled later if needed.
	load->Disable();
	wxCommandEvent e2;
	on_filename_change(e2);

	mainblock->SetSizeHints(this);
	new_sizer->SetSizeHints(this);
	toplevel->SetSizeHints(this);
	Fit();
}

wxwin_project::~wxwin_project()
{
}

void wxwin_project::on_ask_filename(wxCommandEvent& e)
{
	int id = e.GetId();
	try {
		if(id >= ASK_SRAMS_BASE && id <= ASK_SRAMS_LAST) {
			std::string fname = pick_file_member(this, "Choose " + sram_names[id - ASK_SRAMS_BASE], ".");
			sram_files[sram_names[id - ASK_SRAMS_BASE]]->SetValue(towxstring(fname));
		}
		on_filename_change(e);
	} catch(canceled_exception& e) {
	}
}

void wxwin_project::on_filename_change(wxCommandEvent& e)
{
	try {
		boost::lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
		if(boost::lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue())) < 0)
			throw 42;
		size_t lines = authors->GetNumberOfLines();
		for(size_t i = 0; i < lines; i++) {
			std::string l = tostdstring(authors->GetLineText(i));
			if(l == "|")
				throw 43;
		}
		load->Enable();
	} catch(...) {
		load->Disable();
	}
}

void wxwin_project::on_quit(wxCommandEvent& e)
{
	EndModal(0);
}

void wxwin_project::on_load(wxCommandEvent& e)
{
	try {
		moviefile mov = make_movie();
		mov.start_paused = false;
		mov.save(get_config_path() + "/movie.tmp", 0);
		platform::queue("load-state " + get_config_path() + "/movie.tmp");
		EndModal(0);
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading movie", e.what(), wxICON_EXCLAMATION);
		return;
	}
}

struct moviefile wxwin_project::make_movie()
{
	moviefile f;
	f.force_corrupt = false;
	f.gametype = &our_rom->rtype->combine_region(*our_rom->region);
	for(auto i : settings) {
		f.settings[i.first] = i.second.read();
		if(!i.second.get_setting()->validate(f.settings[i.first]))
			throw std::runtime_error((stringfmt() << "Bad value for setting " << i.first).str());
	}
	f.coreversion = our_rom->rtype->get_core_identifier();
	f.gamename = tostdstring(projectname->GetValue());
	f.projectid = get_random_hexstring(40);
	set_mprefix_for_project(f.projectid, tostdstring(prefix->GetValue()));
	f.rerecords = "0";
	for(size_t i = 0; i < sizeof(our_rom->romimg)/sizeof(our_rom->romimg[0]); i++) {
		f.romimg_sha256[i] = our_rom->romimg[i].sha_256;
		f.romxml_sha256[i] = our_rom->romxml[i].sha_256;
	}
	size_t lines = authors->GetNumberOfLines();
	for(size_t i = 0; i < lines; i++) {
		std::string l = tostdstring(authors->GetLineText(i));
		if(l != "" && l != "|")
			f.authors.push_back(split_author(l));
	}
	for(auto i : sram_files) {
		std::string sf = tostdstring(i.second->GetValue());
		if(i.first != "") {
			if(sf != "")
				f.movie_sram[i.first] = read_file_relative(sf, "");
		} else {
			if(sf != "")
				f.anchor_savestate = read_file_relative(sf, "");
		}
	}
	f.is_savestate = false;
	f.movie_rtc_second = f.rtc_second = boost::lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
	f.movie_rtc_subsecond = f.rtc_subsecond = boost::lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue()));
	if(f.movie_rtc_subsecond < 0)
		throw std::runtime_error("RTC subsecond must be positive");
	auto ctrldata = our_rom->rtype->controllerconfig(f.settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex);
	f.input.clear(ports);
	return f;
}

