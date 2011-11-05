#ifndef _wxwidgets_authorseditor__hpp__included__
#define _wxwidgets_authorseditor__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

class wx_authors_editor : public wxDialog
{
public:
	wx_authors_editor(wxWindow* parent);
	bool ShouldPreventAppExit() const;
	void on_authors_change(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
private:
	wxTextCtrl* projectname;
	wxTextCtrl* authors;
	wxButton* okbutton;
	wxButton* cancel;
};

#endif
