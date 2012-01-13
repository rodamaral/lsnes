#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/lua.hpp"
#include "core/mainloop.hpp"
#include "core/memorywatch.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/window.hpp"
#include "core/zip.hpp"

#include <vector>
#include <string>

#include "plat-wxwidgets/menu_dump.hpp"
#include "plat-wxwidgets/platform.hpp"
#include "plat-wxwidgets/window_mainwindow.hpp"
#include "plat-wxwidgets/window_status.hpp"

#define MAXCONTROLLERS MAX_PORTS * MAX_CONTROLLERS_PER_PORT

extern "C"
{
#ifndef UINT64_C
#define UINT64_C(val) val##ULL
#endif
#include <libswscale/swscale.h>
}

enum
{
	wxID_PAUSE = wxID_HIGHEST + 1,
	wxID_FRAMEADVANCE,
	wxID_SUBFRAMEADVANCE,
	wxID_NEXTPOLL,
	wxID_ERESET,
	wxID_AUDIO_ENABLED,
	wxID_SHOW_AUDIO_STATUS,
	wxID_AUDIODEV_FIRST,
	wxID_AUDIODEV_LAST = wxID_AUDIODEV_FIRST + 255,
	wxID_SAVE_STATE,
	wxID_SAVE_MOVIE,
	wxID_LOAD_STATE,
	wxID_LOAD_STATE_RO,
	wxID_LOAD_STATE_RW,
	wxID_LOAD_STATE_P,
	wxID_LOAD_MOVIE,
	wxID_RUN_SCRIPT,
	wxID_RUN_LUA,
	wxID_EVAL_LUA,
	wxID_SAVE_SCREENSHOT,
	wxID_READONLY_MODE,
	wxID_EDIT_AUTHORS,
	wxID_AUTOHOLD_FIRST,
	wxID_AUTOHOLD_LAST = wxID_AUTOHOLD_FIRST + 127,
	wxID_EDIT_AXES,
	wxID_EDIT_SETTINGS,
	wxID_EDIT_KEYBINDINGS,
	wxID_EDIT_ALIAS,
	wxID_EDIT_MEMORYWATCH,
	wxID_SAVE_MEMORYWATCH,
	wxID_LOAD_MEMORYWATCH,
	wxID_DUMP_FIRST,
	wxID_DUMP_LAST = wxID_DUMP_FIRST + 1023,
	wxID_REWIND_MOVIE,
	wxID_EDIT_JUKEBOX
};


namespace
{
	unsigned char* screen_buffer;
	uint32_t old_width;
	uint32_t old_height;
	bool main_window_dirty;
	struct thread* emulation_thread;

	wxString getname()
	{
		std::string windowname = "lsnes rr" + lsnes_version + "[" + bsnes_core_version + "]";
		return towxstring(windowname);
	}

	struct emu_args
	{
		struct loaded_rom* rom;
		struct moviefile* initial;
		bool load_has_to_succeed;
	};

	void* emulator_main(void* _args)
	{
		struct emu_args* args = reinterpret_cast<struct emu_args*>(_args);
		try {
			our_rom = args->rom;
			struct moviefile* movie = args->initial;
			bool has_to_succeed = args->load_has_to_succeed;
			platform::flush_command_queue();
			main_loop(*our_rom, *movie, has_to_succeed);
			signal_program_exit();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			messages << "FATAL: " << e.what() << std::endl;
			platform::fatal_error();
		}
		return NULL;
	}

	void join_emulator_thread()
	{
		emulation_thread->join();
	}

	void handle_wx_mouse(wxMouseEvent& e)
	{
		static uint32_t mask = 0;
		if(e.LeftDown())
			mask |= 1;
		if(e.LeftUp())
			mask &= ~1;
		if(e.MiddleDown())
			mask |= 2;
		if(e.MiddleUp())
			mask &= ~2;
		if(e.RightDown())
			mask |= 4;
		if(e.RightUp())
			mask &= ~4;
		send_mouse_click(e.GetX(), e.GetY(), mask);
	}

	bool is_readonly_mode()
	{
		bool ret;
		runemufn([&ret]() { ret = movb.get_movie().readonly_mode(); });
		return ret;
	}

	bool UI_get_autohold(unsigned pid, unsigned idx)
	{
		bool ret;
		runemufn([&ret, pid, idx]() { ret = controls.autohold(pid, idx); });
		return ret;
	}

	void UI_change_autohold(unsigned pid, unsigned idx, bool newstate)
	{
		runemufn([pid, idx, newstate]() { controls.autohold(pid, idx, newstate); });
	}

	int UI_controller_index_by_logical(unsigned lid)
	{
		int ret;
		runemufn([&ret, lid]() { ret = controls.lcid_to_pcid(lid); });
		return ret;
	}

	int UI_button_id(unsigned pcid, unsigned lidx)
	{
		int ret;
		runemufn([&ret, pcid, lidx]() { ret = controls.button_id(pcid, lidx); });
		return ret;
	}

	class controller_autohold_menu : public wxMenu
	{
	public:
		controller_autohold_menu(unsigned lid, enum devicetype_t dtype);
		void change_type();
		bool is_dummy();
		void on_select(wxCommandEvent& e);
		void update(unsigned pid, unsigned ctrlnum, bool newstate);
	private:
		unsigned our_lid;
		wxMenuItem* entries[MAX_LOGICAL_BUTTONS];
		unsigned enabled_entries;
	};

	class autohold_menu : public wxMenu
	{
	public:
		autohold_menu(wxwin_mainwindow* win);
		void reconfigure();
		void on_select(wxCommandEvent& e);
		void update(unsigned pid, unsigned ctrlnum, bool newstate);
	private:
		controller_autohold_menu* menus[MAXCONTROLLERS];
		wxMenuItem* entries[MAXCONTROLLERS];
	};

	class sound_select_menu : public wxMenu
	{
	public:
		sound_select_menu(wxwin_mainwindow* win);
		void update(const std::string& dev);
		void on_select(wxCommandEvent& e);
	private:
		std::map<std::string, wxMenuItem*> items; 
		std::map<int, std::string> devices;
	};

	class sound_select_menu;

	class broadcast_listener : public information_dispatch
	{
	public:
		broadcast_listener(wxwin_mainwindow* win);
		void set_sound_select(sound_select_menu* sdev);
		void set_autohold_menu(autohold_menu* ah);
		void on_sound_unmute(bool unmute) throw();
		void on_sound_change(const std::string& dev) throw();
		void on_mode_change(bool readonly) throw();
		void on_autohold_update(unsigned pid, unsigned ctrlnum, bool newstate);
		void on_autohold_reconfigure();
	private:
		wxwin_mainwindow* mainw;
		sound_select_menu* sounddev;
		autohold_menu* ahmenu;
	};

	controller_autohold_menu::controller_autohold_menu(unsigned lid, enum devicetype_t dtype)
	{
		platform::set_modal_pause(true);
		our_lid = lid;
		for(unsigned i = 0; i < MAX_LOGICAL_BUTTONS; i++) {
			int id = wxID_AUTOHOLD_FIRST + MAX_LOGICAL_BUTTONS * lid + i;
			entries[i] = AppendCheckItem(id, towxstring(get_logical_button_name(i)));
		}
		change_type();
		platform::set_modal_pause(false);
	}

	void controller_autohold_menu::change_type()
	{
		enabled_entries = 0;
		int pid = controls.lcid_to_pcid(our_lid);
		for(unsigned i = 0; i < MAX_LOGICAL_BUTTONS; i++) {
			int pidx = -1;
			if(pid >= 0)
				pidx = controls.button_id(pid, i);
			if(pidx >= 0) {
				entries[i]->Check(pid > 0 && UI_get_autohold(pid, pidx));
				entries[i]->Enable();
				enabled_entries++;
			} else {
				entries[i]->Check(false);
				entries[i]->Enable(false);
			}
		}
	}

	bool controller_autohold_menu::is_dummy()
	{
		return !enabled_entries;
	}

	void controller_autohold_menu::on_select(wxCommandEvent& e)
	{
		int x = e.GetId();
		if(x < wxID_AUTOHOLD_FIRST + our_lid * MAX_LOGICAL_BUTTONS || x >= wxID_AUTOHOLD_FIRST * 
			(our_lid + 1) * MAX_LOGICAL_BUTTONS) {
			return;
		}
		unsigned lidx = (x - wxID_AUTOHOLD_FIRST) % MAX_LOGICAL_BUTTONS;
		platform::set_modal_pause(true);
		int pid = controls.lcid_to_pcid(our_lid);
		if(pid < 0 || !entries[lidx]) {
			platform::set_modal_pause(false);
			return;
		}
		int pidx = controls.button_id(pid, lidx);
		if(pidx < 0) {
			platform::set_modal_pause(false);
			return;
		}
		//Autohold change on pid=pid, ctrlindx=idx, state
		bool newstate = entries[lidx]->IsChecked();
		UI_change_autohold(pid, pidx, newstate);
		platform::set_modal_pause(false);
	}

	void controller_autohold_menu::update(unsigned pid, unsigned ctrlnum, bool newstate)
	{
		platform::set_modal_pause(true);
		int pid2 = UI_controller_index_by_logical(our_lid);
		if(pid2 < 0 || static_cast<unsigned>(pid) != pid2) {
			platform::set_modal_pause(false);
			return;
		}
		for(unsigned i = 0; i < MAX_LOGICAL_BUTTONS; i++) {
			int idx = UI_button_id(pid2, i);
			if(idx < 0 || static_cast<unsigned>(idx) != ctrlnum)
				continue;
			entries[i]->Check(newstate);
		}
		platform::set_modal_pause(false);
	}


	autohold_menu::autohold_menu(wxwin_mainwindow* win)
	{
		for(unsigned i = 0; i < MAXCONTROLLERS; i++) {
			std::ostringstream str;
			str << "Controller #&" << (i + 1);
			menus[i] = new controller_autohold_menu(i, DT_NONE);
			entries[i] = AppendSubMenu(menus[i], towxstring(str.str()));
			entries[i]->Enable(!menus[i]->is_dummy());
		}
		win->Connect(wxID_AUTOHOLD_FIRST, wxID_AUTOHOLD_LAST, wxEVT_COMMAND_MENU_SELECTED,
			wxCommandEventHandler(autohold_menu::on_select), NULL, this);
		reconfigure();
	}

	void autohold_menu::reconfigure()
	{
		platform::set_modal_pause(true);
		for(unsigned i = 0; i < MAXCONTROLLERS; i++) {
			menus[i]->change_type();
			entries[i]->Enable(!menus[i]->is_dummy());
		}
		platform::set_modal_pause(false);
	}

	void autohold_menu::on_select(wxCommandEvent& e)
	{
		for(unsigned i = 0; i < MAXCONTROLLERS; i++)
			menus[i]->on_select(e);
	}

	void autohold_menu::update(unsigned pid, unsigned ctrlnum, bool newstate)
	{
		for(unsigned i = 0; i < MAXCONTROLLERS; i++)
			menus[i]->update(pid, ctrlnum, newstate);
	}

	sound_select_menu::sound_select_menu(wxwin_mainwindow* win)
	{
		std::string curdev = platform::get_sound_device();
		int j = wxID_AUDIODEV_FIRST;
		for(auto i : platform::get_sound_devices()) {
			items[i.first] = AppendRadioItem(j, towxstring(i.first + "(" + i.second + ")"));
			devices[j] = i.first;
			if(i.first == curdev)
				items[i.first]->Check();
			win->Connect(j, wxEVT_COMMAND_MENU_SELECTED,
				wxCommandEventHandler(sound_select_menu::on_select), NULL, this);
			j++;
		}
	}

	void sound_select_menu::update(const std::string& dev)
	{
		items[dev]->Check();
	}
/*
	void _do_sound_select(void* args)
	{
		std::string* x = reinterpret_cast<std::string*>(args);
		platform::set_sound_device(*x);
	}
*/
	void sound_select_menu::on_select(wxCommandEvent& e)
	{
		std::string devname = devices[e.GetId()];
		if(devname != "")
			runemufn([devname]() { platform::set_sound_device(devname); });
	}

	broadcast_listener::broadcast_listener(wxwin_mainwindow* win)
		: information_dispatch("wxwidgets-broadcast-listener")
	{
		mainw = win;
	}

	void broadcast_listener::set_sound_select(sound_select_menu* sdev)
	{
		sounddev = sdev;
	}

	void broadcast_listener::set_autohold_menu(autohold_menu* ah)
	{
		ahmenu = ah;
	}

	void broadcast_listener::on_sound_unmute(bool unmute) throw()
	{
		runuifun([unmute, mainw]() { mainw->menu_check(wxID_AUDIO_ENABLED, unmute); });
	}

	void broadcast_listener::on_sound_change(const std::string& dev) throw()
	{
		runuifun([dev, sounddev]() { if(sounddev) sounddev->update(dev); });
	}

	void broadcast_listener::on_mode_change(bool readonly) throw()
	{
		runuifun([readonly, mainw]() { mainw->menu_check(wxID_READONLY_MODE, readonly); });
	}

	void broadcast_listener::on_autohold_update(unsigned pid, unsigned ctrlnum, bool newstate)
	{
		runuifun([pid, ctrlnum, newstate, ahmenu]() { ahmenu->update(pid, ctrlnum, newstate); });
	}

	void broadcast_listener::on_autohold_reconfigure()
	{
		runuifun([ahmenu]() { ahmenu->reconfigure(); });
	}

	void _set_readonly(void* args)
	{
		bool s = *reinterpret_cast<bool*>(args);
		movb.get_movie().readonly_mode(s);
		if(!s)
			lua_callback_do_readwrite();
		update_movie_state();
	}

	struct keyentry_mod_data
	{
		wxCheckBox* pressed;
		wxCheckBox* unmasked;
		unsigned tmpflags;
	};

	class wxdialog_keyentry : public wxDialog
	{
	public:
		wxdialog_keyentry(wxWindow* parent);
		void on_change_setting(wxCommandEvent& e);
		void on_ok(wxCommandEvent& e);
		void on_cancel(wxCommandEvent& e);
		std::string getkey();
	private:
		std::map<std::string, keyentry_mod_data> modifiers;
		wxComboBox* mainkey;
		wxButton* ok;
		wxButton* cancel;
	};

	wxdialog_keyentry::wxdialog_keyentry(wxWindow* parent)
		: wxDialog(parent, wxID_ANY, wxT("Specify key"), wxDefaultPosition, wxSize(-1, -1))
	{
		std::vector<wxString> keych;
		std::set<std::string> mods, keys;

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
		pbutton_s->AddStretchSpacer();
		pbutton_s->Add(ok = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
		pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
		ok->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_ok), NULL, this);
		cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxdialog_keyentry::on_cancel), NULL, this);
		top_s->Add(pbutton_s, 0, wxGROW);

		t_s->SetSizeHints(this);
		top_s->SetSizeHints(this);
		Fit();
	}

#define TMPFLAG_UNMASKED 65
#define TMPFLAG_UNMASKED_LINK_CHILD 2
#define TMPFLAG_UNMASKED_LINK_PARENT 68
#define TMPFLAG_PRESSED 8
#define TMPFLAG_PRESSED_LINK_CHILD 16
#define TMPFLAG_PRESSED_LINK_PARENT 32

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

	void wxdialog_keyentry::on_cancel(wxCommandEvent& e)
	{
		EndModal(wxID_CANCEL);
	}

	std::string wxdialog_keyentry::getkey()
	{
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
}

void boot_emulator(loaded_rom& rom, moviefile& movie)
{
	try {
		struct emu_args* a = new emu_args;
		a->rom = &rom;
		a->initial = &movie;
		a->load_has_to_succeed = false;
		platform::set_modal_pause(true);
		emulation_thread = &thread::create(emulator_main, a);
		main_window = new wxwin_mainwindow();
		main_window->Show();
		platform::set_modal_pause(false);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

wxwin_mainwindow::panel::panel(wxWindow* win)
	: wxPanel(win)
{
	initialize_wx_keyboard();
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(panel::on_paint), NULL, this);
	this->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(panel::on_erase), NULL, this);
	this->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(panel::on_keyboard_down), NULL, this);
	this->Connect(wxEVT_KEY_UP, wxKeyEventHandler(panel::on_keyboard_up), NULL, this);
	this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(panel::on_mouse), NULL, this);
	SetMinSize(wxSize(512, 448));
}

void wxwin_mainwindow::menu_start(wxString name)
{
	while(!upper.empty())
		upper.pop();
	current_menu = new wxMenu();
	menubar->Append(current_menu, name);
}

void wxwin_mainwindow::menu_special(wxString name, wxMenu* menu)
{
	while(!upper.empty())
		upper.pop();
	menubar->Append(menu, name);
	current_menu = NULL;
}

void wxwin_mainwindow::menu_special_sub(wxString name, wxMenu* menu)
{
	current_menu->AppendSubMenu(menu, name);
}

void wxwin_mainwindow::menu_entry(int id, wxString name)
{
	current_menu->Append(id, name);
	Connect(id, wxEVT_COMMAND_MENU_SELECTED, 
		wxCommandEventHandler(wxwin_mainwindow::wxwin_mainwindow::handle_menu_click), NULL, this);
}

void wxwin_mainwindow::menu_entry_check(int id, wxString name)
{
	checkitems[id] = current_menu->AppendCheckItem(id, name);
	Connect(id, wxEVT_COMMAND_MENU_SELECTED, 
		wxCommandEventHandler(wxwin_mainwindow::wxwin_mainwindow::handle_menu_click), NULL, this);
}

void wxwin_mainwindow::menu_start_sub(wxString name)
{
	wxMenu* old = current_menu;
	upper.push(current_menu);
	current_menu = new wxMenu();
	old->AppendSubMenu(current_menu, name);
}

void wxwin_mainwindow::menu_end_sub(wxString name)
{
	current_menu = upper.top();
	upper.pop();
}

bool wxwin_mainwindow::menu_ischecked(int id)
{
	if(checkitems.count(id))
		return checkitems[id]->IsChecked();
	else
		return false;
}

void wxwin_mainwindow::menu_check(int id, bool newstate)
{
	if(checkitems.count(id))
		return checkitems[id]->Check(newstate);
	else
		return;
}

void wxwin_mainwindow::menu_separator()
{
	current_menu->AppendSeparator();
}

void wxwin_mainwindow::panel::request_paint()
{
	Refresh();
}

void wxwin_mainwindow::panel::on_paint(wxPaintEvent& e)
{
	render_framebuffer();
	static struct SwsContext* ctx;
	uint8_t* srcp[1];
	int srcs[1];
	uint8_t* dstp[1];
	int dsts[1];
	wxPaintDC dc(this);
	if(!screen_buffer || main_screen.width != old_width || main_screen.height != old_height) {
		if(screen_buffer)
			delete[] screen_buffer;
		screen_buffer = new unsigned char[main_screen.width * main_screen.height * 3];
		old_height = main_screen.height;
		old_width = main_screen.width;
		uint32_t w = main_screen.width;
		uint32_t h = main_screen.height;
		if(w && h)
			ctx = sws_getCachedContext(ctx, w, h, PIX_FMT_RGBA, w, h, PIX_FMT_BGR24, SWS_POINT |
				SWS_CPU_CAPS_MMX2, NULL, NULL, NULL);
		if(w < 512)
			w = 512;
		if(h < 448)
			h = 448;
		SetMinSize(wxSize(w, h));
		main_window->Fit();
	}
	srcs[0] = 4 * main_screen.width;
	dsts[0] = 3 * main_screen.width;
	srcp[0] = reinterpret_cast<unsigned char*>(main_screen.memory);
	dstp[0] = screen_buffer;
	memset(screen_buffer, 0, main_screen.width * main_screen.height * 3);
	uint64_t t1 = get_utime();
	if(main_screen.width && main_screen.height)
		sws_scale(ctx, srcp, srcs, 0, main_screen.height, dstp, dsts);
	uint64_t t2 = get_utime();
	wxBitmap bmp(wxImage(main_screen.width, main_screen.height, screen_buffer, true));
	uint64_t t3 = get_utime();
	dc.DrawBitmap(bmp, 0, 0, false);
	main_window_dirty = false;
}

void wxwin_mainwindow::panel::on_erase(wxEraseEvent& e)
{
	//Blank.
}

void wxwin_mainwindow::panel::on_keyboard_down(wxKeyEvent& e)
{
	handle_wx_keyboard(e, true);
}

void wxwin_mainwindow::panel::on_keyboard_up(wxKeyEvent& e)
{
	handle_wx_keyboard(e, false);
}

void wxwin_mainwindow::panel::on_mouse(wxMouseEvent& e)
{
	handle_wx_mouse(e);
}

wxwin_mainwindow::wxwin_mainwindow()
	: wxFrame(NULL, wxID_ANY, getname(), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxCLOSE_BOX)
{
	broadcast_listener* blistener = new broadcast_listener(this);
	Centre();
	wxFlexGridSizer* toplevel = new wxFlexGridSizer(1, 1, 0, 0);
	toplevel->Add(gpanel = new panel(this), 1, wxGROW);
	toplevel->SetSizeHints(this);
	SetSizer(toplevel);
	Fit();
	gpanel->SetFocus();
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_mainwindow::on_close));
	menubar = new wxMenuBar;
	SetMenuBar(menubar);

	//TOP-level accels: ACFOS.
	//System menu: (ACFOS)EMNPQRU
	menu_start(wxT("&System"));
	menu_entry(wxID_FRAMEADVANCE, wxT("Fra&me advance"));
	menu_entry(wxID_SUBFRAMEADVANCE, wxT("S&ubframe advance"));
	menu_entry(wxID_NEXTPOLL, wxT("&Next poll"));
	menu_entry(wxID_PAUSE, wxT("&Pause/Unpause"));
	menu_separator();
	menu_entry(wxID_ERESET, wxT("&Reset"));
	menu_separator();
	menu_entry(wxID_EDIT_AUTHORS, wxT("&Edit game name && authors"));
	menu_separator();
	menu_entry(wxID_EXIT, wxT("&Quit"));
	menu_separator();
	menu_entry(wxID_ABOUT, wxT("About"));
	//File menu: (ACFOS)DELMNPRTUVW
	menu_start(wxT("&File"));
	menu_entry_check(wxID_READONLY_MODE, wxT("Reado&nly mode"));
	menu_check(wxID_READONLY_MODE, is_readonly_mode());
	menu_separator();
	menu_entry(wxID_SAVE_STATE, wxT("Save stat&e"));
	menu_entry(wxID_SAVE_MOVIE, wxT("Sa&ve movie"));
	menu_separator();
	menu_entry(wxID_LOAD_STATE, wxT("&Load state"));
	menu_entry(wxID_LOAD_STATE_RO, wxT("Loa&d state (readonly)"));
	menu_entry(wxID_LOAD_STATE_RW, wxT("Load s&tate (read-write)"));
	menu_entry(wxID_LOAD_STATE_P, wxT("Load state (&preserve)"));
	menu_entry(wxID_LOAD_MOVIE, wxT("Load &movie"));
	menu_entry(wxID_REWIND_MOVIE, wxT("Re&wind movie"));
	menu_separator();
	menu_entry(wxID_SAVE_SCREENSHOT, wxT("Save sc&reenshot"));
	menu_separator();
	menu_special_sub(wxT("D&ump video"), reinterpret_cast<dumper_menu*>(dmenu = new dumper_menu(this,
		wxID_DUMP_FIRST, wxID_DUMP_LAST)));
	//Autohold menu: (ACFOS)
	menu_special(wxT("&Autohold"), reinterpret_cast<autohold_menu*>(ahmenu = new autohold_menu(this)));
	blistener->set_autohold_menu(reinterpret_cast<autohold_menu*>(ahmenu));
	//Scripting menu: (ACFOS)ERU
	menu_start(wxT("S&cripting"));
	menu_entry(wxID_RUN_SCRIPT, wxT("&Run script"));
	if(lua_supported) {
		menu_separator();
		menu_entry(wxID_EVAL_LUA, wxT("&Evaluate Lua statement"));
		menu_entry(wxID_RUN_LUA, wxT("R&un Lua script"));
	}
	menu_separator();
	menu_entry(wxID_EDIT_MEMORYWATCH, wxT("Edit memory watch"));
	menu_separator();
	menu_entry(wxID_LOAD_MEMORYWATCH, wxT("Load memory watch"));
	menu_entry(wxID_SAVE_MEMORYWATCH, wxT("Save memory watch"));
	//Settings menu: (ACFOS)
	menu_start(wxT("Settings"));
	menu_entry(wxID_EDIT_AXES, wxT("Configure axes"));
	menu_entry(wxID_EDIT_SETTINGS, wxT("Configure settings"));
	menu_entry(wxID_EDIT_KEYBINDINGS, wxT("Configure keybindings"));
	menu_entry(wxID_EDIT_ALIAS, wxT("Configure aliases"));
	menu_entry(wxID_EDIT_JUKEBOX, wxT("Configure jukebox"));
	if(platform::sound_initialized()) {
		//Sound menu: (ACFOS)EHU
		menu_start(wxT("S&ound"));
		menu_entry_check(wxID_AUDIO_ENABLED, wxT("So&unds enabled"));
		menu_check(wxID_AUDIO_ENABLED, platform::is_sound_enabled());
		menu_entry(wxID_SHOW_AUDIO_STATUS, wxT("S&how audio status"));
		menu_separator();
		menu_special_sub(wxT("S&elect sound device"), reinterpret_cast<sound_select_menu*>(sounddev =
			new sound_select_menu(this)));
		blistener->set_sound_select(reinterpret_cast<sound_select_menu*>(sounddev));
	}
}

void wxwin_mainwindow::request_paint()
{
	gpanel->Refresh();
}

void wxwin_mainwindow::on_close(wxCloseEvent& e)
{
	//Veto it for now, latter things will delete it.
	e.Veto();
	platform::queue("quit-emulator");
}

void wxwin_mainwindow::notify_update() throw()
{
	if(!main_window_dirty) {
		main_window_dirty = true;
		gpanel->Refresh();
	}
}

void wxwin_mainwindow::notify_exit() throw()
{
	join_emulator_thread();
	Destroy();
}

void wxwin_mainwindow::handle_menu_click(wxCommandEvent& e)
{
	wxFileDialog* d;
	wxTextEntryDialog* d2;
	std::string filename;
	bool s;
	switch(e.GetId()) {
	case wxID_FRAMEADVANCE:
		platform::queue("+advance-frame");
		platform::queue("-advance-frame");
		return;
	case wxID_SUBFRAMEADVANCE:
		platform::queue("+advance-poll");
		platform::queue("-advance-poll");
		return;
	case wxID_NEXTPOLL:
		platform::queue("advance-skiplag");
		return;
	case wxID_PAUSE:
		platform::queue("pause-emulator");
		return;
	case wxID_RESET:
		platform::queue("reset");
		return;
	case wxID_EXIT:
		platform::queue("quit-emulator");
		return;
	case wxID_AUDIO_ENABLED:
		platform::sound_enable(menu_ischecked(wxID_AUDIO_ENABLED));
		return;
	case wxID_SHOW_AUDIO_STATUS:
		platform::queue("show-sound-status");
		return;
	case wxID_LOAD_MOVIE:
		d = new wxFileDialog(this, wxT("Load Movie"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("load-movie " + filename);
		break;
	case wxID_LOAD_STATE:
		d = new wxFileDialog(this, wxT("Load State"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("load " + filename);
		break;
	case wxID_LOAD_STATE_RO:
		d = new wxFileDialog(this, wxT("Load State (Read-Only)"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("load-readonly " + filename);
		break;
	case wxID_LOAD_STATE_RW:
		d = new wxFileDialog(this, wxT("Load State (Read-Write)"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("load-state " + filename);
		break;
	case wxID_REWIND_MOVIE:
		platform::queue("rewind-movie");
		break;
	case wxID_SAVE_MOVIE:
		d = new wxFileDialog(this, wxT("Save Movie"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("save-movie " + filename);
		break;
	case wxID_SAVE_STATE:
		d = new wxFileDialog(this, wxT("Save State"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("save-state " + filename);
		break;
	case wxID_SAVE_SCREENSHOT:
		d = new wxFileDialog(this, wxT("Save Screenshot"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("take-screenshot " + filename);
		break;
	case wxID_RUN_SCRIPT:
		d = new wxFileDialog(this, wxT("Select Script"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("run-script " + filename);
		break;
	case wxID_RUN_LUA:
		d = new wxFileDialog(this, wxT("Select Lua Script"), wxT("."));
		if(d->ShowModal() == wxID_OK)
			filename = tostdstring(d->GetPath());
		d->Destroy();
		if(filename == "")
			break;
		platform::queue("run-lua " + filename);
		break;
	case wxID_EVAL_LUA:
		d2 = new wxTextEntryDialog(this, wxT("Enter Lua statement:"), wxT("Evaluate Lua"));
		if(d2->ShowModal() == wxID_OK)
			filename = tostdstring(d2->GetValue());
		d2->Destroy();
		platform::queue("evaluate-lua " + filename);
		break;
	case wxID_READONLY_MODE:
		s = menu_ischecked(wxID_READONLY_MODE);
		platform::queue(_set_readonly, &s, true);
		break;
	case wxID_EDIT_AXES:
		wxeditor_axes_display(this);
		break;
	case wxID_EDIT_AUTHORS:
		wxeditor_authors_display(this);
		break;
	case wxID_EDIT_SETTINGS:
		wxeditor_settings_display(this);
		break;
	case wxID_EDIT_KEYBINDINGS:
		menu_edit_keybindings(e);
		break;
	case wxID_EDIT_ALIAS:
		menu_edit_aliases(e);
		break;
	case wxID_EDIT_JUKEBOX:
		menu_edit_jukebox(e);
		break;
	case wxID_EDIT_MEMORYWATCH:
		menu_edit_memorywatch(e);
		break;
	case wxID_SAVE_MEMORYWATCH:
		menu_save_memorywatch(e);
		break;
	case wxID_LOAD_MEMORYWATCH:
		menu_load_memorywatch(e);
		break;
	case wxID_ABOUT: {
		std::ostringstream str;
		str << "Version: lsnes rr" << lsnes_version << std::endl;
		str << "Revision: " << lsnes_git_revision << std::endl;
		str << "Core: " << bsnes_core_version << std::endl;
		wxMessageBox(towxstring(str.str()), _T("About"), wxICON_INFORMATION | wxOK, this);
	}
		break;
	};
}

#define NEW_KEYBINDING "A new binding..."
#define NEW_ALIAS "A new alias..."
#define NEW_WATCH "A new watch..."

void wxwin_mainwindow::menu_edit_keybindings(wxCommandEvent& e)
{
	platform::set_modal_pause(true);
	std::set<std::string> bind;
	runemufn([&bind]() { bind = keymapper::get_bindings(); });
	std::vector<wxString> choices;
	choices.push_back(wxT(NEW_KEYBINDING));
	for(auto i : bind)
		choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, wxT("Select keybinding to edit"),
		wxT("Select binding"), choices.size(), &choices[0]);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	std::string key = tostdstring(d->GetStringSelection());
	d->Destroy();
	if(key == NEW_KEYBINDING) {
		wxdialog_keyentry* d2 = new wxdialog_keyentry(this);
		//wxTextEntryDialog* d2 = new wxTextEntryDialog(this, wxT("Enter key for binding:"),
		//	wxT("Edit binding"), wxT(""));
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			platform::set_modal_pause(false);
			return;
		}
		key = d2->getkey();
		//key = tostdstring(d2->GetValue());
		d2->Destroy();
	}
	std::string old_command_value;
	runemufn([&old_command_value, key]() { old_command_value = keymapper::get_command_for(key); });
	wxTextEntryDialog* d4 = new wxTextEntryDialog(this, wxT("Enter new command for binding:"), wxT("Edit binding"),
		towxstring(old_command_value));
	if(d4->ShowModal() == wxID_CANCEL) {
		d4->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	bool fault = false;
	std::string faulttext;
	std::string newcommand = tostdstring(d4->GetValue());
	runemufn([&fault, &faulttext, key, newcommand]() {
		try {
			keymapper::bind_for(key, newcommand);
		} catch(std::exception& e) {
		}
		});
	if(fault) {
		wxMessageDialog* d3 = new wxMessageDialog(this, towxstring(std::string("Can't bind key: ") +
			faulttext), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d3->ShowModal();
		d3->Destroy();
	}
	d4->Destroy();
	platform::set_modal_pause(false);
}

void strip_CR(std::string& x) throw(std::bad_alloc);

void wxwin_mainwindow::menu_edit_aliases(wxCommandEvent& e)
{
	platform::set_modal_pause(true);
	std::set<std::string> bind;
	runemufn([&bind]() { bind = command::get_aliases(); });
	std::vector<wxString> choices;
	choices.push_back(wxT(NEW_ALIAS));
	for(auto i : bind)
		choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, wxT("Select alias to edit"),
		wxT("Select alias"), choices.size(), &choices[0]);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	std::string alias = tostdstring(d->GetStringSelection());
	d->Destroy();
	if(alias == NEW_ALIAS) {
		wxTextEntryDialog* d2 = new wxTextEntryDialog(this, wxT("Enter name for the new alias:"),
			wxT("Enter alias name"));
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			platform::set_modal_pause(false);
			return;
		}
		alias = tostdstring(d2->GetValue());
		d2->Destroy();
		if(!command::valid_alias_name(alias)) {
			wxMessageDialog* d3 = new wxMessageDialog(this, towxstring(std::string("Not a valid alias "
			"name: ") + alias), wxT("Error"), wxOK | wxICON_EXCLAMATION);
			d3->ShowModal();
			d3->Destroy();
			platform::set_modal_pause(false);
			return;
		}
	}
	std::string old_alias_value = command::get_alias_for(alias);
	wxTextEntryDialog* d4 = new wxTextEntryDialog(this, wxT("Enter new commands for alias:"), wxT("Edit alias"),
		towxstring(old_alias_value), wxOK | wxCANCEL | wxCENTRE | wxTE_MULTILINE);
	if(d4->ShowModal() == wxID_CANCEL) {
		d4->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	std::string newcmd = tostdstring(d4->GetValue());
	runemufn([alias, newcmd]() { command::set_alias_for(alias, newcmd); });
	d4->Destroy();
	platform::set_modal_pause(false);
}

void wxwin_mainwindow::menu_edit_jukebox(wxCommandEvent& e)
{
	platform::set_modal_pause(true);
	std::string x;
	std::vector<std::string> new_jukebox;
	runemufn([&x]() {
			for(auto i : get_jukebox_names())
				x = x + i + "\n";
		});

	wxTextEntryDialog* dialog = new wxTextEntryDialog(this, wxT("List jukebox entries"), wxT("Configure jukebox"),
		towxstring(x), wxOK | wxCANCEL | wxCENTRE | wxTE_MULTILINE);
	if(dialog->ShowModal() == wxID_CANCEL) {
		dialog->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	x = tostdstring(dialog->GetValue());
	dialog->Destroy();

	while(x != "") {
		size_t split = x.find_first_of("\n");
		std::string l;
		if(split < x.length()) {
			l = x.substr(0, split);
			x = x.substr(split + 1);
		} else {
			l = x;
			x = "";
		}
		strip_CR(l);
		if(l != "")
			new_jukebox.push_back(l);
	}
	runemufn([&new_jukebox]() { set_jukebox_names(new_jukebox); });
	status_window->notify_update();
	platform::set_modal_pause(false);
}
	
void wxwin_mainwindow::menu_load_memorywatch(wxCommandEvent& e)
{
	platform::set_modal_pause(true);
	std::set<std::string> old_watches;
	runemufn([&old_watches]() { old_watches = get_watches(); });
	std::map<std::string, std::string> new_watches;
	std::string filename;

	wxFileDialog* d = new wxFileDialog(this, towxstring("Choose memory watch file"), wxT("."));
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	filename = tostdstring(d->GetPath());
	d->Destroy();
	//Did we pick a .zip file?
	try {
		zip_reader zr(filename);
		std::vector<wxString> files;
		for(auto i : zr)
			files.push_back(towxstring(i));
		wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(this, wxT("Select file within .zip"),
			wxT("Select member"), files.size(), &files[0]);
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			platform::set_modal_pause(false);
			return;
		}
		filename = filename + "/" + tostdstring(d2->GetStringSelection());
		d2->Destroy();
	} catch(...) {
		//Ignore error.
	}

	try {
		std::istream& in = open_file_relative(filename, "");
		while(in) {
			std::string wname;
			std::string wexpr;
			std::getline(in, wname);
			std::getline(in, wexpr);
			new_watches[wname] = wexpr;
		}
		delete &in;
	} catch(std::exception& e) {
		wxMessageDialog* d3 = new wxMessageDialog(this, towxstring(std::string("Can't load memory "
			"watch: ") + e.what()), wxT("Error"), wxOK | wxICON_EXCLAMATION);
		d3->ShowModal();
		d3->Destroy();
		platform::set_modal_pause(false);
		return;
	}

	runemufn([&new_watches, &old_watches]() {
		for(auto i : new_watches)
			set_watchexpr_for(i.first, i.second);
		for(auto i : old_watches)
			if(!new_watches.count(i))
				set_watchexpr_for(i, "");
		});
	platform::set_modal_pause(false);
}

void wxwin_mainwindow::menu_save_memorywatch(wxCommandEvent& e)
{
	platform::set_modal_pause(true);
	std::set<std::string> old_watches;
	runemufn([&old_watches]() { old_watches = get_watches(); });
	std::string filename;

	wxFileDialog* d = new wxFileDialog(this, towxstring("Save watches to file"), wxT("."));
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	filename = tostdstring(d->GetPath());
	d->Destroy();

	std::ofstream out(filename.c_str());
	for(auto i : old_watches)
		out << i << std::endl << get_watchexpr_for(i) << std::endl;
	out.close();
	platform::set_modal_pause(false);
}


void wxwin_mainwindow::menu_edit_memorywatch(wxCommandEvent& e)
{
	platform::set_modal_pause(true);
	std::set<std::string> bind;
	runemufn([&bind]() { bind = get_watches(); });
	std::vector<wxString> choices;
	choices.push_back(wxT(NEW_WATCH));
	for(auto i : bind)
		choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d = new wxSingleChoiceDialog(this, wxT("Select watch to edit"),
		wxT("Select watch"), choices.size(), &choices[0]);
	if(d->ShowModal() == wxID_CANCEL) {
		d->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	std::string watch = tostdstring(d->GetStringSelection());
	d->Destroy();
	if(watch == NEW_WATCH) {
		wxTextEntryDialog* d2 = new wxTextEntryDialog(this, wxT("Enter name for the new watch:"),
			wxT("Enter watch name"));
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			platform::set_modal_pause(false);
			return;
		}
		watch = tostdstring(d2->GetValue());
		d2->Destroy();
	}
	std::string old_watch_value = get_watchexpr_for(watch);
	wxTextEntryDialog* d4 = new wxTextEntryDialog(this, wxT("Enter new expression for watch:"), wxT("Edit watch"),
		towxstring(old_watch_value), wxOK | wxCANCEL | wxCENTRE);
	if(d4->ShowModal() == wxID_CANCEL) {
		d4->Destroy();
		platform::set_modal_pause(false);
		return;
	}
	std::string newexpr = tostdstring(d4->GetValue());
	runemufn([watch, newexpr]() { set_watchexpr_for(watch, newexpr); });
	platform::set_modal_pause(false);
	d4->Destroy();
}
