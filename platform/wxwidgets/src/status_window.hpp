#ifndef _wxwidgets_status_window__hpp__included__
#define _wxwidgets_status_window__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include "common.hpp"
#include <map>
#include <string>

class wx_status_window : public wxFrame
{
public:
	wx_status_window();
	~wx_status_window();
	bool ShouldPreventAppExit() const;
	static wx_status_window* ptr;
	void notify_status_change();
};

void repaint_status_window();

#endif
