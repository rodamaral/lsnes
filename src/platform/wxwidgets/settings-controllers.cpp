#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/keymapper.hpp"

namespace
{
	class wxeditor_esettings_controllers : public settings_tab
	{
	public:
		wxeditor_esettings_controllers(wxWindow* parent);
		~wxeditor_esettings_controllers();
		void on_setkey(wxCommandEvent& e);
		void on_clearkey(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
	private:
		wxListBox* category;
		wxListBox* control;
		wxButton* set_button;
		wxButton* clear_button;
		std::map<int, std::string> categories;
		std::map<std::pair<int, int>, std::string> itemlabels;
		std::map<std::pair<int, int>, std::string> items;
		std::map<std::string, controller_key*> realitems;
		void change_category(int cat);
		void refresh();
		std::pair<std::string, std::string> splitkeyname(const std::string& kn);
	};

	wxeditor_esettings_controllers::wxeditor_esettings_controllers(wxWindow* parent)
		: settings_tab(parent)
	{
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);
		wxString empty[1];

		top_s->Add(category = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, empty), 1,
			wxGROW);
		top_s->Add(control = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 1, empty), 1,
			wxGROW);
		category->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_change), NULL, this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(set_button = new wxButton(this, wxID_ANY, wxT("Change")), 0, wxGROW);
		set_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_setkey), NULL, this);
		pbutton_s->Add(clear_button = new wxButton(this, wxID_ANY, wxT("Clear")), 0, wxGROW);
		clear_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_controllers::on_clearkey), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		refresh();
		top_s->SetSizeHints(this);
		Fit();
	}

	std::pair<std::string, std::string> wxeditor_esettings_controllers::splitkeyname(const std::string& kn)
	{
		std::string tmp = kn;
		size_t split = 0;
		for(size_t itr = 0; itr < tmp.length() - 2 && itr < tmp.length(); itr++) {
			unsigned char ch1 = tmp[itr];
			unsigned char ch2 = tmp[itr + 1];
			unsigned char ch3 = tmp[itr + 2];
			if(ch1 == 0xE2 && ch2 == 0x80 && ch3 == 0xA3)
				split = itr;
		}
		if(split)
			return std::make_pair(tmp.substr(0, split), tmp.substr(split + 3));
		else
			return std::make_pair("(Uncategorized)", tmp);
	}

	void wxeditor_esettings_controllers::on_change(wxCommandEvent& e)
	{
		if(closing())
			return;
		int c = category->GetSelection();
		if(c == wxNOT_FOUND) {
			category->SetSelection(0);
			change_category(0);
		} else
			change_category(c);
	}

	void wxeditor_esettings_controllers::change_category(int cat)
	{
		if(closing())
			return;
		std::map<int, std::string> n;
		for(auto i : itemlabels)
			if(i.first.first == cat)
				n[i.first.second] = i.second;

		for(size_t i = 0; i < control->GetCount(); i++)
			if(n.count(i))
				control->SetString(i, towxstring(n[i]));
			else
				control->Delete(i--);
		for(auto i : n)
			if(i.first >= (int)control->GetCount())
				control->Append(towxstring(n[i.first]));
		if(control->GetSelection() == wxNOT_FOUND && !control->IsEmpty())
			control->SetSelection(0);
	}

	wxeditor_esettings_controllers::~wxeditor_esettings_controllers()
	{
	}

	void wxeditor_esettings_controllers::on_setkey(wxCommandEvent& e)
	{
		if(closing())
			return;
		std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
		if(name == "") {
			refresh();
			return;
		}
		try {
			controller_key* ik = realitems[name];
			if(!ik) {
				refresh();
				return;
			}
			bool axis = ik->is_axis();
			std::string wtitle = (axis ? "Specify axis for " : "Specify key for ") + name;
			press_button_dialog* p = new press_button_dialog(this, wtitle, axis);
			p->ShowModal();
			std::string key = p->getkey();
			p->Destroy();
			ik->set(key);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_controllers::on_clearkey(wxCommandEvent& e)
	{
		if(closing())
			return;
		std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
		if(name == "") {
			refresh();
			return;
		}
		try {
			controller_key* ik = realitems[name];
			if(ik)
				ik->set(NULL, 0);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_controllers::refresh()
	{
		if(closing())
			return;
		std::map<controller_key*, std::string> data;
		std::map<std::string, int> cat_set;
		std::map<std::string, int> cat_assign;
		realitems.clear();
		auto x = lsnes_mapper.get_controller_keys();
		for(auto y : x) {
			realitems[y->get_name()] = y;
			data[y] = y->get_string();
		}

		int cidx = 0;
		for(auto i : realitems) {
			std::pair<std::string, std::string> j = splitkeyname(i.first);
			if(!cat_set.count(j.first)) {
				categories[cidx] = j.first;
				cat_assign[j.first] = 0;
				cat_set[j.first] = cidx++;
			}
			items[std::make_pair(cat_set[j.first], cat_assign[j.first])] = i.first;
			std::string text = j.second;
			if(data[i.second] == "")
				text = text + " (not set)";
			else
				text = text + " (" + clean_keystring(data[i.second]) + ")";
			itemlabels[std::make_pair(cat_set[j.first], cat_assign[j.first])] = text;
			cat_assign[j.first]++;
		}

		for(size_t i = 0; i < category->GetCount(); i++)
			if(categories.count(i))
				category->SetString(i, towxstring(categories[i]));
			else
				category->Delete(i--);
		for(auto i : categories)
			if(i.first >= (int)category->GetCount())
				category->Append(towxstring(categories[i.first]));
		if(category->GetSelection() == wxNOT_FOUND && !category->IsEmpty())
			category->SetSelection(0);
		change_category(category->GetSelection());
	}

	settings_tab_factory controllers("Controllers", [](wxWindow* parent) -> settings_tab* {
		return new wxeditor_esettings_controllers(parent);
	});
}
