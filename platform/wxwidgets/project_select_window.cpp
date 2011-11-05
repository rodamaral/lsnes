#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>
#include "project_select_window.hpp"
#include "common.hpp"
#include "zip.hpp"
#include "moviedata.hpp"
#include "emufn.hpp"
#include <stdexcept>
#include <boost/lexical_cast.hpp>

namespace
{
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
}

wx_project_select_window::wx_project_select_window(loaded_rom& rom)
	: wxFrame(NULL, wxID_ANY, wxT("Project settings"), wxDefaultPosition, wxSize(-1, -1),
		primary_window_style)
{
	our_rom = &rom;
	wxString cchoices[7];
	cchoices[0] = wxT(CNAME_NONE);
	cchoices[1] = wxT(CNAME_GAMEPAD);
	cchoices[2] = wxT(CNAME_MULTITAP);
	cchoices[3] = wxT(CNAME_MOUSE);
	cchoices[4] = wxT(CNAME_SUPERSCOPE);
	cchoices[5] = wxT(CNAME_JUSTIFIER);
	cchoices[6] = wxT(CNAME_JUSTIFIERS);

	std::set<std::string> sram_set = get_sram_set();

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(7 + sram_set.size(), 1, 0, 0);
	SetSizer(top_s);

	wxRadioButton* file = new wxRadioButton(this, wxID_ANY, wxT("Load movie/savestate"), wxDefaultPosition,
		wxDefaultSize, wxRB_GROUP);
	wxRadioButton* newp = new wxRadioButton(this, wxID_ANY, wxT("New project"));
	file->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wx_project_select_window::on_file_select), NULL, this);
	newp->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,
		wxCommandEventHandler(wx_project_select_window::on_new_select), NULL, this);
	top_s->Add(file, 0, wxGROW);
	top_s->Add(newp, 0, wxGROW);
	load_file = true;

	filename = new filenamebox(top_s, this, "Movie/Savestate", FNBF_PL | FNBF_OI, this,
		wxCommandEventHandler(wx_project_select_window::on_filename_change));

	wxFlexGridSizer* c_s = new wxFlexGridSizer(4, 2, 0, 0);
	controller1type = new labeledcombobox(c_s, this, "Controller 1 type:", cchoices, 4, 1, false, this,
		wxCommandEventHandler(wx_project_select_window::on_filename_change));
	controller2type = new labeledcombobox(c_s, this, "Controller 2 type:", cchoices, 7, 0, false, this,
		wxCommandEventHandler(wx_project_select_window::on_filename_change));
	c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Initial RTC:")), 0, wxGROW);
	wxFlexGridSizer* t_s = new wxFlexGridSizer(1, 3, 0, 0);
	t_s->Add(rtc_sec = new wxTextCtrl(this, wxID_ANY, wxT("1000000000"), wxDefaultPosition, wxSize(150, -1)), 1,
		 wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT(":")), 0, wxGROW);
	t_s->Add(rtc_subsec = new wxTextCtrl(this, wxID_ANY, wxT("0"), wxDefaultPosition, wxSize(120, -1)), 0, wxGROW);
	rtc_sec->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wx_project_select_window::on_filename_change), NULL, this);
	rtc_subsec->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wx_project_select_window::on_filename_change), NULL, this);
	c_s->Add(t_s, 0, wxGROW);
	top_s->Add(c_s, 0, wxGROW);
	c_s->Add(new wxStaticText(this, wxID_ANY, wxT("Game name:")), 0, wxGROW);
	c_s->Add(projectname = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, -1)), 1, wxGROW);

	top_s->Add(new wxStaticText(this, wxID_ANY, wxT("Authors (one per line):")), 0, wxGROW);
	top_s->Add(authors = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE), 0, wxGROW);
	authors->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wx_project_select_window::on_filename_change), NULL, this);

	for(auto i : sram_set)
		srams[i] = new filenamebox(top_s, this, "SRAM " + i + ":" , FNBF_PL | FNBF_OI, this,
			wxCommandEventHandler(wx_project_select_window::on_filename_change));

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(load = new wxButton(this, wxID_ANY, wxT("Load")), 0, wxGROW);
	pbutton_s->Add(quit = new wxButton(this, wxID_EXIT, wxT("Quit")), 0, wxGROW);
	load->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_project_select_window::on_load), NULL, this);
	quit->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_project_select_window::on_quit), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	wxCommandEvent e;
	on_file_select(e);

	t_s->SetSizeHints(this);
	c_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wx_project_select_window::~wx_project_select_window()
{
	delete controller1type;
	delete controller2type;
	delete filename;
}

void wx_project_select_window::on_file_select(wxCommandEvent& e)
{
	filename->enable();
	controller1type->disable();
	controller2type->disable();
	rtc_sec->Disable();
	rtc_subsec->Disable();
	projectname->Disable();
	authors->Disable();
	load->SetLabel(wxT("Load"));
	on_filename_change(e);
	load_file = true;
	for(auto i : srams)
		i.second->disable();
}

void wx_project_select_window::on_new_select(wxCommandEvent& e)
{
	filename->disable();
	controller1type->enable();
	controller2type->enable();
	rtc_sec->Enable();
	rtc_subsec->Enable();
	projectname->Enable();
	authors->Enable();
	load->SetLabel(wxT("Start"));
	on_filename_change(e);
	load_file = false;
	for(auto i : srams)
		i.second->enable();
}

void wx_project_select_window::on_filename_change(wxCommandEvent& e)
{
	if(filename->is_enabled()) {
		load->Enable(filename->is_nonblank());
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

void wx_project_select_window::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void wx_project_select_window::on_load(wxCommandEvent& e)
{
	try {
		if(load_file) {
			boot_emulator(*our_rom, *new moviefile(filename->get_file()));
		} else {
			boot_emulator(*our_rom, *new moviefile(make_movie()));
		}
		Destroy();
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(this, towxstring(e.what()),
			wxT("Error loading movie"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		return;
	}
}

std::set<std::string> wx_project_select_window::get_sram_set()
{
	std::set<std::string> r;
	for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
		SNES::Cartridge::NonVolatileRAM& s = SNES::cartridge.nvram[i];
		r.insert(sram_name(s.id, s.slot));
	}
	return r;
}

struct moviefile wx_project_select_window::make_movie()
{
	moviefile f;
	f.force_corrupt = false;
	f.gametype = gtype::togametype(our_rom->rtype, our_rom->region);
	f.port1 = get_controller_type(controller1type->get_choice());
	f.port2 = get_controller_type(controller2type->get_choice());
	f.coreversion = bsnes_core_version;
	f.gamename = tostdstring(projectname->GetValue());
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
	for(auto i : srams)
		if(i.second->get_file() != "")
			f.movie_sram[i.first] = read_file_relative(i.second->get_file(), "");
	f.is_savestate = false;
	f.movie_rtc_second = f.rtc_second = boost::lexical_cast<int64_t>(tostdstring(rtc_sec->GetValue()));
	f.movie_rtc_subsecond = f.rtc_subsecond = boost::lexical_cast<int64_t>(tostdstring(rtc_subsec->GetValue()));
	if(f.movie_rtc_subsecond < 0)
		throw std::runtime_error("RTC subsecond must be positive");
	return f;
}
