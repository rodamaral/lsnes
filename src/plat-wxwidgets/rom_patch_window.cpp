#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/framerate.hpp"
#include "core/zip.hpp"

#include "plat-wxwidgets/callrom.hpp"
#include "plat-wxwidgets/project_select_window.hpp"
#include "plat-wxwidgets/rom_patch_window.hpp"

namespace
{
	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	} simple_interface;
}

wx_rom_patch_window::wx_rom_patch_window(loaded_rom& rom)
	: wxFrame(NULL, wxID_ANY, wxT("Patch ROM"), wxDefaultPosition, wxSize(-1, -1),
		primary_window_style)
{
	our_rom = &rom;
	size_t target_count = fill_rom_names(rom.rtype, targets);

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(5, 1, 0, 0);
	SetSizer(top_s);

	wxFlexGridSizer* checksums_s = new wxFlexGridSizer(target_count, 2, 0, 0);
	for(unsigned i = 0; i < 6; i++)
		checksums[i] = NULL;
	for(unsigned i = 0; i < target_count; i++) {
		checksums_s->Add(new wxStaticText(this, wxID_ANY, targets[i]), 0, wxGROW);
		checksums_s->Add(checksums[i] = new wxStaticText(this, wxID_ANY,
			towxstring(get_rom_slot(*our_rom, i).sha256)), 0, wxGROW);
	}
	top_s->Add(checksums_s, 0, wxGROW);

	wxFlexGridSizer* pwhat_s = new wxFlexGridSizer(1, 2, 0, 0);
	pwhat_s->Add(new wxStaticText(this, wxID_ANY, wxT("Patch what:")), 0, wxGROW);
	pwhat_s->Add(patch_what = new wxComboBox(this, wxID_ANY, targets[0], wxDefaultPosition, wxDefaultSize,
		target_count, targets, wxCB_READONLY), 0, wxGROW);
	top_s->Add(pwhat_s, 0, wxGROW);

	patchfile = new filenamebox(top_s, this, "Patch file", FNBF_PL | FNBF_OI | FNBF_PZ, this,
		wxCommandEventHandler(wx_rom_patch_window::on_patchfile_change));

	wxFlexGridSizer* poffset_s = new wxFlexGridSizer(1, 2, 0, 0);
	pwhat_s->Add(new wxStaticText(this, wxID_ANY, wxT("Patch offset:")), 0, wxGROW);
	pwhat_s->Add(patch_offset = new wxTextCtrl(this, wxID_ANY, wxT("")), 0, wxGROW);
	patch_offset->Connect(wxEVT_COMMAND_TEXT_UPDATED,
		wxCommandEventHandler(wx_rom_patch_window::on_patchfile_change), NULL, this);
	top_s->Add(poffset_s, 0, wxGROW);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	wxButton* thats_enough = new wxButton(this, wxID_ANY, wxT("Enough"));
	pbutton_s->Add(thats_enough, 0, wxGROW);
	thats_enough->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_rom_patch_window::on_done), NULL, this);
	dopatch = new wxButton(this, wxID_ANY, wxT("Patch"));
	pbutton_s->Add(dopatch, 0, wxGROW);
	dopatch->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_rom_patch_window::on_do_patch), NULL, this);
	dopatch->Disable();
	wxButton* quitbutton = new wxButton(this, wxID_EXIT, wxT("Quit"));
	pbutton_s->Add(quitbutton, 0, wxGROW);
	quitbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_rom_patch_window::on_quit), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	patch_offset->SetValue(wxT("0"));

	checksums_s->SetSizeHints(this);
	pwhat_s->SetSizeHints(this);
	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wx_rom_patch_window::~wx_rom_patch_window()
{
	delete patchfile;
}

void wx_rom_patch_window::on_patchfile_change(wxCommandEvent& e)
{
	//std::cerr << "wx_rom_patch_window::on_patchfile_change" << std::endl;
	bool ok = true;
	//std::cerr << "wx_rom_patch_window::on_patchfile_change: #1: ok=" << ok << std::endl;
	ok = ok && patchfile->is_nonblank();
	//std::cerr << "wx_rom_patch_window::on_patchfile_change: #2: ok=" << ok << std::endl;
	std::string offsetv = tostdstring(patch_offset->GetValue());
	try {
		int32_t offset = boost::lexical_cast<int32_t>(offsetv);
	} catch(...) {
		ok = false;
	}
	//std::cerr << "wx_rom_patch_window::on_patchfile_change: #3: ok=" << ok << std::endl;
	if(dopatch) {
		//std::cerr << "wx_rom_patch_window::on_patchfile_change: #4: ok=" << ok << std::endl;
		dopatch->Enable(ok);
	}
	//std::cerr << "wx_rom_patch_window::on_patchfile_change: #5: ok=" << ok << std::endl;
}

void wx_rom_patch_window::on_do_patch(wxCommandEvent& e)
{
	try {
		auto patch_contents = read_file_relative(patchfile->get_file(), "");
		size_t patch_index = romname_to_index(our_rom->rtype, patch_what->GetValue());
		if(patch_index == 6)
			throw std::runtime_error("Internal error: Patch WHAT?");
		loaded_slot& s = get_rom_slot(*our_rom, patch_index);
		std::string offsetv = tostdstring(patch_offset->GetValue());
		int32_t offset = boost::lexical_cast<int32_t>(offsetv);
		s.patch(patch_contents, offset);
		checksums[patch_index]->SetLabel(towxstring(s.sha256));
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(this, towxstring(e.what()),
			wxT("Error patching ROM"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
		return;
	}
	patchfile->clear();
}

void wx_rom_patch_window::on_quit(wxCommandEvent& e)
{
	Close(true);
}

void wx_rom_patch_window::on_done(wxCommandEvent& e)
{
	try {
		SNES::interface = &simple_interface;
		our_rom->load();
	} catch(std::exception& e) {
		wxMessageDialog* d = new wxMessageDialog(this, towxstring(e.what()),
			wxT("Error loading ROM"), wxOK | wxICON_EXCLAMATION);
		d->ShowModal();
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
	wx_project_select_window* projwin = new wx_project_select_window(*our_rom);
	projwin->Show();
	Destroy();
}
