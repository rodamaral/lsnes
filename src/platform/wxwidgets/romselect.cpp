//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/statbox.h>
#include <wx/notebook.h>

#include "lsnes.hpp"

#include "core/controller.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/moviedata.hpp"
#include "core/framerate.hpp"
#include "core/project.hpp"
#include "core/random.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "interface/setting.hpp"
#include "library/directory.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

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
			CHECK_UI_THREAD;
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
		CHECK_UI_THREAD;
		label = NULL;
		if(!setting.is_boolean())
			label = new wxStaticText(parent, wxID_ANY, towxstring(setting.hname));
		else
			label = new wxStaticText(parent, wxID_ANY, towxstring(""));
		text = NULL;
		combo = NULL;
		check = NULL;
		if(setting.is_boolean()) {
			check = new wxCheckBox(parent, wxID_ANY, towxstring(setting.hname));
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
		CHECK_UI_THREAD;
		if(text) return tostdstring(text->GetValue());
		if(check) return check->GetValue() ? "1" : "0";
		if(combo) return setting.hvalue_to_ivalue(tostdstring(combo->GetValue()));
		return "";
	}

	class wxwin_newproject : public wxDialog
	{
	public:
		wxwin_newproject(wxWindow* parent, emulator_instance& _inst);
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
		emulator_instance& inst;
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

	wxwin_newproject::wxwin_newproject(wxWindow* parent, emulator_instance& _inst)
		: wxDialog(parent, wxID_ANY, wxT("New Project"), wxDefaultPosition, wxSize(-1, -1),
			wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX), inst(_inst)
	{
		CHECK_UI_THREAD;
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
		CHECK_UI_THREAD;
		if(!inst.mlogic) {
			show_message_ok(this, "Error", "Can't start project without movie", wxICON_EXCLAMATION);
			return;
		}
		project_info pinfo(*inst.dispatch);
		pinfo.id = generate_project_id();
		pinfo.name = tostdstring(projname->GetValue());
		pinfo.rom = inst.rom->get_pack_filename();
		pinfo.last_save = "";
		pinfo.directory = tostdstring(projdir->GetValue());
		pinfo.prefix = tostdstring(projpfx->GetValue());
		auto& m = inst.mlogic->get_mfile();
		pinfo.gametype = m.gametype->get_name();
		pinfo.settings = m.settings;
		pinfo.coreversion = m.coreversion;
		pinfo.gamename = m.gamename;
		pinfo.authors = m.authors;
		pinfo.movie_sram = m.movie_sram;
		pinfo.anchor_savestate = m.anchor_savestate;
		pinfo.movie_rtc_second = m.movie_rtc_second;
		pinfo.movie_rtc_subsecond = m.movie_rtc_subsecond;
		pinfo.projectid = m.projectid;
		pinfo.active_branch = 0;
		pinfo.next_branch = 0;
		inst.project->copy_watches(pinfo);
		inst.project->copy_macros(pinfo, *inst.controls);
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			pinfo.roms[i] = inst.rom->get_rom(i).filename;
			pinfo.romimg_sha256[i] = m.romimg_sha256[i];
			pinfo.romxml_sha256[i] = m.romxml_sha256[i];
			pinfo.namehint[i] = m.namehint[i];
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
		pinfo2->flush();
		project_info* old_proj = inst.project->get();
		inst.project->set(pinfo2, true);
		if(old_proj)
			delete old_proj;
		EndModal(wxID_OK);
	}

	void wxwin_newproject::on_cancel(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		EndModal(wxID_CANCEL);
	}

	void wxwin_newproject::on_memorywatch_select(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
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
		ok = ok && directory::is_directory(tostdstring(projdir->GetValue()));
		okbutton->Enable(ok);
	}

	void wxwin_newproject::on_add(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
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
		CHECK_UI_THREAD;
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
		CHECK_UI_THREAD;
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
		CHECK_UI_THREAD;
		reorder_scripts(-1);
		on_luasel(e);
	}

	void wxwin_newproject::on_down(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		reorder_scripts(1);
		on_luasel(e);
	}

	void wxwin_newproject::on_luasel(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		int sel = luascripts->GetSelection();
		int count = luascripts->GetCount();
		removebutton->Enable(sel != wxNOT_FOUND);
		upbutton->Enable(sel != wxNOT_FOUND && sel > 0);
		downbutton->Enable(sel != wxNOT_FOUND && sel < count - 1);
	}

	void wxwin_newproject::on_directory_select(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		wxDirDialog* d = new wxDirDialog(this, wxT("Select project directory"), projdir->GetValue(),
			wxDD_DIR_MUST_EXIST);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		projdir->SetValue(d->GetPath());
		d->Destroy();
	}

	int get_setting_class(const core_setting& a)
	{
		if(a.is_boolean())
			return 0;
		if(a.is_freetext())
			return 2;
		return 1;
	}

	bool compare_settings(const std::pair<std::string, core_setting*>& a,
		const std::pair<std::string, core_setting*>& b)
	{
		int aclass = get_setting_class(*a.second);
		int bclass = get_setting_class(*b.second);
		if(aclass < bclass) return true;
		if(aclass > bclass) return false;
		if(a.second->hname < b.second->hname) return true;
		if(a.second->hname > b.second->hname) return false;
		return false;
	}

	std::vector<std::pair<std::string, core_setting*>> sort_settingblock(std::map<std::string,
		core_setting>& block)
	{
		std::vector<std::pair<std::string, core_setting*>> ret;
		for(auto& i : block)
			ret.push_back(std::make_pair(i.first, &i.second));
		std::sort(ret.begin(), ret.end(), compare_settings);
		return ret;
	}
}

class wxwin_project : public wxDialog
{
public:
	wxwin_project(emulator_instance& _inst);
	~wxwin_project();
	void on_file_select(wxCommandEvent& e);
	void on_new_select(wxCommandEvent& e);
	void on_filename_change(wxCommandEvent& e);
	void on_ask_filename(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
private:
	struct moviefile& make_movie();
	emulator_instance& inst;
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



void show_projectwindow(wxWindow* modwin, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	if(inst.rom->isnull()) {
		show_message_ok(modwin, "Can't start new movie", "No ROM loaded", wxICON_EXCLAMATION);
		return;
	}
	wxwin_project* projwin = new wxwin_project(inst);
	projwin->ShowModal();
	projwin->Destroy();
}

//------------------------------------------------------------

wxwin_project::wxwin_project(emulator_instance& _inst)
	: wxDialog(NULL, wxID_ANY, wxT("Project settings"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX), inst(_inst)
{
	CHECK_UI_THREAD;
	std::vector<wxString> cchoices;

	std::set<std::string> sram_set = inst.rom->srams();

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
	auto settingblock = inst.rom->get_settings().settings;
	wxFlexGridSizer* mainblock = new wxFlexGridSizer(4 + settingblock.size() + sram_set.size(), 2, 0, 0);
	auto _settingblock = sort_settingblock(settingblock);
	for(auto i : _settingblock) {
		settings.insert(std::make_pair(i.second->iname, setting_select(new_panel, *i.second)));
		mainblock->Add(settings.find(i.second->iname)->second.get_label());
		mainblock->Add(settings.find(i.second->iname)->second.get_control());
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
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
	try {
		raw_lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
		if(raw_lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue())) < 0)
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
	CHECK_UI_THREAD;
	try {
		moviefile& mov = make_movie();
		mov.start_paused = false;
		rrdata_set tmp_rdata;
		mov.save("$MEMORY:wxwidgets-romload-tmp", 0, true, tmp_rdata, false);
		inst.iqueue->queue("load-state $MEMORY:wxwidgets-romload-tmp");
		EndModal(0);
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading movie", e.what(), wxICON_EXCLAMATION);
		return;
	}
}

struct moviefile& wxwin_project::make_movie()
{
	CHECK_UI_THREAD;
	moviefile& f = *new moviefile;
	f.force_corrupt = false;
	f.gametype = &inst.rom->get_sysregion();
	for(auto i : settings) {
		f.settings[i.first] = i.second.read();
		if(!i.second.get_setting().validate(f.settings[i.first]))
			throw std::runtime_error((stringfmt() << "Bad value for setting " << i.first).str());
	}
	f.coreversion = inst.rom->get_core_identifier();
	f.gamename = tostdstring(projectname->GetValue());
	f.projectid = get_random_hexstring(40);
	set_mprefix_for_project(f.projectid, tostdstring(prefix->GetValue()));
	f.rerecords = "0";
	for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
		auto& img = inst.rom->get_rom(i);
		auto& xml = inst.rom->get_markup(i);
		f.romimg_sha256[i] = img.sha_256.read();
		f.romxml_sha256[i] = xml.sha_256.read();
		f.namehint[i] = img.namehint;
	}
	size_t lines = authors->GetNumberOfLines();
	for(size_t i = 0; i < lines; i++) {
		std::string l = tostdstring(authors->GetLineText(i));
		if(l != "" && l != "|")
			f.authors.push_back(split_author(l));
	}
	for(auto i : sram_files) {
		std::string sf = tostdstring(i.second->GetValue());
		std::vector<char>* target;
		if(i.first != "")
			target = &(f.movie_sram[i.first]);
		else
			target = &(f.anchor_savestate);

		if(sf != "") {
			if(moviefile::is_movie_or_savestate(sf)) {
				moviefile::sram_extractor e(sf);
				e.read(i.first, *target);
			} else
				*target = zip::readrel(sf, "");
		}
	}
	f.movie_rtc_second = f.dyn.rtc_second = raw_lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
	f.movie_rtc_subsecond = f.dyn.rtc_subsecond =
		raw_lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue()));
	if(f.movie_rtc_subsecond < 0)
		throw std::runtime_error("RTC subsecond must be positive");
	auto ctrldata = inst.rom->controllerconfig(f.settings);
	portctrl::type_set& ports = portctrl::type_set::make(ctrldata.ports, ctrldata.portindex());
	f.create_default_branch(ports);
	return f;
}

void open_new_project_window(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	if(inst.rom->isnull()) {
		show_message_ok(parent, "Can't start new project", "No ROM loaded", wxICON_EXCLAMATION);
		return;
	}
	wxwin_newproject* projwin = new wxwin_newproject(parent, inst);
	projwin->ShowModal();
	projwin->Destroy();
}
