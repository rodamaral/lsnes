#ifndef _wxwidgets__window_romload__hpp__included__
#define _wxwidgets__window_romload__hpp__included__

#include <string>
#include <wx/wx.h>

class wxwindow_romload
{
public:
	wxwindow_romload(const text& path);
	bool show(wxWindow* parent);
	text get_core() { return core; }
	text get_type() { return type; }
	text get_filename() { return filename; }
private:
	text path;
	text core;
	text type;
	text filename;
};

#endif
