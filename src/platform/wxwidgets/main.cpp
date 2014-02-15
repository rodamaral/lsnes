#include <wx/wx.h>

#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/joystickapi.hpp"
#include "core/keymapper.hpp"
#include "core/loadlib.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/moviefile-common.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include "library/threadtypes.hpp"
#include "library/utf8.hpp"
#include "library/zip.hpp"

#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_messages.hpp"
#include "platform/wxwidgets/window_status.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"

#include <cassert>
#include <boost/lexical_cast.hpp>

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/cmdline.h>
#include <iostream>

#define UISERV_REFRESH_TITLE 9990
#define UISERV_RESIZED 9991
#define UISERV_UIFUN 9992
//#define UISERV_UI_IRQ 9993	Not in use anymore, can be recycled.
#define UISERV_EXIT 9994
#define UISERV_UPDATE_STATUS 9995
#define UISERV_UPDATE_MESSAGES 9996
#define UISERV_UPDATE_SCREEN 9997
#define UISERV_PANIC 9998
#define UISERV_ERROR 9999

wxwin_messages* msg_window;
wxwin_mainwindow* main_window;
std::string our_rom_name;

bool wxwidgets_exiting = false;

namespace
{
	threadid_class ui_thread;
	volatile bool panic_ack = false;
	std::string error_message_text;
	volatile bool modal_dialog_confirm;
	volatile bool modal_dialog_active;
	mutex_class ui_mutex;
	cv_class ui_condition;
	thread_class* joystick_thread_handle;

	void* joystick_thread(int _args)
	{
		joystick_driver_thread_fn();
		return NULL;
	}

	struct uiserv_event : public wxEvent
	{
		uiserv_event(int code)
		{
			SetId(code);
		}

		wxEvent* Clone() const
		{
			return new uiserv_event(*this);
		}
	};

	class ui_services_type : public wxEvtHandler
	{
		bool ProcessEvent(wxEvent& event);
	};

	struct ui_queue_entry
	{
		void(*fn)(void*);
		void* arg;
	};

	std::list<ui_queue_entry> ui_queue;

	bool ui_services_type::ProcessEvent(wxEvent& event)
	{
		int c = event.GetId();
		if(c == UISERV_PANIC) {
			//We need to panic.
			wxMessageBox(_T("Panic: Unrecoverable error, can't continue"), _T("Error"), wxICON_ERROR |
				wxOK);
			panic_ack = true;
		} else if(c == UISERV_REFRESH_TITLE) {
			if(main_window)
				main_window->refresh_title();
		} else if(c == UISERV_RESIZED) {
			if(main_window)
				main_window->notify_resized();
		} else if(c == UISERV_ERROR) {
			std::string text = error_message_text;
			wxMessageBox(towxstring(text), _T("lsnes: Error"), wxICON_EXCLAMATION | wxOK, main_window);
		} else if(c == UISERV_UPDATE_MESSAGES) {
			if(msg_window)
				msg_window->notify_update();
		} else if(c == UISERV_UPDATE_STATUS) {
			if(main_window)
				main_window->notify_update_status();
			wxeditor_movie_update();
			wxeditor_hexeditor_update();
		} else if(c == UISERV_UPDATE_SCREEN) {
			if(main_window)
				main_window->notify_update();
			wxwindow_memorysearch_update();
			wxwindow_tasinput_update();
		} else if(c == UISERV_EXIT) {
			if(main_window)
				main_window->notify_exit();
		} else if(c == UISERV_UIFUN) {
			std::list<ui_queue_entry>::iterator i;
			ui_queue_entry e;
			queue_synchronous_fn_warning = true;
back:
			{
				umutex_class h(ui_mutex);
				if(ui_queue.empty())
					goto end;
				i = ui_queue.begin();
				e = *i;
				ui_queue.erase(i);
			}
			e.fn(e.arg);
			goto back;
end:
			queue_synchronous_fn_warning = false;
		}
		return true;
	}

	ui_services_type* ui_services;

	void post_ui_event(int code)
	{
		uiserv_event uic(code);
		wxPostEvent(ui_services, uic);
	}

	void handle_config_line(std::string line)
	{
		regex_results r;
		if(r = regex("SET[ \t]+([^ \t]+)[ \t]+(.*)", line)) {
			lsnes_vsetc.set(r[1], r[2], true);
			messages << "Setting " << r[1] << " set to " << r[2] << std::endl;
		} else if(r = regex("ALIAS[ \t]+([^ \t]+)[ \t]+(.*)", line)) {
			if(!lsnes_cmd.valid_alias_name(r[1])) {
				messages << "Illegal alias name " << r[1] << std::endl;
				return;
			}
			std::string tmp = lsnes_cmd.get_alias_for(r[1]);
			tmp = tmp + r[2] + "\n";
			lsnes_cmd.set_alias_for(r[1], tmp);
			messages << r[1] << " aliased to " << r[2] << std::endl;
		} else if(r = regex("BIND[ \t]+([^/]*)/([^|]*)\\|([^ \t]+)[ \t]+(.*)", line)) {
			std::string tmp = r[4];
			regex_results r2 = regex("(load|load-smart|load-readonly|load-preserve|load-state"
				"|load-movie|save-state|save-movie)[ \t]+\\$\\{project\\}(.*)\\.lsmv", tmp);
			if(r2) tmp = r2[1] + " $SLOT:" + r2[2];
			lsnes_mapper.bind(r[1], r[2], r[3], tmp);
			if(r[1] != "" || r[2] != "")
				messages << r[1] << "/" << r[2] << " ";
			messages << r[3] << " bound to '" << tmp << "'" << std::endl;
		} else if(r = regex("BUTTON[ \t]+([^ \t]+)[ \t](.*)", line)) {
			keyboard::ctrlrkey* ckey = lsnes_mapper.get_controllerkey(r[2]);
			if(ckey) {
				ckey->append(r[1]);
				messages << r[1] << " bound (button) to " << r[2] << std::endl;
			} else
				button_keys[r[2]] = r[1];
		} else if(r = regex("PREFER[ \t]+([^ \t]+)[ \t]+(.*)", line)) {
			if(r[2] != "") {
				core_selections[r[1]] = r[2];
				messages << "Prefer " << r[2] << " for " << r[1] << std::endl;
			}
		} else
			(stringfmt() << "Unrecognized directive: " << line).throwex();
	}

	void load_configuration()
	{
		std::string cfg = get_config_path() + "/lsneswxw.cfg";
		std::ifstream cfgfile(cfg.c_str());
		std::string line;
		size_t lineno = 1;
		while(std::getline(cfgfile, line)) {
			try {
				handle_config_line(line);
			} catch(std::exception& e) {
				messages << "Error processing line " << lineno << ": " << e.what() << std::endl;
			}
			lineno++;
		}
		refresh_alias_binds();
	}

	void save_configuration()
	{
		std::string cfg = get_config_path() + "/lsneswxw.cfg";
		std::ofstream cfgfile(cfg.c_str());
		//Settings.
		for(auto i : lsnes_vsetc.get_all())
			cfgfile << "SET " << i.first << " " << i.second << std::endl;
		//Aliases.
		for(auto i : lsnes_cmd.get_aliases()) {
			std::string old_alias_value = lsnes_cmd.get_alias_for(i);
			while(old_alias_value != "") {
				std::string aliasline;
				size_t s = old_alias_value.find_first_of("\n");
				if(s < old_alias_value.length()) {
					aliasline = old_alias_value.substr(0, s);
					old_alias_value = old_alias_value.substr(s + 1);
				} else {
					aliasline = old_alias_value;
					old_alias_value = "";
				}
				cfgfile << "ALIAS " << i << " " << aliasline << std::endl;
			}
		}
		//Keybindings.
		for(auto i : lsnes_mapper.get_bindings())
			cfgfile << "BIND " << std::string(i) << " " << lsnes_mapper.get(i) << std::endl;
		//Buttons.
		for(auto i : lsnes_mapper.get_controller_keys()) {
			std::string b;
			unsigned idx = 0;
			while((b = i->get_string(idx++)) != "")
				cfgfile << "BUTTON " << b << " " << i->get_command() << std::endl;
		}
		for(auto i : button_keys)
			cfgfile << "BUTTON " << i.second << " " << i.first << std::endl;
		for(auto i : core_selections)
			if(i.second != "")
				cfgfile << "PREFER " << i.first << " " << i.second << std::endl;
		//Last save.
		std::ofstream lsave(get_config_path() + "/" + our_rom_name + ".ls");
		lsave << last_save;
	}


	void* eloop_helper(int x)
	{
		platform::dummy_event_loop();
		return NULL;
	}

	std::string get_loaded_movie(const std::vector<std::string>& cmdline)
	{
		for(auto i : cmdline)
			if(!i.empty() && i[0] != '-')
				return i;
		return "";
	}
}

wxString towxstring(const std::string& str) throw(std::bad_alloc)
{
	return wxString(str.c_str(), wxConvUTF8);
}

std::string tostdstring(const wxString& str) throw(std::bad_alloc)
{
	return std::string(str.mb_str(wxConvUTF8));
}

wxString towxstring(const std::u32string& str) throw(std::bad_alloc)
{
	return wxString(utf8::to8(str).c_str(), wxConvUTF8);
}

std::u32string tou32string(const wxString& str) throw(std::bad_alloc)
{
	return utf8::to32(std::string(str.mb_str(wxConvUTF8)));
}

std::string pick_archive_member(wxWindow* parent, const std::string& filename) throw(std::bad_alloc)
{
	//Did we pick a .zip file?
	std::string f;
	try {
		zip::reader zr(filename);
		std::vector<wxString> files;
		for(auto i : zr)
			files.push_back(towxstring(i));
		wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(parent, wxT("Select file within .zip"),
			wxT("Select member"), files.size(), &files[0]);
		if(d2->ShowModal() == wxID_CANCEL) {
			d2->Destroy();
			return "";
		}
		f = filename + "/" + tostdstring(d2->GetStringSelection());
		d2->Destroy();
	} catch(...) {
		//Ignore error.
		f = filename;
	}
	return f;
}

void signal_program_exit()
{
	post_ui_event(UISERV_EXIT);
}

void signal_resize_needed()
{
	post_ui_event(UISERV_RESIZED);
}


static const wxCmdLineEntryDesc dummy_descriptor_table[] = {
	{ wxCMD_LINE_PARAM,  NULL, NULL, NULL, wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL |
		wxCMD_LINE_PARAM_MULTIPLE },
	{ wxCMD_LINE_NONE }
};

class lsnes_app : public wxApp
{
public:
	lsnes_app();
	virtual bool OnInit();
	virtual int OnExit();
	virtual void OnInitCmdLine(wxCmdLineParser& parser);
	virtual bool OnCmdLineParsed(wxCmdLineParser& parser);
private:
	bool settings_mode;
	bool pluginmanager_mode;
	std::string c_rom;
	std::string c_file;
	std::vector<std::string> cmdline;
	std::map<std::string, std::string> c_settings;
	std::vector<std::string> c_lua;
	bool exit_immediately;
};

IMPLEMENT_APP(lsnes_app)

lsnes_app::lsnes_app()
{
	settings_mode = false;
	pluginmanager_mode = false;
	exit_immediately = false;
}

void lsnes_app::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.SetDesc(dummy_descriptor_table);
	parser.SetSwitchChars(wxT(""));
}

bool lsnes_app::OnCmdLineParsed(wxCmdLineParser& parser)
{
	for(size_t i = 0; i< parser.GetParamCount(); i++)
		cmdline.push_back(tostdstring(parser.GetParam(i)));
	for(auto i: cmdline) {
		regex_results r;
		if(i == "--help" || i == "-h") {
			std::cout << "--settings: Show the settings dialog" << std::endl;
			std::cout << "--pluginmanager: Show the plugin manager" << std::endl;
			std::cout << "--rom=<filename>: Load specified ROM on startup" << std::endl;
			std::cout << "--load=<filename>: Load specified save/movie on starup" << std::endl;
			std::cout << "--lua=<filename>: Load specified Lua script on startup" << std::endl;
			std::cout << "--set=<a>=<b>: Set setting <a> to value <b>" << std::endl;
			std::cout << "<filename>: Load specified ROM on startup" << std::endl;
			exit_immediately = true;
			return true;
		}
		if(i == "--settings")
			settings_mode = true;
		if(i == "--pluginmanager")
			pluginmanager_mode = true;
		if(r = regex("--set=([^=]+)=(.+)", i))
			c_settings[r[1]] = r[2];
		if(r = regex("--lua=(.+)", i))
			c_lua.push_back(r[1]);
	}
	return true;
}

bool lsnes_app::OnInit()
{
	wxApp::OnInit();
	if(exit_immediately)
		return false;

	reached_main();
	set_random_seed();
	bring_app_foreground();

	if(pluginmanager_mode)
		if(!wxeditor_plugin_manager_display(NULL))
			return false;

	ui_services = new ui_services_type();

	ui_thread = this_thread_id();
	platform::init();

	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;

	loaded_rom dummy_rom;
	std::map<std::string, std::string> settings;
	auto ctrldata = dummy_rom.rtype->controllerconfig(settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex());

	reinitialize_buttonmap();
	controls.set_ports(ports);

	std::string cfgpath = get_config_path();
	autoload_libraries([](const std::string& libname, const std::string& error, bool system) {
		show_message_ok(NULL, "Error loading plugin " + libname, "Error loading '" + libname + "'\n\n" +
			error, wxICON_EXCLAMATION);
		if(!system)
			wxeditor_plugin_manager_notify_fail(libname);
	});
	messages << "Saving per-user data to: " << get_config_path() << std::endl;
	messages << "--- Loading configuration --- " << std::endl;
	load_configuration();
	messages << "--- End running lsnesrc --- " << std::endl;

	if(settings_mode) {
		//We got to boot this up quite a bit to get the joystick driver working.
		//In practicular, we need joystick thread and emulator thread in pause.
		joystick_thread_handle = new thread_class(joystick_thread, 6);
		thread_class* dummy_loop = new thread_class(eloop_helper, 8);
		display_settings_dialog(NULL, NULL);
		platform::exit_dummy_event_loop();
		joystick_driver_signal();
		joystick_thread_handle->join();
		dummy_loop->join();
		save_configuration();
		return false;
	}
	init_lua();

	joystick_thread_handle = new thread_class(joystick_thread, 7);

	msg_window = new wxwin_messages();
	msg_window->Show();

	const std::string movie_file = get_loaded_movie(cmdline);
	loaded_rom rom;
	try {
		moviefile mov;
		rom = construct_rom(movie_file, cmdline);
		rom.load(c_settings, mov.movie_rtc_second, mov.movie_rtc_subsecond);
	} catch(std::exception& e) {
		std::cerr << "Can't load ROM: " << e.what() << std::endl;
		show_message_ok(NULL, "Error loading ROM", std::string("Error loading ROM:\n\n") +
			e.what(), wxICON_EXCLAMATION);
		quit_lua();	//Don't crash.
		return false;
	}

	moviefile* mov = NULL;
	if(movie_file != "")
		try {
			mov = new moviefile(movie_file, *rom.rtype);
			rom.load(mov->settings, mov->movie_rtc_second, mov->movie_rtc_subsecond);
		} catch(std::exception& e) {
			std::cerr << "Can't load state: " << e.what() << std::endl;
			show_message_ok(NULL, "Error loading movie", std::string("Error loading movie:\n\n") +
				e.what(), wxICON_EXCLAMATION);
			quit_lua();	//Don't crash.
			return false;
		}
	else {
		mov = new moviefile(rom, c_settings, DEFAULT_RTC_SECOND, DEFAULT_RTC_SUBSECOND);
	}
	our_rom = rom;
	mov->start_paused = true;
	for(auto i : c_lua)
		lua_add_startup_script(i);
	boot_emulator(rom, *mov);
	return true;
}

int lsnes_app::OnExit()
{
	if(settings_mode)
		return 0;
	//NULL these so no further messages will be sent.
	auto x = msg_window;
	msg_window = NULL;
	main_window = NULL;
	if(x)
		x->Destroy();
	save_configuration();
	information_dispatch::do_dump_end();
	quit_lua();
	movb.release_memory();
	joystick_driver_signal();
	joystick_thread_handle->join();
	platform::quit();
	cleanup_all_keys();
	cleanup_keymapper();
	return 0;
}

namespace
{
	struct _graphics_driver drv = {
		.init = []() -> void {
			initialize_wx_keyboard();
		},
		.quit = []() -> void {},
		.notify_message = []() -> void
		{
			post_ui_event(UISERV_UPDATE_MESSAGES);
		},
		.notify_status = []() -> void
		{
			post_ui_event(UISERV_UPDATE_STATUS);
		},
		.notify_screen = []() -> void
		{
			post_ui_event(UISERV_UPDATE_SCREEN);
		},
		.error_message = [](const std::string& text) -> void {
			error_message_text = text;
			post_ui_event(UISERV_ERROR);
		},
		.fatal_error = []() -> void {
			//Fun: This can be called from any thread!
			if(ui_thread == this_thread_id()) {
				//UI thread.
				platform::set_modal_pause(true);
				wxMessageBox(_T("Panic: Unrecoverable error, can't continue"), _T("Error"),
					wxICON_ERROR | wxOK);
			} else {
				//Emulation thread panic. Signal the UI thread.
				post_ui_event(UISERV_PANIC);
				while(!panic_ack);
			}
		},
		.name = []() -> const char* { return "wxwidgets graphics plugin"; },
		.action_updated = []()
		{
			runuifun([]() -> void { main_window->action_updated(); });
		},
		.request_rom = [](rom_request& req)
		{
			rom_request* _req = &req;
			mutex_class lock;
			cv_class cv;
			bool done = false;
			umutex_class h(lock);
			runuifun([_req, &lock, &cv, &done]() -> void {
				try {
					main_window->request_rom(*_req);
				} catch(...) {
					_req->canceled = true;
				}
				umutex_class h(lock);
				done = true;
				cv.notify_all();
			});
			while(!done)
				cv.wait(h);
		}
	};
	struct graphics_driver _drv(drv);
}

void signal_core_change()
{
	post_ui_event(UISERV_REFRESH_TITLE);
}

void _runuifun_async(void (*fn)(void*), void* arg)
{
	umutex_class h(ui_mutex);
	ui_queue_entry e;
	e.fn = fn;
	e.arg = arg;
	ui_queue.push_back(e);
	post_ui_event(UISERV_UIFUN);
}


canceled_exception::canceled_exception() : std::runtime_error("Dialog canceled") {}

std::string pick_file(wxWindow* parent, const std::string& title, const std::string& startdir)
{
	wxString _title = towxstring(title);
	wxString _startdir = towxstring(startdir);
	std::string filespec;
	filespec = "All files|*";
	wxFileDialog* d = new wxFileDialog(parent, _title, _startdir, wxT(""), towxstring(filespec), wxFD_OPEN);
	if(d->ShowModal() == wxID_CANCEL)
		throw canceled_exception();
	std::string filename = tostdstring(d->GetPath());
	d->Destroy();
	if(filename == "")
		throw canceled_exception();
	return filename;
}

std::string pick_file_member(wxWindow* parent, const std::string& title, const std::string& startdir)
{
	std::string filename = pick_file(parent, title, startdir);
	//Did we pick a .zip file?
	if(!regex_match(".*\\.[zZ][iI][pP]", filename))
		return filename;	//Not a ZIP.
	try {
		zip::reader zr(filename);
		std::vector<std::string> files;
		for(auto i : zr)
			files.push_back(i);
		filename = filename + "/" + pick_among(parent, "Select member", "Select file within .zip", files);
	} catch(canceled_exception& e) {
		//Throw these forward.
		throw;
	} catch(...) {
		//Ignore error.
	}
	return filename;
}

std::string pick_among(wxWindow* parent, const std::string& title, const std::string& prompt,
	const std::vector<std::string>& choices, unsigned defaultchoice)
{
	std::vector<wxString> _choices;
	for(auto i : choices)
		_choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(parent, towxstring(prompt), towxstring(title),
		_choices.size(), &_choices[0]);
	d2->SetSelection(defaultchoice);
	if(d2->ShowModal() == wxID_CANCEL) {
		d2->Destroy();
		throw canceled_exception();
	}
	std::string out = tostdstring(d2->GetStringSelection());
	d2->Destroy();
	return out;
}

std::string pick_text(wxWindow* parent, const std::string& title, const std::string& prompt, const std::string& dflt,
	bool multiline)
{
	wxTextEntryDialog* d2 = new wxTextEntryDialog(parent, towxstring(prompt), towxstring(title), towxstring(dflt),
		wxOK | wxCANCEL | wxCENTRE | (multiline ? wxTE_MULTILINE : 0));
	if(d2->ShowModal() == wxID_CANCEL) {
		d2->Destroy();
		throw canceled_exception();
	}
	std::string text = tostdstring(d2->GetValue());
	d2->Destroy();
	return text;
}

void show_message_ok(wxWindow* parent, const std::string& title, const std::string& text, int icon)
{
	wxMessageDialog* d3 = new wxMessageDialog(parent, towxstring(text), towxstring(title), wxOK | icon);
	d3->ShowModal();
	d3->Destroy();
}
