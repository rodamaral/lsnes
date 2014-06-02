#ifndef _plat_wxwidgets__window_status__hpp__included__
#define _plat_wxwidgets__window_status__hpp__included__

#include <wx/string.h>
#include <wx/wx.h>

#include "platform/wxwidgets/textrender.hpp"

class wxwin_status : public wxFrame
{
public:
	class panel : public text_framebuffer_panel
	{
	public:
		panel(wxWindow* _parent, emulator_instance& _inst, wxWindow* tfocus, unsigned lines);
		//-1: memory watch only, 0: Both, 1: Status only.
		void set_watch_flag(int f) { watch_flag = f; }
	protected:
		void prepare_paint();
	private:
		emulator_instance& inst;
		text_framebuffer fb;
		int watch_flag;
		size_t previous_size;
	};
	wxwin_status(int flag, emulator_instance& _inst, const std::string& title);
	~wxwin_status();
	bool ShouldPreventAppExit() const;
	void notify_update() throw();
	emulator_instance& inst;
	panel* spanel;
};

#endif
