#ifndef _wxwidgets__window_romload__hpp__included__
#define _wxwidgets__window_romload__hpp__included__

#include <string>
#include <wx/wx.h>

class wxwindow_romload
{
public:
	wxwindow_romload(const std::string& path);
	bool show(wxWindow* parent);
	std::string get_core() { return core; }
	std::string get_type() { return type; }
	std::string get_filename() { return filename; }
private:
	std::string path;
	std::string core;
	std::string type;
	std::string filename;
};

#endif
