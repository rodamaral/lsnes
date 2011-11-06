#ifndef _wxwidgets_project_select_window__hpp__included__
#define _wxwidgets_project_select_window__hpp__included__

#include "core/rom.hpp"
#include "core/moviefile.hpp"

#include "plat-wxwidgets/filenamebox.hpp"
#include "plat-wxwidgets/labelcombobox.hpp"

#include <set>
#include <map>
#include <string>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

class wx_project_select_window : public wxFrame
{
public:
	wx_project_select_window(loaded_rom& rom);
	~wx_project_select_window();
	void on_file_select(wxCommandEvent& e);
	void on_new_select(wxCommandEvent& e);
	void on_filename_change(wxCommandEvent& e);
	void on_quit(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
	loaded_rom* our_rom;
private:
	bool load_file;
	std::set<std::string> get_sram_set();
	struct moviefile make_movie();
	filenamebox* filename;
	std::map<std::string, filenamebox*> srams;
	labeledcombobox* controller1type;
	labeledcombobox* controller2type;
	wxTextCtrl* projectname;
	wxTextCtrl* rtc_sec;
	wxTextCtrl* rtc_subsec;
	wxTextCtrl* authors;
	wxButton* load;
	wxButton* quit;
};

#endif