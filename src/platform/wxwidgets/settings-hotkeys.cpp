#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/command.hpp"
#include "core/keymapper.hpp"

namespace
{
	class wxeditor_esettings_hotkeys : public settings_tab
	{
	public:
		wxeditor_esettings_hotkeys(wxWindow* parent);
		~wxeditor_esettings_hotkeys();
		void on_add(wxCommandEvent& e);
		void on_drop(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
		void on_notify() { refresh(); }
	private:
		wxListBox* category;
		wxListBox* control;
		wxButton* pri_button;
		wxButton* sec_button;
		std::map<int, std::string> categories;
		std::map<std::pair<int, int>, std::string> itemlabels;
		std::map<std::pair<int, int>, std::string> items;
		std::map<std::string, inverse_bind*> realitems;
		void change_category(int cat);
		void refresh();
		std::pair<std::string, std::string> splitkeyname(const std::string& kn);
	};


	wxeditor_esettings_hotkeys::wxeditor_esettings_hotkeys(wxWindow* parent)
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
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_change), NULL, this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(pri_button = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		pri_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_add), NULL, this);
		pbutton_s->Add(sec_button = new wxButton(this, wxID_ANY, wxT("Drop")), 0, wxGROW);
		sec_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_hotkeys::on_drop), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		refresh();
		top_s->SetSizeHints(this);
		Fit();
	}

	std::pair<std::string, std::string> wxeditor_esettings_hotkeys::splitkeyname(const std::string& kn)
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

	void wxeditor_esettings_hotkeys::on_change(wxCommandEvent& e)
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

	void wxeditor_esettings_hotkeys::change_category(int cat)
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
		if(control->GetSelection() == wxNOT_FOUND)
			control->SetSelection(0);
	}

	wxeditor_esettings_hotkeys::~wxeditor_esettings_hotkeys()
	{
	}

	void wxeditor_esettings_hotkeys::on_add(wxCommandEvent& e)
	{
		if(closing())
			return;
		std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
		if(name == "") {
			refresh();
			return;
		}
		try {
			inverse_bind* ik = realitems[name];
			if(!ik) {
				refresh();
				return;
			}
			key_entry_dialog* d = new key_entry_dialog(this, "Specify key for " + name, "", false);
			if(d->ShowModal() == wxID_CANCEL) {
				d->Destroy();
				return;
			}
			std::string key = d->getkey();
			d->Destroy();
			ik->append(key);
		} catch(...) {
		}
		refresh();
	}

	void wxeditor_esettings_hotkeys::on_drop(wxCommandEvent& e)
	{
		if(closing())
			return;
		std::string name = items[std::make_pair(category->GetSelection(), control->GetSelection())];
		if(name == "") {
			refresh();
			return;
		}
		try {
			inverse_bind* ik = realitems[name];
			if(!ik) {
				refresh();
				return;
			}
			std::vector<wxString> dropchoices;
			key_specifier tmp;
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

	void wxeditor_esettings_hotkeys::refresh()
	{
		if(closing())
			return;
		std::map<inverse_bind*, std::list<key_specifier>> data;
		std::map<std::string, int> cat_set;
		std::map<std::string, int> cat_assign;
		realitems.clear();
		itemlabels.clear();
		auto x = lsnes_mapper.get_inverses();
		for(auto y : x) {
			realitems[y->getname()] = y;
			key_specifier tmp;
			unsigned idx = 0;
			while((tmp = y->get(idx++)))
				data[y].push_back(tmp);
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
		if(category->GetSelection() == wxNOT_FOUND)
			category->SetSelection(0);
		change_category(category->GetSelection());
	}

	settings_tab_factory hotkeys("Hotkeys", [](wxWindow* parent) -> settings_tab* {
		return new wxeditor_esettings_hotkeys(parent);
	});
}
