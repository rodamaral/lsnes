#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>

#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/mainloop.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/window.hpp"

#include "interface/controller.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/utf8.hpp"
#include "lua/lua.hpp"

#include <algorithm>
#include <cstring>

class wxeditor_autohold : public wxDialog
{
public:
	wxeditor_autohold(wxWindow* parent, emulator_instance& _inst);
	~wxeditor_autohold() throw();
	bool ShouldPreventAppExit() const;
	void on_wclose(wxCloseEvent& e);
	void on_checkbox(wxCommandEvent& e);
private:
	struct dispatch::target<unsigned, unsigned, unsigned, bool> ahupdate;
	struct dispatch::target<unsigned, unsigned, unsigned, unsigned, unsigned> afupdate;
	struct dispatch::target<> ahreconfigure;

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
		bool afstatus;		//Used only by internal version.
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
	emulator_instance& inst;
	std::map<int, control_triple> autoholds;
	std::vector<controller_double> panels;
	void update_controls();
	bool closing;
	wxBoxSizer* hsizer;
};

namespace
{
	wxeditor_autohold* autohold_open;
}

wxeditor_autohold::~wxeditor_autohold() throw() {}

wxeditor_autohold::wxeditor_autohold(wxWindow* parent, emulator_instance& _inst)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Autohold/Autofire"), wxDefaultPosition, wxSize(-1, -1)),
	inst(_inst)
{
	CHECK_UI_THREAD;
	closing = false;
	Centre();
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(hsizer);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_autohold::on_wclose));
	update_controls();
	hsizer->SetSizeHints(this);
	Fit();

	ahupdate.set(inst.dispatch->autohold_update, [this](unsigned port, unsigned controller,
		unsigned ctrlnum, bool newstate) {
		runuifun([this, port, controller, ctrlnum, newstate]() {
			CHECK_UI_THREAD;
			for(auto i : this->autoholds) {
				if(i.second.port != port) continue;
				if(i.second.controller != controller) continue;
				if(i.second.index != ctrlnum) continue;
				i.second.check->SetValue(newstate);
			}
		});
	});
	afupdate.set(inst.dispatch->autofire_update, [this](unsigned port, unsigned controller,
		unsigned ctrlnum, unsigned duty, unsigned cyclelen) {
		runuifun([this, port, controller, ctrlnum, duty]() {
			CHECK_UI_THREAD;
			for(auto i : this->autoholds) {
				if(i.second.port != port) continue;
				if(i.second.controller != controller) continue;
				if(i.second.index != ctrlnum) continue;
				i.second.afcheck->SetValue(duty != 0);
			}
		});
	});
	ahreconfigure.set(inst.dispatch->autohold_reconfigure, [this]() {
		runuifun([this]() {
			CHECK_UI_THREAD;
			try {
				this->update_controls();
			} catch(std::runtime_error& e) {
				//Close the window.
				bool wasc = closing;
				closing = true;
				autohold_open = NULL;
				if(!wasc)
					Destroy();
			}
		});
	});
}

void wxeditor_autohold::on_checkbox(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId();
	if(!autoholds.count(id))
		return;
	auto t = autoholds[id];
	bool isaf = (t.afid == id);
	bool newstate = isaf ? t.afcheck->IsChecked() : t.check->IsChecked();
	bool state = false;
	inst.iqueue->run([t, newstate, &state, isaf]() {
		auto& core = CORE();
		if(isaf) {
			auto _state = core.controls->autofire2(t.port, t.controller, t.index);
			state = (_state.first != 0);
			if(core.lua2->callback_do_button(t.port, t.controller, t.index, newstate ? "autofire 1 2" :
				"autofire"))
				return;
			core.controls->autofire2(t.port, t.controller, t.index, newstate ? 1 : 0, newstate ? 2 : 1);
			state = newstate;
		} else {
			state = core.controls->autohold2(t.port, t.controller, t.index);
			if(core.lua2->callback_do_button(t.port, t.controller, t.index, newstate ? "hold" : "unhold"))
				return;
			core.controls->autohold2(t.port, t.controller, t.index, newstate);
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
	CHECK_UI_THREAD;
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
	autoholds.clear();
	panels.clear();
	std::vector<control_triple> _autoholds;
	std::vector<std::string> _controller_labels;
	inst.iqueue->run([&_autoholds, &_controller_labels](){
		auto& core = CORE();
		std::map<std::string, unsigned> next_in_class;
		portctrl::frame model = core.controls->get_blank();
		const portctrl::type_set& pts = model.porttypes();
		unsigned cnum_g = 0;
		for(unsigned i = 0;; i++) {
			auto pcid = core.controls->lcid_to_pcid(i);
			if(pcid.first < 0)
				break;
			const portctrl::type& pt = pts.port_type(pcid.first);
			const portctrl::controller_set& pci = *(pt.controller_info);
			if((ssize_t)pci.controllers.size() <= pcid.second)
				continue;
			const portctrl::controller& pc = pci.controllers[pcid.second];
			//First check that this has non-hidden buttons.
			bool has_buttons = false;
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const portctrl::button& pcb = pc.buttons[k];
				if(pcb.type == portctrl::button::TYPE_BUTTON && !pcb.shadow)
					has_buttons = true;
			}
			if(!has_buttons)
				continue;
			//Okay, a valid controller.
			if(!next_in_class.count(pc.cclass))
				next_in_class[pc.cclass] = 1;
			uint32_t cnum = next_in_class[pc.cclass]++;
			_controller_labels.push_back((stringfmt() << pc.cclass << "-" << cnum).str());
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const portctrl::button& pcb = pc.buttons[k];
				if(pcb.type != portctrl::button::TYPE_BUTTON || pcb.shadow)
					continue;
				struct control_triple t;
				t.port = pcid.first;
				t.controller = pcid.second;
				t.index = k;
				t.status = core.controls->autohold2(pcid.first, pcid.second, k);
				auto h = core.controls->autofire2(pcid.first, pcid.second, k);
				t.afstatus = (h.first > 0);
				t.logical = cnum_g;
				t.name = pcb.name;
				_autoholds.push_back(t);
			}
			cnum_g++;
		}
	});
	int next_id = wxID_HIGHEST + 1;
	unsigned last_logical = 0xFFFFFFFFUL;
	wxSizer* current = NULL;
	wxPanel* current_p = NULL;
	wxSizer* current_t = NULL;
	for(auto i : _autoholds) {
		if(i.logical != last_logical) {
			//New controller starts.
			if(current_t) {
				hsizer->Add(current_p);
				current_t->SetSizeHints(current_p);
				current_t->Fit(current_p);
			}
			controller_double d;
			current_p = d.panel = new wxPanel(this, wxID_ANY);
			current_t = d.rtop = new wxBoxSizer(wxVERTICAL);
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
		afcheck->SetValue(i.afstatus);
		afcheck->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wxeditor_autohold::on_checkbox), NULL, this);
		current->Add(label);
		current->Add(check);
		current->Add(afcheck);
		autoholds[next_id++] = t;
		autoholds[next_id++] = t;
	}
	if(current_t) {
		hsizer->Add(current_p);
		current_t->SetSizeHints(current_p);
		current_t->Fit(current_p);
	}
	if(_autoholds.empty()) {
		throw std::runtime_error("No controlers");
	}
	hsizer->SetSizeHints(this);
	hsizer->Layout();
	hsizer->Fit(this);
	Fit();
}

bool wxeditor_autohold::ShouldPreventAppExit() const { return false; }

void wxeditor_autohold::on_wclose(wxCloseEvent& e)
{
	CHECK_UI_THREAD;
	bool wasc = closing;
	closing = true;
	autohold_open = NULL;
	if(!wasc)
		Destroy();
}

void wxeditor_autohold_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	if(autohold_open)
		return;
	wxeditor_autohold* v;
	try {
		v = new wxeditor_autohold(parent, inst);
	} catch(std::runtime_error& e) {
		wxMessageBox(_T("No controllers present"), _T("Error"), wxICON_EXCLAMATION | wxOK, parent);
		return;
	}
	v->Show();
	autohold_open = v;
}
