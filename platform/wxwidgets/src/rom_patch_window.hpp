#ifndef _wxwidgets_rom_patch_window__hpp__included__
#define _wxwidgets_rom_patch_window__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include "filenamebox.hpp"
#include "rom.hpp"
#include "common.hpp"

class wx_rom_patch_window : public wxFrame
{
public:
	wx_rom_patch_window(loaded_rom& rom);
	~wx_rom_patch_window();
	void on_patchfile_change(wxCommandEvent& e);
	void on_do_patch(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_done(wxCommandEvent& e);
	loaded_rom* our_rom;
private:
	filenamebox* patchfile;
	wxComboBox* patch_what;
	wxButton* dopatch;
	wxTextCtrl* patch_offset;
	wxStaticText_ptr checksums[6];
	wxString targets[6];
};

#endif