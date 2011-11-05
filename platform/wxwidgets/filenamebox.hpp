#ifndef _wxwidgets_filenamebox__hpp__included__
#define _wxwidgets_filenamebox__hpp__included__

//Permanent label.
#define FNBF_PL 1
//Start disabled.
#define FNBF_SD 2
//No notification on filename change.
#define FNBF_NN 4
//Insert as one item.
#define FNBF_OI 8
//Peek inside .zip files.
#define FNBF_PZ 16

#include <wx/wx.h>
#include <wx/event.h>

class filenamebox : public wxEvtHandler
{
public:
	filenamebox(wxSizer* sizer, wxWindow* parent, const std::string& initial_label, int flags,
		wxEvtHandler* dispatch_to, wxObjectEventFunction on_fn_change);
	~filenamebox();
	void disable();
	void enable();
	void change_label(const std::string& new_label);
	void enable(const std::string& new_label);
	bool is_enabled();
	std::string get_file();
	void on_file_select(wxCommandEvent& e);
	bool is_nonblank();
	bool is_nonblank_or_disabled();
	void clear();
private:
	int given_flags;
	std::string last_label;
	bool enabled;
	wxStaticText* label;
	wxTextCtrl* filename;
	wxButton* file_select;
	wxWindow* pwindow;
};

#endif