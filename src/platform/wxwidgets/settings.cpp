#include "platform/wxwidgets/platform.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <vector>
#include <string>

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
#define FIRMWAREPATH "firmwarepath"
#define ROMPATH "rompath"
#define MOVIEPATH "moviepath"
#define SLOTPATH "slotpath"
#define SAVESLOTS "jukebox-size"



const char* scalealgo_choices[] = {"Fast Bilinear", "Bilinear", "Bicubic", "Experimential", "Point", "Area",
	"Bicubic-Linear", "Gauss", "Sinc", "Lanczos", "Spline"};

namespace
{
	struct keyentry_mod_data
	{
		wxCheckBox* pressed;
		wxCheckBox* unmasked;
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
		std::string currentclass;
		wxComboBox* mainclass;
		wxComboBox* mainkey;
		wxButton* ok;
		wxButton* cancel;
		wxButton* clear;
		bool cleared;
	};

	wxdialog_keyentry::wxdialog_keyentry(wxWindow* parent, const std::string& title, const std::string& spec,
		bool clearable)
		: wxDialog(parent, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1))
	{
		std::vector<wxString> classeslist;
		wxString emptystring;
		std::set<std::string> mods, keys;

		cleared = false;
		runemufn([&mods, &keys, &classes, &classeslist]() {
			std::set<std::string> x;
			mods = modifier::get_set();
			keys = keygroup::get_keys();
			for(auto i : keys) {
				std::string kclass = keygroup::lookup(i).first->get_class();
				if(!x.count(kclass))
					classeslist.push_back(towxstring(kclass));
				x.insert(kclass);
				classes[kclass].insert(i);
			}
			});

		Centre();
		wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
		SetSizer(top_s);

		wxFlexGridSizer* t_s = new wxFlexGridSizer(mods.size() + 1, 3, 0, 0);
		for(auto i : mods) {
			t_s->Add(new wxStaticText(this, wxID_ANY, towxstring(i)), 0, wxGROW);
			keyentry_mod_data m;
			t_s->Add(m.pressed = new wxCheckBox(this, wxID_ANY, wxT("Pressed")), 0, wxGROW);
			t_s->Add(m.unmasked = new wxCheckBox(this, wxID_ANY, wxT("Unmasked")), 0, wxGROW);
			m.pressed->Disable();
			modifiers[i] = m;
			m.pressed->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
			m.unmasked->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
		}
		t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Key")), 0, wxGROW);
		t_s->Add(mainclass = new wxComboBox(this, wxID_ANY, classeslist[0], wxDefaultPosition, wxDefaultSize,
			classeslist.size(), &classeslist[0], wxCB_READONLY), 1, wxGROW);
		mainclass->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxdialog_keyentry::on_classchange), NULL, this);
		t_s->Add(mainkey = new wxComboBox(this, wxID_ANY, emptystring, wxDefaultPosition, wxDefaultSize,
			1, &emptystring, wxCB_READONLY), 1, wxGROW);
		mainkey->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
		top_s->Add(t_s);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		if(clearable)
			pbutton_s->Add(clear = new wxButton(this, wxID_OK, wxT("Clear")), 0, wxGROW);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
		pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
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
		if(modifiers[mod].unmasked->IsEnabled()) {
			wxCommandEvent e;
			modifiers[mod].unmasked->SetValue(true);
			on_change_setting(e);
		}
	}	

	void wxdialog_keyentry::set_mod(const std::string& mod)
	{
		if(!modifiers.count(mod))
			return;
		if(modifiers[mod].pressed->IsEnabled()) {
			wxCommandEvent e;
			modifiers[mod].pressed->SetValue(true);
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
		set_class(_class);
		mainclass->SetValue(towxstring(_class));
		mainkey->SetValue(towxstring(key));
	}		

	void wxdialog_keyentry::on_change_setting(wxCommandEvent& e)
	{
		for(auto& i : modifiers)
			i.second.tmpflags = 0;
		for(auto& i : modifiers) {
			modifier* m = NULL;
			try {
				m = &modifier::lookup(i.first);
			} catch(...) {
				i.second.pressed->Disable();
				i.second.unmasked->Disable();
				continue;
			}
			std::string j = m->linked_name();
			if(i.second.unmasked->GetValue())
				i.second.tmpflags |= TMPFLAG_UNMASKED;
			if(j != "") {
				if(modifiers[j].unmasked->GetValue())
					i.second.tmpflags |= TMPFLAG_UNMASKED_LINK_PARENT;
				if(i.second.unmasked->GetValue())
					modifiers[j].tmpflags |= TMPFLAG_UNMASKED_LINK_CHILD;
			}
			if(i.second.pressed->GetValue())
				i.second.tmpflags |= TMPFLAG_PRESSED;
			if(j != "") {
				if(modifiers[j].pressed->GetValue())
					i.second.tmpflags |= TMPFLAG_PRESSED_LINK_PARENT;
				if(i.second.pressed->GetValue())
					modifiers[j].tmpflags |= TMPFLAG_PRESSED_LINK_CHILD;
			}
		}
		for(auto& i : modifiers) {
			//Unmasked is to be enabled if neither unmasked link flag is set.
			if(i.second.tmpflags & ((TMPFLAG_UNMASKED_LINK_CHILD | TMPFLAG_UNMASKED_LINK_PARENT) & ~64)) {
				i.second.unmasked->SetValue(false);
				i.second.unmasked->Disable();
			} else
				i.second.unmasked->Enable();
			//Pressed is to be enabled if:
			//- This modifier is unmasked or parent is unmasked.
			//- Parent nor child is not pressed.
			if(((i.second.tmpflags & (TMPFLAG_UNMASKED | TMPFLAG_UNMASKED_LINK_PARENT |
				TMPFLAG_PRESSED_LINK_CHILD | TMPFLAG_PRESSED_LINK_PARENT)) & 112) == 64)
				i.second.pressed->Enable();
			else {
				i.second.pressed->SetValue(false);
				i.second.pressed->Disable();
			}
		}
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
			if(i.second.pressed->GetValue()) {
				if(!f)
					x = x + ",";
				f = false;
				x = x + i.first;
			}
		}
		x = x + "/";
		f = true;
		for(auto i : modifiers) {
			if(i.second.unmasked->GetValue()) {
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
	keygroup::parameters params;

	runemufn([aname, &params]() {
		auto k = keygroup::lookup_by_name(aname);
		if(k)
			params = k->get_parameters();
		});

	switch(params.ktype) {
	case keygroup::KT_DISABLED:		didx = 0; break;
	case keygroup::KT_AXIS_PAIR:		didx = 1; break;
	case keygroup::KT_AXIS_PAIR_INVERSE:	didx = 2; break;
	case keygroup::KT_PRESSURE_M0:		didx = 3; break;
	case keygroup::KT_PRESSURE_MP:		didx = 4; break;
	case keygroup::KT_PRESSURE_0M:		didx = 5; break;
	case keygroup::KT_PRESSURE_0P:		didx = 6; break;
	case keygroup::KT_PRESSURE_PM:		didx = 7; break;
	case keygroup::KT_PRESSURE_P0:		didx = 8; break;
	};

	Centre();
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	wxFlexGridSizer* t_s = new wxFlexGridSizer(5, 2, 0, 0);
	t_s->Add(new wxStaticText(this, -1, wxT("Type: ")), 0, wxGROW);
	t_s->Add(type = new wxComboBox(this, wxID_ANY, choices[didx], wxDefaultPosition, wxDefaultSize,
		9, choices, wxCB_READONLY), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("Low: ")), 0, wxGROW);
	t_s->Add(low = new wxTextCtrl(this, -1, towxstring((stringfmt() << params.cal_left).str()), wxDefaultPosition,
		wxSize(100, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("Middle: ")), 0, wxGROW);
	t_s->Add(mid = new wxTextCtrl(this, -1, towxstring((stringfmt() << params.cal_center).str()),
		wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("High: ")), 0, wxGROW);
	t_s->Add(hi = new wxTextCtrl(this, -1, towxstring((stringfmt() << params.cal_right).str()),
		wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
	t_s->Add(new wxStaticText(this, -1, wxT("Tolerance: ")), 0, wxGROW);
	t_s->Add(tol = new wxTextCtrl(this, -1, towxstring((stringfmt() << params.cal_tolerance).str()),
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
	enum keygroup::type _ctype = keygroup::KT_DISABLED;
	enum keygroup::type _ntype = keygroup::KT_AXIS_PAIR;
	int32_t nlow, nmid, nhi;
	double ntol;
	keygroup* k;

	runemufn([&k, aname, &_ctype]() {
		k = keygroup::lookup_by_name(aname);
		if(k)
			_ctype = k->get_parameters().ktype;
		});
	if(!k) {
		//Axis gone away?
		EndModal(wxID_OK);
		return;
	}

	const char* bad_what = NULL;
	try {
		bad_what = "Bad axis type";
		if(_type == AMODE_AXIS_PAIR)			_ntype = keygroup::KT_AXIS_PAIR;
		else if(_type == AMODE_AXIS_PAIR_INVERSE)	_ntype = keygroup::KT_AXIS_PAIR_INVERSE;
		else if(_type == AMODE_DISABLED)		_ntype = keygroup::KT_DISABLED;
		else if(_type == AMODE_PRESSURE_0M)		_ntype = keygroup::KT_PRESSURE_0M;
		else if(_type == AMODE_PRESSURE_0P)		_ntype = keygroup::KT_PRESSURE_0P;
		else if(_type == AMODE_PRESSURE_M0)		_ntype = keygroup::KT_PRESSURE_M0;
		else if(_type == AMODE_PRESSURE_MP)		_ntype = keygroup::KT_PRESSURE_MP;
		else if(_type == AMODE_PRESSURE_P0)		_ntype = keygroup::KT_PRESSURE_P0;
		else if(_type == AMODE_PRESSURE_PM)		_ntype = keygroup::KT_PRESSURE_PM;
		else
			throw 42;
		bad_what = "Bad low calibration value (range is -32768 - 32767)";
		nlow = boost::lexical_cast<int32_t>(_low);
		if(nlow < -32768 || nlow > 32767)
			throw 42;
		bad_what = "Bad middle calibration value (range is -32768 - 32767)";
		nmid = boost::lexical_cast<int32_t>(_mid);
		if(nmid < -32768 || nmid > 32767)
			throw 42;
		bad_what = "Bad high calibration value (range is -32768 - 32767)";
		nhi = boost::lexical_cast<int32_t>(_hi);
		if(nhi < -32768 || nhi > 32767)
			throw 42;
		bad_what = "Bad tolerance (range is 0 - 1)";
		ntol = boost::lexical_cast<double>(_tol);
		if(ntol <= 0 || ntol >= 1)
			throw 42;
	} catch(...) {
		wxMessageBox(towxstring(bad_what), _T("Error"), wxICON_EXCLAMATION | wxOK);
		return;
	}
	
	runemufn([&k, _ctype, _ntype, nlow, nmid, nhi, ntol]() {
		if(_ctype != _ntype)
			k->change_type(_ntype);
		k->change_calibration(nlow, nmid, nhi, ntol);
		});
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
	std::map<std::string, wxButton*> buttons;
	std::map<int, std::string> ids;
	int last_id;
};

namespace
{
	std::string formattype(keygroup::type t)
	{
		if(t == keygroup::KT_AXIS_PAIR) return AMODE_AXIS_PAIR;
		else if(t == keygroup::KT_AXIS_PAIR_INVERSE) return AMODE_AXIS_PAIR_INVERSE;
		else if(t == keygroup::KT_PRESSURE_0M) return AMODE_PRESSURE_0M;
		else if(t == keygroup::KT_PRESSURE_0P) return AMODE_PRESSURE_0P;
		else if(t == keygroup::KT_PRESSURE_M0) return AMODE_PRESSURE_M0;
		else if(t == keygroup::KT_PRESSURE_MP) return AMODE_PRESSURE_MP;
		else if(t == keygroup::KT_PRESSURE_P0) return AMODE_PRESSURE_P0;
		else if(t == keygroup::KT_PRESSURE_PM) return AMODE_PRESSURE_PM;
		else return "Unknown";
	}

	std::string formatsettings(const std::string& name, const keygroup::parameters& s)
	{
		return (stringfmt() << name << ": " << formattype(s.ktype) << " low:" << s.cal_left << " mid:"
			<< s.cal_center << " high:" << s.cal_right << " tolerance:" << s.cal_tolerance).str();
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
	SetMinSize(wxSize(400, 420));
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
	std::map<std::string, keygroup::parameters> x;
	runemufn([&x]() {
		auto axisnames = keygroup::get_axis_set();
		for(auto i : axisnames) {
			keygroup* k = keygroup::lookup_by_name(i);
			if(k)
				x[i] = k->get_parameters();
		}
		});

	for(auto i : x) {
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
	jgrid->Layout();
	this->Refresh();
	Fit();
}

class wxeditor_esettings_paths : public wxPanel
{
public:
	wxeditor_esettings_paths(wxWindow* parent);
	~wxeditor_esettings_paths();
	void on_configure(wxCommandEvent& e);
private:
	void refresh();
	wxStaticText* rompath;
	wxStaticText* firmpath;
	wxStaticText* savepath;
	wxStaticText* slotpath;
	wxStaticText* slots;
	wxFlexGridSizer* top_s;
};

wxeditor_esettings_paths::wxeditor_esettings_paths(wxWindow* parent)
	: wxPanel(parent, -1)
{
	wxButton* tmp;
	top_s = new wxFlexGridSizer(5, 3, 0, 0);
	SetSizer(top_s);
	top_s->Add(new wxStaticText(this, -1, wxT("ROM path: ")), 0, wxGROW);
	top_s->Add(rompath = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 1, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_paths::on_configure), NULL,
		this);
	top_s->Add(new wxStaticText(this, -1, wxT("Firmware path: ")), 0, wxGROW);
	top_s->Add(firmpath = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 2, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_paths::on_configure), NULL,
		this);
	top_s->Add(new wxStaticText(this, -1, wxT("Movie path: ")), 0, wxGROW);
	top_s->Add(savepath = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 3, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_paths::on_configure), NULL,
		this);
	top_s->Add(new wxStaticText(this, -1, wxT("Slot path: ")), 0, wxGROW);
	top_s->Add(slotpath = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 5, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_paths::on_configure), NULL,
		this);
	top_s->Add(new wxStaticText(this, -1, wxT("Save slots: ")), 0, wxGROW);
	top_s->Add(slots = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 4, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_paths::on_configure), NULL,
		this);
	refresh();
	top_s->SetSizeHints(this);
	Fit();
}
wxeditor_esettings_paths::~wxeditor_esettings_paths()
{
}

void wxeditor_esettings_paths::on_configure(wxCommandEvent& e)
{
	std::string name;
	if(e.GetId() == wxID_HIGHEST + 1)
		name = ROMPATH;
	else if(e.GetId() == wxID_HIGHEST + 2)
		name = FIRMWAREPATH;
	else if(e.GetId() == wxID_HIGHEST + 3)
		name = MOVIEPATH;
	else if(e.GetId() == wxID_HIGHEST + 4)
		name = SAVESLOTS;
	else if(e.GetId() == wxID_HIGHEST + 5)
		name = SLOTPATH;
	else
		return;
	std::string val;
	runemufn([&val, name]() { val = setting::get(name); });
	try {
		if(e.GetId() == wxID_HIGHEST + 4)
			val = pick_text(this, "Change number of slots", "Enter number of slots:", val);
		else
			val = pick_text(this, "Change path to", "Enter new path:", val);
	} catch(...) {
		refresh();
		return;
	}
	std::string err;
	runemufn([val, name, &err]() { try { setting::set(name, val); } catch(std::exception& e) { err = e.what(); }});
	if(err != "")
		wxMessageBox(wxT("Invalid value"), wxT("Can't change value"), wxICON_EXCLAMATION | wxOK);
	refresh();
}

void wxeditor_esettings_paths::refresh()
{
	std::string rpath, fpath, spath, nslot, lpath;
	runemufn([&rpath, &fpath, &spath, &nslot, &lpath]() {
		fpath = setting::get(FIRMWAREPATH);
		rpath = setting::get(ROMPATH);
		spath = setting::get(MOVIEPATH);
		nslot = setting::get(SAVESLOTS);
		lpath = setting::get(SLOTPATH);
		});
	rompath->SetLabel(towxstring(rpath));
	firmpath->SetLabel(towxstring(fpath));
	savepath->SetLabel(towxstring(spath));
	slots->SetLabel(towxstring(nslot));
	slotpath->SetLabel(towxstring(lpath));
	top_s->Layout();
	Fit();
}

class wxeditor_esettings_screen : public wxPanel
{
public:
	wxeditor_esettings_screen(wxWindow* parent);
	~wxeditor_esettings_screen();
	void on_configure(wxCommandEvent& e);
private:
	void refresh();
	wxStaticText* xscale;
	wxStaticText* yscale;
	wxStaticText* algo;
	wxFlexGridSizer* top_s;
};

wxeditor_esettings_screen::wxeditor_esettings_screen(wxWindow* parent)
	: wxPanel(parent, -1)
{
	wxButton* tmp;
	top_s = new wxFlexGridSizer(3, 3, 0, 0);
	SetSizer(top_s);
	top_s->Add(new wxStaticText(this, -1, wxT("X scale factor: ")), 0, wxGROW);
	top_s->Add(xscale = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 1, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_screen::on_configure),
		NULL, this);
	top_s->Add(new wxStaticText(this, -1, wxT("Y scale factor: ")), 0, wxGROW);
	top_s->Add(yscale = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 2, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_screen::on_configure),
		NULL, this);
	top_s->Add(new wxStaticText(this, -1, wxT("Scaling type: ")), 0, wxGROW);
	top_s->Add(algo = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
	top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 3, wxT("Change...")), 0, wxGROW);
	tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_screen::on_configure),
		NULL, this);
	refresh();
	top_s->SetSizeHints(this);
	Fit();
}
wxeditor_esettings_screen::~wxeditor_esettings_screen()
{
}

void wxeditor_esettings_screen::on_configure(wxCommandEvent& e)
{
	if(e.GetId() == wxID_HIGHEST + 1) {
		std::string v = (stringfmt() << horizontal_scale_factor).str();
		try {
			v = pick_text(this, "Set X scaling factor", "Enter new horizontal scale factor:", v);
		} catch(...) {
			refresh();
			return;
		}
		double x;
		try {
			x = parse_value<double>(v);
			if(x < 0.25 || x > 10)
				throw 42;
		} catch(...) {
			wxMessageBox(wxT("Bad horizontal scale factor (0.25-10)"), wxT("Input error"),
				wxICON_EXCLAMATION | wxOK);
			refresh();
			return;
		}
		horizontal_scale_factor = x;
	} else if(e.GetId() == wxID_HIGHEST + 2) {
		std::string v = (stringfmt() << vertical_scale_factor).str();
		try {
			v = pick_text(this, "Set Y scaling factor", "Enter new vertical scale factor:", v);
		} catch(...) {
			refresh();
			return;
		}
		double x;
		try {
			x = parse_value<double>(v);
			if(x < 0.25 || x > 10)
				throw 42;
		} catch(...) {
			wxMessageBox(wxT("Bad vertical scale factor (0.25-10)"), wxT("Input error"),
				wxICON_EXCLAMATION | wxOK);
			refresh();
			return;
		}
		vertical_scale_factor = x;
	} else if(e.GetId() == wxID_HIGHEST + 3) {
		std::vector<std::string> choices;
		std::string v;
		int newflags = 1;
		for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			choices.push_back(scalealgo_choices[i]);
		try {
			v = pick_among(this, "Select algorithm", "Select scaling algorithm", choices);
		} catch(...) {
			refresh();
			return;
		}
		for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			if(v == scalealgo_choices[i])
				newflags = 1 << i;
		scaling_flags = newflags;
	}
	refresh();
}

void wxeditor_esettings_screen::refresh()
{
	xscale->SetLabel(towxstring((stringfmt() << horizontal_scale_factor).str()));
	yscale->SetLabel(towxstring((stringfmt() << vertical_scale_factor).str()));
	algo->SetLabel(towxstring(getalgo(scaling_flags)));
	top_s->Layout();
	Fit();
}

class wxeditor_esettings_aliases : public wxPanel
{
public:
	wxeditor_esettings_aliases(wxWindow* parent);
	~wxeditor_esettings_aliases();
	void on_add(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
private:
	std::map<int, std::string> numbers;
	wxListBox* select;
	wxButton* editbutton;
	wxButton* deletebutton;
	void refresh();
	std::string selected();
};

wxeditor_esettings_aliases::wxeditor_esettings_aliases(wxWindow* parent)
	: wxPanel(parent, -1)
{
	wxButton* tmp;

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
	bool enable = (selected() != "");
	editbutton->Enable(enable);
	deletebutton->Enable(enable);
}

void wxeditor_esettings_aliases::on_add(wxCommandEvent& e)
{
	try {
		std::string name = pick_text(this, "Enter alias name", "Enter name for the new alias:");
		if(!command::valid_alias_name(name)) {
			show_message_ok(this, "Error", "Not a valid alias name: " + name, wxICON_EXCLAMATION);
			throw canceled_exception();
		}
		std::string old_alias_value;
		runemufn([name, &old_alias_value]() { old_alias_value = command::get_alias_for(name); });
		std::string newcmd = pick_text(this, "Edit alias", "Enter new commands for '" + name + "':",
			old_alias_value, true);
		runemufn([name, newcmd]() { command::set_alias_for(name, newcmd); });
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_aliases::on_edit(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try {
		std::string old_alias_value;
		runemufn([name, &old_alias_value]() { old_alias_value = command::get_alias_for(name); });
		std::string newcmd = pick_text(this, "Edit alias", "Enter new commands for '" + name + "':",
			old_alias_value, true);
		runemufn([name, newcmd]() { command::set_alias_for(name, newcmd); });
	} catch(...) {
	}
	refresh();
}

void wxeditor_esettings_aliases::on_delete(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	runemufn([name]() { command::set_alias_for(name, ""); });
	refresh();
}

void wxeditor_esettings_aliases::refresh()
{
	int n = select->GetSelection();
	std::set<std::string> bind;
	std::vector<wxString> choices;
	runemufn([&bind]() { bind = command::get_aliases(); });
	for(auto i : bind) {
		numbers[choices.size()] = i;
		choices.push_back(towxstring(i));
	}
	select->Set(choices.size(), &choices[0]);
	if(n == wxNOT_FOUND && select->GetCount())
		select->SetSelection(0);
	else if(n >= select->GetCount())
		select->SetSelection(select->GetCount() ? (select->GetCount() - 1) : wxNOT_FOUND);
	else
		select->SetSelection(n);
	wxCommandEvent e;
	on_change(e);
	select->Refresh();
}

std::string wxeditor_esettings_aliases::selected()
{
	int x = select->GetSelection();
	if(numbers.count(x))
		return numbers[x];
	else
		return "";
}

class wxeditor_esettings_hotkeys : public wxPanel
{
public:
	wxeditor_esettings_hotkeys(wxWindow* parent);
	~wxeditor_esettings_hotkeys();
	void on_primary(wxCommandEvent& e);
	void on_secondary(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
private:
	std::map<std::string, wxTreeItemId> items;
	std::map<std::string, inverse_key*> realitems;
	std::map<std::string, std::string> leafname;
	wxTreeItemId true_root;
	wxTreeCtrl* select;
	wxButton* pri_button;
	wxButton* sec_button;
	void refresh();
	std::string selected();
};

wxeditor_esettings_hotkeys::wxeditor_esettings_hotkeys(wxWindow* parent)
	: wxPanel(parent, -1)
{
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	top_s->Add(select = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS |
		wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT), 1, wxGROW);
	select->Connect(wxEVT_COMMAND_TREE_SEL_CHANGED, wxCommandEventHandler(wxeditor_esettings_hotkeys::on_change),
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

	items[""] = select->AddRoot(wxT("<root>"));

	refresh();
	wxCommandEvent e;
	on_change(e);
	top_s->SetSizeHints(this);
	Fit();
}

void wxeditor_esettings_hotkeys::on_change(wxCommandEvent& e)
{
	bool enable = (selected() != "");
	pri_button->Enable(enable);
	sec_button->Enable(enable);
}

wxeditor_esettings_hotkeys::~wxeditor_esettings_hotkeys()
{
}

void wxeditor_esettings_hotkeys::on_primary(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try {
		std::string key;
		inverse_key* ik = realitems[name];
		if(!ik) {
			refresh();
			return;
		}
		runemufn([&key, ik]() { key = ik->get(true); });
		wxdialog_keyentry* d = new wxdialog_keyentry(this, "Specify key for " + name, key, true);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		key = d->getkey();
		d->Destroy();
		if(key != "")
			runemufn([key, ik]() { ik->set(key, true); });
		else
			runemufn([key, ik]() { ik->clear(true); });
		refresh();
	} catch(...) {
		refresh();
	}
}

void wxeditor_esettings_hotkeys::on_secondary(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try {
		std::string key;
		inverse_key* ik = realitems[name];
		if(!ik) {
			refresh();
			return;
		}
		runemufn([&key, ik]() { key = ik->get(false); });
		wxdialog_keyentry* d = new wxdialog_keyentry(this, "Specify key for " + name, key, true);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		key = d->getkey();
		d->Destroy();
		if(key != "")
			runemufn([key, ik]() { ik->set(key, false); });
		else
			runemufn([key, ik]() { ik->clear(false); });
		refresh();
	} catch(...) {
		refresh();
	}
}

void wxeditor_esettings_hotkeys::refresh()
{
	std::set<std::string> closure_additional;
	std::map<std::string, inverse_key*> keyorder;
	std::map<inverse_key*, std::pair<std::string, std::string>> data;
	runemufn([&data, &keyorder]() {
		auto x = inverse_key::get_ikeys();
		for(auto y : x) {
			keyorder[y->getname()] = y;
			data[y] = std::make_pair(y->get(true), y->get(false));
		}
	});
	//Close keyorder with respect to parents.
	for(auto i : keyorder) {
		std::string tmp = i.first;
		for(size_t itr = 0; itr < tmp.length() - 2 && itr < tmp.length(); itr++) {
			unsigned char ch1 = tmp[itr];
			unsigned char ch2 = tmp[itr + 1];
			unsigned char ch3 = tmp[itr + 2];
			if(ch1 == 0xE2 && ch2 == 0x80 && ch3 == 0xA3) {
				closure_additional.insert(tmp.substr(0, itr));
			}
		}
	}
	for(auto i : closure_additional)
		if(!keyorder.count(i)) {
			keyorder[i] = NULL;
		}
	//Then add the needed entries to the tree. Thanks to the ordering, presence of parent before child is always
	//guaranteed.
	for(auto i : keyorder) {
		if(!items.count(i.first)) {
			//Not found, add.
			//Find the parent name.
			std::string tmp = i.first;
			std::string parent = "";
			std::string leaf = i.first;
			for(size_t itr = 0; itr < tmp.length() - 2 && itr < tmp.length(); itr++) {
				unsigned char ch1 = tmp[itr];
				unsigned char ch2 = tmp[itr + 1];
				unsigned char ch3 = tmp[itr + 2];
				if(ch1 == 0xE2 && ch2 == 0x80 && ch3 == 0xA3) {
					parent = tmp.substr(0, itr);
					leaf = tmp.substr(itr + 3);
				}
			}
			items[i.first] = select->AppendItem(items[parent], towxstring(leaf));
			leafname[i.first] = leaf;
		}
		if(i.second) {
			//Real, set the keys.
			realitems[i.first] = i.second;
			std::string text = leafname[i.first];
			if(data[i.second].first == "")
				text = text + " (not set)";
			else if(data[i.second].second == "")
				text = text + " (" + data[i.second].first + ")";
			else
				text = text + " (" + data[i.second].first + " or " + data[i.second].second + ")";
			select->SetItemText(items[i.first], towxstring(text));
		}
	}
	select->Refresh();
}

std::string wxeditor_esettings_hotkeys::selected()
{
	wxTreeItemId sel = select->GetSelection();
	std::string item;
	bool found = false;
	for(auto i : items)
		if(i.second == sel) {
			found = true;
			item = i.first;
		}
	if(!found || !realitems.count(item))
		return "";
	return item;
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
private:
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

void wxeditor_esettings_bindings::on_change(wxCommandEvent& e)
{
	bool enable = (selected() != "");
	editbutton->Enable(enable);
	deletebutton->Enable(enable);
}

void wxeditor_esettings_bindings::on_add(wxCommandEvent& e)
{
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
		bool fault = false;
		std::string faulttext;
		runemufn([&fault, &faulttext, name, newcommand]() {
			try {
				keymapper::bind_for(name, newcommand);
			} catch(std::exception& e) {
				fault = true;
				faulttext = e.what();
			}
		});
		if(fault)
			wxMessageBox(wxT("Error"), towxstring("Can't bind key: " + faulttext), wxICON_EXCLAMATION);
		refresh();
	} catch(...) {
		refresh();
	}
}

void wxeditor_esettings_bindings::on_edit(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	try {
		std::string old_command_value;
		runemufn([&old_command_value, name]() { old_command_value = keymapper::get_command_for(name); });
		std::string newcommand = pick_text(this, "Edit binding", "Enter new command for binding:",
			old_command_value);
		bool fault = false;
		std::string faulttext;
		runemufn([&fault, &faulttext, name, newcommand]() {
			try {
				keymapper::bind_for(name, newcommand);
			} catch(std::exception& e) {
				fault = true;
				faulttext = e.what();
			}
		});
		if(fault)
			wxMessageBox(wxT("Error"), towxstring("Can't bind key: " + faulttext), wxICON_EXCLAMATION);
		refresh();
	} catch(...) {
		refresh();
	}
}

void wxeditor_esettings_bindings::on_delete(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "") {
		refresh();
		return;
	}
	runemufn([name]() { try { keymapper::bind_for(name, ""); } catch(...) {} });
	refresh();
}

void wxeditor_esettings_bindings::refresh()
{
	int n = select->GetSelection();
	std::map<std::string, std::string> bind;
	std::vector<wxString> choices;
	runemufn([&bind]() {
		std::set<std::string> a = keymapper::get_bindings();
		for(auto i : a)
			bind[i] = keymapper::get_command_for(i);
		});
	for(auto i : bind) {
		numbers[choices.size()] = i.first;
		choices.push_back(towxstring(i.first + " (" + i.second + ")"));
	}
	select->Set(choices.size(), &choices[0]);
	if(n == wxNOT_FOUND && select->GetCount())
		select->SetSelection(0);
	else if(n >= select->GetCount())
		select->SetSelection(select->GetCount() ? (select->GetCount() - 1) : wxNOT_FOUND);
	else
		select->SetSelection(n);
	wxCommandEvent e;
	on_change(e);
	select->Refresh();
}

std::string wxeditor_esettings_bindings::selected()
{
	int x = select->GetSelection();
	if(numbers.count(x))
		return numbers[x];
	else
		return "";
}

class wxeditor_esettings_advanced : public wxPanel, information_dispatch
{
public:
	wxeditor_esettings_advanced(wxWindow* parent);
	~wxeditor_esettings_advanced();
	void on_change(wxCommandEvent& e);
	void on_clear(wxCommandEvent& e);
	void on_selchange(wxCommandEvent& e);
	void on_setting_change(const std::string& setting, const std::string& value);
	void on_setting_clear(const std::string& setting);
	void _refresh();
private:
	void refresh();
	std::set<std::string> settings;
	std::map<std::string, std::string> values;
	std::map<int, std::string> selections;
	std::set<std::string> blankables;
	std::string selected();
	wxButton* changebutton;
	wxButton* clearbutton;
	wxListBox* _settings;
};

wxeditor_esettings_advanced::wxeditor_esettings_advanced(wxWindow* parent)
	: wxPanel(parent, -1), information_dispatch("wxeditor-settings-listener")
{
	wxButton* tmp;

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
	pbutton_s->Add(clearbutton = new wxButton(this, wxID_ANY, wxT("Clear")), 0, wxGROW);
	clearbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_esettings_advanced::on_clear), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	refresh();
	wxCommandEvent e;
	on_selchange(e);
	top_s->SetSizeHints(this);
	Fit();
}

wxeditor_esettings_advanced::~wxeditor_esettings_advanced()
{
}

void wxeditor_esettings_advanced::on_change(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "")
		return;
	std::string value;
	std::string err;
	runemufn([name, &value]() { value = setting::get(name); });
	try {
		value = pick_text(this, "Set value to", "Set " + name + " to value:", value);
	} catch(...) {
		return;
	}
	runemufn([name, value, &err]() {
		try { setting::set(name, value); } catch(std::exception& e) { err = e.what(); }
		});
	if(err != "")
		wxMessageBox(towxstring(err), wxT("Error setting value"), wxICON_EXCLAMATION | wxOK);
}

void wxeditor_esettings_advanced::on_selchange(wxCommandEvent& e)
{
	std::string sel = selected();
	bool enable = (sel != "");
	changebutton->Enable(enable);
	clearbutton->Enable(enable && blankables.count(sel));
}

void wxeditor_esettings_advanced::on_clear(wxCommandEvent& e)
{
	std::string name = selected();
	if(name == "")
		return;
	bool err = false;
	runemufn([name, &err]() { try { setting::blank(name); } catch(...) { err = true; }});
	if(err)
		wxMessageBox(wxT("This setting can't be cleared"), wxT("Error"), wxICON_EXCLAMATION | wxOK);
}

void wxeditor_esettings_advanced::on_setting_change(const std::string& setting, const std::string& value)
{
	wxeditor_esettings_advanced* th = this;
	runuifun([&settings, &values, setting, value, th]() { 
		settings.insert(setting); values[setting] = value; th->_refresh();
		});
}

void wxeditor_esettings_advanced::on_setting_clear(const std::string& setting)
{
	wxeditor_esettings_advanced* th = this;
	runuifun([&settings, &values, setting, th]() {
		settings.insert(setting); values.erase(setting); th->_refresh();
		});
}

void wxeditor_esettings_advanced::refresh()
{
	runemufn([&settings, &values, &blankables]() {
		settings = setting::get_settings_set();
		blankables.clear();
		for(auto i : settings) {
			if(setting::is_set(i))
				values[i] = setting::get(i);
			if(setting::blankable(i))
				blankables.insert(i);
		}
		});
	_refresh();
}

std::string wxeditor_esettings_advanced::selected()
{
	int x = _settings->GetSelection();
	if(selections.count(x))
		return selections[x];
	else
		return "";
}

void wxeditor_esettings_advanced::_refresh()
{
	std::vector<wxString> strings;
	int k = 0;
	for(auto i : settings) {
		if(values.count(i))
			strings.push_back(towxstring(i + " (Value: " + values[i] + ")"));
		else
			strings.push_back(towxstring(i + " (Not set)"));
		selections[k++] = i;
	}
	_settings->Set(strings.size(), &strings[0]);
}


class wxeditor_esettings : public wxDialog
{
public:
	wxeditor_esettings(wxWindow* parent);
	~wxeditor_esettings();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
private:
	wxWindow* joystick_window;
	wxNotebook* tabset;
	wxButton* closebutton;
};

wxeditor_esettings::wxeditor_esettings(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Configure emulator"), wxDefaultPosition, wxSize(-1, -1))
{
	Centre();
	wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	tabset = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
	tabset->AddPage(new wxeditor_esettings_joystick(tabset), wxT("Joysticks"));
	tabset->AddPage(new wxeditor_esettings_paths(tabset), wxT("Paths"));
	tabset->AddPage(new wxeditor_esettings_screen(tabset), wxT("Scaling"));
	tabset->AddPage(new wxeditor_esettings_hotkeys(tabset), wxT("Hotkeys"));
	tabset->AddPage(new wxeditor_esettings_aliases(tabset), wxT("Aliases"));
	tabset->AddPage(new wxeditor_esettings_bindings(tabset), wxT("Bindings"));
	tabset->AddPage(new wxeditor_esettings_advanced(tabset), wxT("Advanced"));
	top_s->Add(tabset, 1, wxGROW);
	
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
}

bool wxeditor_esettings::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_esettings::on_close(wxCommandEvent& e)
{
	EndModal(wxID_OK);
}

void wxsetingsdialog_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_esettings(parent);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
