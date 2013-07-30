#include "platform/wxwidgets/settings-common.hpp"
#include "core/keymapper.hpp"

#define AMODE_DISABLED "Disabled"
#define AMODE_AXIS_PAIR "Axis"
#define AMODE_AXIS_PAIR_INVERSE "Axis (inverted)"
#define AMODE_PRESSURE_M0 "Pressure - to 0"
#define AMODE_PRESSURE_MP "Pressure - to +"
#define AMODE_PRESSURE_0M "Pressure 0 to -"
#define AMODE_PRESSURE_0P "Pressure 0 to +"
#define AMODE_PRESSURE_PM "Pressure + to -"
#define AMODE_PRESSURE_P0 "Pressure + to 0"

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

	wxeditor_esettings_joystick_aconfig::wxeditor_esettings_joystick_aconfig(wxWindow* parent,
		const std::string& _aname)
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
			if(_type == AMODE_AXIS_PAIR)		{c.mode = 1;  c.esign_a = -1;  c.esign_b = 1;  }
			else if(_type == AMODE_AXIS_PAIR_INVERSE) {c.mode = 1;  c.esign_a = 1;   c.esign_b = -1; }
			else if(_type == AMODE_DISABLED)	{c.mode = -1; c.esign_a = -1;  c.esign_b = 1;  }
			else if(_type == AMODE_PRESSURE_0M)	{c.mode = 0;  c.esign_a = 0;   c.esign_b = -1; }
			else if(_type == AMODE_PRESSURE_0P)	{c.mode = 0;  c.esign_a = 0;   c.esign_b = 1;  }
			else if(_type == AMODE_PRESSURE_M0)	{c.mode = 0;  c.esign_a = -1;  c.esign_b = 0;  }
			else if(_type == AMODE_PRESSURE_MP)	{c.mode = 0;  c.esign_a = -1;  c.esign_b = 1;  }
			else if(_type == AMODE_PRESSURE_P0)	{c.mode = 0;  c.esign_a = 1;   c.esign_b = 0;  }
			else if(_type == AMODE_PRESSURE_PM)	{c.mode = 0;  c.esign_a = 1;   c.esign_b = -1; }
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

	class wxeditor_esettings_joystick : public settings_tab
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

	wxeditor_esettings_joystick::wxeditor_esettings_joystick(wxWindow* parent)
		: settings_tab(parent)
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

	settings_tab_factory bindings("Joysticks", [](wxWindow* parent) -> settings_tab* {
		return new wxeditor_esettings_joystick(parent);
	});
}
