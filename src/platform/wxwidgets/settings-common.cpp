#include "platform/wxwidgets/settings-common.hpp"
#include "core/keymapper.hpp"

namespace
{
	std::map<std::string, settings_tab_factory*>& _factories()
	{
		static std::map<std::string, settings_tab_factory*> x;
		return x;
	}
}

settings_tab_factory::settings_tab_factory(const std::string& tabname,
	std::function<settings_tab*(wxWindow* parent)> create_fn)
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

settings_menu::settings_menu(wxWindow* win, int id)
{
	parent = win;
	wxMenuItem* n;
	items[id] = NULL;
	n = Append(id, towxstring("All as tabs..."));
	win->Connect(id++, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(settings_menu::on_selected), NULL,
		this);
	AppendSeparator();
	for(auto i : settings_tab_factory::factories()) {
		items[id] = i;
		n = Append(id, towxstring(i->get_name() + "..."));
		win->Connect(id++, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(settings_menu::on_selected),
			NULL, this);
	}
}

void settings_menu::on_selected(wxCommandEvent& e)
{
	int id = e.GetId();
	if(!items.count(id))
		return;
	display_settings_dialog(parent, items[id]);
}

namespace 
{
	volatile bool keygrab_active = false;	
	std::string pkey;
	std::function<void(std::string key)> keygrab_callback;

	class keygrabber : public keyboard_event_listener
	{
	public:
		keygrabber() { keygrab_active = false; }
		void on_key_event(keyboard_modifier_set& mods, keyboard_key& key, keyboard_event& event)
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
						runuifun([tmp]() { keygrab_callback(tmp); });
					} else
						pkey = "";
				}
			}
		}
	} keygrabber;

	class wxeditor_esettings2 : public wxDialog
	{
	public:
		wxeditor_esettings2(wxWindow* parent, settings_tab_factory* singletab);
		~wxeditor_esettings2();
		bool ShouldPreventAppExit() const;
		void on_close(wxCommandEvent& e);
		void on_notify();
	private:
		wxNotebook* tabset;
		wxButton* closebutton;
		std::list<settings_tab*> tabs;
		std::string get_title(settings_tab_factory* singletab);
	};

	std::string wxeditor_esettings2::get_title(settings_tab_factory* singletab)
	{
		if(!singletab)
			return "lsnes: Configuration";
		else
			return "lsnes: Configuration: " + singletab->get_name();
	}

	wxeditor_esettings2::wxeditor_esettings2(wxWindow* parent, settings_tab_factory* singletab)
		: wxDialog(parent, wxID_ANY, towxstring(get_title(singletab)), wxDefaultPosition, wxSize(-1, -1))
	{
		//Grab keys to prevent the joystick driver from running who knows what commands.
		lsnes_kbd.set_exclusive(&keygrabber);

		Centre();
		wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
		SetSizer(top_s);

		if(singletab) {
			settings_tab* t = singletab->create(this);
			top_s->Add(t);
			t->set_notify([this]() { this->on_notify(); });
			tabs.push_back(t);
		} else {
			tabset = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
			for(auto i : settings_tab_factory::factories()) {
				settings_tab* t = i->create(tabset);
				tabset->AddPage(t, towxstring(i->get_name()));
				t->set_notify([this]() { this->on_notify(); });
				tabs.push_back(t);
			}
			top_s->Add(tabset, 1, wxGROW);
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
		lsnes_kbd.set_exclusive(NULL);
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

void display_settings_dialog(wxWindow* parent, settings_tab_factory* singletab)
{
	modal_pause_holder hld;
	wxDialog* editor;
	try {
		editor = new wxeditor_esettings2(parent, singletab);
		editor->ShowModal();
	} catch(...) {
	}
	editor->Destroy();
}

void settings_activate_keygrab(std::function<void(std::string key)> callback)
{
	keygrab_callback = callback;
	keygrab_active = true;
}

void settings_deactivate_keygrab()
{
	keygrab_active = false;
}
