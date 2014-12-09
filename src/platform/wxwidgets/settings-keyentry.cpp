#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"

namespace
{
	int vert_padding = 40;
	int horiz_padding = 60;
}

press_button_dialog::press_button_dialog(wxWindow* parent, emulator_instance& _inst, const std::string& title,
	bool _axis)
	: wxDialog(parent, wxID_ANY, towxstring(title)), inst(_inst)
{
	CHECK_UI_THREAD;
	axis = _axis;
	wxStaticText* t;
	wxBoxSizer* s2 = new wxBoxSizer(wxVERTICAL);
	wxPanel* p = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxWANTS_CHARS);
	s2->Add(p, 1, wxGROW);
	lastkbdkey = -1;
	mouseflag = 0;
	Centre();
	wxFlexGridSizer* s = new wxFlexGridSizer(3, 3, 0, 0);
	p->SetSizer(s);
	SetSizer(s2);
	s->Add(horiz_padding, vert_padding);
	s->Add(0, 0);
	s->Add(0, 0);
	s->Add(0, 0);
	s->Add(t = new wxStaticText(p, wxID_ANY, wxT("Press the key to assign"), wxDefaultPosition,
		wxSize(-1, -1), wxWANTS_CHARS), 1, wxGROW);
	s->Add(0, 0);
	s->Add(0, 0);
	s->Add(0, 0);
	s->Add(horiz_padding, vert_padding);
	p->SetFocus();
	p->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(press_button_dialog::on_keyboard_down), NULL, this);
	p->Connect(wxEVT_KEY_UP, wxKeyEventHandler(press_button_dialog::on_keyboard_up), NULL, this);
	p->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	p->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	p->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	p->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	p->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	p->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	t->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(press_button_dialog::on_keyboard_down), NULL, this);
	t->Connect(wxEVT_KEY_UP, wxKeyEventHandler(press_button_dialog::on_keyboard_up), NULL, this);
	t->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	t->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	t->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	t->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	t->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	t->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(press_button_dialog::on_mouse), NULL, this);
	settings_activate_keygrab(inst, [this](std::string key) { this->dismiss_with(key); });
	s->SetSizeHints(this);
	Fit();
}

bool press_button_dialog::handle_mousebtn(wxMouseEvent& e, bool(wxMouseEvent::*down)()const,
	bool(wxMouseEvent::*up)()const, const std::string& k, int flag)
{
	CHECK_UI_THREAD;
	if((e.*down)())
		mouseflag = flag;
	if((e.*up)()) {
		if(mouseflag == flag) {
			dismiss_with(k);
			return true;
		} else
			mouseflag = 0;
	}
	return false;
}

void press_button_dialog::on_mouse(wxMouseEvent& e)
{
	handle_mousebtn(e, &wxMouseEvent::LeftDown, &wxMouseEvent::LeftUp, "mouse_left", 1);
	handle_mousebtn(e, &wxMouseEvent::MiddleDown, &wxMouseEvent::MiddleUp, "mouse_center", 2);
	handle_mousebtn(e, &wxMouseEvent::RightDown, &wxMouseEvent::RightUp, "mouse_right", 3);
}

void press_button_dialog::on_keyboard_down(wxKeyEvent& e)
{
	CHECK_UI_THREAD;
	lastkbdkey = e.GetKeyCode();
	mouseflag = 0;
}

void press_button_dialog::on_keyboard_up(wxKeyEvent& e)
{
	CHECK_UI_THREAD;
	int kcode = e.GetKeyCode();
	if(lastkbdkey == kcode) {
		dismiss_with(map_keycode_to_key(kcode));
	} else {
		lastkbdkey = -1;
		mouseflag = 0;
	}
}

void press_button_dialog::dismiss_with(const std::string& _k)
{
	CHECK_UI_THREAD;
	std::string k = _k;
	if(k == "")
		return;
	if(axis) {
		//Check that k is a valid axis.
		try {
			//Remove the +/- postfix if any.
			if(k.length() > 1) {
				char lch = k[k.length() - 1];
				if(lch == '+' || lch == '-')
					k = k.substr(0, k.length() - 1);
			}
			keyboard::key& key = inst.keyboard->lookup_key(k);
			if(key.get_type() != keyboard::KBD_KEYTYPE_AXIS)
				return;
		} catch(...) {
			return;
		}
	}
	if(key == "") {
		settings_deactivate_keygrab(inst);
		key = k;
		EndModal(wxID_OK);
	}
}

key_entry_dialog::key_entry_dialog(wxWindow* parent, emulator_instance& _inst, const std::string& title,
	const std::string& spec, bool clearable)
	: wxDialog(parent, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1)), inst(_inst)
{
	CHECK_UI_THREAD;
	wxString boxchoices[] = { wxT("Released"), wxT("Don't care"), wxT("Pressed") };
	std::vector<wxString> classeslist;
	wxString emptystring;
	std::list<keyboard::modifier*> mods;
	std::list<keyboard::key*> keys;

	wtitle = title;

	cleared = false;
	std::set<std::string> x;
	mods = inst.keyboard->all_modifiers();
	keys = inst.keyboard->all_keys();
	for(auto i : keys) {
		std::string kclass = i->get_class();
		if(!x.count(kclass))
			classeslist.push_back(towxstring(kclass));
		x.insert(kclass);
		for(auto k2 : i->get_subkeys())
			classes[kclass].insert(i->get_name() + k2);
	}

	Centre();
	top_s = new wxFlexGridSizer(3, 1, 0, 0);
	SetSizer(top_s);

	t_s = new wxFlexGridSizer(2, 3, 0, 0);
	wxFlexGridSizer* t2_s = new wxFlexGridSizer(mods.size(), 2, 0, 0);
	for(auto i2 : mods) {
		keyentry_mod_data m;
		std::string i = i2->get_name();
		t2_s->Add(new wxStaticText(this, wxID_ANY, towxstring(i)), 0, wxGROW);
		t2_s->Add(m.pressed = new wxComboBox(this, wxID_ANY, boxchoices[1], wxDefaultPosition,
			wxDefaultSize, 3, boxchoices, wxCB_READONLY), 1, wxGROW);
		m.pressed->SetSelection(1);
		modifiers[i] = m;
		m.pressed->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(key_entry_dialog::on_change_setting), NULL, this);
	}
	top_s->Add(t2_s);
	t_s->Add(new wxStaticText(this, wxID_ANY, wxT("Key")), 0, wxGROW);
	t_s->Add(mainclass = new wxComboBox(this, wxID_ANY, classeslist[0], wxDefaultPosition, wxDefaultSize,
		classeslist.size(), &classeslist[0], wxCB_READONLY), 0, wxGROW);
	mainclass->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(key_entry_dialog::on_classchange), NULL, this);
	t_s->Add(mainkey = new wxComboBox(this, wxID_ANY, emptystring, wxDefaultPosition, wxDefaultSize,
		1, &emptystring, wxCB_READONLY), 1, wxGROW);
	mainkey->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(key_entry_dialog::on_change_setting), NULL, this);
	top_s->Add(t_s);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->Add(press = new wxButton(this, wxID_OK, wxT("Prompt key")), 0, wxGROW);
	press->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(key_entry_dialog::on_pressbutton), NULL, this);
	if(clearable)
		pbutton_s->Add(clear = new wxButton(this, wxID_OK, wxT("Clear")), 0, wxGROW);
	pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
	pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
	pbutton_s->AddStretchSpacer();
	ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(key_entry_dialog::on_ok), NULL, this);
	cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(key_entry_dialog::on_cancel), NULL, this);
	mainclass->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(key_entry_dialog::on_classchange), NULL, this);
	if(clearable)
		clear->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(key_entry_dialog::on_clear), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	if(classeslist.empty())
		throw std::runtime_error("No keys");

	set_class(tostdstring(classeslist[0]));

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

void key_entry_dialog::set_mask(const std::string& mod)
{
	CHECK_UI_THREAD;
	if(!modifiers.count(mod))
		return;
	if(modifiers[mod].pressed->GetSelection() == 1) {
		wxCommandEvent e;
		modifiers[mod].pressed->SetSelection(0);
		on_change_setting(e);
	}
}

void key_entry_dialog::set_mod(const std::string& mod)
{
	CHECK_UI_THREAD;
	if(!modifiers.count(mod))
		return;
	if(modifiers[mod].pressed->GetSelection() != 1) {
		wxCommandEvent e;
		modifiers[mod].pressed->SetSelection(2);
		on_change_setting(e);
	}
}

void key_entry_dialog::set_set(const std::string& mset,
	void (key_entry_dialog::*fn)(const std::string& mod))
{
	CHECK_UI_THREAD;
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

void key_entry_dialog::load_spec(const std::string& spec)
{
	CHECK_UI_THREAD;
	std::string _spec = spec;
	size_t s1 = _spec.find_first_of("/");
	size_t s2 = _spec.find_first_of("|");
	if(s1 >= _spec.length() || s2 >= _spec.length())
		return;		//Bad.
	std::string mod = _spec.substr(0, s1);
	std::string mask = _spec.substr(s1 + 1, s2 - s1 - 1);
	std::string key = _spec.substr(s2 + 1);
	set_set(mask, &key_entry_dialog::set_mask);
	set_set(mod, &key_entry_dialog::set_mod);
	std::string _class;
	for(auto i : classes)
		if(i.second.count(key))
			_class = i.first;
	if(_class != "") {
		set_class(_class);
		mainclass->SetValue(towxstring(_class));
		mainkey->SetValue(towxstring(key));
	}
	t_s->Layout();
	top_s->Layout();
	Fit();
}

void key_entry_dialog::on_change_setting(wxCommandEvent& e)
{
}

void key_entry_dialog::on_pressbutton(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	press_button_dialog* p = new press_button_dialog(this, inst, wtitle, false);
	p->ShowModal();
	std::string key = p->getkey();
	p->Destroy();
	std::string _class;
	for(auto i : classes)
		if(i.second.count(key))
			_class = i.first;
	if(_class == "")
		return;
	set_class(_class);
	mainclass->SetValue(towxstring(_class));
	mainkey->SetValue(towxstring(key));
}

void key_entry_dialog::on_ok(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_OK);
}

void key_entry_dialog::on_clear(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	cleared = true;
	EndModal(wxID_OK);
}

void key_entry_dialog::on_cancel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	EndModal(wxID_CANCEL);
}

void key_entry_dialog::set_class(const std::string& _class)
{
	CHECK_UI_THREAD;
	if(!mainkey)
		return;
	if(currentclass == _class)
		return;
	mainkey->Clear();
	for(auto i : classes[_class])
		mainkey->Append(towxstring(i));
	currentclass = _class;
	mainkey->SetSelection(0);
	t_s->Layout();
	top_s->Layout();
	Fit();
}

void key_entry_dialog::on_classchange(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	set_class(tostdstring(mainclass->GetValue()));
}

std::string key_entry_dialog::getkey()
{
	CHECK_UI_THREAD;
	if(cleared)
		return "";
	std::string x;
	bool f;
	f = true;
	for(auto i : modifiers) {
		if(i.second.pressed->GetSelection() == 2) {
			if(!f)
				x = x + ",";
			f = false;
			x = x + i.first;
		}
	}
	x = x + "/";
	f = true;
	for(auto i : modifiers) {
		if(i.second.pressed->GetSelection() != 1) {
			if(!f)
				x = x + ",";
			f = false;
			x = x + i.first;
		}
	}
	x = x + "|" + tostdstring(mainkey->GetValue());
	return x;
}

std::string clean_keystring(const std::string& in)
{
	regex_results tmp = regex("(.*)/(.*)\\|(.*)", in);
	if(!tmp)
		return in;
	std::string mods = tmp[1];
	std::string mask = tmp[2];
	std::string key = tmp[3];
	std::set<std::string> _mods, _mask;
	std::string tmp2;
	for(auto& tmp2 : token_iterator<char>::foreach(mods, {","}))
		_mods.insert(tmp2);
	for(auto& tmp2 : token_iterator<char>::foreach(mask, {","}))
		_mask.insert(tmp2);
	for(auto i : _mods)
		if(!_mask.count(i))
			return in;
	std::string out;
	for(auto i : _mask)
		if(!_mods.count(i))
			out = out + "!" + i + "+";
		else
			out = out + i + "+";
	out = out + key;
	return out;
}
