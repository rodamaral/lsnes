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
		panel(wxwin_status* _parent, unsigned lines);
		void on_paint(wxPaintEvent& e);
		bool dirty;
		wxwin_status* parent;
	};
	wxwin_status();
	~wxwin_status();
	bool ShouldPreventAppExit() const;
	void notify_update() throw();
	panel* spanel;
};

#endif
