#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "core/controller.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/mainloop.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/ui-services.hpp"
#include "library/zip.hpp"
#include "library/minmax.hpp"
#include "library/json.hpp"

#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"

namespace
{
	std::map<unsigned, const portctrl::controller*> get_controller_set(emulator_instance& inst)
	{
		std::map<unsigned, const portctrl::controller*> r;
		const portctrl::type_set& s = inst.controls->get_blank().porttypes();
		for(unsigned i = 0; i < s.number_of_controllers(); i++) {
			auto g = s.lcid_to_pcid(i);
			if(g.first < 0)
				continue;
			portctrl::controller_set* pcs = s.port_type(g.first).controller_info;
			if(g.second >= pcs->controllers.size())
				continue;
			r[i] = &pcs->controllers[g.second];
		}
		return r;
	}

	std::string summarize_controller(unsigned lcid, const JSON::node& c)
	{
		std::ostringstream s;
		bool first = true;
		uint64_t acnt = 0;
		s << "#" << (lcid + 1) << " [";
		for(auto i : c) {
			if(i.type() == JSON::number)
				acnt = max(acnt, i.as_uint() + 1);
			if(i.type() != JSON::string)
				continue;
			if(!first)
				s << ", ";
			first = false;
			s << i.as_string8();
		}
		if(acnt) {
			if(!first)
				s << ", ";
			if(acnt == 1)
				s << "1 analog";
			else
				s << acnt << " analogs";
		}
		s << "]";
		return s.str();
	}

	class wxeditor_macro_1 : public wxDialog
	{
	public:
		wxeditor_macro_1(wxWindow* parent, emulator_instance& _inst, const std::string& title,
			const portctrl::macro& m);
		void on_ok(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e);
		void on_macro_edit(wxCommandEvent& e);
		portctrl::macro get_macro() { return curmacro; }
	private:
		emulator_instance& inst;
		std::vector<wxCheckBox*> enabled;
		std::vector<wxTextCtrl*> macros;
		wxButton* okbutton;
		wxButton* cancelbutton;
		wxRadioButton* rb_overwrite;
		wxRadioButton* rb_or;
		wxRadioButton* rb_xor;
		portctrl::macro curmacro;
		bool constructing;
	};

	wxeditor_macro_1::wxeditor_macro_1(wxWindow* parent, emulator_instance& _inst, const std::string& title,
		const portctrl::macro& m)
		: wxDialog(parent, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1)), inst(_inst)
	{
		CHECK_UI_THREAD;
		constructing = true;
		curmacro = m;
		Centre();
		wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		size_t ctrlsize = 0;
		for(auto i : curmacro.macros)
			ctrlsize = max(ctrlsize, (size_t)(i.first + 1));
		enabled.resize(ctrlsize);
		macros.resize(ctrlsize);
		for(auto i : curmacro.macros) {
			top_s->Add(new wxStaticText(this, wxID_ANY, towxstring(summarize_controller(i.first,
				i.second._descriptor))), 0, wxGROW);
			wxBoxSizer* tmp = new wxBoxSizer(wxHORIZONTAL);
			tmp->Add(enabled[i.first] = new wxCheckBox(this, wxID_ANY, wxT("Enabled")), 0, wxGROW);
			enabled[i.first]->SetValue(curmacro.macros.count(i.first) &&
				curmacro.macros[i.first].enabled);
			tmp->Add(macros[i.first] = new wxTextCtrl(this, wxID_ANY,
				towxstring(curmacro.macros.count(i.first) ? curmacro.macros[i.first].orig : ""),
				wxDefaultPosition, wxSize(400, -1)), 0, wxGROW);
			macros[i.first]->Connect(wxEVT_COMMAND_TEXT_UPDATED,
				wxCommandEventHandler(wxeditor_macro_1::on_macro_edit), NULL, this);
			top_s->Add(tmp, 1, wxGROW);
		}

		wxBoxSizer* tmp2 = new wxBoxSizer(wxHORIZONTAL);
		rb_overwrite = new wxRadioButton(this, wxID_HIGHEST + 1, wxT("Overwrite"), wxDefaultPosition,
			wxDefaultSize, wxRB_GROUP);
		rb_or = new wxRadioButton(this, wxID_HIGHEST + 1, wxT("OR"));
		rb_xor = new wxRadioButton(this, wxID_HIGHEST + 1, wxT("XOR"));
		tmp2->Add(rb_overwrite, 0, wxGROW);
		tmp2->Add(rb_or, 0, wxGROW);
		tmp2->Add(rb_xor, 0, wxGROW);
		top_s->Add(tmp2, 0, wxGROW);

		switch(curmacro.amode) {
		case portctrl::macro_data::AM_OVERWRITE:	rb_overwrite->SetValue(true); break;
		case portctrl::macro_data::AM_OR:		rb_or->SetValue(true); break;
		case portctrl::macro_data::AM_XOR:		rb_xor->SetValue(true); break;
		};

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
		pbutton_s->Add(cancelbutton = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
		okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_macro_1::on_ok), NULL, this);
		cancelbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_macro_1::on_cancel), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		top_s->SetSizeHints(this);
		Fit();

		constructing = false;
		Show();
	}

	void wxeditor_macro_1::on_ok(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		portctrl::macro m;
		if(rb_overwrite->GetValue()) m.amode = portctrl::macro_data::AM_OVERWRITE;
		if(rb_or->GetValue()) m.amode = portctrl::macro_data::AM_OR;
		if(rb_xor->GetValue()) m.amode = portctrl::macro_data::AM_XOR;
		try {
			for(auto i : curmacro.macros) {
				if(!macros[i.first])
					continue;
				m.macros[i.first] = portctrl::macro_data(tostdstring(macros[i.first]->GetValue()),
					curmacro.macros[i.first].get_descriptor(), i.first);
				m.macros[i.first].enabled = enabled[i.first]->GetValue();
			}
		} catch(std::exception& e) {
			wxMessageBox(towxstring(e.what()), _T("Error parsing macro"), wxICON_EXCLAMATION | wxOK,
				this);
			return;
		}
		curmacro = m;
		EndModal(wxID_OK);
	}

	void wxeditor_macro_1::on_cancel(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		EndModal(wxID_CANCEL);
	}

	void wxeditor_macro_1::on_macro_edit(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(constructing)
			return;
		bool ret = true;
		auto c = get_controller_set(inst);
		for(auto i : curmacro.macros) {
			if(!portctrl::macro_data::syntax_check(tostdstring(macros[i.first]->GetValue()).c_str(),
				curmacro.macros[i.first].get_descriptor()))
				ret = false;
		}
		okbutton->Enable(ret);
	}
}

class wxeditor_macro : public wxDialog
{
public:
	wxeditor_macro(wxWindow* parent, emulator_instance& _inst);
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void on_change(wxCommandEvent& e);
	void on_add(wxCommandEvent& e);
	void on_edit(wxCommandEvent& e);
	void on_rename(wxCommandEvent& e);
	void on_delete(wxCommandEvent& e);
	void on_load(wxCommandEvent& e);
	void on_save(wxCommandEvent& e);
private:
	bool do_edit(const std::string& mname, portctrl::macro& m);
	void update();
	emulator_instance& inst;
	wxButton* closebutton;
	wxButton* addbutton;
	wxButton* editbutton;
	wxButton* renamebutton;
	wxButton* deletebutton;
	wxButton* savebutton;
	wxButton* loadbutton;
	wxListBox* macros;
	std::vector<std::string> macronames;
};

bool wxeditor_macro::ShouldPreventAppExit() const
{
	return false;
}

wxeditor_macro::wxeditor_macro(wxWindow* parent, emulator_instance& _inst)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit macros"), wxDefaultPosition, wxSize(-1, -1)), inst(_inst)
{
	CHECK_UI_THREAD;
	Centre();
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	SetSizer(top_s);

	top_s->Add(macros = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(300, 400), 0, NULL,
		wxLB_SINGLE), 1, wxGROW);
	macros->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
		wxCommandEventHandler(wxeditor_macro::on_change), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(addbutton = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
	pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
	pbutton_s->Add(renamebutton = new wxButton(this, wxID_ANY, wxT("Rename")), 0, wxGROW);
	pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
	pbutton_s->Add(savebutton = new wxButton(this, wxID_ANY, wxT("Save")), 0, wxGROW);
	pbutton_s->Add(loadbutton = new wxButton(this, wxID_ANY, wxT("Load")), 0, wxGROW);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(closebutton = new wxButton(this, wxID_OK, wxT("Close")), 0, wxGROW);
	addbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_add), NULL, this);
	editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_edit), NULL, this);
	renamebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_rename), NULL, this);
	deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_delete), NULL, this);
	savebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_save), NULL, this);
	loadbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_load), NULL, this);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_macro::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	top_s->SetSizeHints(this);
	Fit();

	update();
	Show();
}

void wxeditor_macro::update()
{
	CHECK_UI_THREAD;
	std::set<std::string> macro_list = inst.controls->enumerate_macro();
	std::string current;
	int sel = macros->GetSelection();
	int selpos = -1;
	int idx = 0;
	if(sel != wxNOT_FOUND)
		current = macronames[sel];
	macros->Clear();
	macronames.clear();
	for(auto i : macro_list) {
		macronames.push_back(i);
		macros->Append(towxstring(i));
		if(i == current)
			selpos = idx;
		idx++;
	}
	if(!macro_list.empty()) {
		if(selpos >= 0)
			macros->SetSelection(selpos);
		if(sel < idx)
			macros->SetSelection(sel);
		else
			macros->SetSelection(idx - 1);
	}
	wxCommandEvent e;
	on_change(e);
}

void wxeditor_macro::on_close(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_OK);
}

void wxeditor_macro::on_change(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = macros->GetSelection();
	editbutton->Enable(sel != wxNOT_FOUND);
	deletebutton->Enable(sel != wxNOT_FOUND);
	savebutton->Enable(sel != wxNOT_FOUND);
}

void wxeditor_macro::on_delete(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = macros->GetSelection();
	if(sel == wxNOT_FOUND)
		return;
	std::string macro = macronames[sel];
	inst.controls->erase_macro(macro);
	update();
}

void wxeditor_macro::on_add(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string mname = pick_text(this, "Name new macro", "Enter name for the new macro:", "");
		if(mname == "")
			return;
		portctrl::macro _macro;
		_macro.amode = portctrl::macro_data::AM_XOR;
		auto c = get_controller_set(inst);
		for(auto i : c) {
			_macro.macros[i.first] = portctrl::macro_data("",
				portctrl::macro_data::make_descriptor(*i.second), i.first);
			_macro.macros[i.first].enabled = false;
		}
		if(do_edit("", _macro))
			inst.controls->set_macro(mname, _macro);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		wxMessageBox(towxstring(e.what()), _T("Error creating macro"), wxICON_EXCLAMATION | wxOK, this);
	}
	update();
}

void wxeditor_macro::on_edit(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = macros->GetSelection();
	if(sel == wxNOT_FOUND)
		return;
	std::string macro = macronames[sel];
	portctrl::macro _macro;
	try {
		_macro = inst.controls->get_macro(macro);
	} catch(...) {
		return;
	}
	if(do_edit(macro, _macro))
		inst.controls->set_macro(macro, _macro);
}

void wxeditor_macro::on_rename(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = macros->GetSelection();
	if(sel == wxNOT_FOUND)
		return;
	std::string macro = macronames[sel];
	try {
		std::string mname = pick_text(this, "Rename macro", "Enter new name for the macro:", "");
		if(mname == "")
			return;
		inst.controls->rename_macro(macro, mname);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		wxMessageBox(towxstring(e.what()), _T("Error renaming macro"), wxICON_EXCLAMATION | wxOK, this);
	}
	update();
}

void wxeditor_macro::on_load(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	try {
		std::string mname = pick_text(this, "Name new macro", "Enter name for the new macro:", "");
		if(mname == "")
			return;
		std::string file = choose_file_load(this, "Load macro from", UI_get_project_otherpath(inst),
			filetype_macro);
		std::vector<char> contents = zip::readrel(file, "");
		portctrl::macro m(JSON::node(std::string(contents.begin(), contents.end())));
		inst.controls->set_macro(mname, m);
	} catch(canceled_exception& e) {
	} catch(std::exception& e) {
		wxMessageBox(towxstring(e.what()), _T("Error loading macro"), wxICON_EXCLAMATION | wxOK, this);
	}
	update();
}

void wxeditor_macro::on_save(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int sel = macros->GetSelection();
	if(sel == wxNOT_FOUND)
		return;
	std::string macro = macronames[sel];
	portctrl::macro* _macro;
	try {
		_macro = &inst.controls->get_macro(macro);
	} catch(...) {
		return;
	}
	std::string mdata = _macro->serialize().serialize();
	//Okay, have the macro data, now prompt for file and save.
	try {
		std::string tfile = choose_file_save(this, "Save macro to", UI_get_project_otherpath(inst),
			filetype_macro);
		std::ofstream f(tfile);
		f << mdata;
		if(!f)
			wxMessageBox(towxstring("Error saving macro"), _T("Error"), wxICON_EXCLAMATION | wxOK, this);
	} catch(canceled_exception& e) {
	}
}

bool wxeditor_macro::do_edit(const std::string& mname, portctrl::macro& m)
{
	CHECK_UI_THREAD;
	wxeditor_macro_1* editor;
	bool ret;

	try {
		std::string title;
		if(mname != "")
			title = "Editing macro " + mname;
		else
			title = "Create a new macro";
		editor = new wxeditor_macro_1(this, inst, title, m);
		ret = (editor->ShowModal() == wxID_OK);
		if(ret)
			m = editor->get_macro();
	} catch(...) {
		return false;
	}
	editor->Destroy();
	return ret;
}

void wxeditor_macro_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_macro(parent, inst);
		editor->ShowModal();
	} catch(...) {
		return;
	}
	editor->Destroy();
}
