#include "core/keymapper.hpp"

#include "plat-wxwidgets/axeseditor.hpp"
#include "plat-wxwidgets/common.hpp"

#include <boost/lexical_cast.hpp>
#include <sstream>

#define AMODE_DISABLED "Disabled"
#define AMODE_AXIS_PAIR "Axis"
#define AMODE_AXIS_PAIR_INVERSE "Axis (inverted)"
#define AMODE_PRESSURE_M0 "Pressure - to 0"
#define AMODE_PRESSURE_MP "Pressure - to +"
#define AMODE_PRESSURE_0M "Pressure 0 to -"
#define AMODE_PRESSURE_0P "Pressure 0 to +"
#define AMODE_PRESSURE_PM "Pressure + to -"
#define AMODE_PRESSURE_P0 "Pressure + to 0"

wx_axes_editor_axis::wx_axes_editor_axis(wxSizer* sizer, wxWindow* window, const std::string& name)
{
	wxString choices[9];
	choices[0] = wxT(AMODE_DISABLED);
	choices[1] = wxT(AMODE_AXIS_PAIR);
	choices[2] = wxT(AMODE_AXIS_PAIR_INVERSE);
	choices[3] = wxT(AMODE_PRESSURE_M0);
	choices[4] = wxT(AMODE_PRESSURE_MP);
	choices[5] = wxT(AMODE_PRESSURE_0M);
	choices[6] = wxT(AMODE_PRESSURE_0P);
	choices[7] = wxT(AMODE_PRESSURE_PM);
	choices[8] = wxT(AMODE_PRESSURE_P0);
	size_t defaultidx = 0;
	std::string low;
	std::string mid;
	std::string high;
	std::string tolerance;
	keygroup* k = keygroup::lookup_by_name(name);
	if(!k)
		return;
	struct keygroup::parameters p = k->get_parameters();
	{
		switch(p.ktype) {
		case keygroup::KT_DISABLED:		defaultidx = 0; break;
		case keygroup::KT_AXIS_PAIR:		defaultidx = 1; break;
		case keygroup::KT_AXIS_PAIR_INVERSE:	defaultidx = 2; break;
		case keygroup::KT_PRESSURE_M0:		defaultidx = 3; break;
		case keygroup::KT_PRESSURE_MP:		defaultidx = 4; break;
		case keygroup::KT_PRESSURE_0M:		defaultidx = 5; break;
		case keygroup::KT_PRESSURE_0P:		defaultidx = 6; break;
		case keygroup::KT_PRESSURE_PM:		defaultidx = 7; break;
		case keygroup::KT_PRESSURE_P0:		defaultidx = 8; break;
		};
		std::ostringstream x1;
		std::ostringstream x2;
		std::ostringstream x3;
		std::ostringstream x4;
		x1 << p.cal_left;
		x2 << p.cal_center;
		x3 << p.cal_right;
		x4 << p.cal_tolerance;
		low = x1.str();
		mid = x2.str();
		high = x3.str();
		tolerance = x4.str();
	}

	a_name = name;
	sizer->Add(new wxStaticText(window, wxID_ANY, towxstring(name)), 0, wxGROW);
	sizer->Add(a_type = new wxComboBox(window, wxID_ANY, choices[defaultidx], wxDefaultPosition, wxDefaultSize,
		9, choices, wxCB_READONLY), 0, wxGROW);
	sizer->Add(a_low = new wxTextCtrl(window, wxID_ANY, towxstring(low)), 0, wxGROW);
	sizer->Add(a_mid = new wxTextCtrl(window, wxID_ANY, towxstring(mid)), 0, wxGROW);
	sizer->Add(a_high = new wxTextCtrl(window, wxID_ANY, towxstring(high)), 0, wxGROW);
	sizer->Add(a_tolerance = new wxTextCtrl(window, wxID_ANY, towxstring(tolerance)), 0, wxGROW);
	a_low->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wx_axes_editor::on_value_change), NULL,
		window);
	a_mid->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wx_axes_editor::on_value_change), NULL,
		window);
	a_high->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wx_axes_editor::on_value_change), NULL,
		window);
	a_tolerance->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wx_axes_editor::on_value_change), NULL,
		window);
}

bool wx_axes_editor_axis::is_ok()
{
	int32_t low, mid, high;
	double tolerance;

	try {
		low = boost::lexical_cast<int32_t>(tostdstring(a_low->GetValue()));
		mid = boost::lexical_cast<int32_t>(tostdstring(a_mid->GetValue()));
		high = boost::lexical_cast<int32_t>(tostdstring(a_high->GetValue()));
		tolerance = boost::lexical_cast<double>(tostdstring(a_tolerance->GetValue()));
	} catch(...) {
		return false;
	}

	if(low < -32768 || low > 32767 || low > mid)
		return false;
	if(mid < -32768 || mid > 32767 || mid > high)
		return false;
	if(high < -32768 || high > 32767)
		return false;
	if(tolerance <= 0 || tolerance >= 1)
		return false;
	return true;
}

void wx_axes_editor_axis::apply()
{
	keygroup* k = keygroup::lookup_by_name(a_name);
	if(!k)
		return;

	int32_t low, mid, high;
	double tolerance;
	enum keygroup::type ntype;
	enum keygroup::type ctype = k->get_parameters().ktype;;

	std::string amode = tostdstring(a_type->GetValue());
	if(amode == AMODE_AXIS_PAIR)
		ntype = keygroup::KT_AXIS_PAIR;
	if(amode == AMODE_AXIS_PAIR_INVERSE)
		ntype = keygroup::KT_AXIS_PAIR_INVERSE;
	if(amode == AMODE_DISABLED)
		ntype = keygroup::KT_DISABLED;
	if(amode == AMODE_PRESSURE_0M)
		ntype = keygroup::KT_PRESSURE_0M;
	if(amode == AMODE_PRESSURE_0P)
		ntype = keygroup::KT_PRESSURE_0P;
	if(amode == AMODE_PRESSURE_M0)
		ntype = keygroup::KT_PRESSURE_M0;
	if(amode == AMODE_PRESSURE_MP)
		ntype = keygroup::KT_PRESSURE_MP;
	if(amode == AMODE_PRESSURE_PM)
		ntype = keygroup::KT_PRESSURE_PM;
	if(amode == AMODE_PRESSURE_P0)
		ntype = keygroup::KT_PRESSURE_P0;
	try {
		low = boost::lexical_cast<int32_t>(tostdstring(a_low->GetValue()));
		mid = boost::lexical_cast<int32_t>(tostdstring(a_mid->GetValue()));
		high = boost::lexical_cast<int32_t>(tostdstring(a_high->GetValue()));
		tolerance = boost::lexical_cast<double>(tostdstring(a_tolerance->GetValue()));
	} catch(...) {
		return;
	}
	if(low < -32768 || low > 32767 || low > mid)
		return;
	if(mid < -32768 || mid > 32767 || mid > high)
		return;
	if(high < -32768 || high > 32767)
		return;
	if(tolerance <= 0 || tolerance >= 1)
		return;
	if(ctype != ntype)
		k->change_type(ntype);
	k->change_calibration(low, mid, high, tolerance);
}

wx_axes_editor::wx_axes_editor(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit axes"), wxDefaultPosition, wxSize(-1, -1))
{
	std::set<std::string> axisnames = keygroup::get_axis_set();

	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	wxFlexGridSizer* t_s = new wxFlexGridSizer(axisnames.size() + 1, 6, 0, 0);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Name")), 0, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Type")), 0, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Low")), 0, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Mid")), 0, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("High")), 0, wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Tolerance")), 0, wxGROW);
	for(auto i : axisnames)
		axes.push_back(new wx_axes_editor_axis(t_s, this, i));
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_axes_editor::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wx_axes_editor::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
}

wx_axes_editor::~wx_axes_editor()
{
	for(auto i : axes)
		delete i;
}

bool wx_axes_editor::ShouldPreventAppExit() const
{
	return false;
}

void wx_axes_editor::on_value_change(wxCommandEvent& e)
{
	bool all_ok = true;
	for(auto i : axes)
		all_ok = all_ok && i->is_ok();
	okbutton->Enable(all_ok);
}

void wx_axes_editor::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wx_axes_editor::on_ok(wxCommandEvent& e)
{
	for(auto i : axes)
		i->apply();
	EndModal(wxID_OK);
}
