#include "plat-wxwidgets/callrom.hpp"
#include "plat-wxwidgets/rom_patch_window.hpp"
#include "plat-wxwidgets/rom_select_window.hpp"

wx_rom_select_window::wx_rom_select_window()
	: wxFrame(NULL, wxID_ANY, wxT("Select ROM"), wxDefaultPosition, wxSize(-1, -1),
		primary_window_style)
{
	wxString rtchoices[5];
	wxString rrchoices[3];
	size_t systems = populate_system_choices(rtchoices);
	size_t regions = populate_region_choices(rrchoices);

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(3, 1, 0, 0);
	SetSizer(top_s);

	wxBoxSizer* selects_s = new wxBoxSizer(wxHORIZONTAL);
	rtypec = new labeledcombobox(selects_s, this, "ROM type:", rtchoices, systems, 0, true, this,
		wxCommandEventHandler(wx_rom_select_window::on_romtype_change));
	regionc = new labeledcombobox(selects_s, this, "Region:", rrchoices, regions, 0, true, this,
		wxCommandEventHandler(wx_rom_select_window::on_romtype_change));
	top_s->Add(selects_s, 0, wxGROW);

	//The XMLs don't matter, so don't notify those.
	wxFlexGridSizer* romgrid_s = new wxFlexGridSizer(6, 3, 0, 0);
	main_rom = new filenamebox(romgrid_s, this, "ROM", FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_select_window::on_filename_change));
	main_xml = new filenamebox(romgrid_s, this, "ROM XML", FNBF_NN | FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_select_window::on_filename_change));
	slota_rom = new filenamebox(romgrid_s, this, "SLOT A ROM", FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_select_window::on_filename_change));
	slota_xml = new filenamebox(romgrid_s, this, "SLOT A XML", FNBF_NN | FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_select_window::on_filename_change));
	slotb_rom = new filenamebox(romgrid_s, this, "SLOT A ROM", FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_select_window::on_filename_change));
	slotb_xml = new filenamebox(romgrid_s, this, "SLOT A XML", FNBF_NN | FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_select_window::on_filename_change));

	top_s->Add(romgrid_s, 1, wxGROW);
	wxBoxSizer* button_s = new wxBoxSizer(wxHORIZONTAL);
	button_s->AddStretchSpacer();
	button_s->Add(open_rom = new wxButton(this, wxID_OPEN, wxT("Open ROM")), 0, wxALIGN_RIGHT);
	button_s->Add(quit_button = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxALIGN_RIGHT);
	open_rom->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_rom_select_window::on_open_rom), NULL, this);
	open_rom->Disable();
	quit_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_rom_select_window::on_quit), NULL, this);
	top_s->Add(button_s, 1, wxGROW);

	set_rtype("");

	top_s->SetSizeHints(this);
	Fit();
}

wx_rom_select_window::~wx_rom_select_window()
{
	delete rtypec;
	delete regionc;
	delete main_rom;
	delete main_xml;
	delete slota_rom;
	delete slota_xml;
	delete slotb_rom;
	delete slotb_xml;
}


void wx_rom_select_window::set_rtype(std::string rtype)
{
	bool no_rtype = (current_rtype == "");
	if(rtype == "")
		rtype = rtypec->get_choice();
	if(rtype == current_rtype)
		return;
	if(has_forced_region(rtype))
		regionc->disable(forced_region_for_romtype(rtype));
	else
		regionc->enable();
	wxString tmp[6];
	unsigned c = fill_rom_names(romtype_from_string(rtype), tmp);
	if(c > 0)	main_rom->enable(tostdstring(tmp[0])); else main_rom->disable();
	if(c > 1)	main_xml->enable(tostdstring(tmp[1])); else main_xml->disable();
	if(c > 2)	slota_rom->enable(tostdstring(tmp[2])); else slota_rom->disable();
	if(c > 3)	slota_xml->enable(tostdstring(tmp[3])); else slota_xml->disable();
	if(c > 4)	slotb_rom->enable(tostdstring(tmp[4])); else slotb_rom->disable();
	if(c > 5)	slotb_xml->enable(tostdstring(tmp[5])); else slotb_xml->disable();
	current_rtype = rtype;
	Fit();
}

void wx_rom_select_window::on_filename_change(wxCommandEvent& e)
{
	bool ok = true;
	enum rom_type rtype = romtype_from_string(rtypec->get_choice());
	ok = ok && main_rom->is_nonblank();
	if(rtype == ROMTYPE_BSX || rtype == ROMTYPE_BSXSLOTTED || rtype == ROMTYPE_SGB)
		ok = ok && slota_rom->is_nonblank();
	if(rtype == ROMTYPE_SUFAMITURBO)
		ok = ok && (slota_rom->is_nonblank() || slotb_rom->is_nonblank());
	open_rom->Enable(ok);
}

void wx_rom_select_window::on_romtype_change(wxCommandEvent& e)
{
	set_rtype(rtypec->get_choice());
	on_filename_change(e);
}

void wx_rom_select_window::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void wx_rom_select_window::on_open_rom(wxCommandEvent& e)
{
	rom_files rfiles;
	rfiles.base_file = "";
	rfiles.rtype = romtype_from_string(rtypec->get_choice());
	rfiles.region = region_from_string(regionc->get_choice());
	rfiles.rom = main_rom->get_file();
	rfiles.rom_xml = main_xml->get_file();
	rfiles.slota = slota_rom->get_file();
	rfiles.slota_xml = slota_xml->get_file();
	rfiles.slotb = slotb_rom->get_file();
	rfiles.slotb_xml = slotb_xml->get_file();
	try {
		our_rom = new loaded_rom(rfiles);
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(this, towxstring(e.what()),
			wxT("Error loading ROM"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
	}
	wx_rom_patch_window* projwin = new wx_rom_patch_window(*our_rom);
	projwin->Show();
	Destroy();
}
