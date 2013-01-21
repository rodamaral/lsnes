//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>

#include "lsnes.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/joystickapi.hpp"
#include "core/loadlib.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include "library/threadtypes.hpp"
#include "library/zip.hpp"

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
#define UISERV_MODAL 9999

wxwin_messages* msg_window;
wxwin_mainwindow* main_window;
std::string our_rom_name;

bool dummy_interface = false;
bool wxwidgets_exiting = false;

namespace
{
	threadid_class ui_thread;
	volatile bool panic_ack = false;
	std::string modal_dialog_text;
	volatile bool modal_dialog_confirm;
	volatile bool modal_dialog_active;
	mutex_class ui_mutex;
	cv_class ui_condition;
	thread_class* joystick_thread_handle;

	void* joystick_thread(int _args)
	{
		joystick_driver_thread_fn();
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
		} else if(c == UISERV_MODAL) {
			std::string text;
			bool confirm;
			{
				umutex_class h(ui_mutex);
				text = modal_dialog_text;
				confirm = modal_dialog_confirm;
			}
			if(confirm) {
				int ans = wxMessageBox(towxstring(text), _T("Question"), wxICON_QUESTION | wxOK |
					wxCANCEL, main_window);
				confirm = (ans == wxOK);
			} else {
				wxMessageBox(towxstring(text), _T("Notification"), wxICON_INFORMATION | wxOK,
					main_window);
			}
			{
				umutex_class h(ui_mutex);
				modal_dialog_confirm = confirm;
				modal_dialog_active = false;
				ui_condition.notify_all();
			}
		} else if(c == UISERV_UPDATE_MESSAGES) {
			if(msg_window)
				msg_window->notify_update();
		} else if(c == UISERV_UPDATE_STATUS) {
			if(main_window)
				main_window->notify_update_status();
		} else if(c == UISERV_UPDATE_SCREEN) {
			if(main_window)
				main_window->notify_update();
		} else if(c == UISERV_EXIT) {
			if(main_window)
				main_window->notify_exit();
		} else if(c == UISERV_UIFUN) {
			std::list<ui_queue_entry>::iterator i;
			queue_synchronous_fn_warning = true;
back:
			{
				umutex_class h(ui_mutex);
				if(ui_queue.empty())
					goto end;
				i = ui_queue.begin();
			}
			i->fn(i->arg);
			{
				umutex_class h(ui_mutex);
				ui_queue.erase(i);
			}
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
		if(r = regex("AXIS[ \t]+([^ \t]+)[ \t]+(-?[0-9])[ \t]+(-?[0-9])[ \t]+(-?[0-9])[ \t]+"
			"(-?[0-9]+)[ \t]+(-?[0-9]+)[ \t]+(-?[0-9]+)[ \t]+(0?.[0-9]+)[ \t]*", line)) {
			keyboard_axis_calibration c;
			c.mode = parse_value<int>(r[2]);
			if(c.mode < -1 || c.mode > 1) {
				messages << "Illegal axis mode " << c.mode << std::endl;
				return;
			}
			c.esign_a = parse_value<int>(r[3]);
			c.esign_b = parse_value<int>(r[4]);
			if(c.esign_a < -1 || c.esign_a > 1 || c.esign_b < -1 || c.esign_b > 1 ||
				c.esign_a == c.esign_b || (c.mode == 1 && (c.esign_a == 0 || c.esign_b == 0))) {
				messages << "Illegal axis endings " << c.esign_a << "/" << c.esign_b << std::endl;
				return;
			}
			c.left = parse_value<int32_t>(r[5]);
			c.center = parse_value<int32_t>(r[6]);
			c.right = parse_value<int32_t>(r[7]);
			c.nullwidth = parse_value<double>(r[8]);
			keyboard_key* _k = lsnes_kbd.try_lookup_key(r[1]);
			keyboard_key_axis* k = NULL;
			if(_k)
				k = _k->cast_axis();
			if(!k)
				return;
			k->set_calibration(c);
			messages << "Calibration of " << r[1] << " changed: mode=" << calibration_to_mode(c)
				<< " limits=" << c.left << "(" << c.center << ")" << c.right
				<< " null=" << c.nullwidth << std::endl;
		} else if(r = regex("UNSET[ \t]+([^ \t]+)[ \t]*", line)) {
			try {
				lsnes_set.blank(r[1]);
				messages << "Setting " << r[1] << " unset" << std::endl;
			} catch(std::exception& e) {
				messages << "Can't unset " << r[1] << ": " << e.what() << std::endl;
			}
		} else if(r = regex("SET[ \t]+([^ \t]+)[ \t]+(.*)", line)) {
			try {
				lsnes_set.set(r[1], r[2]);
				messages << "Setting " << r[1] << " set to " << r[2] << std::endl;
			} catch(std::exception& e) {
				messages << "Can't set " << r[1] << ": " << e.what() << std::endl;
			}
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
			lsnes_mapper.bind(r[1], r[2], r[3], r[4]);
			if(r[1] != "" || r[2] != "")
				messages << r[1] << "/" << r[2] << " ";
			messages << r[3] << " bound to '" << r[4] << "'" << std::endl;
		} else if(r = regex("BUTTON[ \t]+([^ \t]+)[ \t](.*)", line)) {
			controller_key* ckey = lsnes_mapper.get_controllerkey(r[2]);
			if(ckey) {
				ckey->set(r[1]);
				messages << r[1] << " bound (button) to " << r[2] << std::endl;
			} else
				button_keys[r[2]] = r[1];
		} else if(r = regex("PREFER[ \t]+([^ \t]+)[ \t]+(.*)", line)) {
			core_selections[r[1]] = r[2];
			messages << "Prefer " << r[2] << " for " << r[1] << std::endl;
		} else
			messages << "Unrecognized directive: " << line << std::endl;
	}

	void load_configuration()
	{
		std::string cfg = get_config_path() + "/lsneswxw.cfg";
		std::ifstream cfgfile(cfg.c_str());
		std::string line;
		while(std::getline(cfgfile, line))
			try {
				handle_config_line(line);
			} catch(std::exception& e) {
				messages << "Error processing line: " << e.what() << std::endl;
			}
	}
	
	void save_configuration()
	{
		std::string cfg = get_config_path() + "/lsneswxw.cfg";
		std::ofstream cfgfile(cfg.c_str());
		//Joystick axis.
		for(auto i : lsnes_kbd.all_keys()) {
			keyboard_key_axis* j = i->cast_axis();
			if(!j)
				continue;
			auto p = j->get_calibration();
			cfgfile << "AXIS " << i->get_name() << " " << p.mode << " " << p.esign_a << " " << p.esign_b
				<< " " << p.left << " " << p.center << " " << p.right << " " << p.nullwidth
				<< std::endl;
		}
		//Settings.
		for(auto i : lsnes_set.get_settings_set()) {
			if(!lsnes_set.is_set(i))
				cfgfile << "UNSET " << i << std::endl;
			else
				cfgfile << "SET " << i << " " << lsnes_set.get(i) << std::endl;
		}
		for(auto i : lsnes_set.get_invalid_values())
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
			std::string b = i->get_string();
			if(b != "")
				cfgfile << "BUTTON " << b << " " << i->get_command() << std::endl;
		}
		for(auto i : button_keys)
			cfgfile << "BUTTON " << i.second << " " << i.first << std::endl;
		for(auto i : core_selections)
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
}

wxString towxstring(const std::string& str) throw(std::bad_alloc)
{
	return wxString(str.c_str(), wxConvUTF8);
}

std::string tostdstring(const wxString& str) throw(std::bad_alloc)
{
	return std::string(str.mb_str(wxConvUTF8));
}

std::string pick_archive_member(wxWindow* parent, const std::string& filename) throw(std::bad_alloc)
{
	//Did we pick a .zip file?
	std::string f;
	try {
		zip_reader zr(filename);
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

void graphics_driver_init() throw()
{
	initialize_wx_keyboard();
}

void graphics_driver_quit() throw()
{
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
	std::string c_rom;
	std::string c_file;
	std::map<std::string, std::string> c_settings;
};

IMPLEMENT_APP(lsnes_app)

lsnes_app::lsnes_app()
{
	settings_mode = false;
}

void lsnes_app::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.SetDesc(dummy_descriptor_table);
	parser.SetSwitchChars(wxT(""));
}

bool lsnes_app::OnCmdLineParsed(wxCmdLineParser& parser)
{
	std::vector<std::string> cmdline;
	for(size_t i = 0; i< parser.GetParamCount(); i++)
		cmdline.push_back(tostdstring(parser.GetParam(i)));
	for(auto i: cmdline) {
		regex_results r;
		if(i == "--settings")
			settings_mode = true;
		if(r = regex("--rom=(.+)", i))
			c_rom = r[1];
		if(r = regex("--load=(.+)", i))
			c_file = r[1];
		if(r = regex("--set=([^=]+)=(.+)", i))
			c_settings[r[1]] = r[2];
	}
}


bool lsnes_app::OnInit()
{
	wxApp::OnInit();

	reached_main();
	set_random_seed();
	bring_app_foreground();

	ui_services = new ui_services_type();

	ui_thread = this_thread_id();
	platform::init();

	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;

	loaded_rom dummy_rom;
	std::map<std::string, std::string> settings;
	auto ctrldata = dummy_rom.rtype->controllerconfig(settings);
	port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex);

	reinitialize_buttonmap();
	controls.set_ports(ports);

	std::string cfgpath = get_config_path();
	autoload_libraries();
	messages << "Saving per-user data to: " << get_config_path() << std::endl;
	messages << "--- Loading configuration --- " << std::endl;
	lsnes_set.set_storage_mode(true);
	load_configuration();
	lsnes_set.set_storage_mode(false);
	messages << "--- End running lsnesrc --- " << std::endl;

	if(settings_mode) {
		//We got to boot this up quite a bit to get the joystick driver working.
		//In practicular, we need joystick thread and emulator thread in pause.
		joystick_thread_handle = new thread_class(joystick_thread, 6);
		thread_class* dummy_loop = new thread_class(eloop_helper, 8);
		wxsetingsdialog_display(NULL, false);
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

	loaded_rom* rom = NULL;
	if(c_rom != "")
		try {
			moviefile mov;
			rom = new loaded_rom(c_rom);
			rom->load(c_settings, mov.movie_rtc_second, mov.movie_rtc_subsecond);
		} catch(std::exception& e) {
			std::cerr << "Can't load ROM: " << e.what() << std::endl;
			return false;
		}
	else
		rom = new loaded_rom;
	moviefile* mov = NULL;
	if(c_file != "")
		try {
			if(!rom)
				throw std::runtime_error("No ROM loaded");
			mov = new moviefile(c_file, *rom->rtype);
			if(c_rom != "")
				rom->load(mov->settings, mov->movie_rtc_second, mov->movie_rtc_subsecond);
		} catch(std::exception& e) {
			std::cerr << "Can't load state: " << e.what() << std::endl;
			return false;
		}
	else {
		mov = new moviefile;
		mov->settings = c_settings;
		auto ctrldata = rom->rtype->controllerconfig(mov->settings);
		port_type_set& ports = port_type_set::make(ctrldata.ports, ctrldata.portindex);
		mov->input.clear(ports);
		mov->coreversion = rom->rtype->get_core_identifier();
		if(c_rom != "") {
			//Initialize the remainder.
			mov->projectid = get_random_hexstring(40);
			mov->rerecords = "0";
			for(size_t i = 0; i < sizeof(rom->romimg)/sizeof(rom->romimg[0]); i++) {
				mov->romimg_sha256[i] = rom->romimg[i].sha_256;
				mov->romxml_sha256[i] = rom->romxml[i].sha_256;
			}
		}
		mov->gametype = &rom->rtype->combine_region(*rom->region);
	}
	our_rom = rom;
	mov->start_paused = true;
	boot_emulator(*rom, *mov);

	return true;
}

int lsnes_app::OnExit()
{
	if(settings_mode)
		return 0;
	//NULL these so no further messages will be sent.
	auto x = msg_window;
	auto y = main_window;
	msg_window = NULL;
	main_window = NULL;
	if(x)
		x->Destroy();
	save_configuration();
	information_dispatch::do_dump_end();
	rrdata::close();
	quit_lua();
	joystick_driver_signal();
	joystick_thread_handle->join();
	platform::quit();
	return 0;
}

void graphics_driver_notify_message() throw()
{
	post_ui_event(UISERV_UPDATE_MESSAGES);
}

void graphics_driver_notify_status() throw()
{
	post_ui_event(UISERV_UPDATE_STATUS);
}

void graphics_driver_notify_screen() throw()
{
	post_ui_event(UISERV_UPDATE_SCREEN);
}

void signal_core_change()
{
	post_ui_event(UISERV_REFRESH_TITLE);
}

bool graphics_driver_modal_message(const std::string& text, bool confirm) throw()
{
	umutex_class h(ui_mutex);
	modal_dialog_active = true;
	modal_dialog_confirm = confirm;
	modal_dialog_text = text;
	post_ui_event(UISERV_MODAL);
	while(modal_dialog_active)
		cv_timed_wait(ui_condition, h, microsec_class(10000));
	return modal_dialog_confirm;
}

void graphics_driver_fatal_error() throw()
{
	//Fun: This can be called from any thread!
	if(ui_thread == this_thread_id()) {
		//UI thread.
		platform::set_modal_pause(true);
		wxMessageBox(_T("Panic: Unrecoverable error, can't continue"), _T("Error"), wxICON_ERROR | wxOK);
	} else {
		//Emulation thread panic. Signal the UI thread.
		post_ui_event(UISERV_PANIC);
		while(!panic_ack);
	}
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

std::string pick_file(wxWindow* parent, const std::string& title, const std::string& startdir, bool forsave)
{
	wxString _title = towxstring(title);
	wxString _startdir = towxstring(startdir);
	wxFileDialog* d = new wxFileDialog(parent, _title, _startdir, wxT(""), wxT("All files|*"), forsave ?
		wxFD_SAVE : wxFD_OPEN);
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
	std::string filename = pick_file(parent, title, startdir, false);
	//Did we pick a .zip file?
	try {
		zip_reader zr(filename);
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
	const std::vector<std::string>& choices)
{
	std::vector<wxString> _choices;
	for(auto i : choices)
		_choices.push_back(towxstring(i));
	wxSingleChoiceDialog* d2 = new wxSingleChoiceDialog(parent, towxstring(prompt), towxstring(title),
		_choices.size(), &_choices[0]);
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


const char* graphics_driver_name = "wxwidgets graphics plugin";
