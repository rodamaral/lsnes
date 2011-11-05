#ifndef _wxwidgets_axeseditor__hpp__included__
#define _wxwidgets_axeseditor__hpp__included__

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <vector>
#include <string>

class wx_axes_editor_axis
{
public:
	wx_axes_editor_axis(wxSizer* sizer, wxWindow* window, const std::string& name);
	bool is_ok();
	void apply();
private:
	std::string a_name;
	wxComboBox* a_type;
	wxTextCtrl* a_low;
	wxTextCtrl* a_mid;
	wxTextCtrl* a_high;
	wxTextCtrl* a_tolerance;
};

class wx_axes_editor : public wxDialog
{
public:
	wx_axes_editor(wxWindow* parent);
	~wx_axes_editor();
	bool ShouldPreventAppExit() const;
	void on_value_change(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
private:
	std::vector<wx_axes_editor_axis*> axes;
	wxButton* okbutton;
	wxButton* cancel;
};

#endif
