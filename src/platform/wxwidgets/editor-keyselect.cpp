#include "platform/wxwidgets/platform.hpp"
#include "core/keymapper.hpp"

#include <wx/wx.h>

namespace
{
	struct keyentry_mod_data
	{
		wxCheckBox* pressed;
		wxCheckBox* unmasked;
		unsigned tmpflags;
	};

	class wxdialog_keyentry : public wxDialog
	{
	public:
		wxdialog_keyentry(wxWindow* parent, const std::string& title, const std::string& spec,
			bool clearable);
		void on_change_setting(wxCommandEvent& e);
		void on_ok(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e);
		void on_clear(wxCommandEvent& e);
		void on_classchange(wxCommandEvent& e);
		std::string getkey();
	private:
		void set_mask(const std::string& mod);
		void set_mod(const std::string& mod);
		void set_set(const std::string& mset,
			void (wxdialog_keyentry::*fn)(const std::string& mod));
		void load_spec(const std::string& spec);
		std::map<std::string, keyentry_mod_data> modifiers;
		wxComboBox* mainclass;
		wxComboBox* mainkey;
		wxButton* ok;
		wxButton* cancel;
		wxButton* clear;
		bool cleared;
	};

	class wxdialog_hotkeys;

	class hotkey
	{
	public:
		hotkey(wxdialog_hotkeys* h, wxWindow* win, wxSizer* s, inverse_key* ikey, unsigned o);
		void update(const std::string& primary, const std::string& secondary);
		inverse_key* get_associated();
		std::string get_title();
	private:
		wxButton* primaryb;
		wxButton* secondaryb;
		inverse_key* associated;
		std::string title;
	};

	class wxdialog_hotkeys : public wxDialog
	{
	public:
		wxdialog_hotkeys(wxWindow* parent);
		~wxdialog_hotkeys();
		void on_close(wxCommandEvent& e);
		void on_reconfig(wxCommandEvent& e);
	private:
		void update();
		wxScrolledWindow* scroll;
		wxSizer* scroll_sizer;
		std::map<unsigned, hotkey*> hotkeys;
		wxButton* close;
	};

	wxdialog_keyentry::wxdialog_keyentry(wxWindow* parent, const std::string& title, const std::string& spec,
		bool clearable)
		: wxDialog(parent, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1))
	{
		std::vector<wxString> keych;
		std::set<std::string> mods, keys;

		cleared = false;
		runemufn([&mods, &keys]() { mods = modifier::get_set(); keys = keygroup::get_keys(); });
		Centre();
		wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
		SetSizer(top_s);

		wxFlexGridSizer* t_s = new wxFlexGridSizer(mods.size() + 1, 3, 0, 0);
		for(auto i : mods) {
			t_s->Add(new wxStaticText(this, wxID_ANY, towxstring(i)), 0, wxGROW);
			keyentry_mod_data m;
			t_s->Add(m.pressed = new wxCheckBox(this, wxID_ANY, wxT("Pressed")), 0, wxGROW);
			t_s->Add(m.unmasked = new wxCheckBox(this, wxID_ANY, wxT("Unmasked")), 0, wxGROW);
			m.pressed->Disable();
			modifiers[i] = m;
			m.pressed->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
			m.unmasked->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
		}
		for(auto i : keys)
			keych.push_back(towxstring(i));
		t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Key")), 0, wxGROW);
		t_s->Add(mainkey = new wxComboBox(this, wxID_ANY, keych[0], wxDefaultPosition, wxDefaultSize,
			keych.size(), &keych[0], wxCB_READONLY), 1, wxGROW);
		mainkey->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxdialog_keyentry::on_change_setting), NULL, this);
		top_s->Add(t_s);

		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		if(clearable)
			pbutton_s->Add(clear = new wxButton(this, wxID_OK, wxT("Clear")), 0, wxGROW);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
		pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_ok), NULL, this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_cancel), NULL, this);
		if(clearable)
			clear->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(wxdialog_keyentry::on_clear), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		t_s->SetSizeHints(this);
		top_s->SetSizeHints(this);
		Fit();

		if(spec != "")
			load_spec(spec);
	}

#define TMPFLAG_UNMASKED 65
#define TMPFLAG_UNMASKED_LINK_CHILD 2
#define TMPFLAG_UNMASKED_LINK_PARENT 68
#define TMPFLAG_PRESSED 8
#define TMPFLAG_PRESSED_LINK_CHILD 16
#define TMPFLAG_PRESSED_LINK_PARENT 32

	void wxdialog_keyentry::set_mask(const std::string& mod)
	{
		if(!modifiers.count(mod))
			return;
		if(modifiers[mod].unmasked->IsEnabled()) {
			wxCommandEvent e;
			modifiers[mod].unmasked->SetValue(true);
			on_change_setting(e);
		}
	}	

	void wxdialog_keyentry::set_mod(const std::string& mod)
	{
		if(!modifiers.count(mod))
			return;
		if(modifiers[mod].pressed->IsEnabled()) {
			wxCommandEvent e;
			modifiers[mod].pressed->SetValue(true);
			on_change_setting(e);
		}
	}	

	void wxdialog_keyentry::set_set(const std::string& mset,
		void (wxdialog_keyentry::*fn)(const std::string& mod))
	{
		std::string rem = mset;
		while(rem != "") {
			size_t s = rem.find_first_of(",");
			if(s >= rem.length()) {
				(this->*fn)(rem);
				break;
			} else {
				(this->*fn)(rem.substr(0, s));
				rem = rem.substr(s + 1);
			}
		}
	}

	void wxdialog_keyentry::load_spec(const std::string& spec)
	{
		std::string _spec = spec;
		size_t s1 = _spec.find_first_of("/");
		size_t s2 = _spec.find_first_of("|");
		if(s1 >= _spec.length() || s2 >= _spec.length())
			return;		//Bad.
		std::string mod = _spec.substr(0, s1);
		std::string mask = _spec.substr(s1 + 1, s2 - s1 - 1);
		std::string key = _spec.substr(s2 + 1);
		set_set(mask, &wxdialog_keyentry::set_mask);
		set_set(mod, &wxdialog_keyentry::set_mod);
		mainkey->SetValue(towxstring(key));
	}		

	void wxdialog_keyentry::on_change_setting(wxCommandEvent& e)
	{
		for(auto& i : modifiers)
			i.second.tmpflags = 0;
		for(auto& i : modifiers) {
			modifier* m = NULL;
			try {
				m = &modifier::lookup(i.first);
			} catch(...) {
				i.second.pressed->Disable();
				i.second.unmasked->Disable();
				continue;
			}
			std::string j = m->linked_name();
			if(i.second.unmasked->GetValue())
				i.second.tmpflags |= TMPFLAG_UNMASKED;
			if(j != "") {
				if(modifiers[j].unmasked->GetValue())
					i.second.tmpflags |= TMPFLAG_UNMASKED_LINK_PARENT;
				if(i.second.unmasked->GetValue())
					modifiers[j].tmpflags |= TMPFLAG_UNMASKED_LINK_CHILD;
			}
			if(i.second.pressed->GetValue())
				i.second.tmpflags |= TMPFLAG_PRESSED;
			if(j != "") {
				if(modifiers[j].pressed->GetValue())
					i.second.tmpflags |= TMPFLAG_PRESSED_LINK_PARENT;
				if(i.second.pressed->GetValue())
					modifiers[j].tmpflags |= TMPFLAG_PRESSED_LINK_CHILD;
			}
		}
		for(auto& i : modifiers) {
			//Unmasked is to be enabled if neither unmasked link flag is set.
			if(i.second.tmpflags & ((TMPFLAG_UNMASKED_LINK_CHILD | TMPFLAG_UNMASKED_LINK_PARENT) & ~64)) {
				i.second.unmasked->SetValue(false);
				i.second.unmasked->Disable();
			} else
				i.second.unmasked->Enable();
			//Pressed is to be enabled if:
			//- This modifier is unmasked or parent is unmasked.
			//- Parent nor child is not pressed.
			if(((i.second.tmpflags & (TMPFLAG_UNMASKED | TMPFLAG_UNMASKED_LINK_PARENT |
				TMPFLAG_PRESSED_LINK_CHILD | TMPFLAG_PRESSED_LINK_PARENT)) & 112) == 64)
				i.second.pressed->Enable();
			else {
				i.second.pressed->SetValue(false);
				i.second.pressed->Disable();
			}
		}
	}

	void wxdialog_keyentry::on_ok(wxCommandEvent& e)
	{
		EndModal(wxID_OK);
	}

	void wxdialog_keyentry::on_clear(wxCommandEvent& e)
	{
		cleared = true;
		EndModal(wxID_OK);
	}

	void wxdialog_keyentry::on_cancel(wxCommandEvent& e)
	{
		EndModal(wxID_CANCEL);
	}

	std::string wxdialog_keyentry::getkey()
	{
		if(cleared)
			return "";
		std::string x;
		bool f;
		f = true;
		for(auto i : modifiers) {
			if(i.second.pressed->GetValue()) {
				if(!f)
					x = x + ",";
				f = false;
				x = x + i.first;
			}
		}
		x = x + "/";
		f = true;
		for(auto i : modifiers) {
			if(i.second.unmasked->GetValue()) {
				if(!f)
					x = x + ",";
				f = false;
				x = x + i.first;
			}
		}
		x = x + "|" + tostdstring(mainkey->GetValue());
		return x;
	}

	wxdialog_hotkeys::wxdialog_hotkeys(wxWindow* parent)
		: wxDialog(parent, wxID_ANY, wxT("Hotkeys"), wxDefaultPosition, wxSize(-1, -1))
	{
		scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, -1));
		scroll->SetMinSize(wxSize(-1, 500));

		Centre();
		wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
		SetSizer(top_s);
		scroll_sizer = new wxFlexGridSizer(0, 3, 0, 0);
		scroll->SetSizer(scroll_sizer);

		//Obtain all the inverses.
		std::set<inverse_key*> inverses;
		runemufn([&inverses]() {
			auto x = inverse_key::get_ikeys();
			for(auto y : x)
				inverses.insert(y);
		});
		unsigned y = 0;
		for(auto x : inverses) {
			hotkeys[y] = new hotkey(this, scroll, scroll_sizer, x, y);
			y++;
		}
		update();

		top_s->Add(scroll);
		scroll->SetScrollRate(0, 20);
		wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(close = new wxButton(this, wxID_CANCEL, wxT("Close")), 0, wxGROW);
		close->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_hotkeys::on_close), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		scroll_sizer->SetSizeHints(this);
		top_s->SetSizeHints(this);
		scroll_sizer->Layout();
		top_s->Layout();
		Fit();
	}

	void wxdialog_hotkeys::on_reconfig(wxCommandEvent& e)
	{
		int id = e.GetId();
		if(id <= wxID_HIGHEST)
			return;
		unsigned button = (id - wxID_HIGHEST - 1) / 2;
		bool primflag = (((id - wxID_HIGHEST - 1) % 2) == 0);
		if(!hotkeys.count(button))
			return;
		wxdialog_keyentry* d = new wxdialog_keyentry(this, "Specify key for " + hotkeys[button]->
			get_title(), hotkeys[button]->get_associated()->get(primflag), true);
		if(d->ShowModal() == wxID_CANCEL) {
			d->Destroy();
			return;
		}
		std::string key = d->getkey();
		d->Destroy();
		if(key != "")
			hotkeys[button]->get_associated()->set(key, primflag);
		else
			hotkeys[button]->get_associated()->clear(primflag);
		update();
	}

	void wxdialog_hotkeys::update()
	{
		std::map<inverse_key*, std::pair<std::string, std::string>> data;
		runemufn([&data]() {
			auto x = inverse_key::get_ikeys();
			for(auto y : x)
				data[y] = std::make_pair(y->get(true), y->get(false));
		});
		for(auto i : hotkeys) {
			inverse_key* j = i.second->get_associated();
			if(!data.count(j))
				continue;
			auto y = data[j];
			i.second->update(y.first, y.second);
		}
	}

	wxdialog_hotkeys::~wxdialog_hotkeys()
	{
		for(auto i : hotkeys)
			delete i.second;
	}

	void wxdialog_hotkeys::on_close(wxCommandEvent& e)
	{
		EndModal(wxID_OK);
	}

	hotkey::hotkey(wxdialog_hotkeys* h, wxWindow* win, wxSizer* s, inverse_key* ikey, unsigned o)
	{
		title = ikey->getname();
		s->Add(new wxStaticText(win, wxID_ANY, towxstring(title)), 0, wxGROW);
		s->Add(primaryb = new wxButton(win, wxID_HIGHEST + 1 + 2 * o, wxT("(none)"), wxDefaultPosition,
			wxSize(200, -1)), 0, wxGROW);
		s->Add(secondaryb = new wxButton(win, wxID_HIGHEST + 2 + 2 * o, wxT("(none)"), wxDefaultPosition,
			wxSize(200, -1)), 0, wxGROW);
		primaryb->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(wxdialog_hotkeys::on_reconfig), NULL, h);
		secondaryb->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(wxdialog_hotkeys::on_reconfig), NULL, h);
		associated = ikey;
	}

	inverse_key* hotkey::get_associated()
	{
		return associated;
	}

	std::string hotkey::get_title()
	{
		return title;
	}

	void hotkey::update(const std::string& primary, const std::string& secondary)
	{
		primaryb->SetLabel((primary != "") ? towxstring(primary) : towxstring("(none)"));
		secondaryb->SetLabel((secondary != "") ? towxstring(secondary) : towxstring("(none)"));
	}
}

std::string wxeditor_keyselect(wxWindow* parent, bool clearable)
{
	wxdialog_keyentry* d = new wxdialog_keyentry(parent, "Specify key", "", clearable);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		throw canceled_exception();
	}
	std::string key = d->getkey();
	d->Destroy();
	return key;
}

void wxeditor_hotkeys_display(wxWindow* parent)
{
	modal_pause_holder hld;
	wxdialog_hotkeys* d = new wxdialog_hotkeys(parent);
	d->ShowModal();
	d->Destroy();
}
