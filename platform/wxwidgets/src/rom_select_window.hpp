#ifndef _wxwidgets_rom_select_window__hpp__included__
#define _wxwidgets_rom_select_window__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include "filenamebox.hpp"
#include "rom.hpp"
#include "labelcombobox.hpp"

class wx_rom_select_window : public wxFrame
{
public:
	wx_rom_select_window();
	~wx_rom_select_window();
	void on_filename_change(wxCommandEvent& e);
	void on_romtype_change(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_open_rom(wxCommandEvent& e);
	loaded_rom* our_rom;
private:
	labeledcombobox* rtypec;
	labeledcombobox* regionc;
	filenamebox* main_rom;
	filenamebox* main_xml;
	filenamebox* slota_rom;
	filenamebox* slota_xml;
	filenamebox* slotb_rom;
	filenamebox* slotb_xml;
	wxButton* open_rom;
	wxButton* quit_button;
	std::string current_rtype;
	void set_rtype(std::string rtype);
};

#endif