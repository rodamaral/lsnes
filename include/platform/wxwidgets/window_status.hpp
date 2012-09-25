#ifndef _plat_wxwidgets__window_status__hpp__included__
#define _plat_wxwidgets__window_status__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>

#include "platform/wxwidgets/textrender.hpp"

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
		//-1: memory watch only, 0: Both, 1: Status only.
		void set_watch_flag(int f) { watch_flag = f; }
	private:
		text_framebuffer statusvars;
		text_framebuffer memorywatches;
		int watch_flag;
	};
	wxwin_status(int flag, const std::string& title);
	~wxwin_status();
	bool ShouldPreventAppExit() const;
	void notify_update() throw();
	panel* spanel;
};

#endif
