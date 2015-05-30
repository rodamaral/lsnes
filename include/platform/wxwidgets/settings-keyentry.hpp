#ifndef _platform__wxwidgets__settings_keyentry__hpp__included__
#define _platform__wxwidgets__settings_keyentry__hpp__included__

#include <string>
#include "platform/wxwidgets/platform.hpp"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <vector>
#include <string>
#include <list>

class press_button_dialog: public wxDialog
{
public:
	press_button_dialog(wxWindow* parent, emulator_instance& _inst, const text& title, bool axis);
	text getkey() { return key; }
	void on_mouse(wxMouseEvent& e);
	void on_keyboard_up(wxKeyEvent& e);
	void on_keyboard_down(wxKeyEvent& e);
	void dismiss_with(const text& k);
private:
	bool handle_mousebtn(wxMouseEvent& e, bool(wxMouseEvent::*down)()const, bool(wxMouseEvent::*up)()const,
		const text& k, int flag);
	emulator_instance& inst;
	text key;
	int mouseflag;
	int lastkbdkey;
	bool axis;
};

class key_entry_dialog : public wxDialog
{
public:
	key_entry_dialog(wxWindow* parent, emulator_instance& _inst, const text& title,
		const text& spec, bool clearable);
	void on_change_setting(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_clear(wxCommandEvent& e);
	void on_pressbutton(wxCommandEvent& e);
	void on_classchange(wxCommandEvent& e);
	text getkey();
private:
	struct keyentry_mod_data
	{
		wxComboBox* pressed;
		unsigned tmpflags;
	};
	void set_mask(const text& mod);
	void set_mod(const text& mod);
	void set_set(const text& mset,
		void (key_entry_dialog::*fn)(const text& mod));
	void load_spec(const text& spec);
	void set_class(const text& _class);
	emulator_instance& inst;
	std::map<text, keyentry_mod_data> modifiers;
	std::map<text, std::set<text>> classes;
	text wtitle;
	text currentclass;
	wxFlexGridSizer* top_s;
	wxFlexGridSizer* t_s;
	wxComboBox* mainclass;
	wxComboBox* mainkey;
	wxButton* press;
	wxButton* ok;
	wxButton* cancel;
	wxButton* clear;
	bool cleared;
};

text clean_keystring(const text& in);


#endif
