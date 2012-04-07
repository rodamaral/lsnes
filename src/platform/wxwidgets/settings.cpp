#include "platform/wxwidgets/platform.hpp"
#include "core/settings.hpp"
#include "library/string.hpp"

#include <wx/wx.h>
#include <wx/notebook.h>
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
	SetSizer(jgrid = new wxBoxSizer(wxVERTICAL));
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
		v = pick_text(this, "Set X scaling factor", "Enter new horizontal scale factor:", v);
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
		v = pick_text(this, "Set Y scaling factor", "Enter new vertical scale factor:", v);
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
