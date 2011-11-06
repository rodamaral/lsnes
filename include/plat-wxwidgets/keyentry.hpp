#ifndef _wxwidgets_keyentry__hpp__included__
#define _wxwidgets_keyentry__hpp__included__

#include "core/window.hpp"

#include "plat-wxwidgets/labelcombobox.hpp"

#include <vector>
#include <string>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

struct keyentry_mod_data
{
	wxCheckBox* pressed;
	wxCheckBox* unmasked;
	unsigned tmpflags;
};

class wx_key_entry : public wxDialog
{
public:
	wx_key_entry(wxWindow* parent);
	void on_change_setting(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	std::string getkey();
private:
	std::map<std::string, keyentry_mod_data> modifiers;
	labeledcombobox* mainkey;
	wxButton* ok;
	wxButton* cancel;
};

#endif
