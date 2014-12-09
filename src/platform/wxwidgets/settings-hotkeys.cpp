#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/command.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"

namespace
{
	enum
	{
		wxID_ADDKEY = wxID_HIGHEST + 1,
		wxID_DROPKEY
	};

	class wxeditor_esettings_hotkeys : public settings_tab
	{
	public:
		wxeditor_esettings_hotkeys(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_hotkeys();
		void on_add(wxCommandEvent& e);
		void on_drop(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
		void on_notify() { refresh(); }
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
	private:
		wxTreeCtrl* controls;
		wxButton* pri_button;
		wxButton* sec_button;
		std::map<string_list<char>, wxTreeItemId> items;
		std::map<string_list<char>, std::string> names;
		std::map<string_list<char>, keyboard::invbind*> realitems;
		void refresh();
		string_list<char> get_selection();
		wxTreeItemId get_item(const string_list<char>& i);
	};


	wxeditor_esettings_hotkeys::wxeditor_esettings_hotkeys(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, _inst)
	{
		CHECK_UI_THREAD;
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(controls = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT), 1, wxGROW);
		controls->SetMinSize(wxSize(500, 400));
		controls->Connect(wxEVT_COMMAND_TREE_SEL_CHANGED,
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_change), NULL, this);
		controls->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxeditor_esettings_hotkeys::on_mouse), NULL,
			this);
		controls->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_esettings_hotkeys::on_mouse), NULL,
			this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(pri_button = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		pri_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_add), NULL, this);
		pri_button->Enable(false);
		pbutton_s->Add(sec_button = new wxButton(this, wxID_ANY, wxT("Drop")), 0, wxGROW);
		sec_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_drop), NULL, this);
		sec_button->Enable(false);
		top_s->Add(pbutton_s, 0, wxGROW);

		items[string_list<char>()] = controls->AddRoot(wxT(""));

		refresh();
		top_s->SetSizeHints(this);
		Fit();
	}

	wxTreeItemId wxeditor_esettings_hotkeys::get_item(const string_list<char>& i)
	{
		CHECK_UI_THREAD;
		if(items.count(i) && items[i].IsOk())
			return items[i];
		return items[i] = controls->AppendItem(get_item(i.strip_one()), towxstring(i[i.size() - 1]));
	}

	string_list<char> wxeditor_esettings_hotkeys::get_selection()
	{
		CHECK_UI_THREAD;
		if(closing())
			return string_list<char>();
		string_list<char> sel;
		wxTreeItemId id = controls->GetSelection();
		for(auto i : items)
			if(i.second == id)
				sel = i.first;
		return sel;
	}

	void wxeditor_esettings_hotkeys::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		string_list<char> sel = get_selection();
		pri_button->Enable(realitems.count(sel));
		sec_button->Enable(realitems.count(sel));
	}

	wxeditor_esettings_hotkeys::~wxeditor_esettings_hotkeys()
	{
	}

	void wxeditor_esettings_hotkeys::on_add(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		string_list<char> sel = get_selection();
		std::string name = names.count(sel) ? names[sel] : "";
		if(name == "") {
			refresh();
			return;
		}
		try {
			keyboard::invbind* ik = realitems[sel];
			if(!ik) {
				refresh();
				return;
			}
			key_entry_dialog* d = new key_entry_dialog(this, inst, "Specify key for " + name, "", false);
			if(d->ShowModal() == wxID_CANCEL) {
				d->Destroy();
				return;
			}
			std::string key = d->getkey();
			d->Destroy();
			if(key != "")
				ik->append(key);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_hotkeys::on_drop(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		string_list<char> sel = get_selection();
		std::string name = names.count(sel) ? names[sel] : "";
		if(name == "") {
			refresh();
			return;
		}
		try {
			keyboard::invbind* ik = realitems[sel];
			if(!ik) {
				refresh();
				return;
			}
			std::vector<wxString> dropchoices;
			keyboard::keyspec tmp;
			unsigned idx = 0;
			while((tmp = (std::string)ik->get(idx++)))
				dropchoices.push_back(towxstring(clean_keystring(tmp)));
			idx = 0;
			if(dropchoices.size() > 1) {
				wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(this,
					towxstring("Select key to remove from set"), towxstring("Pick key to drop"),
				dropchoices.size(), &dropchoices[0]);
				if(d2->ShowModal() == wxID_CANCEL) {
					d2->Destroy();
					refresh();
					return;
				}
				idx = d2->GetSelection();
				d2->Destroy();
			}
			ik->clear(idx);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_hotkeys::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		if(e.GetId() == wxID_ADDKEY)
			on_add(e);
		else if(e.GetId() >= wxID_DROPKEY) {
			string_list<char> sel = get_selection();
			if(!realitems.count(sel))
				return;
			keyboard::invbind* ik = realitems[sel];
			if(!ik)
				return;
			ik->clear(e.GetId() - wxID_DROPKEY);
		}
		refresh();
	}

	void wxeditor_esettings_hotkeys::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		if(!e.RightUp() && !(e.LeftUp() && e.ControlDown()))
			return;
		string_list<char> sel = get_selection();
		if(!realitems.count(sel))
			return;
		keyboard::invbind* ik = realitems[sel];
		if(!ik)
			return;

		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_popup_menu), NULL, this);
		menu.Append(wxID_ADDKEY, towxstring("Add new key"));
		bool first = true;
		unsigned idx = 0;
		keyboard::keyspec tmp;
		while((tmp = ik->get(idx++))) {
			if(first)
				menu.AppendSeparator();
			first = false;
			menu.Append(wxID_DROPKEY + idx - 1, towxstring("Drop " + clean_keystring(tmp)));
		}
		PopupMenu(&menu);
	}

	void wxeditor_esettings_hotkeys::refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::map<keyboard::invbind*, std::list<keyboard::keyspec>> data;
		realitems.clear();
		auto x = inst.mapper->get_inverses();
		for(auto y : x) {
			string_list<char> key = split_on_codepoint(y->getname(), U'\u2023');
			names[key] = y->getname();
			realitems[key] = y;
			keyboard::keyspec tmp;
			unsigned idx = 0;
			while((tmp = y->get(idx++)))
				data[y].push_back(tmp);
		}
		//Delete no longer present stuff.
		for(auto i = items.rbegin(); i != items.rend(); i++) {
			auto j = realitems.lower_bound(i->first);
			if(j == realitems.end() || !i->first.prefix_of(j->first)) {
				//Delete this item.
				if(i->second.IsOk())
					controls->Delete(i->second);
				items[i->first] = wxTreeItemId();
			}
		}
		//Update the rest.
		for(auto i : realitems) {
			string_list<char> key = i.first;
			std::string text = key[key.size() - 1];
			auto& I = data[i.second];
			if(I.empty())
				text = text + " (not set)";
			else if(I.size() == 1)
				text = text + " (" + clean_keystring(*I.begin()) + ")";
			else {
				text = text + " (";
				for(auto i = I.begin(); i != I.end(); i++) {
					if(i != I.begin())
						text = text + ", ";
					text = text + clean_keystring(*i);
				}
				text = text + ")";
			}
			wxTreeItemId id = get_item(key);
			controls->SetItemText(id, towxstring(text));
		}
	}

	settings_tab_factory hotkeys("Hotkeys", [](wxWindow* parent, emulator_instance& _inst) -> settings_tab* {
		return new wxeditor_esettings_hotkeys(parent, _inst);
	});
}
