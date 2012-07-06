#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/loadlib.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/memorywatch.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <wx/dnd.h>

#include <cmath>
#include <vector>
#include <string>

#include "platform/wxwidgets/menu_dump.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"
#include "platform/wxwidgets/window_status.hpp"

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
	wxID_RESET_LUA,
	wxID_EVAL_LUA,
	wxID_SAVE_SCREENSHOT,
	wxID_READONLY_MODE,
	wxID_EDIT_AUTHORS,
	wxID_AUTOHOLD_FIRST,
	wxID_AUTOHOLD_LAST = wxID_AUTOHOLD_FIRST + 1023,
	wxID_EDIT_MEMORYWATCH,
	wxID_SAVE_MEMORYWATCH,
	wxID_LOAD_MEMORYWATCH,
	wxID_DUMP_FIRST,
	wxID_DUMP_LAST = wxID_DUMP_FIRST + 1023,
	wxID_REWIND_MOVIE,
	wxID_MEMORY_SEARCH,
	wxID_CANCEL_SAVES,
	wxID_SHOW_STATUS,
	wxID_SET_SPEED,
	wxID_SET_VOLUME,
	wxID_SPEED_5,
	wxID_SPEED_10,
	wxID_SPEED_17,
	wxID_SPEED_20,
	wxID_SPEED_25,
	wxID_SPEED_33,
	wxID_SPEED_50,
	wxID_SPEED_100,
	wxID_SPEED_150,
	wxID_SPEED_200,
	wxID_SPEED_300,
	wxID_SPEED_TURBO,
	wxID_LOAD_LIBRARY,
	wxID_SETTINGS,
};


double horizontal_scale_factor = 1.0;
double vertical_scale_factor = 1.0;
int scaling_flags = SWS_POINT;

namespace
{
	std::string last_volume = "0dB";
	unsigned char* screen_buffer;
	uint32_t old_width;
	uint32_t old_height;
	int old_flags = SWS_POINT;
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

	keygroup mouse_x("mouse_x", "mouse", keygroup::KT_MOUSE);
	keygroup mouse_y("mouse_y", "mouse", keygroup::KT_MOUSE);
	keygroup mouse_l("mouse_left", "mouse", keygroup::KT_KEY);
	keygroup mouse_m("mouse_center", "mouse", keygroup::KT_KEY);
	keygroup mouse_r("mouse_right", "mouse", keygroup::KT_KEY);
	keygroup mouse_i("mouse_inwindow", "mouse", keygroup::KT_KEY);

	void handle_wx_mouse(wxMouseEvent& e)
	{
		platform::queue(keypress(modifier_set(), mouse_x, e.GetX() / horizontal_scale_factor));
		platform::queue(keypress(modifier_set(), mouse_y, e.GetY() / vertical_scale_factor));
		if(e.Entering())
			platform::queue(keypress(modifier_set(), mouse_i, 1));
		if(e.Leaving())
			platform::queue(keypress(modifier_set(), mouse_i, 0));
		if(e.LeftDown())
			platform::queue(keypress(modifier_set(), mouse_l, 1));
		if(e.LeftUp())
			platform::queue(keypress(modifier_set(), mouse_l, 0));
		if(e.MiddleDown())
			platform::queue(keypress(modifier_set(), mouse_m, 1));
		if(e.MiddleUp())
			platform::queue(keypress(modifier_set(), mouse_m, 0));
		if(e.RightDown())
			platform::queue(keypress(modifier_set(), mouse_r, 1));
		if(e.RightUp())
			platform::queue(keypress(modifier_set(), mouse_r, 0));
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

	void set_speed(double target)
	{
		std::string v = (stringfmt() << target).str();
		if(target < 0)
			setting::set("targetfps", "infinite");
		else
			setting::set("targetfps", v);
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
		modal_pause_holder hld;
		our_lid = lid;
		for(unsigned i = 0; i < MAX_LOGICAL_BUTTONS; i++) {
			int id = wxID_AUTOHOLD_FIRST + MAX_LOGICAL_BUTTONS * lid + i;
			entries[i] = AppendCheckItem(id, towxstring(get_logical_button_name(i)));
		}
		change_type();
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
		modal_pause_holder hld;
		int pid = controls.lcid_to_pcid(our_lid);
		if(pid < 0 || !entries[lidx])
			return;
		int pidx = controls.button_id(pid, lidx);
		if(pidx < 0)
			return;
		//Autohold change on pid=pid, ctrlindx=idx, state
		bool newstate = entries[lidx]->IsChecked();
		UI_change_autohold(pid, pidx, newstate);
	}

	void controller_autohold_menu::update(unsigned pid, unsigned ctrlnum, bool newstate)
	{
		modal_pause_holder hld;
		int pid2 = UI_controller_index_by_logical(our_lid);
		if(pid2 < 0 || static_cast<unsigned>(pid) != pid2)
			return;
		for(unsigned i = 0; i < MAX_LOGICAL_BUTTONS; i++) {
			int idx = UI_button_id(pid2, i);
			if(idx < 0 || static_cast<unsigned>(idx) != ctrlnum)
				continue;
			entries[i]->Check(newstate);
		}
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
		modal_pause_holder hld;
		for(unsigned i = 0; i < MAXCONTROLLERS; i++) {
			menus[i]->change_type();
			entries[i]->Enable(!menus[i]->is_dummy());
		}
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

	path_setting moviepath_setting("moviepath");

	std::string movie_path()
	{
		return setting::get("moviepath");
	}

	class loadfile : public wxFileDropTarget
	{
	public:
		bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
		{
			if(filenames.Count() != 1)
				return false;
			platform::queue("load " + tostdstring(filenames[0]));
			return true;
		}
	};
}

void boot_emulator(loaded_rom& rom, moviefile& movie)
{
	try {
		struct emu_args* a = new emu_args;
		a->rom = &rom;
		a->initial = &movie;
		a->load_has_to_succeed = false;
		modal_pause_holder hld;
		emulation_thread = &thread::create(emulator_main, a);
		main_window = new wxwin_mainwindow();
		main_window->Show();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
}

wxwin_mainwindow::panel::panel(wxWindow* win)
	: wxPanel(win, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS)
{
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
	this->Connect(wxEVT_MOTION, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_ENTER_WINDOW, wxMouseEventHandler(panel::on_mouse), NULL, this);
	this->Connect(wxEVT_LEAVE_WINDOW, wxMouseEventHandler(panel::on_mouse), NULL, this);
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

void wxwin_mainwindow::menu_end_sub()
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
	uint32_t tw = main_screen.width * horizontal_scale_factor + 0.5;
	uint32_t th = main_screen.height * vertical_scale_factor + 0.5;
	if(!screen_buffer || tw != old_width || th != old_height || scaling_flags != old_flags) {
		if(screen_buffer)
			delete[] screen_buffer;
		old_height = th;
		old_width = tw;
		old_flags = scaling_flags;
		uint32_t w = main_screen.width;
		uint32_t h = main_screen.height;
		if(w && h)
			ctx = sws_getCachedContext(ctx, w, h, PIX_FMT_RGBA, tw, th, PIX_FMT_BGR24, scaling_flags,
				NULL, NULL, NULL);
		tw = max(tw, static_cast<uint32_t>(128));
		th = max(th, static_cast<uint32_t>(112));
		screen_buffer = new unsigned char[tw * th * 3];
		SetMinSize(wxSize(tw, th));
		signal_resize_needed();
	}
	srcs[0] = 4 * main_screen.width;
	dsts[0] = 3 * tw;
	srcp[0] = reinterpret_cast<unsigned char*>(main_screen.memory);
	dstp[0] = screen_buffer;
	memset(screen_buffer, 0, tw * th * 3);
	if(main_screen.width && main_screen.height)
		sws_scale(ctx, srcp, srcs, 0, main_screen.height, dstp, dsts);
	wxBitmap bmp(wxImage(tw, th, screen_buffer, true));
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
	toplevel = new wxFlexGridSizer(1, 2, 0, 0);
	toplevel->Add(gpanel = new panel(this), 1, wxGROW);
	toplevel->Add(spanel = new wxwin_status::panel(this, gpanel, 20), 1, wxGROW);
	spanel_shown = true;
	toplevel->SetSizeHints(this);
	SetSizer(toplevel);
	Fit();
	gpanel->SetFocus();
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_mainwindow::on_close));
	menubar = new wxMenuBar;
	SetMenuBar(menubar);

	menu_start(wxT("lsnes"));
	menu_entry_check(wxID_SHOW_STATUS, wxT("Show/Hide status panel"));
	menu_check(wxID_SHOW_STATUS, true);
	if(load_library_supported) {
		menu_entry(wxID_LOAD_LIBRARY, towxstring(std::string("Load ") + library_is_called));
	}
	menu_entry(wxID_SETTINGS, wxT("Configure emulator..."));
	if(platform::sound_initialized()) {
		menu_separator();
		menu_entry_check(wxID_AUDIO_ENABLED, wxT("Sounds enabled"));
		menu_check(wxID_AUDIO_ENABLED, platform::is_sound_enabled());
		menu_entry(wxID_SET_VOLUME, wxT("Set Sound volume"));
		menu_entry(wxID_SHOW_AUDIO_STATUS, wxT("Show audio status"));
		menu_special_sub(wxT("Select sound device"), reinterpret_cast<sound_select_menu*>(sounddev =
			new sound_select_menu(this)));
		blistener->set_sound_select(reinterpret_cast<sound_select_menu*>(sounddev));
	}
	menu_separator();
	menu_entry(wxID_EXIT, wxT("Quit"));
	menu_separator();
	menu_entry(wxID_ABOUT, wxT("About..."));
	menu_start(wxT("System"));
	menu_start_sub(wxT("Load"));
	menu_entry(wxID_LOAD_STATE, wxT("State..."));
	menu_entry(wxID_LOAD_STATE_RO, wxT("State (readonly)..."));
	menu_entry(wxID_LOAD_STATE_RW, wxT("State (read-write)..."));
	menu_entry(wxID_LOAD_STATE_P, wxT("State (preserve input)..."));
	menu_entry(wxID_LOAD_MOVIE, wxT("Movie..."));
	menu_end_sub();
	menu_start_sub(wxT("Save"));
	menu_entry(wxID_SAVE_STATE, wxT("State..."));
	menu_entry(wxID_SAVE_MOVIE, wxT("Movie..."));
	menu_entry(wxID_SAVE_SCREENSHOT, wxT("Screenshot..."));
	menu_entry(wxID_CANCEL_SAVES, wxT("Cancel pending saves"));
	menu_end_sub();
	menu_entry(wxID_REWIND_MOVIE, wxT("Rewind to start"));
	menu_separator();
	menu_entry(wxID_PAUSE, wxT("Pause/Unpause"));
	menu_entry(wxID_FRAMEADVANCE, wxT("Step frame"));
	menu_entry(wxID_SUBFRAMEADVANCE, wxT("Step subframe"));
	menu_entry(wxID_NEXTPOLL, wxT("Step poll"));
	menu_separator();
	menu_entry(wxID_ERESET, wxT("Reset"));
	//Autohold menu: (ACOS)
	menu_special(wxT("Autohold"), reinterpret_cast<autohold_menu*>(ahmenu = new autohold_menu(this)));
	blistener->set_autohold_menu(reinterpret_cast<autohold_menu*>(ahmenu));

	menu_start(wxT("Speed"));
	menu_entry(wxID_SPEED_5, wxT("1/20x"));
	menu_entry(wxID_SPEED_10, wxT("1/10x"));
	menu_entry(wxID_SPEED_17, wxT("1/6x"));
	menu_entry(wxID_SPEED_20, wxT("1/5x"));
	menu_entry(wxID_SPEED_25, wxT("1/4x"));
	menu_entry(wxID_SPEED_33, wxT("1/3x"));
	menu_entry(wxID_SPEED_50, wxT("1/2x"));
	menu_entry(wxID_SPEED_100, wxT("1x"));
	menu_entry(wxID_SPEED_150, wxT("1.5x"));
	menu_entry(wxID_SPEED_200, wxT("2x"));
	menu_entry(wxID_SPEED_300, wxT("3x"));
	menu_entry(wxID_SPEED_TURBO, wxT("Turbo"));
	menu_entry(wxID_SET_SPEED, wxT("Set..."));

	menu_start(wxT("Scripting"));
	menu_entry(wxID_RUN_SCRIPT, wxT("Run batch file..."));
	if(lua_supported) {
		menu_separator();
		menu_entry(wxID_EVAL_LUA, wxT("Evaluate Lua statement..."));
		menu_entry(wxID_RUN_LUA, wxT("Run Lua script..."));
		menu_separator();
		menu_entry(wxID_RESET_LUA, wxT("Reset Lua VM"));
	}
	menu_separator();
	menu_entry(wxID_EDIT_MEMORYWATCH, wxT("Edit memory watch..."));
	menu_separator();
	menu_entry(wxID_LOAD_MEMORYWATCH, wxT("Load memory watch..."));
	menu_entry(wxID_SAVE_MEMORYWATCH, wxT("Save memory watch..."));
	menu_separator();
	menu_entry(wxID_MEMORY_SEARCH, wxT("Memory Search..."));

	menu_start(wxT("Movie"));
	menu_entry_check(wxID_READONLY_MODE, wxT("Readonly mode"));
	menu_check(wxID_READONLY_MODE, is_readonly_mode());
	menu_entry(wxID_EDIT_AUTHORS, wxT("Edit game name && authors..."));

	menu_special(wxT("Capture"), reinterpret_cast<dumper_menu*>(dmenu = new dumper_menu(this,
		wxID_DUMP_FIRST, wxID_DUMP_LAST)));

	gpanel->SetDropTarget(new loadfile());
	spanel->SetDropTarget(new loadfile());
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

void wxwin_mainwindow::notify_resized() throw()
{
	toplevel->Layout();
	toplevel->SetSizeHints(this);
	Fit();
}

void wxwin_mainwindow::notify_update_status() throw()
{
	if(!spanel->dirty) {
		spanel->dirty = true;
		spanel->Refresh();
	}
}

void wxwin_mainwindow::notify_exit() throw()
{
	join_emulator_thread();
	Destroy();
}

#define NEW_KEYBINDING "A new binding..."
#define NEW_ALIAS "A new alias..."
#define NEW_WATCH "A new watch..."

void wxwin_mainwindow::handle_menu_click(wxCommandEvent& e)
{
	try {
		handle_menu_click_cancelable(e);
	} catch(canceled_exception& e) {
		//Ignore.
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		show_message_ok(this, "Error in menu handler", e.what(), wxICON_EXCLAMATION);
	}
}

void wxwin_mainwindow::handle_menu_click_cancelable(wxCommandEvent& e)
{
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
	case wxID_ERESET:
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
	case wxID_CANCEL_SAVES:
		platform::queue("cancel-saves");
		return;
	case wxID_LOAD_MOVIE:
		platform::queue("load-movie " + pick_file(this, "Load Movie", movie_path()));
		return;
	case wxID_LOAD_STATE:
		platform::queue("load " + pick_file(this, "Load State", movie_path()));
		return;
	case wxID_LOAD_STATE_RO:
		platform::queue("load-readonly " + pick_file(this, "Load State (Read-Only)", movie_path()));
		return;
	case wxID_LOAD_STATE_RW:
		platform::queue("load-state " + pick_file(this, "Load State (Read-Write)", movie_path()));
		return;
	case wxID_LOAD_STATE_P:
		platform::queue("load-preserve " + pick_file(this, "Load State (Preserve)", movie_path()));
		return;
	case wxID_REWIND_MOVIE:
		platform::queue("rewind-movie");
		return;
	case wxID_SAVE_MOVIE:
		platform::queue("save-movie " + pick_file(this, "Save Movie", movie_path()));
		return;
	case wxID_SAVE_STATE:
		platform::queue("save-state " + pick_file(this, "Save State", movie_path()));
		return;
	case wxID_SAVE_SCREENSHOT:
		platform::queue("take-screenshot " + pick_file(this, "Save Screenshot", movie_path()));
		return;
	case wxID_RUN_SCRIPT:
		platform::queue("run-script " + pick_file_member(this, "Select Script", "."));
		return;
	case wxID_RUN_LUA:
		platform::queue("run-lua " + pick_file(this, "Select Lua Script", "."));
		return;
	case wxID_RESET_LUA:
		platform::queue("reset-lua");
		return;
	case wxID_EVAL_LUA:
		platform::queue("evaluate-lua " + pick_text(this, "Evaluate Lua", "Enter Lua Statement:"));
		return;
	case wxID_READONLY_MODE:
		s = menu_ischecked(wxID_READONLY_MODE);
		runemufn([s]() {
			movb.get_movie().readonly_mode(s);
			if(!s)
				lua_callback_do_readwrite();
			update_movie_state();
		});
		return;
	case wxID_EDIT_AUTHORS:
		wxeditor_authors_display(this);
		return;
	case wxID_EDIT_MEMORYWATCH: {
		modal_pause_holder hld;
		std::set<std::string> bind;
		runemufn([&bind]() { bind = get_watches(); });
		std::vector<std::string> choices;
		choices.push_back(NEW_WATCH);
		for(auto i : bind)
			choices.push_back(i);
		std::string watch = pick_among(this, "Select watch", "Select watch to edit", choices);
		if(watch == NEW_WATCH)
			watch =  pick_text(this, "Enter watch name", "Enter name for the new watch:");
		std::string newexpr = pick_text(this, "Edit watch", "Enter new expression for watch:",
			get_watchexpr_for(watch));
		runemufn([watch, newexpr]() { set_watchexpr_for(watch, newexpr); });
		return;
	}
	case wxID_SAVE_MEMORYWATCH: {
		modal_pause_holder hld;
		std::set<std::string> old_watches;
		runemufn([&old_watches]() { old_watches = get_watches(); });
		std::string filename = pick_file(this, "Save watches to file", ".");
		std::ofstream out(filename.c_str());
		for(auto i : old_watches) {
			std::string val;
			runemufn([i, &val]() { val = get_watchexpr_for(i); });
			out << i << std::endl << val << std::endl;
		}
		out.close();
		return;
	}
	case wxID_LOAD_MEMORYWATCH: {
		modal_pause_holder hld;
		std::set<std::string> old_watches;
		runemufn([&old_watches]() { old_watches = get_watches(); });
		std::map<std::string, std::string> new_watches;
		std::string filename = pick_file_member(this, "Choose memory watch file", ".");

		try {
			std::istream& in = open_file_relative(filename, "");
			while(in) {
				std::string wname;
				std::string wexpr;
				std::getline(in, wname);
				std::getline(in, wexpr);
				new_watches[strip_CR(wname)] = strip_CR(wexpr);
			}
			delete &in;
		} catch(std::exception& e) {
			show_message_ok(this, "Error", std::string("Can't load memory watch: ") + e.what(),
				wxICON_EXCLAMATION);
			return;
		}

		runemufn([&new_watches, &old_watches]() {
			for(auto i : new_watches)
				set_watchexpr_for(i.first, i.second);
			for(auto i : old_watches)
				if(!new_watches.count(i))
					set_watchexpr_for(i, "");
			});
		return;
	}
	case wxID_MEMORY_SEARCH:
		wxwindow_memorysearch_display();
		return;
	case wxID_ABOUT: {
		std::ostringstream str;
		str << "Version: lsnes rr" << lsnes_version << std::endl;
		str << "Revision: " << lsnes_git_revision << std::endl;
		str << "Core: " << bsnes_core_version << std::endl;
		wxMessageBox(towxstring(str.str()), _T("About"), wxICON_INFORMATION | wxOK, this);
		return;
	}
	case wxID_SHOW_STATUS: {
		bool newstate = menu_ischecked(wxID_SHOW_STATUS);
		if(newstate)
			spanel->Show();
		if(newstate && !spanel_shown)
			toplevel->Add(spanel, 1, wxGROW);
		else if(!newstate && spanel_shown)
			toplevel->Detach(spanel);
		if(!newstate)
			spanel->Hide();
		spanel_shown = newstate;
		toplevel->Layout();
		toplevel->SetSizeHints(this);
		Fit();
		return;
	}
	case wxID_SET_SPEED: {
		bool bad = false;
		std::string value = setting::is_set("targetfps") ? setting::get("targetfps") : "";
		value = pick_text(this, "Set speed", "Enter percentage speed (or \"infinite\"):", value);
		try {
			setting::set("targetfps", value);
		} catch(...) {
			wxMessageBox(wxT("Invalid speed"), _T("Error"), wxICON_EXCLAMATION | wxOK, this);
		}
		return;
	}
	case wxID_SET_VOLUME: {
		std::string value;
		regex_results r;
		double parsed = 1;
		value = pick_text(this, "Set volume", "Enter volume in absolute units, percentage (%) or dB:",
			last_volume);
		if(r = regex("([0-9]*\\.[0-9]+|[0-9]+)", value))
			parsed = strtod(r[1].c_str(), NULL);
		else if(r = regex("([0-9]*\\.[0-9]+|[0-9]+)%", value))
			parsed = strtod(r[1].c_str(), NULL) / 100;
		else if(r = regex("([+-]?([0-9]*.[0-9]+|[0-9]+))dB", value))
			parsed = pow(10, strtod(r[1].c_str(), NULL) / 20);
		else {
			wxMessageBox(wxT("Invalid volume"), _T("Error"), wxICON_EXCLAMATION | wxOK, this);
			return;
		}
		last_volume = value;
		runemufn([parsed]() { platform::global_volume = parsed; });
		return;
	}
	case wxID_SPEED_5:
		set_speed(5);
		break;
	case wxID_SPEED_10:
		set_speed(10);
		break;
	case wxID_SPEED_17:
		set_speed(16.66666666666);
		break;
	case wxID_SPEED_20:
		set_speed(20);
		break;
	case wxID_SPEED_25:
		set_speed(25);
		break;
	case wxID_SPEED_33:
		set_speed(33.3333333333333);
		break;
	case wxID_SPEED_50:
		set_speed(50);
		break;
	case wxID_SPEED_100:
		set_speed(100);
		break;
	case wxID_SPEED_150:
		set_speed(150);
		break;
	case wxID_SPEED_200:
		set_speed(200);
		break;
	case wxID_SPEED_300:
		set_speed(300);
		break;
	case wxID_SPEED_TURBO:
		set_speed(-1);
		break;
	case wxID_LOAD_LIBRARY: {
		std::string name = std::string("load ") + library_is_called;
		load_library(pick_file(this, name, "."));
		break;
	}
	case wxID_SETTINGS:
		wxsetingsdialog_display(this);
		break;
	};
}
