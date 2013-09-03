//Gaah... wx/wx.h (contains something that breaks if included after snes/snes.hpp from bsnes v085.
#include <wx/wx.h>

#include "lsnes.hpp"
#include "core/emucore.hpp"

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "lua/lua.hpp"
#include "core/mainloop.hpp"
#include "core/misc.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "core/rrdata.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/workthread.hpp"
#include "library/string.hpp"
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
	thread_id* ui_thread;
	volatile bool panic_ack = false;
	std::string modal_dialog_text;
	volatile bool modal_dialog_confirm;
	volatile bool modal_dialog_active;
	mutex* ui_mutex;
	condition* ui_condition;
	thread* joystick_thread_handle;

	void* joystick_thread(void* _args)
	{
		joystick_plugin::thread_fn();
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
		} else if(c == UISERV_RESIZED) {
			if(main_window)
				main_window->notify_resized();
		} else if(c == UISERV_MODAL) {
			std::string text;
			bool confirm;
			{
				mutex::holder h(*ui_mutex);
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
				mutex::holder h(*ui_mutex);
				modal_dialog_confirm = confirm;
				modal_dialog_active = false;
				ui_condition->signal();
			}
		} else if(c == UISERV_UPDATE_MESSAGES) {
			if(msg_window)
				msg_window->notify_update();
		} else if(c == UISERV_UPDATE_STATUS) {
			if(main_window)
				main_window->notify_update_status();
			wxeditor_movie_update();
		} else if(c == UISERV_UPDATE_SCREEN) {
			if(main_window)
				main_window->notify_update();
			wxwindow_memorysearch_update();
		} else if(c == UISERV_EXIT) {
			if(main_window)
				main_window->notify_exit();
		} else if(c == UISERV_UIFUN) {
			std::list<ui_queue_entry>::iterator i;
			queue_synchronous_fn_warning = true;
back:
			{
				mutex::holder h(*ui_mutex);
				if(ui_queue.empty())
					goto end;
				i = ui_queue.begin();
			}
			i->fn(i->arg);
			{
				mutex::holder h(*ui_mutex);
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

	void save_configuration()
	{
		std::string cfg = get_config_path() + "/lsneswxw.rc";
		std::ofstream cfgfile(cfg.c_str());
		//Joystick axis.
		for(auto i : keygroup::get_axis_set()) {
			keygroup* k = keygroup::lookup_by_name(i);
			auto p = k->get_parameters();
			cfgfile << "set-axis " << i << " ";
			switch(p.ktype) {
			case keygroup::KT_DISABLED:		cfgfile << "disabled";		break;
			case keygroup::KT_AXIS_PAIR:		cfgfile << "axis";		break;
			case keygroup::KT_AXIS_PAIR_INVERSE:	cfgfile << "axis-inverse";	break;
			case keygroup::KT_PRESSURE_M0:		cfgfile << "pressure-0";	break;
			case keygroup::KT_PRESSURE_MP:		cfgfile << "pressure-+";	break;
			case keygroup::KT_PRESSURE_0M:		cfgfile << "pressure0-";	break;
			case keygroup::KT_PRESSURE_0P:		cfgfile << "pressure0+";	break;
			case keygroup::KT_PRESSURE_PM:		cfgfile << "pressure+-";	break;
			case keygroup::KT_PRESSURE_P0:		cfgfile << "pressure+0";	break;
			};
			cfgfile << " minus=" << p.cal_left << " zero=" << p.cal_center << " plus=" << p.cal_right
				<< " tolerance=" << p.cal_tolerance << std::endl;
		}
		//Settings.
		for(auto i : setting::get_settings_set()) {
			if(!setting::is_set(i))
				cfgfile << "unset-setting " << i << std::endl;
			else
				cfgfile << "set-setting " << i << " " << setting::get(i) << std::endl;
		}
		for(auto i : setting::get_invalid_values())
			cfgfile << "set-setting " << i.first << " " << i.second << std::endl;
		//Aliases.
		for(auto i : command::get_aliases()) {
			std::string old_alias_value = command::get_alias_for(i);
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
				cfgfile << "alias-command " << i << " " << aliasline << std::endl;
			}
		}
		//Keybindings.
		for(auto i : keymapper::get_bindings()) {
			std::string i2 = i;
			size_t s = i2.find_first_of("|");
			size_t s2 = i2.find_first_of("/");
			if(s > i2.length() || s2 > s)
				continue;
			std::string key = i2.substr(s + 1);
			std::string mod = i2.substr(0, s2);
			std::string modspec = i2.substr(s2 + 1, s - s2 - 1);
			std::string old_command_value = keymapper::get_command_for(i);
			if(mod != "" || modspec != "")
				cfgfile << "bind-key " << mod << "/" << modspec << " " << key << " "
					<< old_command_value << std::endl;
			else
				cfgfile << "bind-key " << key << " " << old_command_value << std::endl;
		}
		//Last save.
		std::ofstream lsave(get_config_path() + "/" + our_rom_name + ".ls");
		lsave << last_save;
	}


	void* eloop_helper(void* x)
	{
		platform::dummy_event_loop();
		return NULL;
	}
}

wxString towxstring(const std::string& str) throw(std::bad_alloc)
{
	return wxString(str.c_str(), wxConvUTF8);
}

wxString towxstring(const std::u32string& str) throw(std::bad_alloc)
{
	return wxString(to_u8string(str).c_str(), wxConvUTF8);
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

void graphics_plugin::init() throw()
{
	initialize_wx_keyboard();
}

void graphics_plugin::quit() throw()
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
	std::vector<std::string> c_lua;
	bool exit_immediately;
};

IMPLEMENT_APP(lsnes_app)

lsnes_app::lsnes_app()
{
	settings_mode = false;
	exit_immediately = false;
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
		if(i == "--help" || i == "-h") {
			std::cout << "--settings: Show the settings dialog" << std::endl;
			std::cout << "--rom=<filename>: Load specified ROM on startup" << std::endl;
			std::cout << "--load=<filename>: Load specified save/movie on starup" << std::endl;
			std::cout << "--lua=<filename>: Load specified Lua script on startup" << std::endl;
			std::cout << "<filename>: Load specified ROM on startup" << std::endl;
			exit_immediately = true;
			return true;
		}
		if(i == "--settings")
			settings_mode = true;
		if(r = regex("--rom=(.+)", i))
			c_rom = r[1];
		if(r = regex("--load=(.+)", i))
			c_file = r[1];
		if(r = regex("--lua=(.+)", i))
			c_lua.push_back(r[1]);
		if(r = regex("[^-].*", i))
			c_rom = i;	//Alt. way to specify rom.
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

	ui_services = new ui_services_type();
	ui_mutex = &mutex::aquire();
	ui_condition = &condition::aquire(*ui_mutex);

	bsnes_core_version = get_core_identifier();
	ui_thread = &thread_id::me();
	platform::init();

	messages << "BSNES version: " << bsnes_core_version << std::endl;
	messages << "lsnes version: lsnes rr" << lsnes_version << std::endl;

	controls.set_port(0, porttype_info::port_default(0), false);
	controls.set_port(1, porttype_info::port_default(1), false);

	std::string cfgpath = get_config_path();
	messages << "Saving per-user data to: " << get_config_path() << std::endl;
	messages << "--- Running lsnesrc --- " << std::endl;
	setting::set_storage_mode(true);
	command::invokeC("run-script " + cfgpath + "/lsneswxw.rc");
	setting::set_storage_mode(false);
	messages << "--- End running lsnesrc --- " << std::endl;

	if(settings_mode) {
		//We got to boot this up quite a bit to get the joystick driver working.
		//In practicular, we need joystick thread and emulator thread in pause.
		joystick_thread_handle = &thread::create(joystick_thread, NULL);
		thread* dummy_loop = &thread::create(eloop_helper, NULL);
		wxsetingsdialog_display(NULL, false);
		platform::exit_dummy_event_loop();
		joystick_plugin::signal();
		joystick_thread_handle->join();
		dummy_loop->join();
		save_configuration();
		return false;
	}
	init_lua();

	joystick_thread_handle = &thread::create(joystick_thread, NULL);
	
	msg_window = new wxwin_messages();
	msg_window->Show();

	do_basic_core_init();
	loaded_rom* rom = NULL;
	if(c_rom != "")
		try {
			moviefile mov;
			rom = new loaded_rom(c_rom);
			rom->load(mov.movie_rtc_second, mov.movie_rtc_subsecond);
		} catch(std::exception& e) {
			std::cerr << "Can't load ROM: " << e.what() << std::endl;
			return false;
		}
	else
		rom = new loaded_rom;
	moviefile* mov = NULL;
	if(c_file != "")
		try {
			mov = new moviefile(c_file);
			if(c_rom != "")
				rom->load(mov->movie_rtc_second, mov->movie_rtc_subsecond);
		} catch(std::exception& e) {
			std::cerr << "Can't load state: " << e.what() << std::endl;
			return false;
		}
	else {
		mov = new moviefile;
		mov->port1 = &porttype_info::port_default(0);
		mov->port2 = &porttype_info::port_default(1);
		mov->input.clear(*mov->port1, *mov->port2);
		mov->projectid = get_random_hexstring(40);
		if(c_rom != "") {
			//Initialize the remainder.
			mov->coreversion = bsnes_core_version;
			mov->rerecords = "0";
			for(size_t i = 0; i < sizeof(rom->romimg)/sizeof(rom->romimg[0]); i++) {
				mov->romimg_sha256[i] = rom->romimg[i].sha256;
				mov->romxml_sha256[i] = rom->romxml[i].sha256;
			}
			mov->gametype = &rom->rtype->combine_region(*rom->region);
		}
	}
	mov->start_paused = true;
	for(auto i : c_lua) {
		messages << "Trying to run Lua script: " << i << std::endl;
		command::invokeC("run-lua " + i);
	}
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
	save_configuration();
	information_dispatch::do_dump_end();
	rrdata::close();
	joystick_plugin::signal();
	joystick_thread_handle->join();
	platform::quit();
	return 0;
}

void graphics_plugin::notify_message() throw()
{
	post_ui_event(UISERV_UPDATE_MESSAGES);
}

void graphics_plugin::notify_status() throw()
{
	post_ui_event(UISERV_UPDATE_STATUS);
}

void graphics_plugin::notify_screen() throw()
{
	post_ui_event(UISERV_UPDATE_SCREEN);
}

bool graphics_plugin::modal_message(const std::string& text, bool confirm) throw()
{
	mutex::holder h(*ui_mutex);
	modal_dialog_active = true;
	modal_dialog_confirm = confirm;
	modal_dialog_text = text;
	post_ui_event(UISERV_MODAL);
	while(modal_dialog_active)
		ui_condition->wait(10000);
	return modal_dialog_confirm;
}

void graphics_plugin::fatal_error() throw()
{
	//Fun: This can be called from any thread!
	if(ui_thread->is_me()) {
		//UI thread.
		platform::set_modal_pause(true);
		wxMessageBox(_T("Panic: Unrecoverable error, can't continue"), _T("Error"), wxICON_ERROR | wxOK);
	} else {
		//Emulation thread panic. Signal the UI thread.
		post_ui_event(UISERV_PANIC);
		while(!panic_ack);
	}
}

std::string graphics_plugin::request_rom(core_type& coretype)
{
	core_type* ctype = &coretype;
	std::string outname;
	mutex_class lock;
	cv_class cv;
	bool done = false;
	umutex_class h(lock);
	runuifun([ctype, &outname, &lock, &cv, &done]() -> void {
		if(done)
			return;
		try {
			main_window->request_rom(outname, *ctype);
		} catch(...) {
		}
		umutex_class h(lock);
		done = true;
		cv.notify_all();
	});
	while(!done)
		cv.wait(h);
	return outname;
}

void _runuifun_async(void (*fn)(void*), void* arg)
{
	mutex::holder h(*ui_mutex);
	ui_queue_entry e;
	e.fn = fn;
	e.arg = arg;
	ui_queue.push_back(e);
	auto i = ui_queue.insert(ui_queue.end(), e);
	post_ui_event(UISERV_UIFUN);
}


canceled_exception::canceled_exception() : std::runtime_error("Dialog canceled") {}

std::string pick_file(wxWindow* parent, const std::string& title, const std::string& startdir, bool forsave,
	std::string ext)
{
	wxString _title = towxstring(title);
	wxString _startdir = towxstring(startdir);
	std::string filespec;
	if(ext == "lsmv" && !forsave)
		filespec = "lsmv files|*.lsmv|lsmv backup files|*.lsmv.backup|All files|*";
	else if(ext != "")
		filespec = ext + " files|*." + ext + "|All files|*";
	else
		filespec = "All files|*";
	wxFileDialog* d = new wxFileDialog(parent, _title, _startdir, wxT(""), towxstring(filespec), forsave ?
		wxFD_SAVE : wxFD_OPEN);
	if(d->ShowModal() == wxID_CANCEL)
		throw canceled_exception();
	std::string filename = tostdstring(d->GetPath());
	int findex = d->GetFilterIndex();
	d->Destroy();
	if(filename == "")
		throw canceled_exception();
	if(forsave && ext != "" && findex == 0) {
		//Append extension if needed.
		size_t dpos = filename.find_first_of(".");
		if(dpos > filename.length() || filename.substr(dpos + 1) != ext)
			filename = filename + "." + ext;
	}
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


const char* graphics_plugin::name = "wxwidgets graphics plugin";
