#ifndef _plat_wxwidgets__window_messages__hpp__included__
#define _plat_wxwidgets__window_messages__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>
#include "platform/wxwidgets/textrender.hpp"

class emulator_instance;

class wxwin_messages : public wxFrame
{
public:
	class panel : public text_framebuffer_panel
	{
	public:
		panel(wxwin_messages* _parent, emulator_instance& _inst, unsigned lines);
		void on_resize(wxSizeEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_menu(wxCommandEvent& e);
		virtual wxSize DoGetBestSize() const;
	protected:
		void prepare_paint();
	private:
		emulator_instance& inst;
		wxwin_messages* parent;
		size_t ilines;
		uint64_t line_clicked;
		uint64_t line_declicked;
		uint64_t line_current;
		size_t line_separation;
		bool mouse_held;
		int scroll_acc;
	};
	wxwin_messages(emulator_instance& _inst);
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
	emulator_instance& inst;
	wxComboBox* command;
	panel* mpanel;
};

#endif
