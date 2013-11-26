#ifndef _plat_wxwidgets__window_messages__hpp__included__
#define _plat_wxwidgets__window_messages__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>


class wxwin_messages : public wxFrame
{
public:
	class panel : public wxPanel
	{
	public:
		panel(wxwin_messages* _parent, unsigned lines);
		void on_paint(wxPaintEvent& e);
		void on_resize(wxSizeEvent& e);
		virtual wxSize DoGetBestSize() const;
	private:
		wxwin_messages* parent;
		size_t ilines;
		size_t line_separation;
	};
	wxwin_messages();
	~wxwin_messages();
	void notify_update() throw();
	bool ShouldPreventAppExit() const;
	void notify_message();
	void on_scroll_home(wxCommandEvent& e);
	void on_scroll_pageup(wxCommandEvent& e);
	void on_scroll_lineup(wxCommandEvent& e);
	void on_scroll_linedown(wxCommandEvent& e);
	void on_scroll_pagedown(wxCommandEvent& e);
	void on_scroll_end(wxCommandEvent& e);
	void on_execute(wxCommandEvent& e);
	void on_close(wxCloseEvent& e);
	void reshow();
private:
	wxComboBox* command;
	panel* mpanel;
};

#endif
