#include "core/controller.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/multitrack.hpp"
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
#include <wx/spinctrl.h>

#define MTMODE_PRESERVE "Preserve"
#define MTMODE_OVERWRITE "Overwrite"
#define MTMODE_OR "OR"
#define MTMODE_XOR "XOR"

namespace
{
}

class wxeditor_multitrack : public wxDialog
{
public:
	wxeditor_multitrack(wxWindow* parent);
	~wxeditor_multitrack() throw();
	bool ShouldPreventAppExit() const;
	void on_wclose(wxCloseEvent& e);
	void on_control(wxCommandEvent& e);
private:
	struct dispatch::target<> ahreconfigure;
	struct dispatch::target<bool> ahmodechange;
	struct dispatch::target<unsigned, unsigned, int> ahmtchange;
	struct controller_info
	{
		unsigned port;
		unsigned controller;
		wxStaticText* text;
		wxComboBox* mode;
	};
	struct controller_info2
	{
		std::string name;
		unsigned port;
		unsigned controller;
	};
	std::vector<controller_info> controllers;
	void update_controls();
	bool closing;
	wxFlexGridSizer* vsizer;
	const port_type_set* typeset;
};

namespace
{
	wxeditor_multitrack* multitrack_open;
}

wxeditor_multitrack::~wxeditor_multitrack() throw() {}

wxeditor_multitrack::wxeditor_multitrack(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Multitrack recording"), wxDefaultPosition, wxSize(-1, -1))
{
	typeset = NULL;
	closing = false;
	Centre();
	vsizer = new wxFlexGridSizer(0, 2, 0, 0);
	SetSizer(vsizer);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_multitrack::on_wclose));
	update_controls();
	vsizer->SetSizeHints(this);
	Fit();

	ahreconfigure.set(notify_autohold_reconfigure, [this]() {
		if(typeset && *typeset == controls.get_blank().porttypes())
			return;  //Don't reconfigure if no change.
		lsnes_instance.mteditor.config_altered();
		runuifun([this]() {
			try {
				this->update_controls();
			} catch(std::runtime_error& e) {
				//Close the window.
				bool wasc = closing;
				closing = true;
				multitrack_open = NULL;
				runemufn([]() { lsnes_instance.mteditor.enable(false); });
				if(!wasc)
					Destroy();
			}
		});
	});
	ahmodechange.set(notify_mode_change, [this](bool readonly) {
		runuifun([this, readonly]() {
			for(auto i : controllers)
				i.mode->Enable(readonly);
		});
	});
	ahmtchange.set(notify_multitrack_change, [this](unsigned port, unsigned controller, int state) {
		runuifun([this, port, controller, state]() {
			for(auto i : controllers) {
				if(i.port == port && i.controller == controller) {
					auto cb = i.mode;
					if(state == multitrack_edit::MT_OR)
						cb->SetStringSelection(towxstring(MTMODE_OR));
					if(state == multitrack_edit::MT_OVERWRITE)
						cb->SetStringSelection(towxstring(MTMODE_OVERWRITE));
					if(state == multitrack_edit::MT_PRESERVE)
						cb->SetStringSelection(towxstring(MTMODE_PRESERVE));
					if(state == multitrack_edit::MT_XOR)
						cb->SetStringSelection(towxstring(MTMODE_XOR));
				}
			}
		});
	});
}

void wxeditor_multitrack::on_control(wxCommandEvent& e)
{
	int id = e.GetId();
	if(id < wxID_HIGHEST + 1)
		return;
	size_t ctrl = id - wxID_HIGHEST - 1;
	if(ctrl >= controllers.size())
		return;
	controller_info& ci = controllers[ctrl];
	std::string mode = tostdstring(ci.mode->GetStringSelection());
	runemufn([ci, mode]() {
		if(mode == MTMODE_PRESERVE)
			lsnes_instance.mteditor.set(ci.port, ci.controller, multitrack_edit::MT_PRESERVE);
		else if(mode == MTMODE_OVERWRITE)
			lsnes_instance.mteditor.set(ci.port, ci.controller, multitrack_edit::MT_OVERWRITE);
		else if(mode == MTMODE_OR)
			lsnes_instance.mteditor.set(ci.port, ci.controller, multitrack_edit::MT_OR);
		else if(mode == MTMODE_XOR)
			lsnes_instance.mteditor.set(ci.port, ci.controller, multitrack_edit::MT_XOR);
	});
}

void wxeditor_multitrack::update_controls()
{
	bool readonly = lsnes_instance.mlogic.get_movie().readonly_mode();

	for(auto i : controllers) {
		vsizer->Detach(i.text);
		vsizer->Detach(i.mode);
		i.text->Destroy();
		i.mode->Destroy();
	}
	controllers.clear();
	std::vector<controller_info2> info;
	runemufn([this, &info](){
		std::map<std::string, unsigned> next_in_class;
		controller_frame model = controls.get_blank();
		const port_type_set& pts = model.porttypes();
		typeset = &pts;
		unsigned cnum_g = 0;
		for(unsigned i = 0;; i++) {
			auto pcid = controls.lcid_to_pcid(i);
			if(pcid.first < 0)
				break;
			const port_type& pt = pts.port_type(pcid.first);
			const port_controller_set& pci = *(pt.controller_info);
			if((ssize_t)pci.controllers.size() <= pcid.second)
				continue;
			const port_controller& pc = pci.controllers[pcid.second];
			//First check that this has non-hidden stuff.
			bool has_buttons = false;
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const port_controller_button& pcb = pc.buttons[k];
				if(!pcb.shadow)
					has_buttons = true;
			}
			if(!has_buttons)
				continue;
			//Okay, a valid controller.
			if(!next_in_class.count(pc.cclass))
				next_in_class[pc.cclass] = 1;
			uint32_t cnum = next_in_class[pc.cclass]++;
			controller_info2 _info;
			_info.name = (stringfmt() << pc.cclass << "-" << cnum).str();
			_info.port = pcid.first;
			_info.controller = pcid.second;
			info.push_back(_info);
			cnum_g++;
		}
	});
	if(info.empty())
		throw std::runtime_error("No controllers");
	unsigned index = 0;
	for(auto i : info) {
		struct controller_info _info;
		_info.port = i.port;
		_info.controller = i.controller;
		_info.text = new wxStaticText(this, wxID_ANY, towxstring(i.name));
		vsizer->Add(_info.text, 0, wxGROW);
		std::vector<wxString> choices;
		choices.push_back(towxstring(MTMODE_PRESERVE));
		choices.push_back(towxstring(MTMODE_OVERWRITE));
		choices.push_back(towxstring(MTMODE_OR));
		choices.push_back(towxstring(MTMODE_XOR));
		_info.mode = new wxComboBox(this, wxID_HIGHEST + 1 + index, choices[0], wxDefaultPosition,
			wxDefaultSize, choices.size(), &choices[0], wxCB_READONLY);
		_info.mode->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxeditor_multitrack::on_control), NULL, this);
		if(!readonly)
			_info.mode->Enable(false);
		vsizer->Add(_info.mode, 0, wxGROW);
		controllers.push_back(_info);
		index++;
	}
	vsizer->Layout();
	Fit();
}

bool wxeditor_multitrack::ShouldPreventAppExit() const { return false; }

void wxeditor_multitrack::on_wclose(wxCloseEvent& e)
{
	bool wasc = closing;
	closing = true;
	multitrack_open = NULL;
	runemufn([]() { lsnes_instance.mteditor.enable(false); });
	if(!wasc)
		Destroy();
}

void wxeditor_multitrack_display(wxWindow* parent)
{
	if(multitrack_open)
		return;
	wxeditor_multitrack* v;
	try {
		v = new wxeditor_multitrack(parent);
	} catch(std::runtime_error& e) {
		wxMessageBox(_T("No controllers present"), _T("Error"), wxICON_EXCLAMATION | wxOK, parent);
		return;
	}
	v->Show();
	multitrack_open = v;
	runemufn([]() { lsnes_instance.mteditor.enable(true); });
}
