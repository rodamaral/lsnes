#include "platform/wxwidgets/settings-common.hpp"
#include "core/filedownload.hpp"

namespace
{
	class wxeditor_esettings_download : public settings_tab
	{
	public:
		wxeditor_esettings_download(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_download();
		void on_add(wxCommandEvent& e);
		void on_delete(wxCommandEvent& e);
		void on_change(wxCommandEvent& e);
		void on_change2(wxMouseEvent& e);
		void on_selchange(wxCommandEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
		void _refresh();
	private:
		void refresh();
		std::set<std::string> patterns;
		std::map<std::string, std::string> values;
		std::map<std::string, std::string> names;
		std::map<int, std::string> selections;
		std::string selected();
		wxButton* addbutton;
		wxButton* changebutton;
		wxButton* deletebutton;
		wxListBox* _settings;
	};

	wxeditor_esettings_download::wxeditor_esettings_download(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, inst)
	{
		CHECK_UI_THREAD;
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(_settings = new wxListBox(this, wxID_ANY), 1, wxGROW);
		_settings->SetMinSize(wxSize(300, 400));
		_settings->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_download::on_selchange), NULL, this);
		_settings->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxeditor_esettings_download::on_mouse), NULL,
			this);
		_settings->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_esettings_download::on_mouse), NULL,
			this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(addbutton = new wxButton(this, wxID_ANY, wxT("Add")), 0, wxGROW);
		pbutton_s->Add(changebutton = new wxButton(this, wxID_ANY, wxT("Change")), 0, wxGROW);
		pbutton_s->Add(deletebutton = new wxButton(this, wxID_ANY, wxT("Delete")), 0, wxGROW);
		_settings->Connect(wxEVT_LEFT_DCLICK,
			wxMouseEventHandler(wxeditor_esettings_download::on_change2), NULL, this);
		changebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_download::on_change), NULL, this);
		addbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_download::on_add), NULL, this);
		deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_download::on_delete), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		refresh();
		wxCommandEvent e;
		on_selchange(e);
		top_s->SetSizeHints(this);
		Fit();
	}

	wxeditor_esettings_download::~wxeditor_esettings_download()
	{
	}

	void wxeditor_esettings_download::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		if(e.GetId() == wxID_EDIT)
			on_change(e);
	}

	void wxeditor_esettings_download::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		if(!e.RightUp() && !(e.LeftUp() && e.ControlDown()))
			return;
		if(selected() == "")
			return;
		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_download::on_popup_menu), NULL, this);
		menu.Append(wxID_EDIT, towxstring("Change"));
		PopupMenu(&menu);
	}

	void wxeditor_esettings_download::on_change2(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		wxCommandEvent e2;
		on_change(e2);
	}

	void wxeditor_esettings_download::on_add(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name, value;
		try {
			name = pick_text(this, "Method name", "Name for the new shortcut scheme:\n"
				"(A-Z, a-z, 0-9, +, ., -. Has to start with a letter)", "");
			if(!regex_match("[A-Za-z][A-Za-z0-9+.-]*", name)) {
				wxMessageBox(towxstring("Invalid scheme name"), wxT("Error setting pattern"),
					wxICON_EXCLAMATION | wxOK);
				return;
			}
			value = pick_text(this, "Set URL to", "Set " + name + " to URL:\n"
				"(Use $0 for placeholder, use $$ for literial $)", "");
		} catch(...) {
			return;
		}
		bool error = false;
		std::string errorstr;
		try {
			lsnes_uri_rewrite.set_rewrite(name, value);
		} catch(std::exception& e) {
			error = true;
			errorstr = e.what();
		}
		if(error)
			wxMessageBox(towxstring(errorstr), wxT("Error setting pattern"), wxICON_EXCLAMATION | wxOK);
		else
			refresh();
	}

	void wxeditor_esettings_download::on_delete(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name = selected();
		if(name == "")
			return;
		lsnes_uri_rewrite.delete_rewrite(name);
		refresh();
	}

	void wxeditor_esettings_download::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name = selected();
		if(name == "")
			return;
		std::string value;
		std::string err;
		try {
			value = lsnes_uri_rewrite.get_rewrite(name);
		} catch(...) {
		}
		try {
			value = pick_text(this, "Set URL to", "Set " + name + " to URL:\n"
				"(Use $0 for placeholder, use $$ for literial $)", value);
		} catch(...) {
			return;
		}
		bool error = false;
		std::string errorstr;
		try {
			lsnes_uri_rewrite.set_rewrite(name, value);
		} catch(std::exception& e) {
			error = true;
			errorstr = e.what();
		}
		if(error)
			wxMessageBox(towxstring(errorstr), wxT("Error setting replace"), wxICON_EXCLAMATION | wxOK);
		else
			refresh();
	}

	void wxeditor_esettings_download::on_selchange(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string sel = selected();
		bool enable = (sel != "");
		changebutton->Enable(enable);
		deletebutton->Enable(enable);
	}

	void wxeditor_esettings_download::refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		patterns = lsnes_uri_rewrite.get_schemes();
		for(auto i : patterns) {
			values[i] = lsnes_uri_rewrite.get_rewrite(i);
			names[i] = i;
		}
		_refresh();
	}

	std::string wxeditor_esettings_download::selected()
	{
		CHECK_UI_THREAD;
		if(closing())
			return "";
		int x = _settings->GetSelection();
		if(selections.count(x))
			return selections[x];
		else
			return "";
	}

	void wxeditor_esettings_download::_refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::vector<wxString> strings;
		std::multimap<std::string, std::string> sort;
		int k = 0;
		for(auto i : patterns)
			sort.insert(std::make_pair(names[i], i));
		for(auto i : sort) {
			strings.push_back(towxstring(names[i.second] + " (Target: " + values[i.second] + ")"));
			selections[k++] = i.second;
		}
		_settings->Set(strings.size(), &strings[0]);
	}

	settings_tab_factory download_tab("URI shortcuts", [](wxWindow* parent, emulator_instance& _inst) ->
		settings_tab* {
		return new wxeditor_esettings_download(parent, _inst);
	});
}
