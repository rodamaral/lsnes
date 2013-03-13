#include "core/controller.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/dispatch.hpp"
#include "core/window.hpp"

#include "interface/controller.hpp"
#include "core/mainloop.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/utf8.hpp"

#include <algorithm>
#include <cstring>
#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>

class wxeditor_autohold : public wxDialog, public information_dispatch
{
public:
	wxeditor_autohold(wxWindow* parent);
	~wxeditor_autohold() throw();
	bool ShouldPreventAppExit() const;
	void on_wclose(wxCloseEvent& e);
	void on_checkbox(wxCommandEvent& e);
	void on_autohold_update(unsigned port, unsigned controller, unsigned ctrlnum, bool newstate);
	void on_autofire_update(unsigned port, unsigned controller, unsigned ctrlnum, unsigned duty,
		unsigned cyclelen);
	void on_autohold_reconfigure();
private:
	struct control_triple
	{
		unsigned port;
		unsigned controller;
		unsigned index;
		int afid;
		wxStaticText* label;	//Used only by UI version.
		wxCheckBox* check;	//Used only by UI version.
		wxCheckBox* afcheck;	//Used only by UI version.
		bool status;		//Used only by internal version.
		unsigned logical;	//Logical controller. Internal only.
		std::string name;	//Name. Internal only.
	};
	struct controller_double
	{
		wxPanel* panel;
		wxStaticBox* box;
		wxStaticText* label;
		wxSizer* rtop;
		wxSizer* top;
		wxSizer* grid;
	};
	std::map<int, control_triple> autoholds;
	std::vector<controller_double> panels;
	wxStaticText* nocontrollers;
	void update_controls();
	bool closing;
	wxBoxSizer* hsizer;
};

namespace
{
	wxeditor_autohold* autohold_open;
}

wxeditor_autohold::~wxeditor_autohold() throw() {}

wxeditor_autohold::wxeditor_autohold(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Autohold/Autofire"), wxDefaultPosition, wxSize(-1, -1)),
	information_dispatch("autohold-listener")
{
	closing = false;
	Centre();
	nocontrollers = NULL;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(hsizer);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_autohold::on_wclose));
	update_controls();
	hsizer->SetSizeHints(this);
	Fit();
}

void wxeditor_autohold::on_autohold_update(unsigned port, unsigned controller, unsigned ctrlnum, bool newstate)
{
	runuifun([this, port, controller, ctrlnum, newstate]() { 
		for(auto i : this->autoholds) {
			if(i.second.port != port) continue;
			if(i.second.controller != controller) continue;
			if(i.second.index != ctrlnum) continue;
			i.second.check->SetValue(newstate);
		}
	});
}

void wxeditor_autohold::on_autofire_update(unsigned port, unsigned controller, unsigned ctrlnum, unsigned duty,
	unsigned cyclelen)
{
	runuifun([this, port, controller, ctrlnum, duty]() { 
		for(auto i : this->autoholds) {
			if(i.second.port != port) continue;
			if(i.second.controller != controller) continue;
			if(i.second.index != ctrlnum) continue;
			i.second.afcheck->SetValue(duty != 0);
		}
	});
}

void wxeditor_autohold::on_autohold_reconfigure()
{
	runuifun([this]() { this->update_controls(); });
}

void wxeditor_autohold::on_checkbox(wxCommandEvent& e)
{
	int id = e.GetId();
	if(!autoholds.count(id))
		return;
	auto t = autoholds[id];
	bool isaf = (t.afid == id);
	bool newstate = isaf ? t.afcheck->IsChecked() : t.check->IsChecked();
	bool state = false;
	runemufn([t, newstate, &state, isaf]() {
		if(isaf) {
			auto _state = controls.autofire2(t.port, t.controller, t.index);
			state = (_state.first != 0);
			if(lua_callback_do_button(t.port, t.controller, t.index, newstate ? "autofire 1 2" :
				"autofire"))
				return;
			controls.autofire2(t.port, t.controller, t.index, newstate ? 1 : 0, newstate ? 2 : 1);
			state = newstate;
		} else {
			state = controls.autohold2(t.port, t.controller, t.index);
			if(lua_callback_do_button(t.port, t.controller, t.index, newstate ? "hold" : "unhold"))
				return;
			controls.autohold2(t.port, t.controller, t.index, newstate);
			state = newstate;
		}
	});
	if(isaf)
		t.afcheck->SetValue(state);
	else
		t.check->SetValue(state);
}

void wxeditor_autohold::update_controls()
{
	if(nocontrollers)
		nocontrollers->Destroy();
	for(auto i : autoholds) {
		if(i.first != i.second.afid)
			i.second.label->Destroy();
		if(i.first != i.second.afid)
			i.second.check->Destroy();
		if(i.first == i.second.afid)
			i.second.afcheck->Destroy();
	}
	for(auto i : panels) {
		hsizer->Detach(i.panel);
		i.panel->Destroy();
	}
	nocontrollers = NULL;
	autoholds.clear();
	panels.clear();
	std::vector<control_triple> _autoholds;
	std::vector<std::string> _controller_labels;
	runemufn([&_autoholds, &_controller_labels](){
		std::map<std::string, unsigned> next_in_class;
		controller_frame model = controls.get_blank();
		const port_type_set& pts = model.porttypes();
		unsigned pcnt = pts.ports();
		unsigned cnum_g = 0;
		for(unsigned i = 0;; i++) {
			auto pcid = controls.lcid_to_pcid(i);
			if(pcid.first < 0)
				break;
			const port_type& pt = pts.port_type(pcid.first);
			const port_controller_set& pci = *(pt.controller_info);
			if(pci.controller_count <= pcid.second || !pci.controllers[pcid.second])
				continue;
			const port_controller& pc = *(pci.controllers[pcid.second]);
			//First check that this has non-hidden buttons.
			bool has_buttons = false;
			for(unsigned k = 0; k < pc.button_count; k++) {
				if(!pc.buttons[k])
					continue;
				const port_controller_button& pcb = *(pc.buttons[k]);
				if(pcb.type == port_controller_button::TYPE_BUTTON && !pcb.shadow)
					has_buttons = true;
			}
			if(!has_buttons)
				continue;
			//Okay, a valid controller.
			if(!next_in_class.count(pc.cclass))
				next_in_class[pc.cclass] = 1;
			uint32_t cnum = next_in_class[pc.cclass]++;
			_controller_labels.push_back((stringfmt() << pc.cclass << "-" << cnum).str());
			for(unsigned k = 0; k < pc.button_count; k++) {
				if(!pc.buttons[k])
					continue;
				const port_controller_button& pcb = *(pc.buttons[k]);
				if(pcb.type != port_controller_button::TYPE_BUTTON || pcb.shadow)
					continue;
				struct control_triple t;
				t.port = pcid.first;
				t.controller = pcid.second;
				t.index = k;
				t.status = controls.autohold2(pcid.first, pcid.second, k);
				t.logical = cnum_g;
				t.name = pcb.name;
				_autoholds.push_back(t);
			}
			cnum_g++;
		}
	});
	int next_id = wxID_HIGHEST + 1;
	unsigned last_logical = 0xFFFFFFFFUL;
	wxSizer* current;
	wxPanel* current_p;
	for(auto i : _autoholds) {
		if(i.logical != last_logical) {
			//New controller starts.
			controller_double d;
			current_p = d.panel = new wxPanel(this, wxID_ANY);
			d.rtop = new wxBoxSizer(wxVERTICAL);
			d.panel->SetSizer(d.rtop);
			d.box = new wxStaticBox(d.panel, wxID_ANY, towxstring(_controller_labels[i.logical]));
			d.top = new wxStaticBoxSizer(d.box, wxVERTICAL);
#ifdef __WXMAC__
			d.label = new wxStaticText(d.panel, wxID_ANY, towxstring(_controller_labels[i.logical]));
			d.top->Add(d.label);
#endif
			current = d.grid = new wxFlexGridSizer(0, 3, 0, 0);
			d.top->Add(d.grid);
			d.rtop->Add(d.top);
			hsizer->Add(d.panel);
			panels.push_back(d);
			last_logical = i.logical;
		}
		wxStaticText* label = new wxStaticText(current_p, wxID_ANY, towxstring(i.name));
		wxCheckBox* check = new wxCheckBox(current_p, next_id, wxT("Hold"));
		wxCheckBox* afcheck = new wxCheckBox(current_p, next_id + 1, wxT("Rapid"));
		struct control_triple t;
		t.port = i.port;
		t.controller = i.controller;
		t.index = i.index;
		t.label = label;
		t.check = check;
		t.afcheck = afcheck;
		t.afid = next_id + 1;
		check->SetValue(i.status);
		check->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,  wxCommandEventHandler(wxeditor_autohold::on_checkbox),
			NULL, this);
		afcheck->SetValue(i.status);
		afcheck->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wxeditor_autohold::on_checkbox), NULL, this);
		current->Add(label);
		current->Add(check);
		current->Add(afcheck);
		autoholds[next_id++] = t;
		autoholds[next_id++] = t;
	}
	if(_autoholds.empty()) {
		hsizer->Add(nocontrollers = new wxStaticText(this, wxID_ANY, wxT("No controllers")));
	}
	Fit();
}

bool wxeditor_autohold::ShouldPreventAppExit() const { return false; }

void wxeditor_autohold::on_wclose(wxCloseEvent& e)
{
	bool wasc = closing;
	closing = true;
	autohold_open = NULL;
	if(!wasc)
		Destroy();
}

void wxeditor_autohold_display(wxWindow* parent)
{
	if(autohold_open)
		return;
	wxeditor_autohold* v = new wxeditor_autohold(parent);
	v->Show();
	autohold_open = v;
}
