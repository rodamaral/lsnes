//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/statbox.h>
#include <wx/notebook.h>

#include "lsnes.hpp"
#include "core/emucore.hpp"

#include "core/moviedata.hpp"
#include "core/framerate.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/zip.hpp"

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
	porttype_info& get_controller_type(const std::string& s)
	{
		auto types = porttype_info::get_all();
		for(auto i : types)
			if(s == i->hname)
				return *i;
		return porttype_info::default_type();
	}

	void load_cchoices(std::vector<wxString>& cc, unsigned port, unsigned& dfltidx)
	{
		cc.clear();
		porttype_info& dflt = porttype_info::port_default(port);
		dfltidx = 0;
		auto types = porttype_info::get_all();
		for(auto i : types)
			if(i->legal && i->legal(port)) {
				cc.push_back(towxstring(i->hname));
				if(i == &dflt)
					dfltidx = cc.size() - 1;
			}
	}

	struct loaded_slot& get_rom_slot(struct loaded_rom& rom, unsigned index)
	{
		if(index >= 2 * sizeof(rom.romimg) / sizeof(rom.romimg[0]))
			return rom.romimg[0];
		if(index & 1)
			return rom.romxml[index / 2];
		else
			return rom.romimg[index / 2];
	}

	core_region& region_from_string(core_type& rtype, const std::string& str)
	{
		core_region* x = NULL;
		for(auto i : rtype.get_regions())
			if(i->get_hname() == str)
				return *i;
		return *x;
	}

	unsigned populate_region_choices(std::vector<wxString>& array)
	{
		array.push_back(wxT("(Default)"));
		return 1;
	}

	unsigned populate_system_choices(std::vector<wxString>& array)
	{
		for(auto i : core_type::get_core_types())
			array.push_back(towxstring(i->get_hname()));
		return array.size();
	}

	bool check_present_roms(core_type& rtype, unsigned flags)
	{
		unsigned a = 0;
		unsigned b = 0;
		for(size_t i = 0; i < ROMSELECT_ROM_COUNT && i < rtype.get_image_count(); i++) {
			if((flags >> i) & 1)
				a |= rtype.get_image_info(i).mandatory;
			b |= rtype.get_image_info(i).mandatory;
		}
		return (a == b);
	}

	std::string romname(core_type& rtype, unsigned index)
	{
		if(index >= rtype.get_image_count())
			return "";
		return rtype.get_image_info(index).hname;
	}

	unsigned romname_to_index(core_type& rtype, const wxString& _name)
	{
		std::string name = tostdstring(_name);
		for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++) {
			if(romname(rtype, i) == name)
				return 2 * i;
			if(romname(rtype, i) + MARKUP_POSTFIX == name)
				return 2 * i + 1;
		}
		return 2 * ROMSELECT_ROM_COUNT;
	}

	unsigned fill_rom_names(core_type& rtype, std::string* array)
	{
		unsigned r = 0;
		for(unsigned i = 0; i < ROMSELECT_ROM_COUNT; i++) {
			std::string s = romname(rtype, i);
			if(s.length())
				array[r++] = s;
		}
		return r;
	}

	core_type& romtype_from_string(const std::string& str)
	{
		core_type* x = NULL;
		for(auto i : core_type::get_core_types())
			if(i->get_hname() == str)
				return *i;
		return *x;
	}

	bool has_forced_region(const std::string& str)
	{
		core_type& rtype = romtype_from_string(str);
		return (rtype.get_regions().size() == 1);
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
	wxComboBox* controller1type;
	wxComboBox* controller2type;
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

	std::set<std::string> sram_set = get_sram_set();

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
	wxFlexGridSizer* mainblock = new wxFlexGridSizer(6 + sram_set.size(), 2, 0, 0);
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Controller 1 Type:")), 0, wxGROW);
	load_cchoices(cchoices, 0, dfltidx);
	mainblock->Add(controller1type = new wxComboBox(new_panel, wxID_ANY, cchoices[dfltidx], wxDefaultPosition,
		wxDefaultSize, cchoices.size(), &cchoices[0], wxCB_READONLY), 0, wxGROW);
	load_cchoices(cchoices, 1, dfltidx);
	mainblock->Add(new wxStaticText(new_panel, wxID_ANY, wxT("Controller 2 Type:")), 0, wxGROW);
	mainblock->Add(controller2type = new wxComboBox(new_panel, wxID_ANY, cchoices[dfltidx], wxDefaultPosition,
		wxDefaultSize, cchoices.size(), &cchoices[0], wxCB_READONLY), 0, wxGROW);
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
	f.port1 = &get_controller_type(tostdstring(controller1type->GetValue()));
	f.port2 = &get_controller_type(tostdstring(controller2type->GetValue()));
	f.coreversion = bsnes_core_version;
	f.gamename = tostdstring(projectname->GetValue());
	f.prefix = sanitize_prefix(tostdstring(prefix->GetValue()));
	f.projectid = get_random_hexstring(40);
	f.rerecords = "0";
	for(size_t i = 0; i < sizeof(our_rom->romimg)/sizeof(our_rom->romimg[0]); i++) {
		f.romimg_sha256[i] = our_rom->romimg[i].sha256;
		f.romxml_sha256[i] = our_rom->romxml[i].sha256;
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
	f.input.clear(*f.port1, *f.port2);
	return f;
}

