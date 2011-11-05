#ifndef _wxwidgets_messages_window__hpp__included__
#define _wxwidgets_messages_window__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include "filenamebox.hpp"
#include "common.hpp"

class wx_messages_window : public wxFrame
{
public:
	wx_messages_window();
	~wx_messages_window();
	bool ShouldPreventAppExit() const;
	static wx_messages_window* ptr;
	void notify_message();
	void on_scroll_home(wxCommandEvent& e);
	void on_scroll_pageup(wxCommandEvent& e);
	void on_scroll_lineup(wxCommandEvent& e);
	void on_scroll_linedown(wxCommandEvent& e);
	void on_scroll_pagedown(wxCommandEvent& e);
	void on_scroll_end(wxCommandEvent& e);
	void on_execute(wxCommandEvent& e);
private:
	wxTextCtrl* command;
};

#endif
