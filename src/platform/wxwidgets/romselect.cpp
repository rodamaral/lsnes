//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/statbox.h>
#include <wx/notebook.h>

#include "lsnes.hpp"

#include "core/controller.hpp"
#include "core/moviedata.hpp"
#include "core/moviedata.hpp"
#include "core/framerate.hpp"
#include "core/project.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "interface/setting.hpp"
#include "library/directory.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include <boost/lexical_cast.hpp>

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"


#define ASK_SRAMS_BASE		(wxID_HIGHEST + 129)
#define ASK_SRAMS_LAST		(wxID_HIGHEST + 255)

namespace
{
	std::string generate_project_id()
	{
		return (stringfmt() << time(NULL) << "_" << get_random_hexstring(4)).str();
	}

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
		setting_select(wxWindow* parent, const core_setting& s);
		wxWindow* get_label() { return label; }
		wxWindow* get_control();
		const core_setting& get_setting() { return setting; }
		std::string read();
	private:
		wxStaticText* label;
		wxTextCtrl* text;
		wxCheckBox* check;
		wxComboBox* combo;
		core_setting setting;
	};

	setting_select::setting_select(wxWindow* parent, const core_setting& s)
		: setting(s)
	{
		label = new wxStaticText(parent, wxID_ANY, towxstring(setting.hname));
		text = NULL;
		combo = NULL;
		check = NULL;
		if(setting.is_boolean()) {
			check = new wxCheckBox(parent, wxID_ANY, towxstring(""));
			check->SetValue(s.dflt != "0");
		} else if(setting.is_freetext()) {
			text = new wxTextCtrl(parent, wxID_ANY, towxstring(setting.dflt), wxDefaultPosition,
				wxSize(400, -1));
		} else {
			std::vector<wxString> _hvalues;
			std::string dflt = "";
			for(auto i : setting.values) {
				_hvalues.push_back(towxstring(i.hname));
				if(i.iname == setting.dflt)
					dflt = i.hname;
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
		if(combo) return setting.hvalue_to_ivalue(tostdstring(combo->GetValue()));
		return "";
	}

	class wxwin_newproject : public wxDialog
	{
	public:
		wxwin_newproject(wxWindow* parent);
		~wxwin_newproject();
		void on_ok(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e);
		void on_projname_edit(wxCommandEvent& e);
		void on_memorywatch_select(wxCommandEvent& e);
		void on_directory_select(wxCommandEvent& e);
		void on_add(wxCommandEvent& e);
		void on_remove(wxCommandEvent& e);
		void on_up(wxCommandEvent& e);
		void on_down(wxCommandEvent& e);
		void on_luasel(wxCommandEvent& e);
	private:
		void reorder_scripts(int delta);
		wxTextCtrl* projname;
		wxTextCtrl* memwatch;
		wxTextCtrl* projdir;
		wxTextCtrl* projpfx;
		wxListBox* luascripts;
		wxButton* swatch;
		wxButton* sdir;
		wxButton* addbutton;
		wxButton* removebutton;
		wxButton* upbutton;
		wxButton* downbutton;
		wxButton* okbutton;
		wxButton* cancel;
	};

	wxwin_newproject::~wxwin_newproject()
	{
	}

	wxwin_newproject::wxwin_newproject(wxWindow* parent)
		: wxDialog(parent, wxID_ANY, wxT("New Project"), wxDefaultPosition, wxSize(-1, -1),
			wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
	{
		Centre();
		wxBoxSizer* toplevel = new wxBoxSizer(wxVERTICAL);
		SetSizer(toplevel);

		wxFlexGridSizer* c_s = new wxFlexGridSizer(1, 2, 0, 0);
		c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Project name:")), 0, wxGROW);
		c_s->Add(projname = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1,
			wxGROW);
		projname->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxwin_newproject::on_projname_edit), NULL, this);
		toplevel->Add(c_s);

		wxFlexGridSizer* c4_s = new wxFlexGridSizer(1, 3, 0, 0);
		c4_s->Add(new wxStaticText(this, wxID_ANY, wxT("Directory:")), 0, wxGROW);
		c4_s->Add(projdir = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1,
			wxGROW);
		projdir->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxwin_newproject::on_projname_edit), NULL, this);
		c4_s->Add(sdir = new wxButton(this, wxID_ANY, wxT("...")), 1, wxGROW);
		sdir->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_directory_select), NULL, this);
		toplevel->Add(c4_s);

		wxFlexGridSizer* c5_s = new wxFlexGridSizer(1, 2, 0, 0);
		c5_s->Add(new wxStaticText(this, wxID_ANY, wxT("Prefix:")), 0, wxGROW);
		c5_s->Add(projpfx = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1,
			wxGROW);
		projpfx->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxwin_newproject::on_projname_edit), NULL, this);
		toplevel->Add(c5_s);

		wxFlexGridSizer* c2_s = new wxFlexGridSizer(1, 3, 0, 0);
		c2_s->Add(new wxStaticText(this, wxID_ANY, wxT("Memory watch:")), 0, wxGROW);
		c2_s->Add(memwatch = new wxTextCtrl(this, wxID_ANY, wxT(""),
			wxDefaultPosition, wxSize(350, -1)), 1, wxGROW);
		wxButton* pdir;
		c2_s->Add(swatch = new wxButton(this, wxID_ANY, wxT("...")), 1, wxGROW);
		swatch->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_memorywatch_select), NULL, this);
		toplevel->Add(c2_s);

		toplevel->Add(new wxStaticText(this, wxID_ANY, wxT("Autoload lua scripts:")), 0, wxGROW);
		toplevel->Add(luascripts = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 100)), 1,
			wxEXPAND);
		luascripts->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxwin_newproject::on_luasel), NULL, this);

		wxFlexGridSizer* c3_s = new wxFlexGridSizer(1, 4, 0, 0);
		c3_s->Add(addbutton = new wxButton(this, wxID_ANY, wxT("Add")), 1, wxGROW);
		addbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_add), NULL, this);
		c3_s->Add(removebutton = new wxButton(this, wxID_ANY, wxT("Remove")), 1, wxGROW);
		removebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_remove), NULL, this);
		removebutton->Disable();
		c3_s->Add(upbutton = new wxButton(this, wxID_ANY, wxT("Up")), 1, wxGROW);
		upbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_up), NULL, this);
		upbutton->Disable();
		c3_s->Add(downbutton = new wxButton(this, wxID_ANY, wxT("Down")), 1, wxGROW);
		downbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_down), NULL, this);
		downbutton->Disable();
		toplevel->Add(c3_s);

		wxBoxSizer* buttonbar = new wxBoxSizer(wxHORIZONTAL);
		buttonbar->Add(okbutton = new wxButton(this, wxID_ANY, wxT("OK")), 0, wxGROW);
		buttonbar->AddStretchSpacer();
		buttonbar->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
		okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_ok), NULL, this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_newproject::on_cancel), NULL, this);
		toplevel->Add(buttonbar, 0, wxGROW);
		//This gets re-enabled later if needed.
		okbutton->Disable();

		toplevel->SetSizeHints(this);
		Fit();
	}

	void wxwin_newproject::on_ok(wxCommandEvent& e)
	{
		project_info pinfo;
		pinfo.id = generate_project_id();
		pinfo.name = tostdstring(projname->GetValue());
		pinfo.rom = our_rom.load_filename;
		pinfo.last_save = "";
		pinfo.directory = tostdstring(projdir->GetValue());
		pinfo.prefix = tostdstring(projpfx->GetValue());
		pinfo.gametype = our_movie.gametype->get_name();
		pinfo.settings = our_movie.settings;
		pinfo.coreversion = our_movie.coreversion;
		pinfo.gamename = our_movie.gamename;
		pinfo.authors = our_movie.authors;
		pinfo.movie_sram = our_movie.movie_sram;
		pinfo.anchor_savestate = our_movie.anchor_savestate;
		pinfo.movie_rtc_second = our_movie.movie_rtc_second;
		pinfo.movie_rtc_subsecond = our_movie.movie_rtc_subsecond;
		pinfo.projectid = our_movie.projectid;
		project_copy_watches(pinfo);
		project_copy_macros(pinfo, controls);
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			pinfo.roms[i] = our_rom.romimg[i].filename;
			pinfo.romimg_sha256[i] = our_movie.romimg_sha256[i];
			pinfo.romxml_sha256[i] = our_movie.romxml_sha256[i];
			pinfo.namehint[i] = our_movie.namehint[i];
		}
		for(unsigned i = 0; i < luascripts->GetCount(); i++)
			pinfo.luascripts.push_back(tostdstring(luascripts->GetString(i)));
		if(memwatch->GetValue().length() == 0)
			goto no_watch;
		try {
			std::istream& in = zip::openrel(tostdstring(memwatch->GetValue()), "");
			while(in) {
				std::string wname;
				std::string wexpr;
				std::getline(in, wname);
				std::getline(in, wexpr);
				pinfo.watches[strip_CR(wname)] = strip_CR(wexpr);
			}
			delete &in;
		} catch(std::exception& e) {
			show_message_ok(this, "Error", std::string("Can't load memory watch: ") + e.what(),
				wxICON_EXCLAMATION);
			return;
		}
no_watch:
		project_info* pinfo2 = new project_info(pinfo);
		project_flush(pinfo2);
		project_info* old_proj = project_get();
		project_set(pinfo2, true);
		if(old_proj)
			delete old_proj;
		EndModal(wxID_OK);
	}

	void wxwin_newproject::on_cancel(wxCommandEvent& e)
	{
		EndModal(wxID_CANCEL);
	}

	void wxwin_newproject::on_memorywatch_select(wxCommandEvent& e)
	{
		try {
			std::string lwch = choose_file_load(this, "Select memory watch file", ".", filetype_watch);
			try {
				auto& p = zip::openrel(lwch, "");
				delete &p;
			} catch(std::exception& e) {
				show_message_ok(this, "File not found", "File '" + lwch + "' can't be opened",
					wxICON_EXCLAMATION);
				return;
			}
			memwatch->SetValue(towxstring(lwch));
		} catch(...) {
		}
	}

	void wxwin_newproject::on_projname_edit(wxCommandEvent& e)
	{
		bool ok = true;
		ok = ok && (projname->GetValue().length() > 0);
		ok = ok && (projdir->GetValue().length() > 0);
		ok = ok && (projpfx->GetValue().length() > 0);
		ok = ok && file_is_directory(tostdstring(projdir->GetValue()));
		okbutton->Enable(ok);
	}

	void wxwin_newproject::on_add(wxCommandEvent& e)
	{
		try {
			std::string luascript = choose_file_load(this, "Pick lua script", ".", filetype_lua_script);
			try {
				auto& p = zip::openrel(luascript, "");
				delete &p;
			} catch(std::exception& e) {
				show_message_ok(this, "File not found", "File '" + luascript + "' can't be opened",
					wxICON_EXCLAMATION);
				return;
			}
			luascripts->Append(towxstring(luascript));
		} catch(...) {
		}
	}

	void wxwin_newproject::on_remove(wxCommandEvent& e)
	{
		int sel = luascripts->GetSelection();
		int count = luascripts->GetCount();
		luascripts->Delete(sel);
		if(sel < count - 1)
			luascripts->SetSelection(sel);
		else if(count > 1)
			luascripts->SetSelection(count - 2);
		else
			luascripts->SetSelection(wxNOT_FOUND);
		on_luasel(e);
	}

	void wxwin_newproject::reorder_scripts(int delta)
	{
		int sel = luascripts->GetSelection();
		int count = luascripts->GetCount();
		if(sel == wxNOT_FOUND || sel + delta >= count || sel + delta < 0)
			return;
		wxString a = luascripts->GetString(sel);
		wxString b = luascripts->GetString(sel + delta);
		luascripts->SetString(sel, b);
		luascripts->SetString(sel + delta, a);
		luascripts->SetSelection(sel + delta);
	}

	void wxwin_newproject::on_up(wxCommandEvent& e)
	{
		reorder_scripts(-1);
		on_luasel(e);
	}

	void wxwin_newproject::on_down(wxCommandEvent& e)
	{
		reorder_scripts(1);
		on_luasel(e);
	}

	void wxwin_newproject::on_luasel(wxCommandEvent& e)
	{
		int sel = luascripts->GetSelection();
		int count = luascripts->GetCount();
		removebutton->Enable(sel != wxNOT_FOUND);
		upbutton->Enable(sel != wxNOT_FOUND && sel > 0);
		downbutton->Enable(sel != wxNOT_FOUND && sel < count - 1);
	}

	void wxwin_newproject::on_directory_select(wxCommandEvent& e)
	{
		wxDirDialog* d = new wxDirDialog(this, wxT("Select project directory"), projdir->GetValue(),
			wxDD_DIR_MUST_EXIST);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		projdir->SetValue(d->GetPath());
		d->Destroy();
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
	if(!our_rom.rtype) {
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

	std::set<std::string> sram_set = our_rom.rtype->srams();

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
	wxFlexGridSizer* mainblock = new wxFlexGridSizer(4 + our_rom.rtype->get_settings().settings.size() +
		sram_set.size(), 2, 0, 0);
	for(auto i : our_rom.rtype->get_settings().settings) {
		settings.insert(std::make_pair(i.second.iname, setting_select(new_panel, i.second)));
		mainblock->Add(settings.find(i.second.iname)->second.get_label());
		mainblock->Add(settings.find(i.second.iname)->second.get_control());
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
	buttonbar->Add(quit = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
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
		mov.save("$MEMORY:wxwidgets-romload-tmp", 0, true);
		platform::queue("load-state $MEMORY:wxwidgets-romload-tmp");
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
	f.gametype = &our_rom.rtype->combine_region(*our_rom.region);
	for(auto i : settings) {
		f.settings[i.first] = i.second.read();
		if(!i.second.get_setting().validate(f.settings[i.first]))
			throw std::runtime_error((stringfmt() << "Bad value for setting " << i.first).str());
	}
	f.coreversion = our_rom.rtype->get_core_identifier();
	f.gamename = tostdstring(projectname->GetValue());
	f.projectid = get_random_hexstring(40);
	set_mprefix_for_project(f.projectid, tostdstring(prefix->GetValue()));
	f.rerecords = "0";
	for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
		f.romimg_sha256[i] = our_rom.romimg[i].sha_256.read();
		f.romxml_sha256[i] = our_rom.romxml[i].sha_256.read();
		f.namehint[i] = our_rom.romimg[i].namehint;
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
				f.movie_sram[i.first] = zip::readrel(sf, "");
		} else {
			if(sf != "")
				f.anchor_savestate = zip::readrel(sf, "");
		}
	}
	f.is_savestate = false;
	f.movie_rtc_second = f.rtc_second = boost::lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
	f.movie_rtc_subsecond = f.rtc_subsecond = boost::lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue()));
	if(f.movie_rtc_subsecond < 0)
		throw std::runtime_error("RTC subsecond must be positive");
	auto ctrldata = our_rom.rtype->controllerconfig(f.settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex());
	f.input.clear(ports);
	return f;
}

void open_new_project_window(wxWindow* parent)
{
	if(our_rom.rtype->isnull()) {
		show_message_ok(parent, "Can't start new project", "No ROM loaded", wxICON_EXCLAMATION);
		return;
	}
	wxwin_newproject* projwin = new wxwin_newproject(parent);
	projwin->ShowModal();
	projwin->Destroy();
}
