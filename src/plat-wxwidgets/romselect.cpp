//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>

#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/moviedata.hpp"
#include "core/framerate.hpp"
#include "core/zip.hpp"

#include "plat-wxwidgets/platform.hpp"
#include "plat-wxwidgets/window_romselect.hpp"

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
#define TNAME_SNES "SNES"
#define TNAME_BSX_NS "BS-X (non-slotted)"
#define TNAME_BSX_S "BS-X (slotted)"
#define TNAME_SUFAMITURBO "Sufami Turbo"
#define TNAME_SGB "SGB"
#define RNAME_AUTO "Autodetect"
#define RNAME_NTSC "NTSC"
#define RNAME_PAL "PAL"
#define WNAME_SNES_MAIN "ROM"
#define WNAME_SNES_MAIN_XML "ROM XML"
#define WNAME_BS_MAIN "BS-X BIOS"
#define WNAME_BS_MAIN_XML "BS-X BIOS XML"
#define WNAME_BS_SLOTA "BS FLASH"
#define WNAME_BS_SLOTA_XML "BS FLASH XML"
#define WNAME_ST_MAIN "ST BIOS"
#define WNAME_ST_MAIN_XML "ST BIOS XML"
#define WNAME_ST_SLOTA "SLOT A ROM"
#define WNAME_ST_SLOTA_XML "SLOT A XML"
#define WNAME_ST_SLOTB "SLOT B ROM"
#define WNAME_ST_SLOTB_XML "SLOT B XML"
#define WNAME_SGB_MAIN "SGB BIOS"
#define WNAME_SGB_MAIN_XML "SGB BIOS XML"
#define WNAME_SGB_SLOTA "DMG ROM"
#define WNAME_SGB_SLOTA_XML "BMG XML"


namespace
{
	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	} simple_interface;

	void enable_slot(wxStaticText* label, wxTextCtrl* filename, wxButton* ask, wxCheckBox* hcb,
		const std::string& newlabel)
	{
		label->SetLabel(towxstring(newlabel));
		filename->Enable();
		ask->Enable();
		if(hcb)
			hcb->Enable();
	}

	void disable_slot(wxStaticText* label, wxTextCtrl* filename, wxButton* ask, wxCheckBox* hcb)
	{
		label->SetLabel(wxT(""));
		filename->Disable();
		ask->Disable();
		hcb->Disable();
	}

	std::string sram_name(const nall::string& _id, SNES::Cartridge::Slot slotname)
	{
		std::string id(_id, _id.length());
		if(slotname == SNES::Cartridge::Slot::SufamiTurboA)
			return "slota." + id.substr(1);
		if(slotname == SNES::Cartridge::Slot::SufamiTurboB)
			return "slotb." + id.substr(1);
		return id.substr(1);
	}

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
		switch(index) {
		case 0:		return rom.rom;
		case 1:		return rom.rom_xml;
		case 2:		return rom.slota;
		case 3:		return rom.slota_xml;
		case 4:		return rom.slotb;
		case 5:		return rom.slotb_xml;
		}
		return rom.rom;
	}

	enum rom_region region_from_string(const std::string& str)
	{
		if(str == RNAME_NTSC)
			return REGION_NTSC;
		if(str == RNAME_PAL)
			return REGION_PAL;
		return REGION_AUTO;
	}

	unsigned populate_region_choices(wxString* array)
	{
		array[0] = wxT(RNAME_AUTO);
		array[1] = wxT(RNAME_NTSC);
		array[2] = wxT(RNAME_PAL);
		return 3;
	}

	unsigned populate_system_choices(wxString* array)
	{
		array[0] = wxT(TNAME_SNES);
		array[1] = wxT(TNAME_BSX_NS);
		array[2] = wxT(TNAME_BSX_S);
		array[3] = wxT(TNAME_SUFAMITURBO);
		array[4] = wxT(TNAME_SGB);
		return 5;
	}

	bool check_present_roms(enum rom_type rtype, unsigned flags)
	{
		switch(rtype)
		{
		case ROMTYPE_SNES:
			return ((flags & 1) == 1);
		case ROMTYPE_BSX:
		case ROMTYPE_BSXSLOTTED:
		case ROMTYPE_SGB:
			return ((flags & 5) == 5);
		case ROMTYPE_SUFAMITURBO:
			return ((flags & 1) == 1) && ((flags & 20) != 0);
		default:
			return false;
		};
	}

	wxString romname(enum rom_type rtype, unsigned index)
	{
		switch(rtype) {
		case ROMTYPE_SNES:
			switch(index) {
			case 0:		return wxT(WNAME_SNES_MAIN);
			case 1:		return wxT(WNAME_SNES_MAIN_XML);
			};
			break;
		case ROMTYPE_BSX:
		case ROMTYPE_BSXSLOTTED:
			switch(index) {
			case 0:		return wxT(WNAME_BS_MAIN);
			case 1:		return wxT(WNAME_BS_MAIN_XML);
			case 2:		return wxT(WNAME_BS_SLOTA);
			case 3:		return wxT(WNAME_BS_SLOTA_XML);
			};
			break;
		case ROMTYPE_SUFAMITURBO:
			switch(index) {
			case 0:		return wxT(WNAME_ST_MAIN);
			case 1:		return wxT(WNAME_ST_MAIN_XML);
			case 2:		return wxT(WNAME_ST_SLOTA);
			case 3:		return wxT(WNAME_ST_SLOTA_XML);
			case 4:		return wxT(WNAME_ST_SLOTB);
			case 5:		return wxT(WNAME_ST_SLOTB_XML);
			};
			break;
		case ROMTYPE_SGB:
			switch(index) {
			case 0:		return wxT(WNAME_SGB_MAIN);
			case 1:		return wxT(WNAME_SGB_MAIN_XML);
			case 2:		return wxT(WNAME_SGB_SLOTA);
			case 3:		return wxT(WNAME_SGB_SLOTA_XML);
			};
			break;
		case ROMTYPE_NONE:
			if(index == 0)	return wxT("dummy");
			break;
		}
		return wxT("");
	}

	unsigned romname_to_index(enum rom_type rtype, const wxString& name)
	{
		for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++)
			if(romname(rtype, i) == name)
				return i;
		return ROMSELECT_ROM_COUNT;
	}

	unsigned fill_rom_names(enum rom_type rtype, wxString* array)
	{
		unsigned r = 0;
		for(unsigned i = 0; i < 6; i++) {
			wxString s = romname(rtype, i);
			if(s.Length())
				array[r++] = s;
		}
		return r;
	}

	enum rom_type romtype_from_string(const std::string& str)
	{
		if(str == TNAME_SNES)
			return ROMTYPE_SNES;
		if(str == TNAME_BSX_NS)
			return ROMTYPE_BSX;
		if(str == TNAME_BSX_S)
			return ROMTYPE_BSXSLOTTED;
		if(str == TNAME_SUFAMITURBO)
			return ROMTYPE_SUFAMITURBO;
		if(str == TNAME_SGB)
			return ROMTYPE_SGB;
		return ROMTYPE_NONE;
	}

	bool has_forced_region(const std::string& str)
	{
		enum rom_type rtype = romtype_from_string(str);
		return (rtype != ROMTYPE_SNES && rtype != ROMTYPE_SGB);
	}
}


wxwin_romselect::wxwin_romselect()
	: wxFrame(NULL, wxID_ANY, wxT("Select ROM"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	wxString rtchoices[32];
	wxString rrchoices[32];
	size_t systems = populate_system_choices(rtchoices);
	size_t regions = populate_region_choices(rrchoices);

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
	wxFlexGridSizer* romgrid = new wxFlexGridSizer(ROMSELECT_ROM_COUNT, 4, 0, 0);
	for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++) {
		romgrid->Add(rom_label[i] = new wxStaticText(this, wxID_ANY, wxT("")), 0, wxGROW);
		romgrid->Add(rom_name[i] = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, -1)),
			1, wxGROW);
		romgrid->Add(rom_change[i] = new wxButton(this, ROM_SELECTS_BASE + i, wxT("...")), 0, wxGROW);
		romgrid->Add(rom_headered[i] = new wxCheckBox(this, wxID_ANY, wxT("Headered")), 0, wxGROW);
		rom_name[i]->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxwin_romselect::on_filename_change), NULL, this);
		rom_change[i]->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_romselect::on_ask_rom_filename), NULL, this);
		rom_headered[i]->Disable();
	}
	toplevel->Add(romgrid, 1, wxGROW);

	//Button bar.
	wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->AddStretchSpacer();
	buttons->Add(open_rom = new wxButton(this, wxID_OPEN, wxT("Open ROM")), 0, wxALIGN_RIGHT);
	buttons->Add(quit_button = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxALIGN_RIGHT);
	open_rom->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_romselect::on_open_rom), NULL, this);
	open_rom->Disable();
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
}

void wxwin_romselect::set_rtype(std::string rtype)
{
	bool no_rtype = (current_rtype == "");
	if(rtype == "")
		rtype = tostdstring(romtype_combo->GetValue());
	if(rtype == current_rtype)
		return;
	if(has_forced_region(rtype)) {
		region_combo->Disable();
		remembered_region = tostdstring(region_combo->GetValue());
	} else {
		if(remembered_region != "")
			region_combo->SetValue(towxstring(remembered_region));
		remembered_region = "";
		region_combo->Enable();
	}
	wxString tmp[ROMSELECT_ROM_COUNT];
	unsigned c = fill_rom_names(romtype_from_string(rtype), tmp);
	for(unsigned i = 0; i < c; i++)
		enable_slot(rom_label[i], rom_name[i], rom_change[i], (i & 1) ? NULL : rom_headered[i],
			tostdstring(tmp[i]));
	for(unsigned i = c; i < ROMSELECT_ROM_COUNT; i++)
		disable_slot(rom_label[i], rom_name[i], rom_change[i], rom_headered[i]);
	current_rtype = rtype;
	Fit();
}

void wxwin_romselect::on_ask_rom_filename(wxCommandEvent& e)
{
	try {
		std::string fname = pick_file_member(this, "Choose " + tostdstring(
			rom_label[e.GetId() - ROM_SELECTS_BASE]->GetLabel()), ".");
		wxTextCtrl* textbox = rom_name[e.GetId() - ROM_SELECTS_BASE];
		if(textbox)
			textbox->SetValue(towxstring(fname));
		on_filename_change(e);
	} catch(canceled_exception& e) {
	}
}

void wxwin_romselect::on_filename_change(wxCommandEvent& e)
{
	bool ok = true;
	enum rom_type rtype = romtype_from_string(tostdstring(romtype_combo->GetValue()));
	unsigned flags = 0;
	for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++)
		flags |= ((rom_name[i]->GetValue().Length() != 0) ? (1 << i) : 0);
	open_rom->Enable(check_present_roms(rtype, flags));
}

void wxwin_romselect::on_romtype_change(wxCommandEvent& e)
{
	set_rtype(tostdstring(romtype_combo->GetValue()));
	on_filename_change(e);
}

void wxwin_romselect::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void wxwin_romselect::on_open_rom(wxCommandEvent& e)
{
	rom_files rfiles;
	rfiles.base_file = "";
	rfiles.rtype = romtype_from_string(tostdstring(romtype_combo->GetValue()));
	rfiles.region = region_from_string(tostdstring(region_combo->GetValue()));
	rfiles.rom = tostdstring(rom_name[0]->GetValue());
	rfiles.rom_xml = tostdstring(rom_name[1]->GetValue());
	rfiles.slota = tostdstring(rom_name[2]->GetValue());
	rfiles.slota_xml = tostdstring(rom_name[3]->GetValue());
	rfiles.slotb = tostdstring(rom_name[4]->GetValue());
	rfiles.slotb_xml = tostdstring(rom_name[5]->GetValue());
	rfiles.rom_headered = rom_headered[0]->GetValue();
	rfiles.slota_headered = rom_headered[2]->GetValue();
	rfiles.slotb_headered = rom_headered[4]->GetValue();
	try {
		our_rom = new loaded_rom(rfiles);
		if(our_rom->slota.valid)
			our_rom_name = our_rom->slota.sha256;
		else if(our_rom->slotb.valid)
			our_rom_name = our_rom->slotb.sha256;
		else
			our_rom_name = our_rom->rom.sha256;
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading ROM", e.what(), wxICON_EXCLAMATION);
		return;
	}
	wxwin_patch* projwin = new wxwin_patch(*our_rom);
	projwin->Show();
	Destroy();
}
//---------------------------------------------------
wxwin_patch::wxwin_patch(loaded_rom& rom)
	: wxFrame(NULL, wxID_ANY, wxT("Patch ROM"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	our_rom = &rom;
	wxString targets[ROMSELECT_ROM_COUNT];
	size_t target_count = fill_rom_names(rom.rtype, targets);

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
	patchsel->Add(choosefile = new wxButton(this, wxID_ANY, wxT("...")), 0, wxGROW);
	patchfile->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxwin_patch::on_patchfile_change), NULL, this);
	choosefile->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_patch::on_ask_patchfile), NULL, this);
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
		std::string fname = pick_file_member(this, "Choose patch file", ".");
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
		if(patch_index > ROMSELECT_ROM_COUNT)
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

void wxwin_patch::on_done(wxCommandEvent& e)
{
	try {
		SNES::interface = &simple_interface;
		if(our_rom->slota.valid)
			our_rom_name = our_rom->slota.sha256;
		else if(our_rom->slotb.valid)
			our_rom_name = our_rom->slotb.sha256;
		else
			our_rom_name = our_rom->rom.sha256;
		our_rom->load();
	} catch(std::exception& e) {
		show_message_ok(this, "Error loading ROM", e.what(), wxICON_EXCLAMATION);
		return;
	}
	messages << "Detected region: " << gtype::tostring(our_rom->rtype, our_rom->region) << std::endl;
	if(our_rom->region == REGION_PAL)
		set_nominal_framerate(322445.0/6448.0);
	else if(our_rom->region == REGION_NTSC)
		set_nominal_framerate(10738636.0/178683.0);

	messages << "--- Internal memory mappings ---" << std::endl;
	dump_region_map();
	messages << "--- End of Startup --- " << std::endl;
	wxwin_project* projwin = new wxwin_project(*our_rom);
	projwin->Show();
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
	//6 Top-level blocks.
	//- Radiobutton for load
	//- Radiobutton for new.
	//- Filename/Controllertypes/initRTC/Gamename/SRAMs.
	//- Authors explanation.
	//- Authors
	//- Button bar.
	wxFlexGridSizer* toplevel = new wxFlexGridSizer(6, 1, 0, 0);
	SetSizer(toplevel);

	//Radiobutton for load.
	wxRadioButton* file = new wxRadioButton(this, wxID_ANY, wxT("Load movie/savestate"), wxDefaultPosition,
		wxDefaultSize, wxRB_GROUP);
	file->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wxwin_project::on_file_select), NULL, this);
	toplevel->Add(file, 0, wxGROW);
	load_file = true;
	
	//Radiobutton for new proect.
	wxRadioButton* newp = new wxRadioButton(this, wxID_ANY, wxT("New project"));
	newp->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wxwin_project::on_new_select), NULL, this);
	toplevel->Add(newp, 0, wxGROW);

	//Filename/Controllertypes/Gamename/initRTC/SRAMs.
	wxFlexGridSizer* mainblock = new wxFlexGridSizer(6 + sram_set.size(), 2, 0, 0);
	mainblock->Add(new wxStaticText(this, wxID_ANY, wxT("File to load:")), 0, wxGROW);
	wxFlexGridSizer* fileblock = new wxFlexGridSizer(1, 2, 0, 0);
	fileblock->Add(savefile = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(500, -1)),
		1, wxGROW);
	fileblock->Add(ask_savefile = new wxButton(this, ASK_FILENAME_BUTTON, wxT("...")), 0, wxGROW);
	savefile->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wxwin_project::on_filename_change), NULL, this);
	ask_savefile->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_project::on_ask_filename), NULL, this);
	mainblock->Add(fileblock, 0, wxGROW);
	mainblock->Add(new wxStaticText(this, wxID_ANY, wxT("Controller 1 Type:")), 0, wxGROW);
	mainblock->Add(controller1type = new wxComboBox(this, wxID_ANY, cchoices[1], wxDefaultPosition, wxDefaultSize,
		CONTROLLERTYPES_P1, cchoices, wxCB_READONLY), 0, wxGROW);
	mainblock->Add(new wxStaticText(this, wxID_ANY, wxT("Controller 2 Type:")), 0, wxGROW);
	mainblock->Add(controller2type = new wxComboBox(this, wxID_ANY, cchoices[0], wxDefaultPosition, wxDefaultSize,
		CONTROLLERTYPES, cchoices, wxCB_READONLY), 0, wxGROW);
	mainblock->Add(new wxStaticText(this, wxID_ANY, wxT("Initial RTC value:")), 0, wxGROW);
	wxFlexGridSizer* initrtc = new wxFlexGridSizer(1, 3, 0, 0);
	initrtc->Add(rtc_sec = new wxTextCtrl(this, wxID_ANY, wxT("1000000000"), wxDefaultPosition, wxSize(150, -1)),
		1, wxGROW);
	initrtc->Add(new wxStaticText(this, wxID_ANY, wxT(":")), 0, wxGROW);
	initrtc->Add(rtc_subsec = new wxTextCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition,
		wxSize(150, -1)), 1, wxGROW);
	mainblock->Add(initrtc, 0, wxGROW);
	mainblock->Add(new wxStaticText(this, wxID_ANY, wxT("Game name:")), 0, wxGROW);
	mainblock->Add(projectname = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1,
		wxGROW);
	mainblock->Add(new wxStaticText(this, wxID_ANY, wxT("Save prefix:")), 0, wxGROW);
	mainblock->Add(prefix = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1,
		wxGROW);
	unsigned idx = 0;
	for(auto i : sram_set) {
		mainblock->Add(new wxStaticText(this, wxID_ANY, towxstring("SRAM " + i)), 0, wxGROW);
		wxFlexGridSizer* fileblock2 = new wxFlexGridSizer(1, 2, 0, 0);
		fileblock2->Add(sram_files[i] = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition,
			wxSize(500, -1)), 1, wxGROW);
		fileblock2->Add(sram_choosers[i] = new wxButton(this, ASK_SRAMS_BASE + idx, wxT("...")), 0, wxGROW);
		sram_files[i]->Connect(wxEVT_COMMAND_TEXT_UPDATED,
			wxCommandEventHandler(wxwin_project::on_filename_change), NULL, this);
		sram_choosers[i]->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxwin_project::on_ask_filename), NULL, this);
		mainblock->Add(fileblock2, 0, wxGROW);
		sram_names[idx] = i;
		idx++;
	}
	toplevel->Add(mainblock, 0, wxGROW);

	//Authors
	toplevel->Add(new wxStaticText(this, wxID_ANY, wxT("Authors (one per line):")), 0, wxGROW);
	toplevel->Add(authors = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE), 0, wxGROW);
	authors->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wxwin_project::on_filename_change), NULL,
		this);

	//Button bar.
	wxBoxSizer* buttonbar = new wxBoxSizer(wxHORIZONTAL);
	buttonbar->AddStretchSpacer();
	buttonbar->Add(load = new wxButton(this, wxID_ANY, wxT("Load")), 0, wxGROW);
	buttonbar->Add(quit = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxGROW);
	load->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_project::on_load), NULL, this);
	quit->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_project::on_quit), NULL, this);
	toplevel->Add(buttonbar, 0, wxGROW);

	{
		std::ifstream s(get_config_path() + "/" + our_rom_name + ".ls");
		std::getline(s, last_save);
		savefile->SetValue(towxstring(last_save));
	}

	wxCommandEvent e;
	on_file_select(e);

	mainblock->SetSizeHints(this);
	toplevel->SetSizeHints(this);
	Fit();
}

wxwin_project::~wxwin_project()
{
}

void wxwin_project::on_file_select(wxCommandEvent& e)
{
	savefile->Enable();
	ask_savefile->Enable();
	controller1type->Disable();
	controller2type->Disable();
	rtc_sec->Disable();
	rtc_subsec->Disable();
	projectname->Disable();
	prefix->Disable();
	authors->Disable();
	load->SetLabel(wxT("Load"));
	load_file = true;
	for(auto i : sram_files)
		i.second->Disable();
	for(auto i : sram_choosers)
		i.second->Disable();
	on_filename_change(e);
}

void wxwin_project::on_new_select(wxCommandEvent& e)
{
	savefile->Disable();
	ask_savefile->Disable();
	controller1type->Enable();
	controller2type->Enable();
	rtc_sec->Enable();
	rtc_subsec->Enable();
	projectname->Enable();
	prefix->Enable();
	authors->Enable();
	load->SetLabel(wxT("Start"));
	on_filename_change(e);
	load_file = false;
	for(auto i : sram_files)
		i.second->Enable();
	for(auto i : sram_choosers)
		i.second->Enable();
	on_filename_change(e);
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
			boot_emulator(*our_rom, *new moviefile(tostdstring(savefile->GetValue())));
		} else {
			boot_emulator(*our_rom, *new moviefile(make_movie()));
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
	for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
		SNES::Cartridge::NonVolatileRAM& s = SNES::cartridge.nvram[i];
		r.insert(sram_name(s.id, s.slot));
	}
	return r;
}

struct moviefile wxwin_project::make_movie()
{
	moviefile f;
	f.force_corrupt = false;
	f.gametype = gtype::togametype(our_rom->rtype, our_rom->region);
	f.port1 = get_controller_type(tostdstring(controller1type->GetValue()));
	f.port2 = get_controller_type(tostdstring(controller2type->GetValue()));
	f.coreversion = bsnes_core_version;
	f.gamename = tostdstring(projectname->GetValue());
	f.prefix = sanitize_prefix(tostdstring(prefix->GetValue()));
	f.projectid = get_random_hexstring(40);
	f.rerecords = "0";
	f.rom_sha256 = our_rom->rom.sha256;
	f.romxml_sha256 = our_rom->rom_xml.sha256;
	f.slota_sha256 = our_rom->slota.sha256;
	f.slotaxml_sha256 = our_rom->slota_xml.sha256;
	f.slotb_sha256 = our_rom->slotb.sha256;
	f.slotbxml_sha256 = our_rom->slotb_xml.sha256;
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

