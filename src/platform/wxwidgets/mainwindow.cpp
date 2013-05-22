#include "lsnes.hpp"

#include <wx/dnd.h>
#include "platform/wxwidgets/menu_dump.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"
#include "platform/wxwidgets/window_messages.hpp"
#include "platform/wxwidgets/window_status.hpp"

#include "core/audioapi.hpp"
#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "interface/romtype.hpp"
#include "core/loadlib.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/memorywatch.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/directory.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
#define FUCKED_SYSTEM
#endif

#include <cmath>
#include <vector>
#include <string>


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
	wxID_EHRESET,
	wxID_AUDIO_ENABLED,
	wxID_AUDIO_DEVICE,
	wxID_SAVE_STATE,
	wxID_SAVE_MOVIE,
	wxID_SAVE_SUBTITLES,
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
	wxID_AUTOHOLD,
	wxID_EDIT_MEMORYWATCH,
	wxID_SAVE_MEMORYWATCH,
	wxID_LOAD_MEMORYWATCH,
	wxID_EDIT_SUBTITLES,
	wxID_EDIT_VSUBTITLES,
	wxID_DUMP_FIRST,
	wxID_DUMP_LAST = wxID_DUMP_FIRST + 1023,
	wxID_REWIND_MOVIE,
	wxID_MEMORY_SEARCH,
	wxID_CANCEL_SAVES,
	wxID_SHOW_STATUS,
	wxID_SET_SPEED,
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
	wxID_SPEED_500,
	wxID_SPEED_1000,
	wxID_SPEED_TURBO,
	wxID_LOAD_LIBRARY,
	wxID_SETTINGS,
	wxID_SETTINGS_HOTKEYS,
	wxID_SETTINGS_CONTROLLERS,
	wxID_RELOAD_ROM_IMAGE,
	wxID_LOAD_ROM_IMAGE,
	wxID_NEW_MOVIE,
	wxID_SHOW_MESSAGES,
	wxID_DEDICATED_MEMORY_WATCH,
	wxID_RMOVIE_FIRST,
	wxID_RMOVIE_LAST = wxID_RMOVIE_FIRST + 16,
	wxID_RROM_FIRST,
	wxID_RROM_LAST = wxID_RROM_FIRST + 16,
	wxID_CONFLICTRESOLUTION,
	wxID_VUDISPLAY,
	wxID_MOVIE_EDIT,
	wxID_TASINPUT,
	wxID_NEW_PROJECT,
	wxID_LOAD_PROJECT,
	wxID_CLOSE_PROJECT,
};


double horizontal_scale_factor = 1.0;
double vertical_scale_factor = 1.0;
int scaling_flags = SWS_POINT;
bool hflip_enabled = false;
bool vflip_enabled = false;
bool rotate_enabled = false;

namespace
{
	std::string last_volume = "0dB";
	std::string last_volume_record = "0dB";
	std::string last_volume_voice = "0dB";
	unsigned char* screen_buffer;
	uint32_t* rotate_buffer;
	uint32_t old_width;
	uint32_t old_height;
	int old_flags = SWS_POINT;
	bool old_hflip = false;
	bool old_vflip = false;
	bool old_rotate = false;
	bool main_window_dirty;
	thread_class* emulation_thread;

	std::string munge_name(const std::string& orig)
	{
		std::string newname;
		regex_results r;
		if(r = regex("(.*)\\(([0-9]+)\\)", newname)) {
			uint64_t sequence;
			try {
				sequence = parse_value<uint64_t>(r[2]);
				newname = (stringfmt() << r[1] << "(" << sequence + 1 << ")").str();
			} catch(...) {
				newname = newname + "(2)";
			}
		} else {
			newname = newname + "(2)";
		}
		return newname;
	}

	void handle_watch_load(std::map<std::string, std::string>& new_watches, std::set<std::string>& old_watches)
	{
		auto proj = project_get();
		if(proj) {
			for(auto i : new_watches) {
				std::string name = i.first;
				while(true) {
					if(!old_watches.count(name)) {
						set_watchexpr_for(name, i.second);
						break;
					} else if(get_watchexpr_for(name) == i.second)
						break;
					else
						name = munge_name(name);
				}
			}
		} else {
			for(auto i : new_watches)
				set_watchexpr_for(i.first, i.second);
			for(auto i : old_watches)
				if(!new_watches.count(i))
					set_watchexpr_for(i, "");
		}
	}

	std::string get_default_screenshot_name()
	{
		auto p = project_get();
		if(!p)
			return "";
		else {
			auto files = enumerate_directory(p->directory, ".*-[0-9]+\\.png");
			std::set<std::string> numbers;
			for(auto i : files) {
				size_t split;
#ifdef FUCKED_SYSTEM
				split = i.find_last_of("\\/");
#else
				split = i.find_last_of("/");
#endif
				std::string name = i;
				if(split < name.length())
					name = name.substr(split + 1);
				regex_results r = regex("(.*)-([0-9]+)\\.png", name);
				if(r[1] != p->prefix)
					continue;
				numbers.insert(r[2]);
			}
			for(uint64_t i = 1;; i++) {
				std::string candidate = (stringfmt() << i).str();
				if(!numbers.count(candidate))
					return p->prefix + "-" + candidate + ".png";
			}
		}
	}

	std::string project_prefixname(const std::string ext)
	{
		auto p = project_get();
		if(!p)
			return "";
		else
			return p->prefix + "." + ext;
		
	}

	double pick_volume(wxWindow* win, const std::string& title, std::string& last)
	{
		std::string value;
		regex_results r;
		double parsed = 1;
		value = pick_text(win, title, "Enter volume in absolute units, percentage (%) or dB:",
			last);
		if(r = regex("([0-9]*\\.[0-9]+|[0-9]+)", value))
			parsed = strtod(r[1].c_str(), NULL);
		else if(r = regex("([0-9]*\\.[0-9]+|[0-9]+)%", value))
			parsed = strtod(r[1].c_str(), NULL) / 100;
		else if(r = regex("([+-]?([0-9]*.[0-9]+|[0-9]+))dB", value))
			parsed = pow(10, strtod(r[1].c_str(), NULL) / 20);
		else {
			wxMessageBox(wxT("Invalid volume"), _T("Error"), wxICON_EXCLAMATION | wxOK, win);
			return -1;
		}
		last = value;
		return parsed;
	}

	void recent_rom_selected(const std::string& file)
	{
		platform::queue("unpause-emulator");
		platform::queue("reload-rom " + file);
	}

	void recent_movie_selected(const std::string& file)
	{
		platform::queue("load-smart " + file);
	}

	wxString getname()
	{
		std::string windowname = "lsnes rr" + lsnes_version + " [";
		auto p = project_get();
		if(p)
			windowname = windowname + p->name;
		else
			windowname = windowname + our_rom->rtype->get_core_identifier();
		windowname = windowname + "]";
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
			messages << "Using core: " << our_rom->rtype->get_core_identifier() << std::endl;
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

	keyboard_mouse_calibration mouse_cal = {0};
	keyboard_key_mouse mouse_x(lsnes_kbd, "mouse_x", "mouse", mouse_cal);
	keyboard_key_mouse mouse_y(lsnes_kbd, "mouse_y", "mouse", mouse_cal);
	keyboard_key_key mouse_l(lsnes_kbd, "mouse_left", "mouse");
	keyboard_key_key mouse_m(lsnes_kbd, "mouse_center", "mouse");
	keyboard_key_key mouse_r(lsnes_kbd, "mouse_right", "mouse");
	keyboard_key_key mouse_i(lsnes_kbd, "mouse_inwindow", "mouse");

	void handle_wx_mouse(wxMouseEvent& e)
	{
		platform::queue(keypress(keyboard_modifier_set(), mouse_x, e.GetX() / horizontal_scale_factor));
		platform::queue(keypress(keyboard_modifier_set(), mouse_y, e.GetY() / vertical_scale_factor));
		if(e.Entering())
			platform::queue(keypress(keyboard_modifier_set(), mouse_i, 1));
		if(e.Leaving())
			platform::queue(keypress(keyboard_modifier_set(), mouse_i, 0));
		if(e.LeftDown())
			platform::queue(keypress(keyboard_modifier_set(), mouse_l, 1));
		if(e.LeftUp())
			platform::queue(keypress(keyboard_modifier_set(), mouse_l, 0));
		if(e.MiddleDown())
			platform::queue(keypress(keyboard_modifier_set(), mouse_m, 1));
		if(e.MiddleUp())
			platform::queue(keypress(keyboard_modifier_set(), mouse_m, 0));
		if(e.RightDown())
			platform::queue(keypress(keyboard_modifier_set(), mouse_r, 1));
		if(e.RightUp())
			platform::queue(keypress(keyboard_modifier_set(), mouse_r, 0));
	}

	bool is_readonly_mode()
	{
		bool ret;
		runemufn([&ret]() { ret = movb.get_movie().readonly_mode(); });
		return ret;
	}

	std::pair<int, int> UI_controller_index_by_logical(unsigned lid)
	{
		std::pair<int, int> ret;
		runemufn([&ret, lid]() { ret = controls.lcid_to_pcid(lid); });
		return ret;
	}

	void set_speed(double target)
	{
		if(target < 0)
			set_speed_multiplier(std::numeric_limits<double>::infinity());
		else
			set_speed_multiplier(target / 100);
	}

	class broadcast_listener : public information_dispatch
	{
	public:
		broadcast_listener(wxwin_mainwindow* win);
		void on_sound_unmute(bool unmute) throw();
		void on_mode_change(bool readonly) throw();
		void on_core_change();
		void on_new_core();
	private:
		wxwin_mainwindow* mainw;
	};

	broadcast_listener::broadcast_listener(wxwin_mainwindow* win)
		: information_dispatch("wxwidgets-broadcast-listener")
	{
		mainw = win;
	}

	void broadcast_listener::on_core_change()
	{
		signal_core_change();
	}

	void broadcast_listener::on_sound_unmute(bool unmute) throw()
	{
		runuifun([this, unmute]() { this->mainw->menu_check(wxID_AUDIO_ENABLED, unmute); });
	}

	void broadcast_listener::on_mode_change(bool readonly) throw()
	{
		runuifun([this, readonly]() { this->mainw->menu_check(wxID_READONLY_MODE, readonly); });
	}

	void update_preferences()
	{
		preferred_core.clear();
		for(auto i : core_type::get_core_types()) {
			std::string val = i->get_hname() + " / " + i->get_core_identifier();
			for(auto j : i->get_extensions()) {
				std::string key = "ext:" + j;
				if(core_selections.count(key) && core_selections[key] == val)
					preferred_core[key] = i;
			}
			std::string key2 = "type:" + i->get_iname();
			if(core_selections.count(key2) && core_selections[key2] == val)
				preferred_core[key2] = i;
			
		}
	}

	void broadcast_listener::on_new_core()
	{
		update_preferences();
	}

	std::string movie_path()
	{
		return lsnes_vset["moviepath"].str();
	}

	std::string rom_path()
	{
		return lsnes_vset["rompath"].str();
	}

	bool is_lsnes_movie(const std::string& filename)
	{
		try {
			zip_reader r(filename);
			std::istream& s = r["systemid"];
			std::string s2;
			std::getline(s, s2);
			delete &s;
			istrip_CR(s2);
			return (s2 == "lsnes-rr1");
		} catch(...) {
			return false;
		}
	}

	class loadfile : public wxFileDropTarget
	{
	public:
		loadfile(wxwin_mainwindow* win) : pwin(win) {};
		bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
		{
			bool ret = false;
			if(filenames.Count() == 2) {
				if(is_lsnes_movie(tostdstring(filenames[0])) &&
					!is_lsnes_movie(tostdstring(filenames[1]))) {
					platform::queue("unpause-emulator");
					platform::queue("reload-rom " + tostdstring(filenames[1]));
					platform::queue("load-smart " + tostdstring(filenames[0]));
					ret = true;
				}
				if(!is_lsnes_movie(tostdstring(filenames[0])) &&
					is_lsnes_movie(tostdstring(filenames[1]))) {
					platform::queue("unpause-emulator");
					platform::queue("reload-rom " + tostdstring(filenames[0]));
					platform::queue("load-smart " + tostdstring(filenames[1]));
					ret = true;
				}
			}
			if(filenames.Count() == 1) {
				if(is_lsnes_movie(tostdstring(filenames[0]))) {
					platform::queue("load-smart " + tostdstring(filenames[0]));
					pwin->recent_movies->add(tostdstring(filenames[0]));
					ret = true;
				} else {
					platform::queue("unpause-emulator");
					platform::queue("reload-rom " + tostdstring(filenames[0]));
					pwin->recent_roms->add(tostdstring(filenames[0]));
					ret = true;
				}
			}
			return ret;
		}
		wxwin_mainwindow* pwin;
	};
}

void boot_emulator(loaded_rom& rom, moviefile& movie)
{
	update_preferences();
	try {
		struct emu_args* a = new emu_args;
		a->rom = &rom;
		a->initial = &movie;
		a->load_has_to_succeed = false;
		modal_pause_holder hld;
		emulation_thread = new thread_class(emulator_main, a);
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

void wxwin_mainwindow::menu_enable(int id, bool newstate)
{
	auto item = menubar->FindItem(id);
	if(!item)
		return;
	item->Enable(newstate);
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
	uint32_t tw, th;
	bool aux = hflip_enabled || vflip_enabled || rotate_enabled;
	if(rotate_enabled) {
		tw = main_screen.get_height() * horizontal_scale_factor + 0.5;
		th = main_screen.get_width() * vertical_scale_factor + 0.5;
	} else {
		tw = main_screen.get_width() * horizontal_scale_factor + 0.5;
		th = main_screen.get_height() * vertical_scale_factor + 0.5;
	}
	if(!tw || !th) {
		main_window_dirty = false;
		return;
	}
	if(!screen_buffer || tw != old_width || th != old_height || scaling_flags != old_flags ||
		hflip_enabled != old_hflip || vflip_enabled != old_vflip || rotate_enabled != old_rotate) {
		if(screen_buffer)
			delete[] screen_buffer;
		if(rotate_buffer)
			delete[] rotate_buffer;
		old_height = th;
		old_width = tw;
		old_flags = scaling_flags;
		old_hflip = hflip_enabled;
		old_vflip = vflip_enabled;
		old_rotate = rotate_enabled;
		uint32_t w = main_screen.get_width();
		uint32_t h = main_screen.get_height();
		if(w && h)
			ctx = sws_getCachedContext(ctx, rotate_enabled ? h : w, rotate_enabled ? w : h, PIX_FMT_RGBA,
				tw, th, PIX_FMT_BGR24, scaling_flags, NULL, NULL, NULL);
		tw = max(tw, static_cast<uint32_t>(128));
		th = max(th, static_cast<uint32_t>(112));
		screen_buffer = new unsigned char[tw * th * 3];
		if(aux)
			rotate_buffer = new uint32_t[main_screen.get_width() * main_screen.get_height()];
		SetMinSize(wxSize(tw, th));
		signal_resize_needed();
	}
	if(aux) {
		//Hflip, Vflip or rotate active.
		size_t width = main_screen.get_width();
		size_t height = main_screen.get_height();
		size_t width1 = width - 1;
		size_t height1 = height - 1;
		size_t stride = main_screen.rowptr(1) - main_screen.rowptr(0);
		uint32_t* pixels = main_screen.rowptr(0);
		if(rotate_enabled) {
			for(unsigned y = 0; y < height; y++) {
				uint32_t* pixels2 = pixels + (vflip_enabled ? (height1 - y) : y) * stride;
				uint32_t* dpixels = rotate_buffer + (height1 - y);
				if(hflip_enabled)
					for(unsigned x = 0; x < width; x++)
						dpixels[x * height] = pixels2[width1 - x];
				else
					for(unsigned x = 0; x < width; x++)
						dpixels[x * height] = pixels2[x];
			}
		} else {
			for(unsigned y = 0; y < height; y++) {
				uint32_t* pixels2 = pixels + (vflip_enabled ? (height1 - y) : y) * stride;
				uint32_t* dpixels = rotate_buffer + y * width;
				if(hflip_enabled)
					for(unsigned x = 0; x < width; x++)
						dpixels[x] = pixels2[width1 - x];
				else
					for(unsigned x = 0; x < width; x++)
						dpixels[x] = pixels2[x];
			}
		}
	}
	srcs[0] = 4 * (rotate_enabled ? main_screen.get_height() : main_screen.get_width());
	dsts[0] = 3 * tw;
	srcp[0] = reinterpret_cast<unsigned char*>(aux ? rotate_buffer : main_screen.rowptr(0));
	dstp[0] = screen_buffer;
	memset(screen_buffer, 0, tw * th * 3);
	if(main_screen.get_width() && main_screen.get_height())
		sws_scale(ctx, srcp, srcs, 0, rotate_enabled ? main_screen.get_width() : main_screen.get_height(),
		dstp, dsts);
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
	mwindow = NULL;
	toplevel = new wxFlexGridSizer(1, 2, 0, 0);
	toplevel->Add(gpanel = new panel(this), 1, wxGROW);
	toplevel->Add(spanel = new wxwin_status::panel(this, gpanel, 20), 1, wxGROW);
	spanel_shown = true;
	toplevel->SetSizeHints(this);
	SetSizer(toplevel);
	Fit();
	gpanel->SetFocus();
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_mainwindow::on_close));
	SetMenuBar(menubar = new wxMenuBar);
	SetStatusBar(statusbar = new wxStatusBar(this));

	menu_start(wxT("File"));
	menu_start_sub(wxT("New"));
	menu_entry(wxID_NEW_MOVIE, wxT("Movie..."));
	menu_entry(wxID_NEW_PROJECT, wxT("Project..."));
	menu_end_sub();
	menu_start_sub(wxT("Load"));
	menu_entry(wxID_LOAD_STATE, wxT("State..."));
	menu_entry(wxID_LOAD_STATE_RO, wxT("State (readonly)..."));
	menu_entry(wxID_LOAD_STATE_RW, wxT("State (read-write)..."));
	menu_entry(wxID_LOAD_STATE_P, wxT("State (preserve input)..."));
	menu_entry(wxID_LOAD_MOVIE, wxT("Movie..."));
	if(loaded_library::call_library() != "") {
		menu_separator();
		menu_entry(wxID_LOAD_LIBRARY, towxstring(std::string("Load ") + loaded_library::call_library()));
	}
	menu_separator();
	menu_entry(wxID_RELOAD_ROM_IMAGE, wxT("Reload ROM"));
	menu_entry(wxID_LOAD_ROM_IMAGE, wxT("ROM..."));
	menu_entry(wxID_LOAD_PROJECT, wxT("Project..."));
	menu_separator();
	menu_special_sub(wxT("Recent ROMs"), recent_roms = new recent_menu(this, wxID_RROM_FIRST, wxID_RROM_LAST,
		get_config_path() + "/recent-roms.txt", recent_rom_selected));
	menu_special_sub(wxT("Recent Movies"), recent_movies = new recent_menu(this, wxID_RMOVIE_FIRST,
		wxID_RMOVIE_LAST, get_config_path() + "/recent-movies.txt", recent_movie_selected));
	menu_separator();
	menu_entry(wxID_CONFLICTRESOLUTION, wxT("Conflict resolution"));
	menu_end_sub();
	menu_start_sub(wxT("Save"));
	menu_entry(wxID_SAVE_STATE, wxT("State..."));
	menu_entry(wxID_SAVE_MOVIE, wxT("Movie..."));
	menu_entry(wxID_SAVE_SCREENSHOT, wxT("Screenshot..."));
	menu_entry(wxID_SAVE_SUBTITLES, wxT("Subtitles..."));
	menu_entry(wxID_CANCEL_SAVES, wxT("Cancel pending saves"));
	menu_end_sub();
	menu_start_sub(wxT("Close"));
	menu_entry(wxID_CLOSE_PROJECT, wxT("Project"));
	menu_enable(wxID_CLOSE_PROJECT, project_get() != NULL);
	menu_end_sub();
	menu_separator();
	menu_entry(wxID_EXIT, wxT("Quit"));

	menu_start(wxT("System"));
	menu_entry(wxID_PAUSE, wxT("Pause/Unpause"));
	menu_entry(wxID_FRAMEADVANCE, wxT("Step frame"));
	menu_entry(wxID_SUBFRAMEADVANCE, wxT("Step subframe"));
	menu_entry(wxID_NEXTPOLL, wxT("Step poll"));
	menu_entry(wxID_ERESET, wxT("Reset"));
	menu_entry(wxID_EHRESET, wxT("Power cycle"));

	menu_start(wxT("Movie"));
	menu_entry_check(wxID_READONLY_MODE, wxT("Readonly mode"));
	menu_check(wxID_READONLY_MODE, is_readonly_mode());
	menu_entry(wxID_EDIT_AUTHORS, wxT("Edit game name && authors..."));
	menu_entry(wxID_EDIT_SUBTITLES, wxT("Edit subtitles..."));
	menu_entry(wxID_EDIT_VSUBTITLES, wxT("Edit commantary track..."));
	menu_separator();
	menu_entry(wxID_REWIND_MOVIE, wxT("Rewind to start"));

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
	menu_entry(wxID_SPEED_500, wxT("5x"));
	menu_entry(wxID_SPEED_1000, wxT("10x"));
	menu_entry(wxID_SPEED_TURBO, wxT("Turbo"));
	menu_entry(wxID_SET_SPEED, wxT("Set..."));

	menu_start(wxT("Tools"));
	menu_entry(wxID_RUN_SCRIPT, wxT("Run batch file..."));
	menu_separator();
	menu_entry(wxID_EVAL_LUA, wxT("Evaluate Lua statement..."));
	menu_entry(wxID_RUN_LUA, wxT("Run Lua script..."));
	menu_separator();
	menu_entry(wxID_RESET_LUA, wxT("Reset Lua VM"));
	menu_separator();
	menu_entry(wxID_AUTOHOLD, wxT("Autohold/Autofire..."));
	menu_entry(wxID_TASINPUT, wxT("TAS input plugin..."));
	menu_separator();
	menu_entry(wxID_EDIT_MEMORYWATCH, wxT("Edit memory watch..."));
	menu_separator();
	menu_entry(wxID_LOAD_MEMORYWATCH, wxT("Load memory watch..."));
	menu_entry(wxID_SAVE_MEMORYWATCH, wxT("Save memory watch..."));
	menu_separator();
	menu_entry(wxID_MEMORY_SEARCH, wxT("Memory Search..."));
	menu_separator();
	menu_entry(wxID_MOVIE_EDIT, wxT("Edit movie..."));
	menu_separator();
	menu_special_sub(wxT("Video Capture"), reinterpret_cast<dumper_menu*>(dmenu = new dumper_menu(this,
		wxID_DUMP_FIRST, wxID_DUMP_LAST)));

	menu_start(wxT("Configure"));
	menu_entry_check(wxID_SHOW_STATUS, wxT("Show status panel"));
	menu_check(wxID_SHOW_STATUS, true);
	menu_entry_check(wxID_DEDICATED_MEMORY_WATCH, wxT("Dedicated memory watch"));
	menu_entry(wxID_SHOW_MESSAGES, wxT("Show messages"));
	menu_entry(wxID_SETTINGS, wxT("Configure emulator..."));
	menu_entry(wxID_SETTINGS_HOTKEYS, wxT("Configure hotkeys..."));
	menu_entry(wxID_SETTINGS_CONTROLLERS, wxT("Configure controllers..."));
	if(audioapi_driver_initialized()) {
		menu_separator();
		menu_entry_check(wxID_AUDIO_ENABLED, wxT("Sounds enabled"));
		menu_entry(wxID_VUDISPLAY, wxT("VU display / volume controls"));
		menu_check(wxID_AUDIO_ENABLED, platform::is_sound_enabled());
		menu_entry(wxID_AUDIO_DEVICE, wxT("Set audio device"));
	}

	menu_start(wxT("Help"));
	menu_entry(wxID_ABOUT, wxT("About..."));

	gpanel->SetDropTarget(new loadfile(this));
	spanel->SetDropTarget(new loadfile(this));
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
	spanel->request_paint();
	if(mwindow)
		mwindow->notify_update();
}

void wxwin_mainwindow::notify_exit() throw()
{
	wxwidgets_exiting = true;
	join_emulator_thread();
	Destroy();
}

std::u32string read_variable_map(const std::map<std::string, std::u32string>& vars, const std::string& key)
{
	if(!vars.count(key))
		return U"";
	return vars.find(key)->second;
}

void wxwin_mainwindow::update_statusbar(const std::map<std::string, std::u32string>& vars)
{
	if(vars.empty())
		return;
	try {
		std::basic_ostringstream<char32_t> s;
		bool recording = (read_variable_map(vars, "!mode") == U"R");
		if(recording)
			s << U"Frame: " << read_variable_map(vars, "!frame");
		else
			s << U"Frame: " << read_variable_map(vars, "!frame") << U"/" <<
				read_variable_map(vars, "!length");
		s << U"  Lag: " << read_variable_map(vars, "!lag");
		s << U"  Subframe: " << read_variable_map(vars, "!subframe");
		if(vars.count("!saveslot"))
			s << U"  Slot: " << read_variable_map(vars, "!saveslot");
		if(vars.count("!saveslotinfo"))
			s << U" [" << read_variable_map(vars, "!saveslotinfo") << U"]";
		s << U"  Speed: " << read_variable_map(vars, "!speed") << "%";
		s << " ";
		if(read_variable_map(vars, "!dumping") != U"")
			s << U" Dumping";
		if(read_variable_map(vars, "!mode") == U"C")
			s << U" Corrupt";
		else if(read_variable_map(vars, "!mode") == U"R")
			s << U" Recording";
		else if(read_variable_map(vars, "!mode") == U"P")
			s << U" Playback";
		else if(read_variable_map(vars, "!mode") == U"F")
			s << U" Finished";
		else 
			s << U" Unknown";
		statusbar->SetStatusText(towxstring(s.str()));
	} catch(std::exception& e) {
	}
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

void wxwin_mainwindow::refresh_title() throw()
{
	SetTitle(getname());
	auto p = project_get();
	menu_enable(wxID_RELOAD_ROM_IMAGE, !p);
	menu_enable(wxID_LOAD_ROM_IMAGE, !p);
	menu_enable(wxID_CLOSE_PROJECT, p != NULL);
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
	case wxID_EHRESET:
		platform::queue("reset-hard");
		return;
	case wxID_EXIT:
		platform::queue("quit-emulator");
		return;
	case wxID_AUDIO_ENABLED:
		platform::sound_enable(menu_ischecked(wxID_AUDIO_ENABLED));
		return;
	case wxID_AUDIO_DEVICE:
		wxeditor_sounddev_display(this);
		return;
	case wxID_CANCEL_SAVES:
		platform::queue("cancel-saves");
		return;
	case wxID_LOAD_MOVIE:
		filename = pick_file(this, "Load Movie", project_moviepath(), false, "lsmv");
		recent_movies->add(filename);
		platform::queue("load-movie " + filename);
		return;
	case wxID_LOAD_STATE:
		filename = pick_file(this, "Load State", project_moviepath(), false, project_savestate_ext());
		recent_movies->add(filename);
		platform::queue("load " + filename);
		return;
	case wxID_LOAD_STATE_RO:
		filename = pick_file(this, "Load State (Read-Only)", project_moviepath(), false,
			project_savestate_ext());
		recent_movies->add(filename);
		platform::queue("load-readonly " + filename);
		return;
	case wxID_LOAD_STATE_RW:
		filename = pick_file(this, "Load State (Read-Write)", project_moviepath(), false,
			project_savestate_ext());
		recent_movies->add(filename);
		platform::queue("load-state " + filename);
		return;
	case wxID_LOAD_STATE_P:
		filename = pick_file(this, "Load State (Preserve)", project_moviepath(), false,
			project_savestate_ext());
		recent_movies->add(filename);
		platform::queue("load-preserve " + filename);
		return;
	case wxID_REWIND_MOVIE:
		platform::queue("rewind-movie");
		return;
	case wxID_SAVE_MOVIE:
		filename = pick_file(this, "Save Movie", project_moviepath(), true, "lsmv",
			project_prefixname("lsmv"));
		recent_movies->add(filename);
		platform::queue("save-movie " + filename);
		return;
	case wxID_SAVE_SUBTITLES:
		platform::queue("save-subtitle " + pick_file(this, "Save Subtitle (.sub)", project_moviepath(), true,
			"sub", project_prefixname("sub")));
		return;
	case wxID_SAVE_STATE:
		filename = pick_file(this, "Save State", project_moviepath(), true, project_savestate_ext());
		recent_movies->add(filename);
		platform::queue("save-state " + filename);
		return;
	case wxID_SAVE_SCREENSHOT:
		platform::queue("take-screenshot " + pick_file(this, "Save Screenshot", project_moviepath(), true,
			"png", get_default_screenshot_name()));
		return;
	case wxID_RUN_SCRIPT:
		platform::queue("run-script " + pick_file_member(this, "Select Script", project_otherpath()));
		return;
	case wxID_RUN_LUA:
		platform::queue("run-lua " + pick_file(this, "Select Lua Script", project_otherpath(), false, "lua"));
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
			graphics_driver_notify_status();
		});
		return;
	case wxID_AUTOHOLD:
		wxeditor_autohold_display(this);
		return;
	case wxID_EDIT_AUTHORS:
		wxeditor_authors_display(this);
		return;
	case wxID_EDIT_SUBTITLES:
		wxeditor_subtitles_display(this);
		return;
	case wxID_EDIT_VSUBTITLES:
		show_wxeditor_voicesub(this);
		return;
	case wxID_EDIT_MEMORYWATCH:
		wxeditor_memorywatch_display(this);
		return;
	case wxID_SAVE_MEMORYWATCH: {
		modal_pause_holder hld;
		std::set<std::string> old_watches;
		runemufn([&old_watches]() { old_watches = get_watches(); });
		std::string filename = pick_file(this, "Save watches to file", project_otherpath(), true, "lwch");
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
		std::string filename = pick_file(this, "Choose memory watch file", project_otherpath(), "lwch");
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
			handle_watch_load(new_watches, old_watches);
		});
		return;
	}
	case wxID_MEMORY_SEARCH:
		wxwindow_memorysearch_display();
		return;
	case wxID_TASINPUT:
		wxeditor_tasinput_display(this);
		return;
	case wxID_ABOUT: {
		std::ostringstream str;
		str << "Version: lsnes rr" << lsnes_version << std::endl;
		str << "Revision: " << lsnes_git_revision << std::endl;
		for(auto i : core_core::all_cores())
			if(!i->is_hidden())
				str << "Core: " << i->get_core_identifier() << std::endl;
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
	case wxID_DEDICATED_MEMORY_WATCH: {
		bool newstate = menu_ischecked(wxID_DEDICATED_MEMORY_WATCH);
		if(newstate && !mwindow) {
			mwindow = new wxwin_status(-1, "Memory Watch");
			spanel->set_watch_flag(1);
			mwindow->Show();
		} else if(!newstate && mwindow) {
			mwindow->Destroy();
			mwindow = NULL;
			spanel->set_watch_flag(0);
		}
		return;
	}
	case wxID_SET_SPEED: {
		std::string value = "infinite";
		double val = get_speed_multiplier();
		if(!(val == std::numeric_limits<double>::infinity()))
			value = (stringfmt() << (100 * val)).str();
		value = pick_text(this, "Set speed", "Enter percentage speed (or \"infinite\"):", value);
		try {
			if(value == "infinite")
				set_speed_multiplier(std::numeric_limits<double>::infinity());
			else {
				double v = parse_value<double>(value) / 100;
				if(v <= 0.0001)
					throw 42;
				set_speed_multiplier(v);
			}
		} catch(...) {
			wxMessageBox(wxT("Invalid speed"), _T("Error"), wxICON_EXCLAMATION | wxOK, this);
		}
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
	case wxID_SPEED_500:
		set_speed(500);
		break;
	case wxID_SPEED_1000:
		set_speed(1000);
		break;
	case wxID_SPEED_TURBO:
		set_speed(-1);
		break;
	case wxID_LOAD_LIBRARY: {
		std::string name = std::string("load ") + loaded_library::call_library();
		with_loaded_library(new loaded_library(pick_file(this, name, project_otherpath(), false,
			loaded_library::call_library_ext())));
		handle_post_loadlibrary();
		break;
	}
	case wxID_SETTINGS:
		wxsetingsdialog_display(this, 0);
		break;
	case wxID_SETTINGS_HOTKEYS:
		wxsetingsdialog_display(this, 1);
		break;
	case wxID_SETTINGS_CONTROLLERS:
		wxsetingsdialog_display(this, 2);
		break;
	case wxID_LOAD_ROM_IMAGE:
		filename = pick_file_member(this, "Select new ROM image", rom_path());
		recent_roms->add(filename);
		platform::queue("unpause-emulator");
		platform::queue("reload-rom " + filename);
		return;
	case wxID_RELOAD_ROM_IMAGE:
		platform::queue("unpause-emulator");
		platform::queue("reload-rom");
		return;
	case wxID_NEW_MOVIE:
		show_projectwindow(this);
		return;
	case wxID_SHOW_MESSAGES:
		msg_window->reshow();
		return;
	case wxID_CONFLICTRESOLUTION:
		show_conflictwindow(this);
		return;
	case wxID_VUDISPLAY:
		open_vumeter_window(this);
		return;
	case wxID_MOVIE_EDIT:
		wxeditor_movie_display(this);
		return;
	case wxID_NEW_PROJECT:
		open_new_project_window(this);
		return;
	case wxID_LOAD_PROJECT: {
		auto projects = project_enumerate();
		std::vector<std::string> a;
		std::vector<wxString> b;
		for(auto i : projects) {
			a.push_back(i.first);
			b.push_back(towxstring(i.second));
		}
		if(a.empty()) {
			show_message_ok(this, "Load project", "No projects available", wxICON_EXCLAMATION);
			return;
		}
		wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(this, wxT("Select project to switch to:"),
			wxT("Load project"), b.size(), &b[0]);
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			throw canceled_exception();
		}
		std::string id = a[d2->GetSelection()];
		runemufn([id]() -> void {
			try {
				delete &project_load(id);	//Check.
				switch_projects(id);
			} catch(std::exception& e) {
				messages << "Failed to change project: " << e.what() << std::endl;
			}
			
		});
		return;
	}
	case wxID_CLOSE_PROJECT:
		runemufn([]() -> void { project_set(NULL); });
		return;
	};
}
