#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>

#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/mainloop.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "library/zip.hpp"
#include "library/json.hpp"
#include <stdexcept>

#include "platform/wxwidgets/platform.hpp"


class wxeditor_action : public wxDialog
{
public:
	wxeditor_action(wxWindow* parent, const std::string& label, const std::list<interface_action_param>& _params);
	std::vector<interface_action_paramval> get_results() { return results; }
	void on_ok(wxCommandEvent& e);
	void on_cancel(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
private:
	wxButton* ok;
	wxButton* cancel;
	std::list<interface_action_param> params;
	std::list<wxWindow*> controls;
	std::vector<interface_action_paramval> results;
};

wxeditor_action::wxeditor_action(wxWindow* parent, const std::string& label,
	const std::list<interface_action_param>& _params)
	: wxDialog(parent, wxID_ANY, towxstring("lsnes: Action " + label), wxDefaultPosition, wxSize(-1, -1))

{
	CHECK_UI_THREAD;
	params = _params;
	Centre();
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	for(auto i : params) {
		regex_results r;
		if(r = regex("string(:(.*))?", i.model)) {
			wxBoxSizer* tmp1 = new wxBoxSizer(wxHORIZONTAL);
			tmp1->Add(new wxStaticText(this, wxID_ANY, towxstring(i.name)), 0, wxGROW);
			wxTextCtrl* tmp2 = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition,
				wxSize(200, -1));
			controls.push_back(tmp2);
			tmp1->Add(tmp2, 1, wxGROW);
			tmp2->Connect(wxEVT_COMMAND_TEXT_UPDATED,
				wxCommandEventHandler(wxeditor_action::on_change), NULL, this);
			top_s->Add(tmp1, 0, wxGROW);
		} else if(r = regex("int:(-?[0-9]+),(-?[0-9]+)", i.model)) {
			int64_t low, high;
			try {
				low = parse_value<int64_t>(r[1]);
				high = parse_value<int64_t>(r[2]);
			} catch(...) {
				show_message_ok(this, "Internal error", (stringfmt() << "Unknown limits in '"
					<< i.model << "'.").str(), wxICON_EXCLAMATION);
				return;
			}
			wxBoxSizer* tmp1 = new wxBoxSizer(wxHORIZONTAL);
			tmp1->Add(new wxStaticText(this, wxID_ANY, towxstring(i.name)), 0, wxGROW);
			wxSpinCtrl* tmp2 = new wxSpinCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
				wxSP_ARROW_KEYS, low, high, low);
			controls.push_back(tmp2);
			tmp1->Add(tmp2, 1, wxGROW);
			tmp2->Connect(wxEVT_COMMAND_TEXT_UPDATED,
				wxCommandEventHandler(wxeditor_action::on_change), NULL, this);
			top_s->Add(tmp1, 0, wxGROW);
		} else if(r = regex("enum:(.*)", i.model)) {
			std::vector<wxString> choices;
			try {
				JSON::node e(r[1]);
				for(auto i : e) {
					if(i.type() == JSON::string)
						choices.push_back(towxstring(i.as_string8()));
					else if(i.type() == JSON::array)
						choices.push_back(towxstring(i.index(1).as_string8()));
					else
						throw std::runtime_error("Choice not array nor string");
				}
			} catch(std::exception& e) {
				show_message_ok(this, "Internal error", (stringfmt() << "JSON parse error parsing "
					<< "model: " << e.what()).str(), wxICON_EXCLAMATION);
				return;
			}
			wxBoxSizer* tmp1 = new wxBoxSizer(wxHORIZONTAL);
			tmp1->Add(new wxStaticText(this, wxID_ANY, towxstring(i.name)), 0, wxGROW);
			wxComboBox* tmp2 = new wxComboBox(this, wxID_ANY, choices[0], wxDefaultPosition,
				wxDefaultSize, choices.size(), &choices[0], wxCB_READONLY);
			controls.push_back(tmp2);
			tmp1->Add(tmp2, 1, wxGROW);
			tmp2->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
				wxCommandEventHandler(wxeditor_action::on_change), NULL, this);
			top_s->Add(tmp1, 0, wxGROW);
		} else if(regex_match("bool", i.model)) {
			wxCheckBox* tmp2 = new wxCheckBox(this, wxID_ANY, towxstring(i.name));
			controls.push_back(tmp2);
			top_s->Add(tmp2, 0, wxGROW);
		} else if(regex_match("toggle", i.model)) {
			//Nothing for toggles.
		} else {
			show_message_ok(this, "Internal error", (stringfmt() << "Unknown parameter model in '"
				<< i.model << "'.").str(), wxICON_EXCLAMATION);
			return;
		}
	}

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_action::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_action::on_cancel), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	top_s->SetSizeHints(this);
	wxCommandEvent d;
	on_change(d);
}

void wxeditor_action::on_ok(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::list<interface_action_param>::iterator i;
	std::list<wxWindow*>::iterator j;
	for(i = params.begin(), j = controls.begin(); i != params.end() && j != controls.end(); i++, j++) {
		regex_results r;
		interface_action_paramval pv;
		if(r = regex("string(:(.*))?", i->model)) {
			std::string p;
			try {
				p = tostdstring(reinterpret_cast<wxTextCtrl*>(*j)->GetValue());
				if(r[2] != "" && !regex_match(r[2], p)) {
					show_message_ok(this, "Error in parameters",
						(stringfmt() << "String (" << i->name << ") does not satisfy "
						<< "constraints.").str(), wxICON_EXCLAMATION);
					return;
				}
			} catch(...) {
				show_message_ok(this, "Internal error", (stringfmt() << "Bad constraint in '"
					<< i->model << "'.").str(), wxICON_EXCLAMATION);
				return;
			}
			pv.s = p;
		} else if(r = regex("int:([0-9]+),([0-9]+)", i->model)) {
			int64_t low, high, v;
			try {
				low = parse_value<int64_t>(r[1]);
				high = parse_value<int64_t>(r[2]);
			} catch(...) {
				show_message_ok(this, "Internal error", (stringfmt() << "Unknown limits in '"
					<< i->model << "'.").str(), wxICON_EXCLAMATION);
				return;
			}
			v = reinterpret_cast<wxSpinCtrl*>(*j)->GetValue();
			if(v < low || v > high) {
				show_message_ok(this, "Error in parameters",
					(stringfmt() << "Integer (" << i->name << ") out of range.").str(),
					wxICON_EXCLAMATION);
				return;
			}
			pv.i = v;
		} else if(r = regex("enum:(.*)", i->model)) {
			int v = reinterpret_cast<wxComboBox*>(*j)->GetSelection();
			if(v == wxNOT_FOUND) {
				show_message_ok(this, "Error in parameters",
					(stringfmt() << "No selection for '" << i->name << "'.").str(),
					wxICON_EXCLAMATION);
				return;
			}
			pv.i = v;
		} else if(regex_match("bool", i->model)) {
			pv.b = reinterpret_cast<wxCheckBox*>(*j)->GetValue();
		} else if(regex_match("toggle", i->model)) {
			//Empty.
		} else {
			show_message_ok(this, "Internal error", (stringfmt() << "Unknown parameter model in '"
				<< i->model << "'.").str(), wxICON_EXCLAMATION);
			return;
		}
		results.push_back(pv);
	}
	EndModal(wxID_OK);
}

void wxeditor_action::on_cancel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}

void wxeditor_action::on_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::list<interface_action_param>::iterator i;
	std::list<wxWindow*>::iterator j;
	for(i = params.begin(), j = controls.begin(); i != params.end() && j != controls.end(); i++, j++) {
		regex_results r;
		if(r = regex("string(:(.*))?", i->model)) {
			try {
				std::string p = tostdstring(reinterpret_cast<wxTextCtrl*>(*j)->GetValue());
				if(r[2] != "" && !regex_match(r[2], p)) {
					goto bad;
				}
			} catch(...) {
				goto bad;
			}
		} else if(r = regex("int:([0-9]+),([0-9]+)", i->model)) {
			int64_t low, high, v;
			try {
				low = parse_value<int64_t>(r[1]);
				high = parse_value<int64_t>(r[2]);
			} catch(...) {
				goto bad;
			}
			v = reinterpret_cast<wxSpinCtrl*>(*j)->GetValue();
			if(v < low || v > high)
				goto bad;
		} else if(r = regex("enum:(.*)", i->model)) {
			if(reinterpret_cast<wxComboBox*>(*j)->GetSelection() == wxNOT_FOUND)
				goto bad;
		} else if(regex_match("bool", i->model)) {
		} else if(regex_match("toggle", i->model)) {
		} else {
			goto bad;
		}
	}
	ok->Enable();
	return;
bad:
	ok->Disable();
}


std::vector<interface_action_paramval> prompt_action_params(wxWindow* parent, const std::string& label,
	const std::list<interface_action_param>& params)
{
	CHECK_UI_THREAD;
	//Empty special case.
	if(params.empty())
		return std::vector<interface_action_paramval>();
	//Another special case.
	if(params.size() == 1 && params.begin()->model == std::string("toggle")) {
		std::vector<interface_action_paramval> x;
		x.push_back(interface_action_paramval());
		return x;
	}
	modal_pause_holder hld;
	try {
		wxeditor_action* f = new wxeditor_action(parent, label, params);
		int r = f->ShowModal();
		if(r == wxID_CANCEL) {
			f->Destroy();
			throw canceled_exception();
		}
		auto p = f->get_results();
		f->Destroy();
		return p;
	} catch(canceled_exception& e) {
		throw;
	} catch(...) {
		throw canceled_exception();
	}
}
