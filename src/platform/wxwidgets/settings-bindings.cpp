#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include <wx/defs.h>

namespace
{

	class wxeditor_esettings_bindings : public settings_tab
	{
	public:
		wxeditor_esettings_bindings(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_bindings();
		void on_add(wxCommandEvent& e);
		void on_edit(wxCommandEvent& e);
		void on_delete(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
	private:
		std::map<int, std::string> numbers;
		wxListBox* select;
		void refresh();
		std::set<std::string> settings;
		std::map<std::string, std::string> values;
		std::map<int, std::string> selections;
		std::string selected();
		wxButton* editbutton;
		wxButton* deletebutton;
		wxListBox* _settings;
	};

	wxeditor_esettings_bindings::wxeditor_esettings_bindings(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, _inst)
	{
		CHECK_UI_THREAD;
		wxButton* tmp;

		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(select = new wxListBox(this, wxID_ANY), 1, wxGROW);
		select->SetMinSize(wxSize(500, 400));
		select->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_bindings::on_change), NULL, this);
		select->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxeditor_esettings_bindings::on_mouse), NULL,
			this);
		select->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_esettings_bindings::on_mouse), NULL,
			this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(tmp = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_bindings::on_add),
			NULL, this);
		pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
		editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_bindings::on_edit), NULL, this);
		pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
		deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_bindings::on_delete), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		refresh();
		wxCommandEvent e;
		on_change(e);
		top_s->SetSizeHints(this);
		Fit();
	}

	wxeditor_esettings_bindings::~wxeditor_esettings_bindings()
	{
	}

	void wxeditor_esettings_bindings::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		if(e.GetId() == wxID_EDIT)
			on_edit(e);
		else if(e.GetId() == wxID_DELETE)
			on_delete(e);
	}

	void wxeditor_esettings_bindings::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		if(!e.RightUp() && !(e.LeftUp() && e.ControlDown()))
			return;
		if(selected() == "")
			return;
		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_bindings::on_popup_menu), NULL, this);
		menu.Append(wxID_EDIT, towxstring("Edit"));
		menu.AppendSeparator();
		menu.Append(wxID_DELETE, towxstring("Delete"));
		PopupMenu(&menu);
	}

	void wxeditor_esettings_bindings::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		bool enable = (selected() != "");
		editbutton->Enable(enable);
		deletebutton->Enable(enable);
	}

	void wxeditor_esettings_bindings::on_add(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		try {
			std::string name;
			key_entry_dialog* d = new key_entry_dialog(this, inst, "Specify new key", "", false);
			if(d->ShowModal() == wxID_CANCEL) {
				d->Destroy();
				throw 42;
			}
			name = d->getkey();
			d->Destroy();

			std::string newcommand = pick_text(this, "New binding", "Enter command for binding:", "");
			try {
				inst.mapper->set(name, newcommand);
			} catch(std::exception& e) {
				wxMessageBox(wxT("Error"), towxstring(std::string("Can't bind key: ") + e.what()),
					wxICON_EXCLAMATION);
			}
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_bindings::on_edit(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name = selected();
		if(name == "") {
			refresh();
			return;
		}
		try {
			std::string old_command_value = inst.mapper->get(name);
			std::string newcommand = pick_text(this, "Edit binding", "Enter new command for binding:",
				old_command_value);
			try {
				inst.mapper->set(name, newcommand);
			} catch(std::exception& e) {
				wxMessageBox(wxT("Error"), towxstring(std::string("Can't bind key: ") + e.what()),
					wxICON_EXCLAMATION);
			}
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_bindings::on_delete(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name = selected();
		if(name == "") {
			refresh();
			return;
		}
		try { inst.mapper->set(name, ""); } catch(...) {}
		refresh();
	}

	void wxeditor_esettings_bindings::refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		int n = select->GetSelection();
		std::map<std::string, std::string> bind;
		std::vector<wxString> choices;
		std::list<keyboard::keyspec> a = inst.mapper->get_bindings();
		for(auto i : a)
			bind[i] = inst.mapper->get(i);
		for(auto i : bind) {
			if(i.second == "")
				continue;
			numbers[choices.size()] = i.first;
			choices.push_back(towxstring(clean_keystring(i.first) + " (" + i.second + ")"));
		}
		select->Set(choices.size(), &choices[0]);
		if(n == wxNOT_FOUND && select->GetCount())
			select->SetSelection(0);
		else if(n >= (int)select->GetCount())
			select->SetSelection(select->GetCount() ? (int)(select->GetCount() - 1) : wxNOT_FOUND);
		else
			select->SetSelection(n);
		wxCommandEvent e;
		on_change(e);
		select->Refresh();
	}

	std::string wxeditor_esettings_bindings::selected()
	{
		CHECK_UI_THREAD;
		if(closing())
			return "";
		int x = select->GetSelection();
		if(numbers.count(x))
			return numbers[x];
		else
			return "";
	}

	settings_tab_factory bindings("Bindings", [](wxWindow* parent, emulator_instance& _inst) -> settings_tab* {
		return new wxeditor_esettings_bindings(parent, _inst);
	});
}
