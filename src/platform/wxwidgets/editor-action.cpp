#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/mainloop.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "library/zip.hpp"

#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>

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
	params = _params;
		Centre();
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	for(auto i : params) {
		regex_results r;
		if(r = regex("string(:(.*))?", i.model)) {
			wxBoxSizer* tmp1 = new wxBoxSizer(wxHORIZONTAL);
			tmp1->Add(new wxStaticText(this, wxID_ANY, towxstring(i.name)), 0, wxGROW);
			wxTextCtrl* tmp2 = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(200, -1));
			controls.push_back(tmp2);
			tmp1->Add(tmp2, 1, wxGROW);
			tmp2->Connect(wxEVT_COMMAND_TEXT_UPDATED,
				wxCommandEventHandler(wxeditor_action::on_change), NULL, this);
			top_s->Add(tmp1, 0, wxGROW);
		} else if(r = regex("int:(-?[0-9]+),(-?[0-9]+)", i.model)) {
			int64_t low, high, v;
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
		} else if(i.model == "bool") {
			wxCheckBox* tmp2 = new wxCheckBox(this, wxID_ANY, towxstring(i.name));
			controls.push_back(tmp2);
			top_s->Add(tmp2, 0, wxGROW);
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
	std::list<interface_action_param>::iterator i;
	std::list<wxWindow*>::iterator j;
	for(i = params.begin(), j = controls.begin(); i != params.end() && j != controls.end(); i++, j++) {
		regex_results r;
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
			interface_action_paramval pv;
			pv.s = p;
			results.push_back(pv);
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
			interface_action_paramval pv;
			pv.i = v;
			results.push_back(pv);
		} else if(i->model == "bool") {
			bool b = reinterpret_cast<wxCheckBox*>(*j)->GetValue();
			interface_action_paramval pv;
			pv.b = b;
			results.push_back(pv);
		} else {
			show_message_ok(this, "Internal error", (stringfmt() << "Unknown parameter model in '"
				<< i->model << "'.").str(), wxICON_EXCLAMATION);
			return;
		}		
	}
	EndModal(wxID_OK);
}

void wxeditor_action::on_cancel(wxCommandEvent& e)
{
	EndModal(wxID_CANCEL);
}

void wxeditor_action::on_change(wxCommandEvent& e)
{
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
		} else if(i->model == "bool") {
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
	//Empty special case.
	if(params.empty())
		return std::vector<interface_action_paramval>();
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

