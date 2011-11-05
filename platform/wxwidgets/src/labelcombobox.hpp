#ifndef _labelcombobox__hpp__included__
#define _labelcombobox__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/string.h>
#include "filenamebox.hpp"
#include "common.hpp"
#include <string>

class labeledcombobox
{
public:
	labeledcombobox(wxSizer* sizer, wxWindow* parent, const std::string& label, wxString* choices,
		size_t choice_count, size_t defaultidx, bool start_enabled, wxEvtHandler* dispatch_to,
		wxObjectEventFunction on_fn_change);
	std::string get_choice();
	void enable();
	void disable(const wxString& choice);
	void disable();
private:
	wxStaticText* label;
	wxComboBox* combo;
	wxString remembered;
	bool enabled;
	bool forced;
};

#endif
