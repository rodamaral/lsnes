#ifndef _plat_wxwidgets__window_romselect__hpp__included__
#define _plat_wxwidgets__window_romselect__hpp__included__

#include "core/rom.hpp"

#include <string>
#include <map>
#include <set>
#include <wx/wx.h>
#include <wx/string.h>

#define ROMSELECT_ROM_COUNT 6

class wxwin_romselect : public wxFrame
{
public:
	wxwin_romselect();
	~wxwin_romselect();
	void on_filename_change(wxCommandEvent& e);
	void on_romtype_change(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_open_rom(wxCommandEvent& e);
	void on_ask_rom_filename(wxCommandEvent& e);
	loaded_rom* our_rom;
private:
	wxComboBox* romtype_combo;
	wxComboBox* region_combo;
	wxStaticText* rom_label[ROMSELECT_ROM_COUNT];
	wxTextCtrl* rom_name[ROMSELECT_ROM_COUNT];
	wxButton* rom_change[ROMSELECT_ROM_COUNT];
	wxButton* open_rom;
	wxButton* quit_button;
	std::string current_rtype;
	std::string remembered_region;
	void set_rtype(std::string rtype);
};

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
	loaded_rom* our_rom;
private:
	bool load_file;
	std::set<std::string> get_sram_set();
	struct moviefile make_movie();
	wxTextCtrl* savefile;
	wxButton* ask_savefile;
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

#endif
