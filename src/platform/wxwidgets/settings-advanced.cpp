#include "platform/wxwidgets/settings-common.hpp"
#include "core/instance.hpp"
#include "core/settings.hpp"

namespace
{
	enum
	{
		wxID_CHANGE = wxID_HIGHEST + 1
	};

	class wxeditor_esettings_advanced : public settings_tab
	{
	public:
		wxeditor_esettings_advanced(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_advanced();
		void on_change(wxCommandEvent& e);
		void on_change2(wxMouseEvent& e);
		void on_selchange(wxCommandEvent& e);
		void on_setting_change(const settingvar::base& val);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
		void _refresh();
		struct listener : public settingvar::listener
		{
			listener(settingvar::group& group, wxeditor_esettings_advanced& _obj)
				: obj(_obj), grp(group)
			{
				group.add_listener(*this);
			}
			~listener() throw()
			{
				grp.remove_listener(*this);
			}
			void on_setting_change(settingvar::group& grp, const settingvar::base& val)
			{
				obj.on_setting_change(val);
			}
			wxeditor_esettings_advanced& obj;
			settingvar::group& grp;
		};
	private:
		listener _listener;
		void refresh();
		std::set<std::string> settings;
		std::map<std::string, std::string> values;
		std::map<std::string, std::string> names;
		std::map<int, std::string> selections;
		std::string selected();
		wxButton* changebutton;
		wxListBox* _settings;
	};

	wxeditor_esettings_advanced::wxeditor_esettings_advanced(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, _inst), _listener(*inst.settings, *this)
	{
		CHECK_UI_THREAD;
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		top_s->Add(_settings = new wxListBox(this, wxID_ANY), 1, wxGROW);
		_settings->SetMinSize(wxSize(300, 400));
		_settings->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_advanced::on_selchange), NULL, this);
		_settings->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(wxeditor_esettings_advanced::on_mouse), NULL,
			this);
		_settings->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxeditor_esettings_advanced::on_mouse), NULL,
			this);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(changebutton = new wxButton(this, wxID_ANY, wxT("Change")), 0, wxGROW);
		_settings->Connect(wxEVT_LEFT_DCLICK,
			wxMouseEventHandler(wxeditor_esettings_advanced::on_change2), NULL, this);
		changebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_advanced::on_change), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		refresh();
		wxCommandEvent e;
		on_selchange(e);
		top_s->SetSizeHints(this);
		Fit();
	}

	wxeditor_esettings_advanced::~wxeditor_esettings_advanced()
	{
	}

	std::string change_value_of_boolean(const std::string& name, const settingvar::description& desc,
		const std::string& current)
	{
		return string_to_bool(current) ? "0" : "1";
	}

	std::string change_value_of_enumeration(wxWindow* parent, const std::string& name,
		const settingvar::description& desc, const std::string& current)
	{
		CHECK_UI_THREAD;
		std::vector<std::string> valset;
		unsigned dflt = 0;
		for(unsigned i = 0; i <= desc._enumeration->max_val(); i++) {
			valset.push_back(desc._enumeration->get(i));
			if(desc._enumeration->get(i) == current)
				dflt = i;
		}
		return pick_among(parent, "Set value to", "Set " + name + " to value:", valset, dflt);
	}

	std::string change_value_of_string(wxWindow* parent, const std::string& name,
		const settingvar::description& desc, const std::string& current)
	{
		return pick_text(parent, "Set value to", "Set " + name + " to value:", current);
	}

	class numeric_inputbox : public wxDialog
	{
	public:
		numeric_inputbox(wxWindow* parent, const std::string& name, int64_t minval, int64_t maxval,
			const std::string& val)
			: wxDialog(parent, wxID_ANY, wxT("Set value to"))
		{
			CHECK_UI_THREAD;
			wxSizer* s1 = new wxBoxSizer(wxVERTICAL);
			SetSizer(s1);
			s1->Add(new wxStaticText(this, wxID_ANY, towxstring("Set " + name + " to value:")), 0,
				wxGROW);

			s1->Add(sp = new wxSpinCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
				wxSP_ARROW_KEYS, minval, maxval, parse_value<int64_t>(val)), 1, wxGROW);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->AddStretchSpacer();
			wxButton* t;
			pbutton_s->Add(t = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(numeric_inputbox::on_button), NULL, this);
			pbutton_s->Add(t = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(numeric_inputbox::on_button), NULL, this);
			s1->Add(pbutton_s, 0, wxGROW);

			s1->SetSizeHints(this);
		}
		std::string get_value() { return (stringfmt() << sp->GetValue()).str(); }
		void on_button(wxCommandEvent& e) { EndModal(e.GetId()); }
	private:
		wxSpinCtrl* sp;
	};

	class path_inputbox : public wxDialog
	{
	public:
		path_inputbox(wxWindow* parent, const std::string& name, const std::string& val)
			: wxDialog(parent, wxID_ANY, wxT("Set path to"))
		{
			CHECK_UI_THREAD;
			wxButton* t;
			wxSizer* s1 = new wxBoxSizer(wxVERTICAL);
			SetSizer(s1);
			s1->Add(new wxStaticText(this, wxID_ANY, towxstring("Set " + name + " to value:")), 0,
				wxGROW);
			wxSizer* s2 = new wxBoxSizer(wxHORIZONTAL);
			s2->Add(pth = new wxTextCtrl(this, wxID_ANY, towxstring(val), wxDefaultPosition,
				wxSize(400, -1)), 1, wxGROW);
			s2->Add(t = new wxButton(this, wxID_HIGHEST + 1, wxT("...")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(path_inputbox::on_pbutton), NULL, this);
			s1->Add(s2, 1, wxGROW);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->AddStretchSpacer();
			pbutton_s->Add(t = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(path_inputbox::on_button), NULL, this);
			pbutton_s->Add(t = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
			t->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(path_inputbox::on_button), NULL, this);
			s1->Add(pbutton_s, 0, wxGROW);

			s1->SetSizeHints(this);
		}
		std::string get_value() { return tostdstring(pth->GetValue()); }
		void on_pbutton(wxCommandEvent& e) {
			wxDirDialog* d;
			d = new wxDirDialog(this, wxT("Select project directory"),
				pth->GetValue(), wxDD_DIR_MUST_EXIST);
			if(d->ShowModal() == wxID_CANCEL) {
				d->Destroy();
				return;
			}
			pth->SetValue(d->GetPath());
			d->Destroy();
		}
		void on_button(wxCommandEvent& e) {
			switch(e.GetId()) {
			case wxID_OK:
			case wxID_CANCEL:
				EndModal(e.GetId());
				break;
			};
		}
	private:
		wxTextCtrl* pth;
	};

	std::string change_value_of_numeric(wxWindow* parent, const std::string& name,
		const settingvar::description& desc, const std::string& current)
	{
		CHECK_UI_THREAD;
		auto d = new numeric_inputbox(parent, name, desc.min_val, desc.max_val, current);
		int x = d->ShowModal();
		if(x == wxID_CANCEL) {
			d->Destroy();
			throw canceled_exception();
		}
		std::string v = d->get_value();
		d->Destroy();
		return v;
	}

	std::string change_value_of_path(wxWindow* parent, const std::string& name,
		const settingvar::description& desc, const std::string& current)
	{
		CHECK_UI_THREAD;
		auto d = new path_inputbox(parent, name, current);
		int x = d->ShowModal();
		if(x == wxID_CANCEL) {
			d->Destroy();
			throw canceled_exception();
		}
		std::string v = d->get_value();
		d->Destroy();
		return v;
	}

	void wxeditor_esettings_advanced::on_popup_menu(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		if(e.GetId() == wxID_EDIT)
			on_change(e);
	}

	void wxeditor_esettings_advanced::on_mouse(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		if(!e.RightUp() && !(e.LeftUp() && e.ControlDown()))
			return;
		if(selected() == "")
			return;
		wxMenu menu;
		menu.Connect(wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_advanced::on_popup_menu), NULL, this);
		menu.Append(wxID_EDIT, towxstring("Change"));
		PopupMenu(&menu);
	}

	void wxeditor_esettings_advanced::on_change2(wxMouseEvent& e)
	{
		CHECK_UI_THREAD;
		wxCommandEvent e2;
		on_change(e2);
	}

	void wxeditor_esettings_advanced::on_change(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string name = selected();
		if(name == "")
			return;
		std::string value;
		std::string err;
		value = inst.setcache->get(name);
		auto model = inst.setcache->get_description(name);
		try {
			switch(model.type) {
			case settingvar::description::T_BOOLEAN:
				value = change_value_of_boolean(name, model, value); break;
			case settingvar::description::T_NUMERIC:
				value = change_value_of_numeric(this, name, model, value); break;
			case settingvar::description::T_STRING:
				value = change_value_of_string(this, name, model, value); break;
			case settingvar::description::T_PATH:
				value = change_value_of_path(this, name, model, value); break;
			case settingvar::description::T_ENUMERATION:
				value = change_value_of_enumeration(this, name, model, value); break;
			default:
				value = change_value_of_string(this, name, model, value); break;
			};
		} catch(...) {
			return;
		}
		inst.iqueue->run([this, name, value]() {
			run_show_error(this, "Error setting value", "", [name, value]() {
				CORE().setcache->set(name, value);
			});
		});
	}

	void wxeditor_esettings_advanced::on_selchange(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::string sel = selected();
		bool enable = (sel != "");
		changebutton->Enable(enable);
	}

	void wxeditor_esettings_advanced::on_setting_change(const settingvar::base& val)
	{
		if(closing())
			return;
		runuifun([this, &val]() {
			CHECK_UI_THREAD;
			std::string setting = val.get_iname();
			std::string value = val.str();
			this->settings.insert(setting);
			this->values[setting] = value;
			this->_refresh();
		});
	}

	void wxeditor_esettings_advanced::refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		settings = inst.setcache->get_keys();
		for(auto i : settings) {
			values[i] = inst.setcache->get(i);
			names[i] = inst.setcache->get_hname(i);
		}
		_refresh();
	}

	std::string wxeditor_esettings_advanced::selected()
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

	void wxeditor_esettings_advanced::_refresh()
	{
		CHECK_UI_THREAD;
		if(closing())
			return;
		std::vector<wxString> strings;
		std::multimap<std::string, std::string> sort;
		int k = 0;
		for(auto i : settings)
			sort.insert(std::make_pair(names[i], i));
		for(auto i : sort) {
			//FIXME: Do something with this?
			//auto description = inst.setcache->get_description(i.second);
			strings.push_back(towxstring(names[i.second] + " (Value: " + values[i.second] + ")"));
			selections[k++] = i.second;
		}
		_settings->Set(strings.size(), &strings[0]);
	}

	settings_tab_factory advanced_tab("Advanced", [](wxWindow* parent, emulator_instance& _inst) ->
		settings_tab* {
		return new wxeditor_esettings_advanced(parent, _inst);
	});
}
