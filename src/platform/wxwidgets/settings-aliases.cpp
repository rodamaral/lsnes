#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/command.hpp"
#include "core/instance.hpp"
#include <wx/defs.h>

namespace
{
	class wxeditor_esettings_aliases : public settings_tab
	{
	public:
		wxeditor_esettings_aliases(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_aliases();
		void on_add(wxCommandEvent& e);
		void on_edit(wxCommandEvent& e);
		void on_delete(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
	private:
		std::map<int, std::string> numbers;
		wxListBox* select;
		wxButton* editbutton;
		wxButton* deletebutton;
		void refresh();
		std::string selected();
	};

	wxeditor_esettings_aliases::wxeditor_esettings_aliases(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, _inst)
	{
		CHECK_UI_THREAD;
		wxButton* tmp;

		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(select = new wxListBox(this, wxID_ANY), 1, wxGROW);
		select->SetMinSize(wxSize(300, 400));
		select->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_aliases::on_change), NULL, this);
		select->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxeditor_esettings_aliases::on_mouse), NULL,
			this);
		select->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_esettings_aliases::on_mouse), NULL,
			this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(tmp = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxeditor_esettings_aliases::on_add),
			NULL, this);
		pbutton_s->Add(editbutton = new wxButton(this, wxID_ANY, wxT("Edit")), 0, wxGROW);
		editbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_aliases::on_edit), NULL, this);
		pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
		deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_aliases::on_delete), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		refresh();
		wxCommandEvent e;
		on_change(e);
		top_s->SetSizeHints(this);
		Fit();
	}

	wxeditor_esettings_aliases::~wxeditor_esettings_aliases()
	{
	}

	void wxeditor_esettings_aliases::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		bool enable = (selected() != "");
		editbutton->Enable(enable);
		deletebutton->Enable(enable);
	}

	void wxeditor_esettings_aliases::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		if(e.GetId() == wxID_EDIT)
			on_edit(e);
		else if(e.GetId() == wxID_DELETE)
			on_delete(e);
	}

	void wxeditor_esettings_aliases::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		if(!e.RightUp() && !(e.LeftUp() && e.ControlDown()))
			return;
		if(selected() == "")
			return;
		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_aliases::on_popup_menu), NULL, this);
		menu.Append(wxID_EDIT, towxstring("Edit"));
		menu.AppendSeparator();
		menu.Append(wxID_DELETE, towxstring("Delete"));
		PopupMenu(&menu);
	}

	void wxeditor_esettings_aliases::on_add(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		try {
			std::string name = pick_text(this, "Enter alias name", "Enter name for the new alias:");
			if(!inst.command->valid_alias_name(name)) {
				show_message_ok(this, "Error", "Not a valid alias name: " + name, wxICON_EXCLAMATION);
				throw canceled_exception();
			}
			std::string old_alias_value = inst.command->get_alias_for(name);
			std::string newcmd = pick_text(this, "Edit alias", "Enter new commands for '" + name + "':",
				old_alias_value, true);
			inst.command->set_alias_for(name, newcmd);
			(*inst.abindmanager)();
			do_notify();
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_aliases::on_edit(wxCommandEvent& e)
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
			std::string old_alias_value = inst.command->get_alias_for(name);
			std::string newcmd = pick_text(this, "Edit alias", "Enter new commands for '" + name + "':",
				old_alias_value, true);
			inst.command->set_alias_for(name, newcmd);
			(*inst.abindmanager)();
			do_notify();
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_aliases::on_delete(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name = selected();
		if(name == "") {
			refresh();
			return;
		}
		inst.command->set_alias_for(name, "");
		(*inst.abindmanager)();
		do_notify();
		refresh();
	}

	void wxeditor_esettings_aliases::refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		int n = select->GetSelection();
		std::set<std::string> bind;
		std::vector<wxString> choices;
		bind = inst.command->get_aliases();
		for(auto i : bind) {
			numbers[choices.size()] = i;
			choices.push_back(towxstring(i));
		}
		select->Set(choices.size(), &choices[0]);
		if(n == wxNOT_FOUND && select->GetCount())
			select->SetSelection(0);
		else if(n >= (int)select->GetCount())
			select->SetSelection(select->GetCount() ? (select->GetCount() - 1) : wxNOT_FOUND);
		else
			select->SetSelection(n);
		wxCommandEvent e;
		on_change(e);
		select->Refresh();
	}

	std::string wxeditor_esettings_aliases::selected()
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

	settings_tab_factory aliases("Aliases", [](wxWindow* parent, emulator_instance& _inst) -> settings_tab* {
		return new wxeditor_esettings_aliases(parent, _inst);
	});
}
