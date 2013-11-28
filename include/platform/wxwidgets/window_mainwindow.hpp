#ifndef _plat_wxwidgets__window_mainwindow__hpp__included__
#define _plat_wxwidgets__window_mainwindow__hpp__included__

#include "core/filedownload.hpp"
#include "core/window.hpp"
#include "platform/wxwidgets/window_status.hpp"
#include "platform/wxwidgets/menu_recent.hpp"
#include "platform/wxwidgets/menu_loadrom.hpp"
#include "library/dispatch.hpp"
#include "library/recentfiles.hpp"

#include <stack>

#include <wx/string.h>
#include <wx/wx.h>

class wxwin_mainwindow : public wxFrame
{
public:
	class panel : public wxPanel
	{
	public:
		panel(wxWindow* win);
		void on_paint(wxPaintEvent& e);
		void request_paint();
		void on_erase(wxEraseEvent& e);
		void on_keyboard_down(wxKeyEvent& e);
		void on_keyboard_up(wxKeyEvent& e);
		void on_mouse(wxMouseEvent& e);
	};
	wxwin_mainwindow();
	void request_paint();
	void notify_update() throw();
	void notify_update_status() throw();
	void notify_resized() throw();
	void notify_exit() throw();
	void refresh_title() throw();
	void on_close(wxCloseEvent& e);
	void menu_start(wxString name);
	void menu_special(wxString name, wxMenu* menu);
	void menu_special_sub(wxString name, wxMenu* menu);
	void menu_entry(int id, wxString name);
	void menu_entry_check(int id, wxString name);
	void menu_start_sub(wxString name);
	void menu_end_sub();
	bool menu_ischecked(int id);
	void menu_check(int id, bool newstate);
	void menu_enable(int id, bool newstate);
	void menu_separator();
	void handle_menu_click(wxCommandEvent& e);
	void update_statusbar(const std::map<std::string, std::u32string>& vars);
	void action_updated();
	void enter_or_leave_fullscreen(bool fs);
	void request_rom(rom_request& req);
	recent_menu<recentfile_multirom>* recent_roms;
	recent_menu<recentfile_path>* recent_movies;
	recent_menu<recentfile_path>* recent_scripts;
	loadrom_menu* loadroms;
	file_download* download_in_progress;
private:
	void do_load_rom_image(core_type* t);
	void handle_menu_click_cancelable(wxCommandEvent& e);
	panel* gpanel;
	wxMenu* current_menu;
	wxMenuBar* menubar;
	wxStatusBar* statusbar;
	wxwin_status::panel* spanel;
	bool spanel_shown;
	wxwin_status* mwindow;
	wxFlexGridSizer* toplevel;
	std::map<int, wxMenuItem*> checkitems;
	std::stack<wxMenu*> upper;
	void* sysmenu;
	void* ahmenu;
	void* sounddev1;
	void* sounddev2;
	void* dmenu;
	struct dispatch_target<> corechange;
	struct dispatch_target<> titlechange;
	struct dispatch_target<> newcore;
	struct dispatch_target<bool> unmuted;
	struct dispatch_target<bool> modechange;
};

#endif
