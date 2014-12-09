#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "library/string.hpp"

namespace
{
	enum
	{
		wxID_ADDKEY = wxID_HIGHEST + 1,
		wxID_DROPKEY
	};

	class wxeditor_esettings_controllers : public settings_tab
	{
	public:
		wxeditor_esettings_controllers(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_controllers();
		void on_setkey(wxCommandEvent& e);
		void on_clearkey(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
	private:
		wxTreeCtrl* controls;
		std::map<string_list<char>, wxTreeItemId> items;
		std::map<string_list<char>, std::string> names;
		std::map<string_list<char>, keyboard::ctrlrkey*> realitems;
		wxButton* set_button;
		wxButton* clear_button;
		void refresh();
		string_list<char> get_selection();
		wxTreeItemId get_item(const string_list<char>& i);
	};

	wxeditor_esettings_controllers::wxeditor_esettings_controllers(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, _inst)
	{
		CHECK_UI_THREAD;
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(controls = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxTR_HIDE_ROOT | wxTR_LINES_AT_ROOT), 1, wxGROW);
		controls->SetMinSize(wxSize(300, 400));
		controls->Connect(wxEVT_COMMAND_TREE_SEL_CHANGED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_change), NULL, this);
		controls->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxeditor_esettings_controllers::on_mouse), NULL,
			this);
		controls->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_esettings_controllers::on_mouse), NULL,
			this);
		controls->SetMinSize(wxSize(400, 300));

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(set_button = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		set_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_setkey), NULL, this);
		set_button->Enable(false);
		pbutton_s->Add(clear_button = new wxButton(this, wxID_ANY, wxT("Drop")), 0, wxGROW);
		clear_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_clearkey), NULL, this);
		clear_button->Enable(false);
		top_s->Add(pbutton_s, 0, wxGROW);

		items[string_list<char>()] = controls->AddRoot(wxT(""));

		refresh();
		top_s->SetSizeHints(this);
		Fit();
	}

	wxTreeItemId wxeditor_esettings_controllers::get_item(const string_list<char>& i)
	{
		CHECK_UI_THREAD;
		if(i.size() == 1)
			return items[string_list<char>()];
		if(items.count(i) && items[i].IsOk())
			return items[i];
		return items[i] = controls->AppendItem(get_item(i.strip_one()), towxstring(i[i.size() - 1]));
	}

	string_list<char> wxeditor_esettings_controllers::get_selection()
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

	void wxeditor_esettings_controllers::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		string_list<char> sel = get_selection();
		set_button->Enable(realitems.count(sel));
		clear_button->Enable(realitems.count(sel));
	}

	wxeditor_esettings_controllers::~wxeditor_esettings_controllers()
	{
	}

	void wxeditor_esettings_controllers::on_setkey(wxCommandEvent& e)
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
			keyboard::ctrlrkey* ik = realitems[sel];
			if(!ik) {
				refresh();
				return;
			}
			bool axis = ik->is_axis();
			std::string wtitle = (axis ? "Specify axis for " : "Specify key for ") + name;
			press_button_dialog* p = new press_button_dialog(this, inst, wtitle, axis);
			p->ShowModal();
			std::string key = p->getkey();
			p->Destroy();
			if(key != "")
				ik->append(key);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_controllers::on_clearkey(wxCommandEvent& e)
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
			keyboard::ctrlrkey* ik = realitems[sel];
			if(!ik) {
				refresh();
				return;
			}
			std::vector<wxString> dropchoices;
			std::string tmp;
			unsigned idx = 0;
			while((tmp = ik->get_string(idx++)) != "")
				dropchoices.push_back(towxstring(tmp));
			idx = 0;
			if(dropchoices.size() == 0) {
				refresh();
				return;
			}
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
			auto g = ik->get(idx);
			ik->remove(g.first, g.second);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_controllers::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		if(e.GetId() == wxID_ADDKEY)
			on_setkey(e);
		else if(e.GetId() >= wxID_DROPKEY) {
			string_list<char> sel = get_selection();
			if(!realitems.count(sel))
				return;
			keyboard::ctrlrkey* ik = realitems[sel];
			if(!ik)
				return;
			auto g = ik->get(e.GetId() - wxID_DROPKEY);
			ik->remove(g.first, g.second);
		}
		refresh();
	}

	void wxeditor_esettings_controllers::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		if(!e.RightUp() && !(e.LeftUp() && e.ControlDown()))
			return;
		string_list<char> sel = get_selection();
		if(!realitems.count(sel))
			return;
		keyboard::ctrlrkey* ik = realitems[sel];
		if(!ik)
			return;

		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_popup_menu), NULL, this);
		menu.Append(wxID_ADDKEY, towxstring("Add new key"));
		bool first = true;
		unsigned idx = 0;
		std::string tmp;
		while((tmp = ik->get_string(idx++)) != "") {
			if(first)
				menu.AppendSeparator();
			first = false;
			menu.Append(wxID_DROPKEY + idx - 1, towxstring("Drop " + tmp));
		}
		PopupMenu(&menu);
	}

	void wxeditor_esettings_controllers::refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::map<keyboard::ctrlrkey*, std::string> data;
		auto x = inst.mapper->get_controller_keys();
		realitems.clear();
		for(auto y : x) {
			string_list<char> key = split_on_codepoint(y->get_name(), U'\u2023');
			names[key] = y->get_name();
			realitems[key] = y;
			std::string tmp;
			unsigned idx = 0;
			while((tmp = y->get_string(idx++)) != "")
				data[y] += ((data[y] != "") ? ", " : "") + tmp;
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
		for(auto i : realitems) {
			string_list<char> key = i.first;
			std::string text = key[key.size() - 1];
			if(data[i.second] == "")
				text = text + " (not set)";
			else
				text = text + " (" + clean_keystring(data[i.second]) + ")";
			wxTreeItemId id = get_item(key);
			controls->SetItemText(id, towxstring(text));
		}

		wxCommandEvent e;
		on_change(e);
	}

	settings_tab_factory controllers("Controllers", [](wxWindow* parent, emulator_instance& _inst) ->
		settings_tab* {
		return new wxeditor_esettings_controllers(parent, _inst);
	});
}
