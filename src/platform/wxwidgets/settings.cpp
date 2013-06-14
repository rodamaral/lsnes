#include "platform/wxwidgets/platform.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/mainloop.hpp"
#include "core/moviedata.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <vector>
#include <string>

#include "library/string.hpp"
#include <boost/lexical_cast.hpp>
#include <sstream>

extern "C"
{
#ifndef UINT64_C
#define UINT64_C(val) val##ULL
#endif
#include <libswscale/swscale.h>
}

#define AMODE_DISABLED "Disabled"
#define AMODE_AXIS_PAIR "Axis"
#define AMODE_AXIS_PAIR_INVERSE "Axis (inverted)"
#define AMODE_PRESSURE_M0 "Pressure - to 0"
#define AMODE_PRESSURE_MP "Pressure - to +"
#define AMODE_PRESSURE_0M "Pressure 0 to -"
#define AMODE_PRESSURE_0P "Pressure 0 to +"
#define AMODE_PRESSURE_PM "Pressure + to -"
#define AMODE_PRESSURE_P0 "Pressure + to 0"

const char* scalealgo_choices[] = {"Fast Bilinear", "Bilinear", "Bicubic", "Experimential", "Point", "Area",
	"Bicubic-Linear", "Gauss", "Sinc", "Lanczos", "Spline"};

namespace
{
	class wxdialog_pressbutton;
	volatile bool keygrab_active = false;
	std::string pkey;
	wxdialog_pressbutton* presser = NULL;

	std::string get_title(int mode)
	{
		switch(mode) {
		case 0:
			return "lsnes: Configure emulator";
		case 1:
			return "lsnes: Configure hotkeys";
		case 2:
			return "lsnes: Configure controllers";
		}
		return "";
	}

	void report_grab_key(const std::string& name);

	class keygrabber : public keyboard_event_listener
	{
	public:
		keygrabber() { keygrab_active = false; }
		void on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event)
		{
			if(!keygrab_active)
				return;
			uint32_t dev = event.get_change_mask();
			auto subkeys = key.get_subkeys();
			for(unsigned i = 0; i < 16 && i < subkeys.size(); i++) {
				std::string pname = key.get_name() + subkeys[i];
				if(((dev >> (2 * i)) & 3) == 3)
					pkey = pname;
				if(((dev >> (2 * i)) & 3) == 2) {
					if(pkey == pname) {
						keygrab_active = false;
						std::string tmp = pkey;
						runuifun([tmp]() { report_grab_key(tmp); });
					} else
						pkey = "";
				}
			}
		}
	} keygrabber;

	std::string clean_keystring(const std::string& in)
	{
		regex_results tmp = regex("(.*)/(.*)\\|(.*)", in);
		if(!tmp)
			return in;
		std::string mods = tmp[1];
		std::string mask = tmp[2];
		std::string key = tmp[3];
		std::set<std::string> _mods, _mask;
		std::string tmp2;
		while(mods != "") {
			extract_token(mods, tmp2, ",");
			_mods.insert(tmp2);
		}
		while(mask != "") {
			extract_token(mask, tmp2, ",");
			_mask.insert(tmp2);
		}
		for(auto i : _mods)
			if(!_mask.count(i))
				return in;
		std::string out;
		for(auto i : _mask)
			if(!_mods.count(i))
				out = out + "!" + i + "+";
			else
				out = out + i + "+";
		out = out + key;
		return out;
	}

/**
 * Extract token out of string.
 *
 * Parameter str: The original string and the rest of the string on return.
 * Parameter tok: The extracted token will be written here.
 * Parameter sep: The characters to split on (empty always extracts the rest).
 * Parameter seq: If true, skip whole sequence of token ending characters.
 * Returns: The character token splitting occured on (-1 if end of string, -2 if string is empty).
 */
int extract_token(std::string& str, std::string& tok, const char* sep, bool seq = false) throw(std::bad_alloc);

	class wxdialog_pressbutton : public wxDialog
	{
	public:
		wxdialog_pressbutton(wxWindow* parent, const std::string& title, bool axis);
		std::string getkey() { return key; }
		void on_mouse(wxMouseEvent& e);
		void on_keyboard_up(wxKeyEvent& e);
		void on_keyboard_down(wxKeyEvent& e);
		void dismiss_with(const std::string& k);
	private:
		bool handle_mousebtn(wxMouseEvent& e, bool(wxMouseEvent::*down)()const, bool(wxMouseEvent::*up)()const,
			const std::string& k, int flag);
		std::string key;
		int mouseflag;
		int lastkbdkey;
		bool axis;
	};

	void report_grab_key(const std::string& name)
	{
		presser->dismiss_with(name);
	}

	int vert_padding = 40;
	int horiz_padding = 60;

	wxdialog_pressbutton::wxdialog_pressbutton(wxWindow* parent, const std::string& title, bool _axis)
		: wxDialog(parent, wxID_ANY, towxstring(title))
	{
		axis = _axis;
		wxStaticText* t;
		wxBoxSizer* s2 = new wxBoxSizer(wxVERTICAL);
		wxPanel* p = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxWANTS_CHARS);
		s2->Add(p, 1, wxGROW);
		lastkbdkey = -1;
		mouseflag = 0;
		Centre();
		wxFlexGridSizer* s = new wxFlexGridSizer(3, 3, 0, 0);
		p->SetSizer(s);
		SetSizer(s2);
		s->Add(horiz_padding, vert_padding);
		s->Add(0, 0);
		s->Add(0, 0);
		s->Add(0, 0);
		s->Add(t = new wxStaticText(p, wxID_ANY, wxT("Press the key to assign"), wxDefaultPosition,
			wxSize(-1, -1), wxWANTS_CHARS), 1, wxGROW);
		s->Add(0, 0);
		s->Add(0, 0);
		s->Add(0, 0);
		s->Add(horiz_padding, vert_padding);
		p->SetFocus();
		p->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(wxdialog_pressbutton::on_keyboard_down), NULL, this);
		p->Connect(wxEVT_KEY_UP, wxKeyEventHandler(wxdialog_pressbutton::on_keyboard_up), NULL, this);
		p->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		p->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		p->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		p->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		p->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		p->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		t->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(wxdialog_pressbutton::on_keyboard_down), NULL, this);
		t->Connect(wxEVT_KEY_UP, wxKeyEventHandler(wxdialog_pressbutton::on_keyboard_up), NULL, this);
		t->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		t->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		t->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		t->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		t->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		t->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxdialog_pressbutton::on_mouse), NULL, this);
		presser = this;
		keygrab_active = true;
		s->SetSizeHints(this);
		Fit();
	}

	bool wxdialog_pressbutton::handle_mousebtn(wxMouseEvent& e, bool(wxMouseEvent::*down)()const,
		bool(wxMouseEvent::*up)()const, const std::string& k, int flag)
	{
		if((e.*down)())
			mouseflag = flag;
		if((e.*up)()) {
			if(mouseflag == flag) {
				dismiss_with(k);
				return true;
			} else
				mouseflag = 0;
		}
		return false;
	}

	void wxdialog_pressbutton::on_mouse(wxMouseEvent& e)
	{
		handle_mousebtn(e, &wxMouseEvent::LeftDown, &wxMouseEvent::LeftUp, "mouse_left", 1);
		handle_mousebtn(e, &wxMouseEvent::MiddleDown, &wxMouseEvent::MiddleUp, "mouse_center", 2);
		handle_mousebtn(e, &wxMouseEvent::RightDown, &wxMouseEvent::RightUp, "mouse_right", 3);
	}

	void wxdialog_pressbutton::on_keyboard_down(wxKeyEvent& e)
	{
		lastkbdkey = e.GetKeyCode();
		mouseflag = 0;
	}

	void wxdialog_pressbutton::on_keyboard_up(wxKeyEvent& e)
	{
		int kcode = e.GetKeyCode();
		if(lastkbdkey == kcode) {
			dismiss_with(map_keycode_to_key(kcode));
		} else {
			lastkbdkey = -1;
			mouseflag = 0;
		}
	}

	void wxdialog_pressbutton::dismiss_with(const std::string& _k)
	{
		std::string k = _k;
		if(k == "")
			return;
		if(axis) {
			//Check that k is a valid axis.
			try {
				//Remove the +/- postfix if any.
				if(k.length() > 1) {
					char lch = k[k.length() - 1];
					if(lch == '+' || lch == '-')
						k = k.substr(0, k.length() - 1);
				}
				keyboard_key& key = lsnes_kbd.lookup_key(k);
				if(key.get_type() != KBD_KEYTYPE_AXIS)
					return;
			} catch(...) {
				return;
			}
		}
		if(key == "") {
			keygrab_active = false;
			key = k;
			EndModal(wxID_OK);
		}
	}

	struct keyentry_mod_data
	{
		wxComboBox* pressed;
		unsigned tmpflags;
	};

	class wxdialog_keyentry : public wxDialog
	{
	public:
		wxdialog_keyentry(wxWindow* parent, const std::string& title, const std::string& spec,
			bool clearable);
		void on_change_setting(wxCommandEvent& e);
		void on_ok(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e);
		void on_clear(wxCommandEvent& e);
		void on_pressbutton(wxCommandEvent& e);
		void on_classchange(wxCommandEvent& e);
		std::string getkey();
	private:
		void set_mask(const std::string& mod);
		void set_mod(const std::string& mod);
		void set_set(const std::string& mset,
			void (wxdialog_keyentry::*fn)(const std::string& mod));
		void load_spec(const std::string& spec);
		void set_class(const std::string& _class);
		std::map<std::string, keyentry_mod_data> modifiers;
		std::map<std::string, std::set<std::string>> classes;
		std::string wtitle;
		std::string currentclass;
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

	wxdialog_keyentry::wxdialog_keyentry(wxWindow* parent, const std::string& title, const std::string& spec,
		bool clearable)
		: wxDialog(parent, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1))
	{
		wxString boxchoices[] = { wxT("Released"), wxT("Don't care"), wxT("Pressed") };
		std::vector<wxString> classeslist;
		wxString emptystring;
		std::list<keyboard_modifier*> mods;
		std::list<keyboard_key*> keys;

		wtitle = title;

		cleared = false;
		std::set<std::string> x;
		mods = lsnes_kbd.all_modifiers();
		keys = lsnes_kbd.all_keys();
		for(auto i : keys) {
			std::string kclass = i->get_class();
			if(!x.count(kclass))
				classeslist.push_back(towxstring(kclass));
			x.insert(kclass);
			for(auto k2 : i->get_subkeys())
				classes[kclass].insert(i->get_name() + k2);
		}

		Centre();
		top_s = new wxFlexGridSizer(3, 1, 0, 0);
		SetSizer(top_s);

		t_s = new wxFlexGridSizer(2, 3, 0, 0);
		wxFlexGridSizer* t2_s = new wxFlexGridSizer(mods.size(), 2, 0, 0);
		for(auto i2 : mods) {
			keyentry_mod_data m;
			std::string i = i2->get_name();
			t2_s->Add(new wxStaticText(this, wxID_ANY, towxstring(i)), 0, wxGROW);
			t2_s->Add(m.pressed = new wxComboBox(this, wxID_ANY, boxchoices[1], wxDefaultPosition, 
				wxDefaultSize, 3, boxchoices, wxCB_READONLY), 1, wxGROW);
			m.pressed->SetSelection(1);
			modifiers[i] = m;
			m.pressed->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
		}
		top_s->Add(t2_s);
		t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Key")), 0, wxGROW);
		t_s->Add(mainclass = new wxComboBox(this, wxID_ANY, classeslist[0], wxDefaultPosition, wxDefaultSize,
			classeslist.size(), &classeslist[0], wxCB_READONLY), 0, wxGROW);
		mainclass->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxdialog_keyentry::on_classchange), NULL, this);
		t_s->Add(mainkey = new wxComboBox(this, wxID_ANY, emptystring, wxDefaultPosition, wxDefaultSize,
			1, &emptystring, wxCB_READONLY), 1, wxGROW);
		mainkey->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
		top_s->Add(t_s);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->Add(press = new wxButton(this, wxID_OK, wxT("Prompt key")), 0, wxGROW);
		press->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_pressbutton), NULL, this);
		if(clearable)
			pbutton_s->Add(clear = new wxButton(this, wxID_OK, wxT("Clear")), 0, wxGROW);
		pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
		pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
		pbutton_s->AddStretchSpacer();
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_ok), NULL, this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_cancel), NULL, this);
		mainclass->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxdialog_keyentry::on_classchange), NULL, this);
		if(clearable)
			clear->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_clear), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		set_class(tostdstring(classeslist[0]));

		t_s->SetSizeHints(this);
		top_s->SetSizeHints(this);
		Fit();

		if(spec != "")
			load_spec(spec);
	}

#define TMPFLAG_UNMASKED 65
#define TMPFLAG_UNMASKED_LINK_CHILD 2
#define TMPFLAG_UNMASKED_LINK_PARENT 68
#define TMPFLAG_PRESSED 8
#define TMPFLAG_PRESSED_LINK_CHILD 16
#define TMPFLAG_PRESSED_LINK_PARENT 32

	void wxdialog_keyentry::set_mask(const std::string& mod)
	{
		if(!modifiers.count(mod))
			return;
		if(modifiers[mod].pressed->GetSelection() == 1) {
			wxCommandEvent e;
			modifiers[mod].pressed->SetSelection(0);
			on_change_setting(e);
		}
	}	

	void wxdialog_keyentry::set_mod(const std::string& mod)
	{
		if(!modifiers.count(mod))
			return;
		if(modifiers[mod].pressed->GetSelection() != 1) {
			wxCommandEvent e;
			modifiers[mod].pressed->SetSelection(2);
			on_change_setting(e);
		}
	}	

	void wxdialog_keyentry::set_set(const std::string& mset,
		void (wxdialog_keyentry::*fn)(const std::string& mod))
	{
		std::string rem = mset;
		while(rem != "") {
			size_t s = rem.find_first_of(",");
			if(s >= rem.length()) {
				(this->*fn)(rem);
				break;
			} else {
				(this->*fn)(rem.substr(0, s));
				rem = rem.substr(s + 1);
			}
		}
	}

	void wxdialog_keyentry::load_spec(const std::string& spec)
	{
		std::string _spec = spec;
		size_t s1 = _spec.find_first_of("/");
		size_t s2 = _spec.find_first_of("|");
		if(s1 >= _spec.length() || s2 >= _spec.length())
			return;		//Bad.
		std::string mod = _spec.substr(0, s1);
		std::string mask = _spec.substr(s1 + 1, s2 - s1 - 1);
		std::string key = _spec.substr(s2 + 1);
		set_set(mask, &wxdialog_keyentry::set_mask);
		set_set(mod, &wxdialog_keyentry::set_mod);
		std::string _class;
		for(auto i : classes)
			if(i.second.count(key))
				_class = i.first;
		if(_class != "") {
			set_class(_class);
			mainclass->SetValue(towxstring(_class));
			mainkey->SetValue(towxstring(key));
		}
		t_s->Layout();
		top_s->Layout();
		Fit();
	}

	void wxdialog_keyentry::on_change_setting(wxCommandEvent& e)
	{
	}

	void wxdialog_keyentry::on_pressbutton(wxCommandEvent& e)
	{
		wxdialog_pressbutton* p = new wxdialog_pressbutton(this, wtitle, false);
		p->ShowModal();
		std::string key = p->getkey();
		p->Destroy();
		std::string _class;
		for(auto i : classes)
			if(i.second.count(key))
				_class = i.first;
		if(_class == "")
			return;
		set_class(_class);
		mainclass->SetValue(towxstring(_class));
		mainkey->SetValue(towxstring(key));
	}

	void wxdialog_keyentry::on_ok(wxCommandEvent& e)
	{
		EndModal(wxID_OK);
	}

	void wxdialog_keyentry::on_clear(wxCommandEvent& e)
	{
		cleared = true;
		EndModal(wxID_OK);
	}

	void wxdialog_keyentry::on_cancel(wxCommandEvent& e)
	{
		EndModal(wxID_CANCEL);
	}

	void wxdialog_keyentry::set_class(const std::string& _class)
	{
		if(!mainkey)
			return;
		if(currentclass == _class)
			return;
		mainkey->Clear();
		for(auto i : classes[_class])
			mainkey->Append(towxstring(i));
		currentclass = _class;
		mainkey->SetSelection(0);
		t_s->Layout();
		top_s->Layout();
		Fit();
	}

	void wxdialog_keyentry::on_classchange(wxCommandEvent& e)
	{
		set_class(tostdstring(mainclass->GetValue()));
	}

	std::string wxdialog_keyentry::getkey()
	{
		if(cleared)
			return "";
		std::string x;
		bool f;
		f = true;
		for(auto i : modifiers) {
			if(i.second.pressed->GetSelection() == 2) {
				if(!f)
					x = x + ",";
				f = false;
				x = x + i.first;
			}
		}
		x = x + "/";
		f = true;
		for(auto i : modifiers) {
			if(i.second.pressed->GetSelection() != 1) {
				if(!f)
					x = x + ",";
				f = false;
				x = x + i.first;
			}
		}
		x = x + "|" + tostdstring(mainkey->GetValue());
		return x;
	}
}

class wxeditor_esettings_joystick_aconfig : public wxDialog
{
public:
	wxeditor_esettings_joystick_aconfig(wxWindow* parent, const std::string& _aname);
	~wxeditor_esettings_joystick_aconfig();
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
private:
	std::string aname;
	wxComboBox* type;
	wxTextCtrl* low;
	wxTextCtrl* mid;
	wxTextCtrl* hi;
	wxTextCtrl* tol;
	wxButton* okbutton;
	wxButton* cancel;
};

wxeditor_esettings_joystick_aconfig::wxeditor_esettings_joystick_aconfig(wxWindow* parent, const std::string& _aname)
	: wxDialog(parent, -1, towxstring("Configure axis " + _aname))
{
	wxString choices[9];
	int didx = 1;
	choices[0] = wxT(AMODE_DISABLED);
	choices[1] = wxT(AMODE_AXIS_PAIR);
	choices[2] = wxT(AMODE_AXIS_PAIR_INVERSE);
	choices[3] = wxT(AMODE_PRESSURE_M0);
	choices[4] = wxT(AMODE_PRESSURE_MP);
	choices[5] = wxT(AMODE_PRESSURE_0M);
	choices[6] = wxT(AMODE_PRESSURE_0P);
	choices[7] = wxT(AMODE_PRESSURE_PM);
	choices[8] = wxT(AMODE_PRESSURE_P0);

	aname = _aname;

	keyboard_axis_calibration c;
	keyboard_key* _k = lsnes_kbd.try_lookup_key(aname);
	keyboard_key_axis* k = NULL;
	if(_k)
		k = _k->cast_axis();
	if(k)
		c = k->get_calibration();

	if(c.mode == -1)	didx = 0;
	else if(c.mode == 1 && c.esign_a == -1 && c.esign_b == 1)	didx = 1;
	else if(c.mode == 1 && c.esign_a == 1 && c.esign_b == -1)	didx = 2;
	else if(c.mode == 0 && c.esign_a == -1 && c.esign_b == 0)	didx = 3;
	else if(c.mode == 0 && c.esign_a == -1 && c.esign_b == 1)	didx = 4;
	else if(c.mode == 0 && c.esign_a == 0 && c.esign_b == -1)	didx = 5;
	else if(c.mode == 0 && c.esign_a == 0 && c.esign_b == 1)	didx = 6;
	else if(c.mode == 0 && c.esign_a == 1 && c.esign_b == -1)	didx = 7;
	else if(c.mode == 0 && c.esign_a == 1 && c.esign_b == 0)	didx = 8;

	Centre();
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	wxFlexGridSizer* t_s = new wxFlexGridSizer(5, 2, 0, 0);
	t_s->Add(new wxStaticText(this, -1, wxT("Type: ")), 0, wxGROW);
	t_s->Add(type = new wxComboBox(this, wxID_ANY, choices[didx], wxDefaultPosition, wxDefaultSize,
		9, choices, wxCB_READONLY), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("Low: ")), 0, wxGROW);
	t_s->Add(low = new wxTextCtrl(this, -1, towxstring((stringfmt() << c.left).str()), wxDefaultPosition,
		wxSize(100, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("Middle: ")), 0, wxGROW);
	t_s->Add(mid = new wxTextCtrl(this, -1, towxstring((stringfmt() << c.center).str()),
		wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("High: ")), 0, wxGROW);
	t_s->Add(hi = new wxTextCtrl(this, -1, towxstring((stringfmt() << c.right).str()),
		wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("Tolerance: ")), 0, wxGROW);
	t_s->Add(tol = new wxTextCtrl(this, -1, towxstring((stringfmt() << c.nullwidth).str()),
		wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_joystick_aconfig::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_joystick_aconfig::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wxeditor_esettings_joystick_aconfig::~wxeditor_esettings_joystick_aconfig()
{
}

void wxeditor_esettings_joystick_aconfig::on_ok(wxCommandEvent& e)
{
	std::string _type = tostdstring(type->GetValue());
	std::string _low = tostdstring(low->GetValue());
	std::string _mid = tostdstring(mid->GetValue());
	std::string _hi = tostdstring(hi->GetValue());
	std::string _tol = tostdstring(tol->GetValue());
	keyboard_key_axis* k = NULL;
	keyboard_key* _k;
	
	_k = lsnes_kbd.try_lookup_key(aname);
	if(_k)
		k = _k->cast_axis();
	if(!k) {
		//Axis gone away?
		EndModal(wxID_OK);
		return;
	}

	keyboard_axis_calibration c;
	const char* bad_what = NULL;
	try {
		bad_what = "Bad axis type";
		if(_type == AMODE_AXIS_PAIR)			{c.mode = 1;  c.esign_a = -1;  c.esign_b = 1;  }
		else if(_type == AMODE_AXIS_PAIR_INVERSE)	{c.mode = 1;  c.esign_a = 1;   c.esign_b = -1; }
		else if(_type == AMODE_DISABLED)		{c.mode = -1; c.esign_a = -1;  c.esign_b = 1;  }
		else if(_type == AMODE_PRESSURE_0M)		{c.mode = 0;  c.esign_a = 0;   c.esign_b = -1; }
		else if(_type == AMODE_PRESSURE_0P)		{c.mode = 0;  c.esign_a = 0;   c.esign_b = 1;  }
		else if(_type == AMODE_PRESSURE_M0)		{c.mode = 0;  c.esign_a = -1;  c.esign_b = 0;  }
		else if(_type == AMODE_PRESSURE_MP)		{c.mode = 0;  c.esign_a = -1;  c.esign_b = 1;  }
		else if(_type == AMODE_PRESSURE_P0)		{c.mode = 0;  c.esign_a = 1;   c.esign_b = 0;  }
		else if(_type == AMODE_PRESSURE_PM)		{c.mode = 0;  c.esign_a = 1;   c.esign_b = -1; }
		else
			throw 42;
		bad_what = "Bad low calibration value (range is -32768 - 32767)";
		c.left = boost::lexical_cast<int32_t>(_low);
		bad_what = "Bad middle calibration value (range is -32768 - 32767)";
		c.center = boost::lexical_cast<int32_t>(_mid);
		bad_what = "Bad high calibration value (range is -32768 - 32767)";
		c.right = boost::lexical_cast<int32_t>(_hi);
		bad_what = "Bad tolerance (range is 0 - 1)";
		c.nullwidth = boost::lexical_cast<double>(_tol);
		if(c.nullwidth <= 0 || c.nullwidth >= 1)
			throw 42;
	} catch(...) {
		wxMessageBox(towxstring(bad_what), _T("Error"), wxICON_EXCLAMATION | wxOK);
		return;
	}
	k->set_calibration(c);
	EndModal(wxID_OK);
}

void wxeditor_esettings_joystick_aconfig::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

class wxeditor_esettings_joystick : public wxPanel
{
public:
	wxeditor_esettings_joystick(wxWindow* parent);
	~wxeditor_esettings_joystick();
	void on_configure(wxCommandEvent& e);
private:
	void refresh();
	wxSizer* jgrid;
	wxStaticText* no_joysticks;
	std::map<std::string, wxButton*> buttons;
	std::map<int, std::string> ids;
	int last_id;
};

namespace
{
	std::string formattype(const keyboard_axis_calibration& s)
	{
		if(s.mode == -1) return AMODE_DISABLED;
		else if(s.mode == 1 && s.esign_b == 1) return AMODE_AXIS_PAIR;
		else if(s.mode == 1 && s.esign_b == -1) return AMODE_AXIS_PAIR_INVERSE;
		else if(s.mode == 0 && s.esign_a == 0 && s.esign_b == -1) return AMODE_PRESSURE_0M;
		else if(s.mode == 0 && s.esign_a == 0 && s.esign_b == 1) return AMODE_PRESSURE_0P;
		else if(s.mode == 0 && s.esign_a == -1 && s.esign_b == 0) return AMODE_PRESSURE_M0;
		else if(s.mode == 0 && s.esign_a == -1 && s.esign_b == 1) return AMODE_PRESSURE_MP;
		else if(s.mode == 0 && s.esign_a == 1 && s.esign_b == 0) return AMODE_PRESSURE_P0;
		else if(s.mode == 0 && s.esign_a == 1 && s.esign_b == -1) return AMODE_PRESSURE_PM;
		else return "Unknown";
	}

	std::string formatsettings(const std::string& name, const keyboard_axis_calibration& s)
	{
		return (stringfmt() << name << ": " << formattype(s) << " low:" << s.left << " mid:"
			<< s.center << " high:" << s.right << " tolerance:" << s.nullwidth).str();
	}

	std::string getalgo(int flags)
	{
		for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			if(flags & (1 << i))
				return scalealgo_choices[i];
		return "unknown";
	}
}

wxeditor_esettings_joystick::wxeditor_esettings_joystick(wxWindow* parent)
	: wxPanel(parent, -1)
{
	last_id = wxID_HIGHEST + 1;
	no_joysticks = new wxStaticText(this, wxID_ANY, wxT("Sorry, no joysticks detected"));
	no_joysticks->SetMinSize(wxSize(400, -1));
	no_joysticks->Hide();
	SetSizer(jgrid = new wxFlexGridSizer(0, 1, 0, 0));
	refresh();
	jgrid->SetSizeHints(this);
	Fit();
}

wxeditor_esettings_joystick::~wxeditor_esettings_joystick()
{
}

void wxeditor_esettings_joystick::on_configure(wxCommandEvent& e)
{
	if(!ids.count(e.GetId()))
		return;
	wxDialog* d = new wxeditor_esettings_joystick_aconfig(this, ids[e.GetId()]);
	d->ShowModal();
	d->Destroy();
	refresh();
}

void wxeditor_esettings_joystick::refresh()
{
	//Collect the new settings.
	std::map<std::string, keyboard_axis_calibration> x;
	auto axisset = lsnes_kbd.all_keys();
	for(auto i : axisset) {
		keyboard_key_axis* j = i->cast_axis();
		if(!j)
			continue;
		x[i->get_name()] = j->get_calibration();
	}

	unsigned jcount = 0;
	for(auto i : x) {
		jcount++;
		if(buttons.count(i.first)) {
			//Okay, this already exists. Update.
			buttons[i.first]->SetLabel(towxstring(formatsettings(i.first, i.second)));
			if(!buttons[i.first]->IsShown()) {
				jgrid->Add(buttons[i.first], 1, wxGROW);
				buttons[i.first]->Show();
			}
		} else {
			//New button.
			ids[last_id] = i.first;
			buttons[i.first] = new wxButton(this, last_id++, towxstring(formatsettings(i.first,
				i.second)));
			buttons[i.first]->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(wxeditor_esettings_joystick::on_configure), NULL, this);
			jgrid->Add(buttons[i.first], 1, wxGROW);
		}
	}
	for(auto i : buttons) {
		if(!x.count(i.first)) {
			//Removed button.
			i.second->Hide();
			jgrid->Detach(i.second);
		}
	}
	if(jcount > 0) {
		jgrid->Detach(no_joysticks);
		no_joysticks->Hide();
	} else {
		no_joysticks->Show();
		jgrid->Add(no_joysticks);
	}
	jgrid->Layout();
	this->Refresh();
	Fit();
}

class wxeditor_esettings_settings : public wxPanel
{
public:
	wxeditor_esettings_settings(wxWindow* parent);
	~wxeditor_esettings_settings();
	void on_configure(wxCommandEvent& e);
	wxCheckBox* hflip;
	wxCheckBox* vflip;
	wxCheckBox* rotate;
private:
	void refresh();
	wxFlexGridSizer* top_s;
	wxStaticText* xscale;
	wxStaticText* yscale;
	wxStaticText* algo;
};

wxeditor_esettings_settings::wxeditor_esettings_settings(wxWindow* parent)
	: wxPanel(parent, -1)
{
	wxButton* tmp;
	top_s = new wxFlexGridSizer(8, 3, 0, 0);
	SetSizer(top_s);
	top_s->Add(new wxStaticText(this, -1, wxT("X scale factor: ")), 0, wxGROW);
	top_s->Add(xscale = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 6, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_settings::on_configure),
		NULL, this);
	top_s->Add(new wxStaticText(this, -1, wxT("Y scale factor: ")), 0, wxGROW);
	top_s->Add(yscale = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 7, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_settings::on_configure),
		NULL, this);
	top_s->Add(new wxStaticText(this, -1, wxT("Scaling type: ")), 0, wxGROW);
	top_s->Add(algo = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 8, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_settings::on_configure),
		NULL, this);
	top_s->Add(new wxStaticText(this, -1, wxT("Hflip: ")), 0, wxGROW);
	top_s->Add(hflip = new wxCheckBox(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(new wxStaticText(this, -1, wxT("")), 0, wxGROW);
	top_s->Add(new wxStaticText(this, -1, wxT("Vflip: ")), 0, wxGROW);
	top_s->Add(vflip = new wxCheckBox(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(new wxStaticText(this, -1, wxT("")), 0, wxGROW);
	top_s->Add(new wxStaticText(this, -1, wxT("Rotate: ")), 0, wxGROW);
	top_s->Add(rotate = new wxCheckBox(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(new wxStaticText(this, -1, wxT("")), 0, wxGROW);

	refresh();
	top_s->SetSizeHints(this);
	Fit();
}
wxeditor_esettings_settings::~wxeditor_esettings_settings()
{
}

void wxeditor_esettings_settings::on_configure(wxCommandEvent& e)
{
	std::vector<std::string> sa_choices;
	std::string v;
	int newflags = 1;
	for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			sa_choices.push_back(scalealgo_choices[i]);
	std::string name;
	if(e.GetId() <= wxID_HIGHEST || e.GetId() > wxID_HIGHEST + 10)
		return;
	std::string val;
	try {
		if(e.GetId() == wxID_HIGHEST + 6) {
			val = (stringfmt() << horizontal_scale_factor).str();
			val = pick_text(this, "Set X scaling factor", "Enter new horizontal scale factor (0.25-10):",
				val);
		} else if(e.GetId() == wxID_HIGHEST + 7) {
			val = (stringfmt() << horizontal_scale_factor).str();
			val = pick_text(this, "Set Y scaling factor", "Enter new vertical scale factor (0.25-10):",
				val);
		} else if(e.GetId() == wxID_HIGHEST + 8) {
			val = pick_among(this, "Select algorithm", "Select scaling algorithm", sa_choices);
		}
	} catch(...) {
		refresh();
		return;
	}
	std::string err;
	try {
		if(e.GetId() == wxID_HIGHEST + 6) {
			double x = parse_value<double>(val);
			if(x < 0.25 || x > 10)
				throw "Bad horizontal scaling factor (0.25-10)";
			horizontal_scale_factor = x;
		} else if(e.GetId() == wxID_HIGHEST + 7) {
			double x = parse_value<double>(val);
			if(x < 0.25 || x > 10)
				throw "Bad vertical scaling factor (0.25-10)";
			vertical_scale_factor = x;
		} else if(e.GetId() == wxID_HIGHEST + 8) {
			for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
				if(val == scalealgo_choices[i])
					newflags = 1 << i;
			scaling_flags = newflags;
		}
	} catch(std::exception& e) {
		wxMessageBox(towxstring(std::string("Invalid value: ") + e.what()), wxT("Can't change value"),
			wxICON_EXCLAMATION | wxOK);
	}
	refresh();
}

void wxeditor_esettings_settings::refresh()
{
	xscale->SetLabel(towxstring((stringfmt() << horizontal_scale_factor).str()));
	yscale->SetLabel(towxstring((stringfmt() << vertical_scale_factor).str()));
	algo->SetLabel(towxstring(getalgo(scaling_flags)));
	hflip->SetValue(hflip_enabled);
	vflip->SetValue(vflip_enabled);
	rotate->SetValue(rotate_enabled);
	top_s->Layout();
	Fit();
}

class wxeditor_esettings_hotkeys : public wxPanel
{
public:
	wxeditor_esettings_hotkeys(wxWindow* parent);
	~wxeditor_esettings_hotkeys();
	void on_primary(wxCommandEvent& e);
	void on_secondary(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
	void prepare_destroy();
	void call_refresh() { refresh(); }
private:
	bool destruction_underway;
	wxListBox* category;
	wxListBox* control;
	wxButton* pri_button;
	wxButton* sec_button;
	std::map<int, std::string> categories;
	std::map<std::pair<int, int>, std::string> itemlabels;
	std::map<std::pair<int, int>, std::string> items;
	std::map<std::string, inverse_bind*> realitems;
	void change_category(int cat);
	void refresh();
	std::pair<std::string, std::string> splitkeyname(const std::string& kn);
};

class wxeditor_esettings_aliases : public wxPanel
{
public:
	wxeditor_esettings_aliases(wxWindow* parent, wxeditor_esettings_hotkeys* _hotkeys);
	~wxeditor_esettings_aliases();
	void on_add(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
	void prepare_destroy();
private:
	bool destruction_underway;
	std::map<int, std::string> numbers;
	wxListBox* select;
	wxButton* editbutton;
	wxButton* deletebutton;
	wxeditor_esettings_hotkeys& hotkeys;
	void refresh();
	std::string selected();
};

wxeditor_esettings_aliases::wxeditor_esettings_aliases(wxWindow* parent, wxeditor_esettings_hotkeys* _hotkeys)
	: wxPanel(parent, -1), hotkeys(*_hotkeys)
{
	wxButton* tmp;

	destruction_underway = false;
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	top_s->Add(select = new wxListBox(this, wxID_ANY), 1, wxGROW);
	select->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(wxeditor_esettings_aliases::on_change),
		NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(tmp = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_aliases::on_add), NULL,
		this);
	pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
	editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_aliases::on_edit),
		NULL, this);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_aliases::on_delete), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	refresh();
	wxCommandEvent e;
	on_change(e);
	top_s->SetSizeHints(this);
	Fit();
}

wxeditor_esettings_aliases::~wxeditor_esettings_aliases()
{
}

void wxeditor_esettings_aliases::on_change(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	bool enable = (selected() != "");
	editbutton->Enable(enable);
	deletebutton->Enable(enable);
}

void wxeditor_esettings_aliases::on_add(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	try {
		std::string name = pick_text(this, "Enter alias name", "Enter name for the new alias:");
		if(!lsnes_cmd.valid_alias_name(name)) {
			show_message_ok(this, "Error", "Not a valid alias name: " + name, wxICON_EXCLAMATION);
			throw canceled_exception();
		}
		std::string old_alias_value = lsnes_cmd.get_alias_for(name);
		std::string newcmd = pick_text(this, "Edit alias", "Enter new commands for '" + name + "':",
			old_alias_value, true);
		lsnes_cmd.set_alias_for(name, newcmd);
		refresh_alias_binds();
		hotkeys.call_refresh();
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_aliases::on_edit(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try {
		std::string old_alias_value = lsnes_cmd.get_alias_for(name);
		std::string newcmd = pick_text(this, "Edit alias", "Enter new commands for '" + name + "':",
			old_alias_value, true);
		lsnes_cmd.set_alias_for(name, newcmd);
		refresh_alias_binds();
		hotkeys.call_refresh();
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_aliases::on_delete(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	lsnes_cmd.set_alias_for(name, "");
	refresh_alias_binds();
	hotkeys.call_refresh();
	refresh();
}

void wxeditor_esettings_aliases::refresh()
{
	if(destruction_underway)
		return;
	int n = select->GetSelection();
	std::set<std::string> bind;
	std::vector<wxString> choices;
	bind = lsnes_cmd.get_aliases();
	for(auto i : bind) {
		numbers[choices.size()] = i;
		choices.push_back(towxstring(i));
	}
	select->Set(choices.size(), &choices[0]);
	if(n == wxNOT_FOUND && select->GetCount())
		select->SetSelection(0);
	else if(n >= (int)select->GetCount())
		select->SetSelection(select->GetCount() ? (select->GetCount() - 1) : wxNOT_FOUND);
	else
		select->SetSelection(n);
	wxCommandEvent e;
	on_change(e);
	select->Refresh();
}

std::string wxeditor_esettings_aliases::selected()
{
	if(destruction_underway)
		return "";
	int x = select->GetSelection();
	if(numbers.count(x))
		return numbers[x];
	else
		return "";
}

void wxeditor_esettings_aliases::prepare_destroy()
{
	destruction_underway = true;
}

wxeditor_esettings_hotkeys::wxeditor_esettings_hotkeys(wxWindow* parent)
	: wxPanel(parent, -1)
{
	destruction_underway = false;
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);
	wxString empty[1];

	top_s->Add(category = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, empty), 1, wxGROW);
	top_s->Add(control = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, empty), 1, wxGROW);
	category->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(wxeditor_esettings_hotkeys::on_change),
		NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(pri_button = new wxButton(this, wxID_ANY, wxT("Change primary")), 0, wxGROW);
	pri_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_hotkeys::on_primary), NULL, this);
	pbutton_s->Add(sec_button = new wxButton(this, wxID_ANY, wxT("Change secondary")), 0, wxGROW);
	sec_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_hotkeys::on_secondary), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	refresh();
	top_s->SetSizeHints(this);
	Fit();
}

void wxeditor_esettings_hotkeys::prepare_destroy()
{
	destruction_underway = true;
}

std::pair<std::string, std::string> wxeditor_esettings_hotkeys::splitkeyname(const std::string& kn)
{
	std::string tmp = kn;
	size_t split = 0;
	for(size_t itr = 0; itr < tmp.length() - 2 && itr < tmp.length(); itr++) {
		unsigned char ch1 = tmp[itr];
		unsigned char ch2 = tmp[itr + 1];
		unsigned char ch3 = tmp[itr + 2];
		if(ch1 == 0xE2 && ch2 == 0x80 && ch3 == 0xA3)
			split = itr;
	}
	if(split)
		return std::make_pair(tmp.substr(0, split), tmp.substr(split + 3));
	else
		return std::make_pair("(Uncategorized)", tmp);
}

void wxeditor_esettings_hotkeys::on_change(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	int c = category->GetSelection();
	if(c == wxNOT_FOUND) {
		category->SetSelection(0);
		change_category(0);
	} else
		change_category(c);
}

void wxeditor_esettings_hotkeys::change_category(int cat)
{
	if(destruction_underway)
		return;
	std::map<int, std::string> n;
	for(auto i : itemlabels)
		if(i.first.first == cat)
			n[i.first.second] = i.second;
	
	for(size_t i = 0; i < control->GetCount(); i++)
		if(n.count(i))
			control->SetString(i, towxstring(n[i]));
		else
			control->Delete(i--);
	for(auto i : n)
		if(i.first >= (int)control->GetCount())
			control->Append(towxstring(n[i.first]));
	if(control->GetSelection() == wxNOT_FOUND)
		control->SetSelection(0);
}

wxeditor_esettings_hotkeys::~wxeditor_esettings_hotkeys()
{
}

void wxeditor_esettings_hotkeys::on_primary(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
	if(name == "") {
		refresh();
		return;
	}
	try {
		inverse_bind* ik = realitems[name];
		if(!ik) {
			refresh();
			return;
		}
		std::string key = ik->get(true);
		wxdialog_keyentry* d = new wxdialog_keyentry(this, "Specify key for " + name, key, true);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		key = d->getkey();
		d->Destroy();
		if(key != "")
			ik->set(key, true);
		else
			ik->clear(true);
		
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_hotkeys::on_secondary(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
	if(name == "") {
		refresh();
		return;
	}
	try {
		inverse_bind* ik = realitems[name];
		if(!ik) {
			refresh();
			return;
		}
		std::string key = ik->get(false);
		wxdialog_keyentry* d = new wxdialog_keyentry(this, "Specify key for " + name, key, true);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		key = d->getkey();
		d->Destroy();
		if(key != "")
			ik->set(key, false);
		else
			ik->clear(false);
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_hotkeys::refresh()
{
	if(destruction_underway)
		return;
	std::map<inverse_bind*, std::pair<key_specifier, key_specifier>> data;
	std::map<std::string, int> cat_set;
	std::map<std::string, int> cat_assign;
	realitems.clear();
	itemlabels.clear();
	auto x = lsnes_mapper.get_inverses();
	for(auto y : x) {
		realitems[y->getname()] = y;
		data[y] = std::make_pair(y->get(true), y->get(false));
	}

	int cidx = 0;
	for(auto i : realitems) {
		std::pair<std::string, std::string> j = splitkeyname(i.first);
		if(!cat_set.count(j.first)) {
			categories[cidx] = j.first;
			cat_assign[j.first] = 0;
			cat_set[j.first] = cidx++;
		}
		items[std::make_pair(cat_set[j.first], cat_assign[j.first])] = i.first;
		std::string text = j.second;
		if(!data[i.second].first)
			text = text + " (not set)";
		else if(!data[i.second].second)
			text = text + " (" + clean_keystring(data[i.second].first) + ")";
		else
			text = text + " (" + clean_keystring(data[i.second].first) + " or " +
				clean_keystring(data[i.second].second) + ")";
		itemlabels[std::make_pair(cat_set[j.first], cat_assign[j.first])] = text;
		cat_assign[j.first]++;
	}

	for(size_t i = 0; i < category->GetCount(); i++)
		if(categories.count(i))
			category->SetString(i, towxstring(categories[i]));
		else
			category->Delete(i--);
	for(auto i : categories)
		if(i.first >= (int)category->GetCount())
			category->Append(towxstring(categories[i.first]));
	if(category->GetSelection() == wxNOT_FOUND)
		category->SetSelection(0);
	change_category(category->GetSelection());
}

class wxeditor_esettings_controllers : public wxPanel
{
public:
	wxeditor_esettings_controllers(wxWindow* parent);
	~wxeditor_esettings_controllers();
	void on_setkey(wxCommandEvent& e);
	void on_clearkey(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
	void prepare_destroy();
private:
	bool destruction_underway;
	wxListBox* category;
	wxListBox* control;
	wxButton* set_button;
	wxButton* clear_button;
	std::map<int, std::string> categories;
	std::map<std::pair<int, int>, std::string> itemlabels;
	std::map<std::pair<int, int>, std::string> items;
	std::map<std::string, controller_key*> realitems;
	void change_category(int cat);
	void refresh();
	std::pair<std::string, std::string> splitkeyname(const std::string& kn);
};

wxeditor_esettings_controllers::wxeditor_esettings_controllers(wxWindow* parent)
	: wxPanel(parent, -1)
{
	destruction_underway = false;
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);
	wxString empty[1];

	top_s->Add(category = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, empty), 1, wxGROW);
	top_s->Add(control = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, empty), 1, wxGROW);
	category->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_esettings_controllers::on_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(set_button = new wxButton(this, wxID_ANY, wxT("Change")), 0, wxGROW);
	set_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_controllers::on_setkey), NULL, this);
	pbutton_s->Add(clear_button = new wxButton(this, wxID_ANY, wxT("Clear")), 0, wxGROW);
	clear_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_controllers::on_clearkey), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	refresh();
	top_s->SetSizeHints(this);
	Fit();
}

void wxeditor_esettings_controllers::prepare_destroy()
{
	destruction_underway = true;
}

std::pair<std::string, std::string> wxeditor_esettings_controllers::splitkeyname(const std::string& kn)
{
	std::string tmp = kn;
	size_t split = 0;
	for(size_t itr = 0; itr < tmp.length() - 2 && itr < tmp.length(); itr++) {
		unsigned char ch1 = tmp[itr];
		unsigned char ch2 = tmp[itr + 1];
		unsigned char ch3 = tmp[itr + 2];
		if(ch1 == 0xE2 && ch2 == 0x80 && ch3 == 0xA3)
			split = itr;
	}
	if(split)
		return std::make_pair(tmp.substr(0, split), tmp.substr(split + 3));
	else
		return std::make_pair("(Uncategorized)", tmp);
}

void wxeditor_esettings_controllers::on_change(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	int c = category->GetSelection();
	if(c == wxNOT_FOUND) {
		category->SetSelection(0);
		change_category(0);
	} else
		change_category(c);
}

void wxeditor_esettings_controllers::change_category(int cat)
{
	if(destruction_underway)
		return;
	std::map<int, std::string> n;
	for(auto i : itemlabels)
		if(i.first.first == cat)
			n[i.first.second] = i.second;
	
	for(size_t i = 0; i < control->GetCount(); i++)
		if(n.count(i))
			control->SetString(i, towxstring(n[i]));
		else
			control->Delete(i--);
	for(auto i : n)
		if(i.first >= (int)control->GetCount())
			control->Append(towxstring(n[i.first]));
	if(control->GetSelection() == wxNOT_FOUND && !control->IsEmpty())
		control->SetSelection(0);
}

wxeditor_esettings_controllers::~wxeditor_esettings_controllers()
{
}

void wxeditor_esettings_controllers::on_setkey(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
	if(name == "") {
		refresh();
		return;
	}
	try {
		controller_key* ik = realitems[name];
		if(!ik) {
			refresh();
			return;
		}
		bool axis = ik->is_axis();
		std::string wtitle = (axis ? "Specify axis for " : "Specify key for ") + name;
		wxdialog_pressbutton* p = new wxdialog_pressbutton(this, wtitle, axis);
		p->ShowModal();
		std::string key = p->getkey();
		p->Destroy();
		ik->set(key);
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_controllers::on_clearkey(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
	if(name == "") {
		refresh();
		return;
	}
	try {
		controller_key* ik = realitems[name];
		if(ik)
			ik->set(NULL, 0);
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_controllers::refresh()
{
	if(destruction_underway)
		return;
	std::map<controller_key*, std::string> data;
	std::map<std::string, int> cat_set;
	std::map<std::string, int> cat_assign;
	realitems.clear();
	auto x = lsnes_mapper.get_controller_keys();
	for(auto y : x) {
		realitems[y->get_name()] = y;
		data[y] = y->get_string();
	}

	int cidx = 0;
	for(auto i : realitems) {
		std::pair<std::string, std::string> j = splitkeyname(i.first);
		if(!cat_set.count(j.first)) {
			categories[cidx] = j.first;
			cat_assign[j.first] = 0;
			cat_set[j.first] = cidx++;
		}
		items[std::make_pair(cat_set[j.first], cat_assign[j.first])] = i.first;
		std::string text = j.second;
		if(data[i.second] == "")
			text = text + " (not set)";
		else
			text = text + " (" + clean_keystring(data[i.second]) + ")";
		itemlabels[std::make_pair(cat_set[j.first], cat_assign[j.first])] = text;
		cat_assign[j.first]++;
	}

	for(size_t i = 0; i < category->GetCount(); i++)
		if(categories.count(i))
			category->SetString(i, towxstring(categories[i]));
		else
			category->Delete(i--);
	for(auto i : categories)
		if(i.first >= (int)category->GetCount())
			category->Append(towxstring(categories[i.first]));
	if(category->GetSelection() == wxNOT_FOUND && !category->IsEmpty())
		category->SetSelection(0);
	change_category(category->GetSelection());
}

class wxeditor_esettings_bindings : public wxPanel
{
public:
	wxeditor_esettings_bindings(wxWindow* parent);
	~wxeditor_esettings_bindings();
	void on_add(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
	void prepare_destroy();
private:
	bool destruction_underway;
	std::map<int, std::string> numbers;
	wxListBox* select;
	void refresh();
	std::set<std::string> settings;
	std::map<std::string, std::string> values;
	std::map<int, std::string> selections;
	std::string selected();
	wxButton* editbutton;
	wxButton* deletebutton;
	wxListBox* _settings;
};

wxeditor_esettings_bindings::wxeditor_esettings_bindings(wxWindow* parent)
	: wxPanel(parent, -1)
{
	destruction_underway = false;
	wxButton* tmp;

	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	top_s->Add(select = new wxListBox(this, wxID_ANY), 1, wxGROW);
	select->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(wxeditor_esettings_bindings::on_change),
		NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(tmp = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_bindings::on_add), NULL,
		this);
	pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
	editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_bindings::on_edit),
		NULL, this);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_bindings::on_delete), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	refresh();
	wxCommandEvent e;
	on_change(e);
	top_s->SetSizeHints(this);
	Fit();
}

wxeditor_esettings_bindings::~wxeditor_esettings_bindings()
{
}

void wxeditor_esettings_bindings::prepare_destroy()
{
	destruction_underway = true;
}

void wxeditor_esettings_bindings::on_change(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	bool enable = (selected() != "");
	editbutton->Enable(enable);
	deletebutton->Enable(enable);
}

void wxeditor_esettings_bindings::on_add(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	try {
		std::string name;
		wxdialog_keyentry* d = new wxdialog_keyentry(this, "Specify new key", "", false);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			throw 42;
		}
		name = d->getkey();
		d->Destroy();

		std::string newcommand = pick_text(this, "New binding", "Enter command for binding:", "");
		try {
			lsnes_mapper.set(name, newcommand);
		} catch(std::exception& e) {
			wxMessageBox(wxT("Error"), towxstring(std::string("Can't bind key: ") + e.what()),
				wxICON_EXCLAMATION);
		}
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_bindings::on_edit(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try {
		std::string old_command_value = lsnes_mapper.get(name);
		std::string newcommand = pick_text(this, "Edit binding", "Enter new command for binding:",
			old_command_value);
		try {
			lsnes_mapper.set(name, newcommand);
		} catch(std::exception& e) {
			wxMessageBox(wxT("Error"), towxstring(std::string("Can't bind key: ") + e.what()),
				wxICON_EXCLAMATION);
		}
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_bindings::on_delete(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try { lsnes_mapper.set(name, ""); } catch(...) {}
	refresh();
}

void wxeditor_esettings_bindings::refresh()
{
	if(destruction_underway)
		return;
	int n = select->GetSelection();
	std::map<std::string, std::string> bind;
	std::vector<wxString> choices;
	std::list<key_specifier> a = lsnes_mapper.get_bindings();
	for(auto i : a)
		bind[i] = lsnes_mapper.get(i);
	for(auto i : bind) {
		numbers[choices.size()] = i.first;
		choices.push_back(towxstring(clean_keystring(i.first) + " (" + i.second + ")"));
	}
	select->Set(choices.size(), &choices[0]);
	if(n == wxNOT_FOUND && select->GetCount())
		select->SetSelection(0);
	else if(n >= (int)select->GetCount())
		select->SetSelection(select->GetCount() ? (int)(select->GetCount() - 1) : wxNOT_FOUND);
	else
		select->SetSelection(n);
	wxCommandEvent e;
	on_change(e);
	select->Refresh();
}

std::string wxeditor_esettings_bindings::selected()
{
	if(destruction_underway)
		return "";
	int x = select->GetSelection();
	if(numbers.count(x))
		return numbers[x];
	else
		return "";
}

class wxeditor_esettings_advanced : public wxPanel
{
public:
	wxeditor_esettings_advanced(wxWindow* parent);
	~wxeditor_esettings_advanced();
	void on_change(wxCommandEvent& e);
	void on_selchange(wxCommandEvent& e);
	void on_setting_change(const setting_var_base& val);
	void _refresh();
	void prepare_destroy();
	struct listener : public setting_var_listener
	{
		listener(setting_var_group& group, wxeditor_esettings_advanced& _obj)
			: grp(group), obj(_obj)
		{
			group.add_listener(*this);
		}
		~listener() throw()
		{
			grp.remove_listener(*this);
		}
		void on_setting_change(setting_var_group& grp, const setting_var_base& val)
		{
			obj.on_setting_change(val);
		}
		wxeditor_esettings_advanced& obj;
		setting_var_group& grp;
	};
private:
	listener _listener;
	bool destruction_underway;
	void refresh();
	std::set<std::string> settings;
	std::map<std::string, std::string> values;
	std::map<std::string, std::string> names;
	std::map<int, std::string> selections;
	std::string selected();
	wxButton* changebutton;
	wxListBox* _settings;
};

wxeditor_esettings_advanced::wxeditor_esettings_advanced(wxWindow* parent)
	: wxPanel(parent, -1), _listener(lsnes_vset, *this)
{
	destruction_underway = false;

	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	top_s->Add(_settings = new wxListBox(this, wxID_ANY), 1, wxGROW);
	_settings->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_esettings_advanced::on_selchange), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(changebutton = new wxButton(this, wxID_ANY, wxT("Change")), 0, wxGROW);
	changebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_advanced::on_change), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	refresh();
	wxCommandEvent e;
	on_selchange(e);
	top_s->SetSizeHints(this);
	Fit();
}

void wxeditor_esettings_advanced::prepare_destroy()
{
	destruction_underway = true;
}

wxeditor_esettings_advanced::~wxeditor_esettings_advanced()
{
}

namespace
{
	std::string change_value_of_boolean(const std::string& name, const setting_var_description& desc,
		const std::string& current)
	{
		return string_to_bool(current) ? "0" : "1";
	}

	std::string change_value_of_enumeration(wxWindow* parent, const std::string& name,
		const setting_var_description& desc, const std::string& current)
	{
		std::vector<std::string> valset;
		unsigned dflt = 0;
		for(unsigned i = 0; i <= desc.enumeration->max_val(); i++) {
			valset.push_back(desc.enumeration->get(i));
			if(desc.enumeration->get(i) == current)
				dflt = i;
		}
		return pick_among(parent, "Set value to", "Set " + name + " to value:", valset, dflt);
	}

	std::string change_value_of_string(wxWindow* parent, const std::string& name,
		const setting_var_description& desc, const std::string& current)
	{
		return pick_text(parent, "Set value to", "Set " + name + " to value:", current);
	}

	class numeric_inputbox : public wxDialog
	{
	public:
		numeric_inputbox(wxWindow* parent, const std::string& name, int64_t minval, int64_t maxval,
			const std::string& val)
			: wxDialog(parent, wxID_ANY, wxT("Set value to"))
		{
			wxSizer* s1 = new wxBoxSizer(wxVERTICAL);
			SetSizer(s1);
			s1->Add(new wxStaticText(this, wxID_ANY, towxstring("Set " + name + " to value:")), 0,
				wxGROW);
			
			s1->Add(sp = new wxSpinCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
				wxSP_ARROW_KEYS, minval, maxval, parse_value<int64_t>(val)), 1, wxGROW);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->AddStretchSpacer();
			wxButton* t;
			pbutton_s->Add(t = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(numeric_inputbox::on_button), NULL, this);
			pbutton_s->Add(t = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(numeric_inputbox::on_button), NULL, this);
			s1->Add(pbutton_s, 0, wxGROW);

			s1->SetSizeHints(this);
		}
		std::string get_value() { return (stringfmt() << sp->GetValue()).str(); }
		void on_button(wxCommandEvent& e) { EndModal(e.GetId()); }
	private:
		wxSpinCtrl* sp;
	};

	class path_inputbox : public wxDialog
	{
	public:
		path_inputbox(wxWindow* parent, const std::string& name, const std::string& val)
			: wxDialog(parent, wxID_ANY, wxT("Set path to"))
		{
			wxButton* t;
			wxSizer* s1 = new wxBoxSizer(wxVERTICAL);
			SetSizer(s1);
			s1->Add(new wxStaticText(this, wxID_ANY, towxstring("Set " + name + " to value:")), 0,
				wxGROW);
			wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
			s2->Add(pth = new wxTextCtrl(this, wxID_ANY, towxstring(val), wxDefaultPosition,
				wxSize(400, -1)), 1, wxGROW);
			s2->Add(t = new wxButton(this, wxID_HIGHEST + 1, wxT("...")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(path_inputbox::on_pbutton), NULL, this);
			s1->Add(s2, 1, wxGROW);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->AddStretchSpacer();
			pbutton_s->Add(t = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(path_inputbox::on_button), NULL, this);
			pbutton_s->Add(t = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(path_inputbox::on_button), NULL, this);
			s1->Add(pbutton_s, 0, wxGROW);

			s1->SetSizeHints(this);
		}
		std::string get_value() { return tostdstring(pth->GetValue()); }
		void on_pbutton(wxCommandEvent& e) {
			wxDirDialog* d;
			d = new wxDirDialog(this, wxT("Select project directory"),
				pth->GetValue(), wxDD_DIR_MUST_EXIST);
			if(d->ShowModal() == wxID_CANCEL) {
				d->Destroy();
				return;
			}
			pth->SetValue(d->GetPath());
			d->Destroy();
		}
		void on_button(wxCommandEvent& e) {
			wxDirDialog* d;
			switch(e.GetId()) {
			case wxID_OK:
			case wxID_CANCEL:
				EndModal(e.GetId());
				break;
			};
		}
	private:
		wxTextCtrl* pth;
	};

	std::string change_value_of_numeric(wxWindow* parent, const std::string& name,
		const setting_var_description& desc, const std::string& current)
	{
		auto d = new numeric_inputbox(parent, name, desc.min_val, desc.max_val, current);
		int x = d->ShowModal();
		if(x == wxID_CANCEL) {
			d->Destroy();
			throw canceled_exception();
		}
		std::string v = d->get_value();
		d->Destroy();
		return v;
	}

	std::string change_value_of_path(wxWindow* parent, const std::string& name,
		const setting_var_description& desc, const std::string& current)
	{
		auto d = new path_inputbox(parent, name, current);
		int x = d->ShowModal();
		if(x == wxID_CANCEL) {
			d->Destroy();
			throw canceled_exception();
		}
		std::string v = d->get_value();
		d->Destroy();
		return v;
	}
}

void wxeditor_esettings_advanced::on_change(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string name = selected();
	if(name == "")
		return;
	std::string value;
	std::string err;
	value = lsnes_vsetc.get(name);
	auto model = lsnes_vsetc.get_description(name);
	try {
		switch(model.type) {
		case setting_var_description::T_BOOLEAN:
			value = change_value_of_boolean(name, model, value); break;
		case setting_var_description::T_NUMERIC:
			value = change_value_of_numeric(this, name, model, value); break;
		case setting_var_description::T_STRING:
			value = change_value_of_string(this, name, model, value); break;
		case setting_var_description::T_PATH:
			value = change_value_of_path(this, name, model, value); break;
		case setting_var_description::T_ENUMERATION:
			value = change_value_of_enumeration(this, name, model, value); break;
		default:
			value = change_value_of_string(this, name, model, value); break;
		};
	} catch(...) {
		return;
	}
	bool error = false;
	std::string errorstr;
	runemufn([&error, &errorstr, name, value]() {
		try {
			lsnes_vsetc.set(name, value);
		} catch(std::exception& e) {
			error = true;
			errorstr = e.what();
		}
	});
	if(error)
		wxMessageBox(towxstring(errorstr), wxT("Error setting value"), wxICON_EXCLAMATION | wxOK);
}

void wxeditor_esettings_advanced::on_selchange(wxCommandEvent& e)
{
	if(destruction_underway)
		return;
	std::string sel = selected();
	bool enable = (sel != "");
	changebutton->Enable(enable);
}

void wxeditor_esettings_advanced::on_setting_change(const setting_var_base& val)
{
	if(destruction_underway)
		return;
	runuifun([this, &val]() {
		std::string setting = val.get_iname();
		std::string value = val.str();
		this->settings.insert(setting);
		this->values[setting] = value;
		this->_refresh();
		});
}

void wxeditor_esettings_advanced::refresh()
{
	if(destruction_underway)
		return;
	settings = lsnes_vsetc.get_keys();
	for(auto i : settings) {
		values[i] = lsnes_vsetc.get(i);
		names[i] = lsnes_vset[i].get_hname();
	}
	_refresh();
}

std::string wxeditor_esettings_advanced::selected()
{
	if(destruction_underway)
		return "";
	int x = _settings->GetSelection();
	if(selections.count(x))
		return selections[x];
	else
		return "";
}

void wxeditor_esettings_advanced::_refresh()
{
	if(destruction_underway)
		return;
	std::vector<wxString> strings;
	std::multimap<std::string, std::string> sort;
	int k = 0;
	for(auto i : settings)
		sort.insert(std::make_pair(names[i], i));
	for(auto i : sort) {
		auto description = lsnes_vsetc.get_description(i.second);
		strings.push_back(towxstring(names[i.second] + " (Value: " + values[i.second] + ")"));
		selections[k++] = i.second;
	}
	_settings->Set(strings.size(), &strings[0]);
}


class wxeditor_esettings : public wxDialog
{
public:
	wxeditor_esettings(wxWindow* parent, int mode);
	~wxeditor_esettings();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
private:
	wxWindow* joystick_window;
	wxNotebook* tabset;
	wxButton* closebutton;
	wxeditor_esettings_hotkeys* hotkeytab;
	wxeditor_esettings_controllers* controllertab;
	wxeditor_esettings_bindings* bindtab;
	wxeditor_esettings_advanced* advtab;
	wxeditor_esettings_settings* settingstab;
	wxeditor_esettings_aliases* aliastab;
};

wxeditor_esettings::wxeditor_esettings(wxWindow* parent, int mode)
	: wxDialog(parent, wxID_ANY, towxstring(get_title(mode)), wxDefaultPosition, wxSize(-1, -1))
{
	//Grab keys to prevent the joystick driver from running who knows what commands.
	lsnes_kbd.set_exclusive(&keygrabber);

	Centre();
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	if(mode == 1) {
		hotkeytab = new wxeditor_esettings_hotkeys(this);
		controllertab = NULL;
		bindtab = NULL;
		advtab = NULL;
		settingstab = NULL;
		aliastab = NULL;
		top_s->Add(hotkeytab);
	} else if(mode == 2) {
		hotkeytab = NULL;
		controllertab = new wxeditor_esettings_controllers(this);
		bindtab = NULL;
		advtab = NULL;
		settingstab = NULL;
		aliastab = NULL;
		top_s->Add(controllertab);
	} else {
		tabset = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
		tabset->AddPage(new wxeditor_esettings_joystick(tabset), wxT("Joysticks"));
		tabset->AddPage(settingstab = new wxeditor_esettings_settings(tabset), wxT("Display"));
		tabset->AddPage(hotkeytab = new wxeditor_esettings_hotkeys(tabset), wxT("Hotkeys"));
		tabset->AddPage(controllertab = new wxeditor_esettings_controllers(tabset), wxT("Controllers"));
		tabset->AddPage(aliastab = new wxeditor_esettings_aliases(tabset, hotkeytab), wxT("Aliases"));
		tabset->AddPage(bindtab = new wxeditor_esettings_bindings(tabset), wxT("Bindings"));
		tabset->AddPage(advtab = new wxeditor_esettings_advanced(tabset), wxT("Advanced"));
		top_s->Add(tabset, 1, wxGROW);
	}
	
	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(closebutton = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	top_s->SetSizeHints(this);
	Fit();
}

wxeditor_esettings::~wxeditor_esettings()
{
	if(hotkeytab) hotkeytab->prepare_destroy();
	if(controllertab) controllertab->prepare_destroy();
	if(bindtab) bindtab->prepare_destroy();
	if(advtab) advtab->prepare_destroy();
	if(aliastab) aliastab->prepare_destroy();
	lsnes_kbd.set_exclusive(NULL);
}

bool wxeditor_esettings::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_esettings::on_close(wxCommandEvent& e)
{
	if(settingstab) {
		hflip_enabled = settingstab->hflip->GetValue();
		vflip_enabled = settingstab->vflip->GetValue();
		rotate_enabled = settingstab->rotate->GetValue();
	}
	EndModal(wxID_OK);
}

void wxsetingsdialog_display(wxWindow* parent, int mode)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_esettings(parent, mode);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
