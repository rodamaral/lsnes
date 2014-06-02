#include "platform/wxwidgets/settings-common.hpp"
#include "core/instance.hpp"
#include "core/instance-map.hpp"
#include "core/keymapper.hpp"

namespace
{
	std::map<std::string, settings_tab_factory*>& _factories()
	{
		static std::map<std::string, settings_tab_factory*> x;
		return x;
	}

	class wxeditor_esettings2;
	instance_map<wxeditor_esettings2> sdialogs;
}


settings_tab_factory::settings_tab_factory(const std::string& tabname,
	std::function<settings_tab*(wxWindow* parent, emulator_instance& inst)> create_fn)
	: _tabname(tabname), _create_fn(create_fn)
{
	_factories()[_tabname] = this;
}

settings_tab_factory::~settings_tab_factory()
{
	_factories().erase(_tabname);
}

std::list<settings_tab_factory*> settings_tab_factory::factories()
{
	std::list<settings_tab_factory*> f;
	for(auto i : _factories())
		f.push_back(i.second);
	return f;
}

settings_menu::settings_menu(wxWindow* win, emulator_instance& _inst, int id)
	: inst(_inst)
{
	parent = win;
	items[id] = NULL;
	Append(id, towxstring("All as tabs..."));
	win->Connect(id++, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(settings_menu::on_selected), NULL,
		this);
	AppendSeparator();
	for(auto i : settings_tab_factory::factories()) {
		items[id] = i;
		Append(id, towxstring(i->get_name() + "..."));
		win->Connect(id++, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(settings_menu::on_selected),
			NULL, this);
	}
}

void settings_menu::on_selected(wxCommandEvent& e)
{
	int id = e.GetId();
	if(!items.count(id))
		return;
	display_settings_dialog(parent, inst, items[id]);
}

namespace
{
	volatile bool keygrab_active = false;
	std::string pkey;
	std::function<void(std::string key)> keygrab_callback;

	class keygrabber : public keyboard::event_listener
	{
	public:
		keygrabber()
		{
			keygrab_active = false;
		}
		void on_key_event(keyboard::modifier_set& mods, keyboard::key& key, keyboard::event& event)
		{
			if(!keygrab_active)
				return;
			uint32_t dev = event.get_change_mask();
			auto subkeys = key.get_subkeys();
			for(unsigned i = 0; i < 16 && i < subkeys.size(); i++) {
				std::string pname = key.get_name() + subkeys[i];
				if(((dev >> (2 * i)) & 3) == 3)
					pkey = pname;
				if(((dev >> (2 * i)) & 3) == 2) {
					if(pkey == pname) {
						keygrab_active = false;
						std::string tmp = pkey;
						runuifun([this, tmp]() { this->keygrab_callback(tmp); });
					} else
						pkey = "";
				}
			}
		}
		volatile bool keygrab_active;
		std::string pkey;
		std::function<void(std::string key)> keygrab_callback;
	};

	class wxeditor_esettings2 : public wxDialog
	{
	public:
		wxeditor_esettings2(emulator_instance& _inst, wxWindow* parent, settings_tab_factory* singletab);
		~wxeditor_esettings2();
		bool ShouldPreventAppExit() const;
		void on_close(wxCommandEvent& e);
		void on_notify();
	private:
		emulator_instance& inst;
		wxNotebook* tabset;
		wxButton* closebutton;
		std::list<settings_tab*> tabs;
		std::string get_title(settings_tab_factory* singletab);
	public:
		keygrabber keygrab;
	};

	std::string wxeditor_esettings2::get_title(settings_tab_factory* singletab)
	{
		if(!singletab)
			return "lsnes: Configuration";
		else
			return "lsnes: Configuration: " + singletab->get_name();
	}

	wxeditor_esettings2::wxeditor_esettings2(emulator_instance& _inst, wxWindow* parent,
		settings_tab_factory* singletab)
		: wxDialog(parent, wxID_ANY, towxstring(get_title(singletab)), wxDefaultPosition, wxSize(-1, -1),
			wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER), inst(_inst)
	{
		//Grab keys to prevent the joystick driver from running who knows what commands.
		inst.keyboard->set_exclusive(&keygrab);

		Centre();
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		if(singletab) {
			//If this throws, let it throw through.
			settings_tab* t = singletab->create(this, inst);
			top_s->Add(t, 1, wxGROW);
			t->set_notify([this]() { this->on_notify(); });
			tabs.push_back(t);
		} else {
			int created = 0;
			tabset = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
			for(auto i : settings_tab_factory::factories()) {
				settings_tab* t;
				try {
					t = i->create(tabset, inst);
				} catch(...) {
					continue;
				}
				tabset->AddPage(t, towxstring(i->get_name()));
				t->set_notify([this]() { this->on_notify(); });
				tabs.push_back(t);
				created++;
			}
			top_s->Add(tabset, 1, wxGROW);
			if(!created)
				throw std::runtime_error("Nothing to configure here, move along");
		}

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(closebutton = new wxButton(this, wxID_ANY, wxT("Close")), 0, wxGROW);
		closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings2::on_close), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		top_s->SetSizeHints(this);
		Fit();
	}

	wxeditor_esettings2::~wxeditor_esettings2()
	{
		for(auto i : tabs)
			i->notify_close();
		inst.keyboard->set_exclusive(NULL);
		sdialogs.remove(inst);
	}

	bool wxeditor_esettings2::ShouldPreventAppExit() const
	{
		return false;
	}

	void wxeditor_esettings2::on_close(wxCommandEvent& e)
	{
		for(auto i : tabs)
			i->on_close();
		EndModal(wxID_OK);
	}

	void wxeditor_esettings2::on_notify()
	{
		for(auto i : tabs)
			i->on_notify();
	}
}

void display_settings_dialog(wxWindow* parent, emulator_instance& inst, settings_tab_factory* singletab)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		try {
			editor = sdialogs.create(inst, parent, singletab);
		} catch(std::exception& e) {
			std::string title = "Configure";
			if(singletab)
				title = "Configure " + singletab->get_name();
			show_message_ok(parent, title, e.what(), wxICON_EXCLAMATION);
			return;
		}
		editor->ShowModal();
	} catch(...) {
		return;
	}
	editor->Destroy();
	do_save_configuration();
}

void settings_activate_keygrab(emulator_instance& inst, std::function<void(std::string key)> callback)
{
	auto s = sdialogs.lookup(inst);
	if(!s) return;
	s->keygrab.keygrab_callback = callback;
	s->keygrab.keygrab_active = true;
}

void settings_deactivate_keygrab(emulator_instance& inst)
{
	auto s = sdialogs.lookup(inst);
	if(!s) return;
	s->keygrab.keygrab_active = false;
}
