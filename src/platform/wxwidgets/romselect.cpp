#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/statbox.h>
#include <wx/notebook.h>

#include "core/moviedata.hpp"
#include "core/framerate.hpp"
#include "core/settings.hpp"
#include "library/zip.hpp"
#include "interface/core.hpp"

#include "platform/wxwidgets/platform.hpp"

#define ROM_SELECTS_BASE	(wxID_HIGHEST + 0)
#define ROM_SELECTS_LAST	(wxID_HIGHEST + 127)
#define ASK_FILENAME_BUTTON	(wxID_HIGHEST + 128)
#define ASK_SRAMS_BASE		(wxID_HIGHEST + 129)
#define ASK_SRAMS_LAST		(wxID_HIGHEST + 255)

#define CONTROLLERTYPES_P1 4
#define CONTROLLERTYPES 7
#define CNAME_NONE "None"
#define CNAME_GAMEPAD "Gamepad"
#define CNAME_MULTITAP "Multitap"
#define CNAME_MOUSE "Mouse"
#define CNAME_SUPERSCOPE "Superscope"
#define CNAME_JUSTIFIER "Justifier"
#define CNAME_JUSTIFIERS "2 Justifiers"
#define MARKUP_POSTFIX " Markup"


void patching_done(struct loaded_rom& rom, wxWindow* modwin);

#define ROMSELECT_ROM_COUNT 27

namespace
{
	porttype_t get_controller_type(const std::string& s)
	{
		if(s == CNAME_NONE)
			return PT_NONE;
		if(s == CNAME_GAMEPAD)
			return PT_GAMEPAD;
		if(s == CNAME_MULTITAP)
			return PT_MULTITAP;
		if(s == CNAME_MOUSE)
			return PT_MOUSE;
		if(s == CNAME_SUPERSCOPE)
			return PT_SUPERSCOPE;
		if(s == CNAME_JUSTIFIER)
			return PT_JUSTIFIER;
		if(s == CNAME_JUSTIFIERS)
			return PT_JUSTIFIERS;
		return PT_INVALID;
	}

	struct loaded_slot& get_rom_slot(struct loaded_rom& rom, unsigned index)
	{
		if(index % 2)
			return rom.markup_slots[index / 2];
		else
			return rom.main_slots[index / 2];
	}

	struct region_info_structure* region_from_string(struct systype_info_structure* s, const std::string& str)
	{
		for(size_t i = 0; i < s->region_slots(); i++) {
			auto r = s->region_slot(i);
			if(r->get_hname() == str)
				return r;
		}
		return s->region_slot(0);
	}

	unsigned populate_region_choices(struct systype_info_structure* s, wxString* array)
	{
		if(!s) {
			array[0] = towxstring("NULL");
			return 1;
		}
		size_t x = s->region_slots();
		for(size_t i = 0; i < x; i++)
			array[i] = towxstring(s->region_slot(i)->get_hname());
		return x;
	}

	unsigned populate_system_choices(wxString* array)
	{
		size_t x = emucore_systype_slots();
		for(size_t i = 0; i < x; i++)
			array[i] = towxstring(emucore_systype_slot(i)->get_hname());
		return x;
	}

	bool check_present_roms(struct systype_info_structure* s, unsigned flags)
	{
		if(!s)
			return false;
		size_t x = s->rom_slots();
		unsigned p = 0;
		unsigned a = 0;
		for(size_t i = 0; i < x; i++) {
			unsigned lflags = s->rom_slot(i)->mandatory_flags();
			a |= lflags;
			if((flags >> i) & 1)
				p |= lflags;
		}
		return (p == a);
	}

	std::string romname(struct systype_info_structure* s, unsigned index)
	{
		if(index >= s->rom_slots())
			return "";
		return s->rom_slot(index)->get_hname();
	}

	unsigned romname_to_index(struct systype_info_structure* s, const wxString& _name)
	{
		std::string name = tostdstring(_name);
		for(unsigned i = 0; i < s->rom_slots(); i++) {
			auto base = s->rom_slot(i)->get_hname();
			if(base == name)
				return 2 * i;
			if(base + MARKUP_POSTFIX == name)
				return 2 * i + 1;
		}
		return 2 * s->rom_slots();
	}

	unsigned fill_rom_names(struct systype_info_structure* s, std::string* array)
	{
		unsigned r = 0;
		for(unsigned i = 0; i < s->rom_slots(); i++) {
			std::string t = romname(s, i);
			if(t.length())
				array[r++] = t;
		}
		return r;
	}

	struct systype_info_structure* romtype_from_string(const std::string& str)
	{
		for(size_t i = 0; i < emucore_systype_slots(); i++) {
			auto j = emucore_systype_slot(i);
			if(j->get_hname() == str)
				return j;
		}
		return NULL;
	}

	bool has_forced_region(struct systype_info_structure* s)
	{
		return (s->region_slots() <= 1);
	}

	std::string game_id(struct loaded_rom& r)
	{
		for(size_t i = 1; i < r.main_slots.size(); i++)
			if(r.main_slots[i].valid)
				return r.main_slots[i].sha256;
		return r.main_slots[0].sha256;
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

	path_setting rompath_setting("rompath");

	std::string rom_path()
	{
		//This is pre-boot, so read directly.
		return rompath_setting;
	}

	struct rom_slot_callback
	{
		virtual void on_file_change() = 0;
	};

	class rom_slot : public wxEvtHandler
	{
	public:
		rom_slot(wxWindow* inWindow, rom_slot_callback* _cb);
		void show(wxSizer* sizer);
		void hide(wxSizer* sizer);
		void change(const std::string& name, bool has_markup);
		std::string get_filename();
		std::string get_markup();
		void on_change(wxCommandEvent& e);
		void on_filename_b(wxCommandEvent& e);
		void on_markup_b(wxCommandEvent& e);
	private:
		wxPanel* panel;
		wxBoxSizer* top;
		wxStaticBox* box;
		wxButton* filename_pick;
		wxButton* markup_pick;
		wxTextCtrl* filename;
		wxTextCtrl* markup;
		bool markup_flag;
		bool enabled;
		rom_slot_callback* cb;
		wxStaticBoxSizer* intsizer;
	};

	rom_slot::rom_slot(wxWindow* in_window, rom_slot_callback* _cb)
	{
		wxSizer* tmp;
		cb = _cb;
		panel = new wxPanel(in_window);
		top = new wxBoxSizer(wxVERTICAL);
		panel->SetSizer(top);
		box = new wxStaticBox(panel, wxID_ANY, wxT(""));
		intsizer = new wxStaticBoxSizer(box, wxVERTICAL);
		intsizer->Add(new wxStaticText(panel, wxID_ANY, wxT("File")));
		tmp = new wxBoxSizer(wxHORIZONTAL);
		tmp->Add(filename = new wxTextCtrl(panel, wxID_ANY, wxT(""), wxDefaultPosition,
			wxSize(400,-1)), 1, wxGROW);
		filename->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(rom_slot::on_change), NULL, this);
		filename->SetDropTarget(new textboxloadfilename(filename));
		tmp->Add(filename_pick = new wxButton(panel, wxID_ANY, wxT("Pick...")));
		filename_pick->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(rom_slot::on_filename_b),
			NULL, this);;
		intsizer->Add(tmp);
		intsizer->Add(new wxStaticText(panel, wxID_ANY, wxT("Default mappings override file (optional) ")));
		tmp = new wxBoxSizer(wxHORIZONTAL);
		tmp->Add(markup = new wxTextCtrl(panel, wxID_ANY, wxT(""), wxDefaultPosition,
			wxSize(400,-1)), 1, wxGROW);
		markup->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(rom_slot::on_change), NULL, this);
		markup->SetDropTarget(new textboxloadfilename(markup));
		tmp->Add(markup_pick = new wxButton(panel, wxID_ANY, wxT("Pick...")));
		markup_pick->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(rom_slot::on_markup_b),
			NULL, this);
		intsizer->Add(tmp);
		markup->Enable(markup_flag = false);
		top->Add(intsizer);
		hide(NULL);
		enabled = false;
	}

	void rom_slot::on_change(wxCommandEvent& e)
	{
		if(cb)
			cb->on_file_change();
	}

	void rom_slot::show(wxSizer* sizer)
	{
		panel->Show();
		sizer->Add(panel);
		enabled = true;
		intsizer->Layout();
		top->Layout();
		sizer->Layout();
	}

	void rom_slot::hide(wxSizer* sizer)
	{
		if(sizer)
			sizer->Detach(panel);
		enabled = false;
		panel->Hide();
		if(sizer)
			sizer->Layout();
	}

	void rom_slot::change(const std::string& name, bool has_markup)
	{
		box->SetLabel(towxstring(name));
		markup_flag = has_markup;
		markup->Enable(has_markup);
		markup_pick->Enable(has_markup);
	}
	
	std::string rom_slot::get_filename()
	{
		if(!enabled)
			return "";
		return tostdstring(filename->GetValue());
	}

	std::string rom_slot::get_markup()
	{
		if(!enabled || !markup_flag)
			return "";
		return tostdstring(markup->GetValue());
	}

	void rom_slot::on_filename_b(wxCommandEvent& e)
	{
		try {
			std::string fname = pick_file_member(panel, "Choose ROM file", rom_path());
			filename->SetValue(towxstring(fname));
			on_change(e);
		} catch(canceled_exception& e) {
		}
	}

	void rom_slot::on_markup_b(wxCommandEvent& e)
	{
		try {
			std::string fname = pick_file_member(panel, "Choose markup file", rom_path());
			markup->SetValue(towxstring(fname));
			on_change(e);
		} catch(canceled_exception& e) {
		}
	}
}

class wxwin_romselect : public wxFrame, rom_slot_callback
{
public:
	wxwin_romselect();
	~wxwin_romselect();
	void on_filename_change(wxCommandEvent& e);
	void on_romtype_change(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_open_rom(wxCommandEvent& e);
	void on_apply_rom(wxCommandEvent& e);
	void on_openapply_rom(wxCommandEvent& e, bool apply);
	void on_file_change();
	loaded_rom* our_rom;
private:
	wxComboBox* romtype_combo;
	wxComboBox* region_combo;
	wxBoxSizer* romgrid;
	rom_slot* slots[ROMSELECT_ROM_COUNT];
	wxButton* apply_rom;
	wxButton* open_rom;
	wxButton* quit_button;
	std::string current_rtype;
	std::string remembered_region;
	void set_rtype(std::string rtype);
};

void open_rom_select_window()
{
	wxwin_romselect* romwin = new wxwin_romselect();
	romwin->Show();
}

class wxwin_patch : public wxFrame
{
public:
	wxwin_patch(loaded_rom& rom);
	~wxwin_patch();
	void on_patchfile_change(wxCommandEvent& e);
	void on_ask_patchfile(wxCommandEvent& e);
	void on_do_patch(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_done(wxCommandEvent& e);
	loaded_rom* our_rom;
private:
	wxComboBox* patch_what;
	wxStaticText* checksums[ROMSELECT_ROM_COUNT];
	wxTextCtrl* patchfile;
	wxButton* choosefile;
	wxButton* dopatch;
	wxTextCtrl* patch_offset;
};

class wxwin_project : public wxFrame
{
public:
	wxwin_project(loaded_rom& rom);
	~wxwin_project();
	void on_file_select(wxCommandEvent& e);
	void on_new_select(wxCommandEvent& e);
	void on_filename_change(wxCommandEvent& e);
	void on_ask_filename(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
	void on_tab_select(wxNotebookEvent& e);
	loaded_rom* our_rom;
private:
	bool load_file;
	std::set<std::string> get_sram_set();
	struct moviefile make_movie();
	wxTextCtrl* savefile;
	wxButton* ask_savefile;
	wxNotebook* notebook;
	std::map<std::string, wxTextCtrl*> sram_files;
	std::map<std::string, wxButton*> sram_choosers;
	wxComboBox* controller1type;
	wxComboBox* controller2type;
	wxCheckBox* start_pause;
	wxTextCtrl* projectname;
	wxTextCtrl* prefix;
	wxTextCtrl* rtc_sec;
	wxTextCtrl* rtc_subsec;
	wxTextCtrl* authors;
	wxButton* load;
	wxButton* quit;
	std::map<unsigned, std::string> sram_names;
};


wxwin_romselect::wxwin_romselect()
	: wxFrame(NULL, wxID_ANY, wxT("Select ROM"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	wxString rtchoices[128];
	wxString rrchoices[128];
	size_t systems = populate_system_choices(rtchoices);
	size_t regions = populate_region_choices(NULL, rrchoices);

	Centre();

	//The toplevel sizer contains three sizers:
	//- The top bar having ROM type / Region selects.
	//- The middle area having ROM filename boxes.
	//- The bottom bar having OK/Quit buttons.
	wxFlexGridSizer* toplevel = new wxFlexGridSizer(3, 1, 0, 0);
	SetSizer(toplevel);

	//The ROM type / Region selects.
	wxBoxSizer* selects = new wxBoxSizer(wxHORIZONTAL);
	selects->Add(new wxStaticText(this, wxID_ANY, wxT("ROM type:")), 0, wxGROW);
	selects->Add(romtype_combo = new wxComboBox(this, wxID_ANY, rtchoices[0], wxDefaultPosition, wxDefaultSize,
		systems, rtchoices, wxCB_READONLY), 0, wxGROW);
	selects->Add(new wxStaticText(this, wxID_ANY, wxT("Region:")), 0, wxGROW);
	selects->Add(region_combo = new wxComboBox(this, wxID_ANY, rrchoices[0], wxDefaultPosition, wxDefaultSize,
		regions, rrchoices, wxCB_READONLY), 0, wxGROW);
	romtype_combo->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxwin_romselect::on_romtype_change), NULL, this);
	region_combo->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxwin_romselect::on_romtype_change), NULL, this);
	toplevel->Add(selects, 0, wxGROW);

	//ROM filename selects
	romgrid = new wxBoxSizer(wxVERTICAL);
	for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++)
		slots[i] = new rom_slot(this, this);
	toplevel->Add(romgrid, 1, wxGROW);

	//Button bar.
	wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->Add(apply_rom = new wxButton(this, wxID_ANY, wxT("Apply patches")), 0, wxALIGN_RIGHT);
	buttons->Add(open_rom = new wxButton(this, wxID_OPEN, wxT("Open ROM")), 0, wxALIGN_RIGHT);
	buttons->AddStretchSpacer();
	buttons->Add(quit_button = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxALIGN_RIGHT);
	apply_rom->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_romselect::on_apply_rom), NULL, this);
	open_rom->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_romselect::on_open_rom), NULL, this);
	open_rom->Disable();
	apply_rom->Disable();
	quit_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_romselect::on_quit), NULL, this);
	toplevel->Add(buttons, 1, wxGROW);

	//Initialize form.
	set_rtype("");

	//Display it.
	toplevel->SetSizeHints(this);
	Fit();
}

wxwin_romselect::~wxwin_romselect()
{
	for(size_t i = 0; i < ROMSELECT_ROM_COUNT; i++)
		delete slots[i];
}

void wxwin_romselect::set_rtype(std::string rtype)
{
	bool no_rtype = (current_rtype == "");
	if(rtype == "")
		rtype = tostdstring(romtype_combo->GetValue());
	if(rtype == current_rtype)
		return;
	auto _rtype = romtype_from_string(rtype);
	if(has_forced_region(_rtype)) {
		region_combo->Disable();
		remembered_region = tostdstring(region_combo->GetValue());
		region_combo->Clear();
		region_combo->Append(towxstring(_rtype->region_slot(0)->get_hname()));
		region_combo->SetSelection(0);
	} else {
		if(remembered_region == "")
			remembered_region = tostdstring(region_combo->GetValue());
		region_combo->Clear();
		bool old_ok = false;
		for(size_t i = 0; i < _rtype->region_slots(); i++) {
			std::string x = _rtype->region_slot(i)->get_hname();
			region_combo->Append(towxstring(x));
			if(x == remembered_region)
				old_ok = true;
		}
		region_combo->SetSelection(0);
		if(old_ok)
			region_combo->SetValue(towxstring(remembered_region));
		remembered_region = "";
		region_combo->Enable();
	}
	std::string tmp[ROMSELECT_ROM_COUNT];
	unsigned c = fill_rom_names(_rtype, tmp);
	for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++)
		slots[i]->hide(romgrid);
	for(unsigned i = 0; i < c; i++) {
		slots[i]->change(tmp[i], true);
		slots[i]->show(romgrid);
	}
	romgrid->Layout();
	current_rtype = rtype;
	Fit();
}

void wxwin_romselect::on_file_change()
{
	bool ok = true;
	auto rtype = romtype_from_string(tostdstring(romtype_combo->GetValue()));
	unsigned flags = 0;
	for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++)
		flags |= ((slots[i]->get_filename() != "") ? (1 << i) : 0);
	open_rom->Enable(check_present_roms(rtype, flags));
	apply_rom->Enable(check_present_roms(rtype, flags));
}

void wxwin_romselect::on_romtype_change(wxCommandEvent& e)
{
	set_rtype(tostdstring(romtype_combo->GetValue()));
	on_file_change();
}

void wxwin_romselect::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void wxwin_romselect::on_open_rom(wxCommandEvent& e)
{
	on_openapply_rom(e, false);
}

void wxwin_romselect::on_apply_rom(wxCommandEvent& e)
{
	on_openapply_rom(e, true);
}


void wxwin_romselect::on_openapply_rom(wxCommandEvent& e, bool apply)
{
	rom_files rfiles;
	rfiles.base_file = "";
	rfiles.rtype = romtype_from_string(tostdstring(romtype_combo->GetValue()));
	rfiles.region = region_from_string(rfiles.rtype, tostdstring(region_combo->GetValue()));
	rfiles.main_slots.resize(rfiles.rtype->rom_slots());
	rfiles.markup_slots.resize(rfiles.rtype->rom_slots());
	for(size_t i = 0; i < rfiles.rtype->rom_slots() && i < ROMSELECT_ROM_COUNT; i++) {
		rfiles.main_slots[i] = slots[i]->get_filename();
		rfiles.markup_slots[i] = slots[i]->get_markup();
	}
	try {
		our_rom = new loaded_rom(rfiles);
		our_rom_name = game_id(*our_rom);
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading ROM", e.what(), wxICON_EXCLAMATION);
		return;
	}
	if(apply) {
		wxwin_patch* projwin = new wxwin_patch(*our_rom);
		projwin->Show();
	} else {
		patching_done(*our_rom, this);
	}
	Destroy();
}
//---------------------------------------------------
wxwin_patch::wxwin_patch(loaded_rom& rom)
	: wxFrame(NULL, wxID_ANY, wxT("Patch ROM"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	our_rom = &rom;
	wxString targets[2 * ROMSELECT_ROM_COUNT];
	std::string _targets[ROMSELECT_ROM_COUNT];
	size_t _target_count = fill_rom_names(rom.rtype, _targets);
	size_t target_count = 0;
	for(auto i = 0; i < _target_count; i++) {
		targets[2 * i] = towxstring(_targets[i]);
		targets[2 * i + 1] = towxstring(_targets[i] + MARKUP_POSTFIX);
		target_count += 2;
	}

	//Toplevel has 5 blocks:
	//- Checksums for ROMs.
	//- Patch what select.
	//- Filename select.
	//- Patch offset.
	//- Button bar.
	Centre();
	wxFlexGridSizer* toplevel = new wxFlexGridSizer(5, 1, 0, 0);
	SetSizer(toplevel);

	//Checksums block.
	wxFlexGridSizer* hashes = new wxFlexGridSizer(target_count, 2, 0, 0);
	for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++)
		checksums[i] = NULL;
	for(unsigned i = 0; i < target_count; i++) {
		std::string hash = get_rom_slot(*our_rom, i).sha256;
		if(hash == "")
			hash = "<Not present>";
		hashes->Add(new wxStaticText(this, wxID_ANY, targets[i]), 0, wxGROW);
		hashes->Add(checksums[i] = new wxStaticText(this, wxID_ANY, towxstring(hash)), 0, wxGROW);
	}
	toplevel->Add(hashes, 0, wxGROW);

	//Target select.
	wxFlexGridSizer* targetselect = new wxFlexGridSizer(1, 2, 0, 0);
	targetselect->Add(new wxStaticText(this, wxID_ANY, wxT("Patch what:")), 0, wxGROW);
	targetselect->Add(patch_what = new wxComboBox(this, wxID_ANY, targets[0], wxDefaultPosition, wxDefaultSize,
		target_count, targets, wxCB_READONLY), 0, wxGROW);
	toplevel->Add(targetselect, 0, wxGROW);

	//Patchfile.
	wxFlexGridSizer* patchsel = new wxFlexGridSizer(1, 3, 0, 0);
	patchsel->Add(new wxStaticText(this, wxID_ANY, wxT("File:")), 0, wxGROW);
	patchsel->Add(patchfile = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, -1)),
		1, wxGROW);
	patchsel->Add(choosefile = new wxButton(this, wxID_ANY, wxT("Pick")), 0, wxGROW);
	patchfile->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxwin_patch::on_patchfile_change), NULL, this);
	choosefile->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_patch::on_ask_patchfile), NULL, this);
	patchfile->SetDropTarget(new textboxloadfilename(patchfile));
	toplevel->Add(patchsel, 0, wxGROW);

	//Patch offset.
	wxFlexGridSizer* offsetselect = new wxFlexGridSizer(1, 2, 0, 0);
	offsetselect->Add(new wxStaticText(this, wxID_ANY, wxT("Patch offset:")), 0, wxGROW);
	offsetselect->Add(patch_offset = new wxTextCtrl(this, wxID_ANY, wxT("")), 0, wxGROW);
	patch_offset->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxwin_patch::on_patchfile_change), NULL, this);
	toplevel->Add(offsetselect, 0, wxGROW);

	//Button bar
	wxBoxSizer* buttonbar = new wxBoxSizer(wxHORIZONTAL);
	buttonbar->AddStretchSpacer();
	wxButton* thats_enough = new wxButton(this, wxID_ANY, wxT("Enough"));
	buttonbar->Add(thats_enough, 0, wxGROW);
	thats_enough->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_patch::on_done), NULL, this);
	dopatch = new wxButton(this, wxID_ANY, wxT("Patch"));
	buttonbar->Add(dopatch, 0, wxGROW);
	dopatch->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_patch::on_do_patch), NULL, this);
	dopatch->Disable();
	wxButton* quitbutton = new wxButton(this, wxID_EXIT, wxT("Quit"));
	buttonbar->Add(quitbutton, 0, wxGROW);
	quitbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_patch::on_quit), NULL, this);
	toplevel->Add(buttonbar, 0, wxGROW);

	patch_offset->SetValue(wxT("0"));

	hashes->SetSizeHints(this);
	targetselect->SetSizeHints(this);
	buttonbar->SetSizeHints(this);
	toplevel->SetSizeHints(this);
	Fit();
}

void wxwin_patch::on_ask_patchfile(wxCommandEvent& e)
{
	try {
		std::string fname = pick_file_member(this, "Choose patch file", rom_path());
		patchfile->SetValue(towxstring(fname));
		on_patchfile_change(e);
	} catch(canceled_exception& e) {
	}
}

wxwin_patch::~wxwin_patch()
{
}

void wxwin_patch::on_patchfile_change(wxCommandEvent& e)
{
	bool ok = true;
	ok = ok && (patchfile->GetValue().Length() != 0);
	std::string offsetv = tostdstring(patch_offset->GetValue());
	try {
		int32_t offset = boost::lexical_cast<int32_t>(offsetv);
	} catch(...) {
		ok = false;
	}
	if(dopatch)
		dopatch->Enable(ok);
}

void wxwin_patch::on_do_patch(wxCommandEvent& e)
{
	try {
		auto patch_contents = read_file_relative(tostdstring(patchfile->GetValue()), "");
		size_t patch_index = romname_to_index(our_rom->rtype, patch_what->GetValue());
		if(patch_index > 2 * ROMSELECT_ROM_COUNT)
			throw std::runtime_error("Internal error: Patch WHAT?");
		loaded_slot& s = get_rom_slot(*our_rom, patch_index);
		std::string offsetv = tostdstring(patch_offset->GetValue());
		int32_t offset = boost::lexical_cast<int32_t>(offsetv);
		s.patch(patch_contents, offset);
		checksums[patch_index]->SetLabel(towxstring(s.sha256));
	} catch(std::exception& e) {
		show_message_ok(this, "Error patching ROM", e.what(), wxICON_EXCLAMATION);
		return;
	}
	patchfile->SetValue(wxT(""));
}

void wxwin_patch::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void patching_done(struct loaded_rom& rom, wxWindow* modwin)
{
	struct loaded_rom* our_rom = &rom;
	try {
		emucore_basic_init();
		our_rom_name = game_id(*our_rom);
		our_rom->load();
	} catch(std::exception& e) {
		show_message_ok(modwin, "Error loading ROM", e.what(), wxICON_EXCLAMATION);
		return;
	}
	messages << "Detected region: " << our_rom->rtype->get_sysregion(our_rom->region->get_iname())->get_iname()
		<< std::endl;
	auto vrate = emucore_get_video_rate();
	set_nominal_framerate(1.0 * vrate.first / vrate.second);

	messages << "--- Internal memory mappings ---" << std::endl;
	dump_region_map();
	messages << "--- End of Startup --- " << std::endl;
	wxwin_project* projwin = new wxwin_project(*our_rom);
	projwin->Show();
}

void wxwin_patch::on_done(wxCommandEvent& e)
{
	patching_done(*our_rom, this);
	Destroy();
}
//------------------------------------------------------------

wxwin_project::wxwin_project(loaded_rom& rom)
	: wxFrame(NULL, wxID_ANY, wxT("Project settings"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	our_rom = &rom;
	wxString cchoices[CONTROLLERTYPES];
	cchoices[0] = wxT(CNAME_NONE);
	cchoices[1] = wxT(CNAME_GAMEPAD);
	cchoices[2] = wxT(CNAME_MULTITAP);
	cchoices[3] = wxT(CNAME_MOUSE);
	cchoices[4] = wxT(CNAME_SUPERSCOPE);
	cchoices[5] = wxT(CNAME_JUSTIFIER);
	cchoices[6] = wxT(CNAME_JUSTIFIERS);

	std::set<std::string> sram_set = get_sram_set();

	Centre();
	//2 Top-level block.
	//- Notebook
	//- Button bar.
	wxBoxSizer* toplevel = new wxBoxSizer(wxVERTICAL);
	SetSizer(toplevel);
	notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
	wxPanel* load_panel = new wxPanel(notebook);
	wxPanel* new_panel = new wxPanel(notebook);

	//The load page.
	wxSizer* load_sizer = new wxFlexGridSizer(1, 3, 0, 0);
	wxSizer* load_sizer2 = new wxBoxSizer(wxVERTICAL);
	load_panel->SetSizer(load_sizer2);
	load_sizer2->Add(load_sizer, 0, wxGROW);
	load_sizer->Add(new wxStaticText(load_panel, wxID_ANY, wxT("File to load:")), 0, wxGROW);
	load_sizer->Add(savefile = new wxTextCtrl(load_panel, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, -1)),
		1, wxGROW);
	savefile->SetDropTarget(new textboxloadfilename(savefile));

	load_sizer->Add(ask_savefile = new wxButton(load_panel, ASK_FILENAME_BUTTON, wxT("Pick")), 0, wxGROW);
	load_sizer2->Add(start_pause = new wxCheckBox(load_panel, wxID_ANY, wxT("Start paused")), 0, wxGROW);
	savefile->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxwin_project::on_filename_change), NULL, this);
	ask_savefile->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_project::on_ask_filename), NULL, this);
	notebook->AddPage(load_panel, wxT("Load save/movie"));

	//The new page.
	//3 Page-level blocks.
	//- Controllertypes/initRTC/Gamename/SRAMs.
	//- Authors explanation.
	//- Authors
	wxFlexGridSizer* new_sizer = new wxFlexGridSizer(3, 1, 0, 0);
	new_panel->SetSizer(new_sizer);
	//Controllertypes/Gamename/initRTC/SRAMs.
	wxFlexGridSizer* mainblock = new wxFlexGridSizer(5 + sram_set.size(), 2, 0, 0);
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Controller 1 Type:")), 0, wxGROW);
	mainblock->Add(controller1type = new wxComboBox(new_panel, wxID_ANY, cchoices[1], wxDefaultPosition,
		wxDefaultSize, CONTROLLERTYPES_P1, cchoices, wxCB_READONLY), 0, wxGROW);
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Controller 2 Type:")), 0, wxGROW);
	mainblock->Add(controller2type = new wxComboBox(new_panel, wxID_ANY, cchoices[0], wxDefaultPosition,
		wxDefaultSize, CONTROLLERTYPES, cchoices, wxCB_READONLY), 0, wxGROW);
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
	for(auto i : sram_set) {
		mainblock->Add(new wxStaticText(new_panel, wxID_ANY, towxstring("SRAM " + i)), 0, wxGROW);
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

	notebook->Connect(wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler(wxwin_project::on_tab_select),
		NULL, this);
	notebook->AddPage(new_panel, wxT("New movie"));
	toplevel->Add(notebook, 1, wxGROW);

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

	bool file_filled = false;
	{
		std::ifstream s(get_config_path() + "/" + our_rom_name + ".ls");
		std::getline(s, last_save);
		savefile->SetValue(towxstring(last_save));
		if(last_save != "")
			file_filled = true;
	}

	//This gets re-enabled later if needed.
	load_file = true;
	load->Disable();
	notebook->SetSelection(file_filled ? 0 : 1);
	wxCommandEvent e2;
	on_filename_change(e2);

	mainblock->SetSizeHints(this);
	new_sizer->SetSizeHints(this);
	load_sizer->SetSizeHints(this);
	toplevel->SetSizeHints(this);
	Fit();
}

wxwin_project::~wxwin_project()
{
}

void wxwin_project::on_tab_select(wxNotebookEvent& e)
{
	int p = e.GetSelection();
	if(p < 0) {
		load->Disable();
		return;
	} else if(p == 0) {
		load_file = true;
		load->SetLabel(wxT("Load"));
	} else if(p == 1) {
		load_file = false;
		load->SetLabel(wxT("Start"));
	}
	wxCommandEvent e2;
	on_filename_change(e2);
	e.Skip();
}

void wxwin_project::on_ask_filename(wxCommandEvent& e)
{
	int id = e.GetId();
	try {
		if(id == ASK_FILENAME_BUTTON) {
			std::string fname = pick_file(this, "Choose save/movie", ".");
			savefile->SetValue(towxstring(fname));
		} else if(id >= ASK_SRAMS_BASE && id <= ASK_SRAMS_LAST) {
			std::string fname = pick_file_member(this, "Choose " + sram_names[id - ASK_SRAMS_BASE], ".");
			sram_files[sram_names[id - ASK_SRAMS_BASE]]->SetValue(towxstring(fname));
		}
		on_filename_change(e);
	} catch(canceled_exception& e) {
	}
}

void wxwin_project::on_filename_change(wxCommandEvent& e)
{
	if(load_file) {
		load->Enable(savefile->GetValue().Length() != 0);
	} else {
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
}

void wxwin_project::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void wxwin_project::on_load(wxCommandEvent& e)
{
	try {
		if(load_file) {
			moviefile& mov = *new moviefile(tostdstring(savefile->GetValue()));
			mov.start_paused = start_pause->GetValue();
			boot_emulator(*our_rom, mov);
		} else {
			moviefile& mov = *new moviefile(make_movie());
			mov.start_paused = start_pause->GetValue();
			boot_emulator(*our_rom, mov);
		}
		Destroy();
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading movie", e.what(), wxICON_EXCLAMATION);
		return;
	}
}

std::set<std::string> wxwin_project::get_sram_set()
{
	std::set<std::string> r;
	for(size_t i = 0; i < emucore_sram_slots(); i++) {
		sram_slot_structure* s = emucore_sram_slot(i);
		r.insert(s->get_name());
	}
	return r;
}

struct moviefile wxwin_project::make_movie()
{
	moviefile f;
	f.force_corrupt = false;
	f.port1 = get_controller_type(tostdstring(controller1type->GetValue()));
	f.port2 = get_controller_type(tostdstring(controller2type->GetValue()));
	f.coreversion = emucore_get_version();
	f.gamename = tostdstring(projectname->GetValue());
	f.prefix = sanitize_prefix(tostdstring(prefix->GetValue()));
	f.projectid = get_random_hexstring(40);
	f.rerecords = "0";
	copy_romdata_to_movie(f, *our_rom);
	size_t lines = authors->GetNumberOfLines();
	for(size_t i = 0; i < lines; i++) {
		std::string l = tostdstring(authors->GetLineText(i));
		if(l != "" && l != "|")
			f.authors.push_back(split_author(l));
	}
	for(auto i : sram_files) {
		std::string sf = tostdstring(i.second->GetValue());
		if(sf != "")
			f.movie_sram[i.first] = read_file_relative(sf, "");
	}
	f.is_savestate = false;
	f.movie_rtc_second = f.rtc_second = boost::lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
	f.movie_rtc_subsecond = f.rtc_subsecond = boost::lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue()));
	if(f.movie_rtc_subsecond < 0)
		throw std::runtime_error("RTC subsecond must be positive");
	f.input.clear(f.port1, f.port2);
	return f;
}

