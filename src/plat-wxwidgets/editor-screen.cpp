#include "plat-wxwidgets/platform.hpp"
#include "library/string.hpp"

#include <wx/wx.h>
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

const char* algo_choices[] = {"Fast Bilinear", "Bilinear", "Bicubic", "Experimential", "Point", "Area",
	"Bicubic-Linear", "Gauss", "Sinc", "Lanczos", "Spline"};

class wxeditor_screen : public wxDialog
{
public:
	wxeditor_screen(wxWindow* parent, double& _horiz, double& _vert, int& _flags);
	~wxeditor_screen();
	bool ShouldPreventAppExit() const;
	void on_value_change(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_ok(wxCommandEvent& e);
private:
	double& horiz;
	double& vert;
	int& flags;
	wxButton* okbutton;
	wxButton* cancel;
	wxTextCtrl* horizbox;
	wxTextCtrl* vertbox;
	wxComboBox* algo;
};

wxeditor_screen::wxeditor_screen(wxWindow* parent, double& _horiz, double& _vert, int& _flags)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Screen scaling"), wxDefaultPosition, wxSize(-1, -1)),
	horiz(_horiz), vert(_vert), flags(_flags)
{
	std::set<std::string> axisnames;
	std::string h_x, v_x;
	int algoidx;
	wxString _algo_choices[sizeof(algo_choices) / sizeof(algo_choices[0])];

	for(size_t i = 0; i < sizeof(_algo_choices) / sizeof(_algo_choices[0]); i++)
		_algo_choices[i] = towxstring(algo_choices[i]);

	h_x = (stringfmt() << _horiz).str();
	v_x = (stringfmt() << _vert).str();
	algoidx = 0;
	for(size_t i = 0; i < sizeof(algo_choices) / sizeof(_algo_choices[0]); i++)
		if(_flags & (1 << i)) {
			algoidx = i;
			break;
		}
	
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	wxFlexGridSizer* t_s = new wxFlexGridSizer(3, 2, 0, 0);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Horizontal factor:")), 0, wxGROW);
	t_s->Add(horizbox = new wxTextCtrl(this, wxID_ANY, towxstring(h_x), wxDefaultPosition, wxSize(100, -1)), 1,
		wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Vertical factor:")), 0, wxGROW);
	t_s->Add(vertbox = new wxTextCtrl(this, wxID_ANY, towxstring(v_x), wxDefaultPosition, wxSize(100, -1)), 1,
		wxGROW);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Method:")), 0, wxGROW);
	t_s->Add(algo = new wxComboBox(this, wxID_ANY, _algo_choices[algoidx], wxDefaultPosition, 
		wxDefaultSize, sizeof(_algo_choices) / sizeof(_algo_choices[0]), _algo_choices, wxCB_READONLY), 1,
		wxGROW);
	top_s->Add(t_s);

	horizbox->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wxeditor_screen::on_value_change), NULL,
		this);
	vertbox->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(wxeditor_screen::on_value_change), NULL,
		this);
	algo->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxeditor_screen::on_value_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_screen::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_screen::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	t_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Fit();
	
}

wxeditor_screen::~wxeditor_screen()
{
}

bool wxeditor_screen::ShouldPreventAppExit() const
{
	return false;
}

void wxeditor_screen::on_value_change(wxCommandEvent& e)
{
	double hscale;
	double vscale;
	int newflags = 0;
	bool valid = true;
	try {
		hscale = parse_value<double>(tostdstring(horizbox->GetValue()));
		vscale = parse_value<double>(tostdstring(vertbox->GetValue()));
		if(hscale < 0.25 || hscale > 10)
			throw std::runtime_error("Specified scale out of range");
		if(vscale < 0.25 || vscale > 10)
			throw std::runtime_error("Specified scale out of range");
		std::string a = tostdstring(algo->GetValue());
		for(size_t i = 0; i < sizeof(algo_choices) / sizeof(algo_choices[0]); i++)
			if(a == algo_choices[i])
				newflags = (1 << i);
		if(newflags == 0)
			throw std::runtime_error("Specified algorithm invalid");
	} catch(...) {
		valid = false;
	}
	okbutton->Enable(valid);
}

void wxeditor_screen::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_screen::on_ok(wxCommandEvent& e)
{
	double hscale;
	double vscale;
	int newflags = 0;

	try {
		hscale = parse_value<double>(tostdstring(horizbox->GetValue()));
		vscale = parse_value<double>(tostdstring(vertbox->GetValue()));
		if(hscale < 0.25 || hscale > 10)
			throw std::runtime_error("Specified scale out of range");
		if(vscale < 0.25 || vscale > 10)
			throw std::runtime_error("Specified scale out of range");
		std::string a = tostdstring(algo->GetValue());
		for(size_t i = 0; i < sizeof(algo_choices) / sizeof(algo_choices[0]); i++)
			if(a == algo_choices[i])
				newflags = (1 << i);
		if(newflags == 0)
			throw std::runtime_error("Specified algorithm invalid");
	} catch(...) {
		return;
	}

	horiz = hscale;
	vert = vscale;
	flags = newflags;
	EndModal(wxID_OK);
}



void wxeditor_screen_display(wxWindow* parent, double& horiz, double& vert, int& flags)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_screen(parent, horiz, vert, flags);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}
