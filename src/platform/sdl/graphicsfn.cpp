#include "lsnes.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/framerate.hpp"
#include "core/window.hpp"
#include "interface/core.hpp"
#include "platform/sdl/platform.hpp"

#include <unistd.h>

screen_model* screenmod;

#define USERCODE_TIMER 0
#define USERCODE_PAINT 1

#define SPECIALMODE_NORMAL 0
#define SPECIALMODE_COMMAND 1
#define SPECIALMODE_IDENTIFY 2
#define SPECIALMODE_MODAL 3

#ifdef SDL_NO_JOYSTICK
	unsigned translate_sdl_joystick(SDL_Event& e, keypress& k1)
	{
		return 0;
	}
#endif

namespace
{
	//Dirty flags for various displays.
	volatile bool messages_dirty = false;
	volatile bool status_dirty = false;
	volatile bool screen_dirty = false;
	volatile bool fullscreen_console = false;
	bool fullscreen_console_active = false;
	//If true, repaint request is in flight. Protected by ui_mutex.
	volatile bool repaint_in_flight = false;
	//If true, timer event has occured.
	volatile bool timer_triggered = false;
	//If true, modal dialog is to be displayed. Pull low to ack the dialog. Protected by ui_mutex.
	volatile bool modal_dialog_active = false;
	//If modal_dialog_active is true, this is the text for the dialog box. Protected by ui_mutex.
	std::string modal_dialog_text;
	//If true, the modal dialog is confirmation dialog. Pull low if confirmation dialog is canceled. Protected
	//by ui_mutex.
	volatile bool modal_dialog_confirm = true;
	//Set if emulator panics.
	volatile bool paniced = false;
	//Set when user dismisses panic prompt (emulator can exit).
	volatile bool panic_ack = false;
	//Set after SIGALRM handler has got installed.
	bool sigalrm_handler_installed = false;
	//Special mode.
	unsigned special_mode;
	volatile bool identify_requested;
	volatile bool emulator_thread_exited = false;
	//The command line modal to use.
	commandline_model cmdline;
	//Mutex protecting various variables and associated mutex for waking the emulator thread from some blocking
	//operations.
	mutex* ui_mutex;
	condition* ui_condition;
	//Set to true when SDL has been initialized.
	bool sdl_init = false;
	//Thread ID of UI thread (for identifying it).
	thread_id* ui_thread;
	//Timer for implementing stuff like autorepeat.
	SDL_TimerID timer_id;
	//Timer IRQ counter. Used by identify key stuff.
	volatile unsigned timer_irq_counter;

	void sigalrm_handler(int s)
	{
		_exit(1);
	}

	Uint32 timer_cb(Uint32 interval, void* param)
	{
		timer_triggered = true;
		timer_irq_counter = timer_irq_counter + 1;
		return interval;
	}

	void arm_sigalrm()
	{
#ifdef SIGALRM
		if(!sigalrm_handler_installed)
			signal(SIGALRM, sigalrm_handler);
		sigalrm_handler_installed = true;
		//alarm(15);
#endif
	}

	void ui_panic()
	{
		//Be very careful what you use here, as program state can be really unpredictable!
		try {
			platform::message("PANIC: Cannot continue, press ESC or close window to exit.");
			screenmod->repaint_full();
			screenmod->flip();
		} catch(...) {
			//Just crash.
			panic_ack = true;
			return;
		}
		while(true) {
			SDL_Event e;
			if(SDL_WaitEvent(&e)) {
				if(e.type == SDL_QUIT)
					break;
				if(e.type == SDL_ACTIVEEVENT) {
					screenmod->repaint_full();
					screenmod->flip();
				}
				if(e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_ESCAPE)
					break;
			}
		}
		panic_ack = true;
	}

	void wake_ui()
	{
		mutex::holder h(*ui_mutex);
		if(!repaint_in_flight) {
			//Wake the UI.
			repaint_in_flight = true;
		}
	}

	function_ptr_command<> identify_key("identify-key", "Identify a key",
		"Syntax: identify-key\nIdentifies a (pseudo-)key.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			identify_requested = true;
			wake_ui();
		});

	function_ptr_command<> scroll_up("scroll-up", "Scroll messages a page up",
		"Syntax: scroll-up\nScrolls message console backward one page.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mutex::holder h(platform::msgbuf_lock());
			platform::msgbuf.scroll_up_page();
		});

	function_ptr_command<> scroll_fullup("scroll-fullup", "Scroll messages to beginning",
		"Syntax: scroll-fullup\nScrolls message console to its beginning.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mutex::holder h(platform::msgbuf_lock());
			platform::msgbuf.scroll_beginning();
		});

	function_ptr_command<> scroll_fulldown("scroll-fulldown", "Scroll messages to end",
		"Syntax: scroll-fulldown\nScrolls message console to its end.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mutex::holder h(platform::msgbuf_lock());
			platform::msgbuf.scroll_end();
		});

	function_ptr_command<> scrolldown("scroll-down", "Scroll messages a page down",
		"Syntax: scroll-up\nScrolls message console forward one page.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			mutex::holder h(platform::msgbuf_lock());
			platform::msgbuf.scroll_down_page();
		});

	function_ptr_command<> toggle_console("toggle-console", "Toggle console between small and full window",
		"Syntax: toggle-console\nToggles console between small and large.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			fullscreen_console = !fullscreen_console;
			wake_ui();
		});

	class keygrabber : public information_dispatch
	{
	public:
		keygrabber() : information_dispatch("sdl-key-grabber") { idmode = false; }
		void enter_id_mode()
		{
			keys = "";
			idmode = true;
		}
		bool got_id()
		{
			return keys != "";
		}
		std::string get_id()
		{
			return keys;
		}
		void leave_id_mode()
		{
			keys = "";
			idmode = false;
		}
		void on_key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned subkey,
			bool polarity, const std::string& name)
		{
			if(idmode && !polarity) {
				keys = keys + "key: " + name + "\n";
			}
		}
		bool idmode;
		std::string keys;
	} keygrabber;

	void emu_ungrab_keys(void* dummy)
	{
		keygrabber.ungrab_keys();
		keygrabber.leave_id_mode();
	}

	void emu_grab_keys_identify(void* dummy)
	{
		keygrabber.enter_id_mode();
		keygrabber.grab_keys();
	}

	void emu_grab_keys_nonid(void* dummy)
	{
		keygrabber.leave_id_mode();
		keygrabber.grab_keys();
	}

	void emu_handle_quit_signal(void* dummy)
	{
		information_dispatch::do_close();
	}

	void emu_handle_identify(void* dummy)
	{
		if(keygrabber.got_id()) {
			std::string k = keygrabber.get_id();
			keygrabber.leave_id_mode();
			platform::modal_message(k, false);
		}
		//Exiting the modal mode undoes key grab and modal pause.
	}

	//Grab keys, setting or unsetting id mode.
	void ui_grab_keys(bool idmode)
	{
		if(idmode)
			platform::queue(emu_grab_keys_identify, NULL, true);
		else
			platform::queue(emu_grab_keys_nonid, NULL, true);
	}

	//Grab keys, unset id mode, special.
	void ui_grab_keys_special()
	{
		keygrabber.leave_id_mode();
		keygrabber.grab_keys();
	}

	//Ungrab keys.
	void ui_ungrab_keys(bool direct = false)
	{
		if(direct) {
			keygrabber.ungrab_keys();
			keygrabber.leave_id_mode();
		} else
			platform::queue(emu_ungrab_keys, NULL, true);
	}

	//Handle identify timer interrupt.
	void ui_handle_identify()
	{
		//This call has to be asynchronous.
		platform::queue(emu_handle_identify, NULL, false);
	}

	//Handle QUIT in normal state.
	void ui_handle_quit_signal()
	{
		platform::queue(emu_handle_quit_signal, NULL, false);
	}

	keygroup mouse_x("mouse_x", keygroup::KT_MOUSE);
	keygroup mouse_y("mouse_y", keygroup::KT_MOUSE);
	keygroup mouse_l("mouse_left", keygroup::KT_KEY);
	keygroup mouse_m("mouse_center", keygroup::KT_KEY);
	keygroup mouse_r("mouse_right", keygroup::KT_KEY);
}

void notify_emulator_exit()
{
	emulator_thread_exited = true;
	wake_ui();
}

void ui_loop()
{
	uint32_t mouse_state = 0;
	uint32_t k;
	uint64_t kts;
	std::string cmd;
	bool modal_dialog_was_active = false;
	while(!emulator_thread_exited) {
		bool commandline_updated = false;
		SDL_Event e;
		SDLKey kbdkey;
		bool iskbd = false;
		bool polarity;
		bool full = false;
		memset(&e, 0, sizeof(e));
		{
			ui_mutex->lock();
			int r = SDL_PollEvent(&e);
			if(!repaint_in_flight && !timer_triggered && !r) {
				ui_mutex->unlock();
				usleep(2000);
				continue;
			}
			ui_mutex->unlock();
		}
		if(e.type == SDL_KEYUP) {
			iskbd = true;
			polarity = false;
			kbdkey = e.key.keysym.sym;
			//std::cerr << "Keyup symbol " << kbdkey << "(held=" << (get_utime() - kts) << "us)" << std::endl;
		} else if(e.type == SDL_KEYDOWN) {
			iskbd = true;
			polarity = true;
			kbdkey = e.key.keysym.sym;
			//std::cerr << "Keydown symbol " << kbdkey << std::endl;
			kts = get_utime();
		}
		if(e.type == SDL_VIDEOEXPOSE || e.type == SDL_ACTIVEEVENT)
			full = true;
		//Handle panics.
		if(paniced) {
			ui_panic();
			while(true);
		}
		if(e.type == SDL_MOUSEMOTION && special_mode == SPECIALMODE_NORMAL) {
			platform::queue(keypress(modifier_set(), mouse_x, e.motion.x - 6));
			platform::queue(keypress(modifier_set(), mouse_y, e.motion.y - 6));
		}
		if(e.type == SDL_MOUSEBUTTONDOWN && special_mode == SPECIALMODE_NORMAL) {
			int i;
			platform::queue(keypress(modifier_set(), mouse_x, e.button.x - 6));
			platform::queue(keypress(modifier_set(), mouse_y, e.button.y - 6));
			switch(e.button.button) {
			case SDL_BUTTON_LEFT:
				platform::queue(keypress(modifier_set(), mouse_l, 1));
				break;
			case SDL_BUTTON_MIDDLE:
				platform::queue(keypress(modifier_set(), mouse_m, 1));
				break;
			case SDL_BUTTON_RIGHT:
				platform::queue(keypress(modifier_set(), mouse_r, 1));
				break;
			};
		}
		if(e.type == SDL_MOUSEBUTTONUP && special_mode == SPECIALMODE_NORMAL) {
			platform::queue(keypress(modifier_set(), mouse_x, e.button.x - 6));
			platform::queue(keypress(modifier_set(), mouse_y, e.button.y - 6));
			switch(e.button.button) {
			case SDL_BUTTON_LEFT:
				platform::queue(keypress(modifier_set(), mouse_l, 0));
				break;
			case SDL_BUTTON_MIDDLE:
				platform::queue(keypress(modifier_set(), mouse_m, 0));
				break;
			case SDL_BUTTON_RIGHT:
				platform::queue(keypress(modifier_set(), mouse_r, 0));
				break;
			};
		}
		//Handle entering identify mode.
		if(identify_requested) {
			identify_requested = false;
			special_mode = SPECIALMODE_IDENTIFY;
			ui_grab_keys(true);
			platform::message("Press key to identify...");
		}
		//Handle entering modal dialog.
		if(!modal_dialog_was_active && modal_dialog_active) {
			screenmod->set_modal(modal_dialog_text, modal_dialog_confirm);
			special_mode = SPECIALMODE_MODAL;
			modal_dialog_was_active = true;
			ui_grab_keys_special();
		}
		//Handle special modes.
		switch(special_mode) {
		case SPECIALMODE_NORMAL:
			//Enable command line if needed.
			if(iskbd && kbdkey == SDLK_ESCAPE) {
				if(!cmdline.enabled() && !polarity) {
					cmdline.enable();
					commandline_updated = true;
					special_mode = SPECIALMODE_COMMAND;
					platform::set_modal_pause(true);
					ui_grab_keys(false);
					//std::cerr << "Entered commandline mode." << std::endl;
				}
				continue;
			}
			break;
		case SPECIALMODE_COMMAND:
			if(timer_triggered || (e.type == SDL_USEREVENT && e.user.code == USERCODE_TIMER)) {
				cmdline.tick();
				timer_triggered = false;
			}
			cmd = cmdline.key(get_command_edit_operation(e, true));
			if(cmd != "") {
				//std::cerr << "To execute: '" << cmd << "'" << std::endl;
				platform::queue(cmd);
			}
			commandline_updated = true;
			if(!cmdline.enabled()) {
				//std::cerr << "Exited commandline mode." << std::endl;
				//Exiting commandline mode.
				special_mode = SPECIALMODE_NORMAL;
				platform::set_modal_pause(false);
				ui_ungrab_keys();
			}
			break;
		case SPECIALMODE_IDENTIFY:
			if(timer_triggered || (e.type == SDL_USEREVENT && e.user.code == USERCODE_TIMER)) {
				ui_handle_identify();
				timer_triggered = false;
			}
			break;
		case SPECIALMODE_MODAL:
			if((iskbd && !polarity && kbdkey == SDLK_ESCAPE) || e.type == SDL_QUIT) {
				//Negative response.
				modal_dialog_confirm = false;
				modal_dialog_active = false;
				screenmod->clear_modal();
				special_mode = SPECIALMODE_NORMAL;
				//We NAK the command in case modal dialog was somehow entered with command active.
				cmdline.key(SPECIAL_NAK);
				ui_ungrab_keys(true);
				platform::set_modal_pause(false);
				modal_dialog_was_active = false;
				mutex::holder h(*ui_mutex);
				ui_condition->signal();
			}
			if(iskbd && !polarity && (kbdkey == SDLK_RETURN || kbdkey == SDLK_KP_ENTER)) {
				//Positive response.
				modal_dialog_active = false;
				screenmod->clear_modal();
				modal_dialog_was_active = false;
				special_mode = SPECIALMODE_NORMAL;
				//We NAK the command in case modal dialog was somehow entered with command active.
				cmdline.key(SPECIAL_NAK);
				platform::set_modal_pause(false);
				ui_ungrab_keys(true);
				mutex::holder h(*ui_mutex);
				ui_condition->signal();
			}
			break;
		}
		timer_triggered = false;
		if(e.type == SDL_QUIT)
			ui_handle_quit_signal();
		//Yes, normal key handling is done even if in commandline or other modal mode.
		keypress k;
		if(translate_sdl_key(e, k)) {
			platform::queue(k);
		}
		if(translate_sdl_joystick(e, k)) {
			platform::queue(k);
		}
		//Handle repaints.
		bool status = false;
		bool screen = false;
		bool pmessages = false;
		bool new_fsc = false;
		bool toggle_fsc = false;
		{
			mutex::holder h(*ui_mutex);
			pmessages = messages_dirty;
			status = status_dirty;
			screen = screen_dirty;
			new_fsc = fullscreen_console;
			messages_dirty = false;
			screen_dirty = false;
			status_dirty = false;
			if(new_fsc != fullscreen_console_active)
				toggle_fsc = true;
			else
				toggle_fsc = false;
			repaint_in_flight = false;
		}
		//If screen is dirty (irrespective if full repaint would be done), render the screeen.
		if(screen)
			render_framebuffer();
		bool any = status || pmessages || screen || commandline_updated || toggle_fsc || full;
		if(special_mode == SPECIALMODE_MODAL || full) {
			//FIXME: Use less intensive paint for SPECIALMODE_MODAL.
			screenmod->repaint_full();
			any = true;
		} else {
			if(status) {
				screenmod->repaint_status();
			}
			if(pmessages) {
				screenmod->repaint_messages();
			}
			if(screen) {
				screenmod->repaint_screen();
			}
			if(commandline_updated) {
				screenmod->repaint_commandline();
			}
			if(toggle_fsc) {
				screenmod->set_fullscreen_console(new_fsc);
				fullscreen_console_active = new_fsc;
			}
		}
		if(any)
			screenmod->flip();
	}
}

void graphics_plugin::init() throw()
{
	if(!screenmod)
		screenmod = new screen_model;
	if(!ui_mutex)
		ui_mutex = &mutex::aquire();
	if(!ui_condition)
		ui_condition = &condition::aquire(*ui_mutex);
	screenmod->set_command_line(&cmdline);
	arm_sigalrm();
	ui_thread = &thread_id::me();
	init_sdl_keys();
	if(!sdl_init) {
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER);
		SDL_EnableUNICODE(true);
		sdl_init = true;
		timer_id = SDL_AddTimer(30, timer_cb, NULL);
	}
	//Doing full repaint will open the window.
	screenmod->repaint_full();
	std::string windowname = "lsnes rr" + lsnes_version + "[" + emucore_get_version() + "]";
	SDL_WM_SetCaption(windowname.c_str(), "lsnes");
}

void graphics_plugin::quit() throw()
{
	if(sdl_init) {
		SDL_Quit();
		sdl_init = false;
		SDL_RemoveTimer(timer_id);
	}
	deinit_sdl_keys();
}

void graphics_plugin::notify_message() throw()
{
	messages_dirty = true;
	wake_ui();
}

void graphics_plugin::notify_status() throw()
{
	status_dirty = true;
	wake_ui();
}

void graphics_plugin::notify_screen() throw()
{
	screen_dirty = true;
	wake_ui();
}

bool graphics_plugin::modal_message(const std::string& text, bool confirm) throw()
{
	bool answer = false;
	try {
		//Make the UI thread do the prompting.
		mutex::holder h(*ui_mutex);
		modal_dialog_active = true;
		modal_dialog_text = text;
		modal_dialog_confirm = confirm;
		while(modal_dialog_active)
			ui_condition->wait(100000);
		answer = modal_dialog_confirm;
	} catch(std::bad_alloc& e) {
		OOM_panic();
	}
	return answer;
}

void graphics_plugin::fatal_error() throw()
{
	//Fun... This can be called from any thread.
	if(ui_thread->is_me()) {
		ui_panic();
	} else {
		paniced = true;
		wake_ui();
		//Busywait as program state may be very unpredictable.
		while(!panic_ack);
	}
}

const char* graphics_plugin::name = "SDL graphics plugin";
