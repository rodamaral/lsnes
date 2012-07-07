#ifndef _plat_wxwidgets__window_status__hpp__included__
#define _plat_wxwidgets__window_status__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>

class wxwin_status : public wxFrame
{
public:
	class panel : public wxPanel
	{
	public:
		panel(wxWindow* _parent, wxWindow* tfocus, unsigned lines);
		bool AcceptsFocus () const;
		void on_focus(wxFocusEvent& e);
		void on_paint(wxPaintEvent& e);
		bool dirty;
		wxWindow* parent;
		wxWindow* tfocuswin;
	};
	wxwin_status();
	~wxwin_status();
	bool ShouldPreventAppExit() const;
	void notify_update() throw();
	panel* spanel;
};

#endif
