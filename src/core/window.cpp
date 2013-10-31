#include "core/audioapi.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/joystickapi.hpp"
#include "lua/lua.hpp"
#include "core/misc.hpp"
#include "core/window.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "library/threadtypes.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <deque>
#include <sys/time.h>
#include <unistd.h>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#define MAXMESSAGES 5000
#define INIT_WIN_SIZE 6

namespace
{
	volatile bool _system_thread_available = false;
}

keypress::keypress()
{
	key1 = NULL;
	key2 = NULL;
	value = 0;
}

keypress::keypress(keyboard_modifier_set mod, keyboard_key& _key, short _value)
{
	modifiers = mod;
	key1 = &_key;
	key2 = NULL;
	value = _value;
}

keypress::keypress(keyboard_modifier_set mod, keyboard_key& _key, keyboard_key& _key2, short _value)
{
	modifiers = mod;
	key1 = &_key;
	key2 = &_key2;
	value = _value;
}

volatile bool platform::do_exit_dummy_event_loop = false;

namespace
{
	bool queue_function_run = false;

	function_ptr_command<> identify_key(lsnes_cmd, "show-plugins", "Show plugins in use",
		"Syntax: show-plugins\nShows plugins in use.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			messages << "Graphics:\t" << graphics_driver_name() << std::endl;
			messages << "Sound:\t" << audioapi_driver_name() << std::endl;
			messages << "Joystick:\t" << joystick_driver_name() << std::endl;
		});

	function_ptr_command<const std::string&> enable_sound(lsnes_cmd, "enable-sound", "Enable/Disable sound",
		"Syntax: enable-sound <on/off>\nEnable or disable sound.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			switch(string_to_bool(args)) {
			case 1:
				if(!audioapi_driver_initialized())
					throw std::runtime_error("Sound failed to initialize and is disabled");
				platform::sound_enable(true);
				break;
			case 0:
				if(audioapi_driver_initialized())
					platform::sound_enable(false);
				break;
			default:
				throw std::runtime_error("Bad sound setting");
			}
		});

	inverse_bind ienable_sound(lsnes_mapper, "enable-sound on", "Sound‣Enable");
	inverse_bind idisable_sound(lsnes_mapper, "enable-sound off", "Sound‣Disable");

	emulator_status emustatus;

	class window_output
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;
		window_output(int* dummy)
		{
		}

		void close()
		{
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			size_t oldsize = stream.size();
			stream.resize(oldsize + n);
			memcpy(&stream[oldsize], s, n);
			while(true) {
				size_t lf = stream.size();
				for(size_t i = 0; i < stream.size(); i++)
					if(stream[i] == '\n') {
						lf = i;
						break;
					}
				if(lf == stream.size())
					break;
				std::string foo(stream.begin(), stream.begin() + lf);
				platform::message(foo);
				if(lf + 1 < stream.size())
					memmove(&stream[0], &stream[lf + 1], stream.size() - lf - 1);
				stream.resize(stream.size() - lf - 1);
			}
			return n;
		}
	protected:
		std::vector<char> stream;
	};

	class msgcallback : public messagebuffer::update_handler
	{
	public:
		~msgcallback() throw() {};
		void messagebuffer_update() throw(std::bad_alloc, std::runtime_error)
		{
			platform::notify_message();
		}
	} msg_callback_obj;

	std::ofstream system_log;
	bool sounds_enabled = true;
}

emulator_status& platform::get_emustatus() throw()
{
	return emustatus;
}

void platform::sound_enable(bool enable) throw()
{
	audioapi_driver_enable(enable);
	sounds_enabled = enable;
	notify_sound_unmute(enable);
}

void platform::set_sound_device(const std::string& pdev, const std::string& rdev) throw()
{
	std::string old_play = audioapi_driver_get_device(false);
	std::string old_rec = audioapi_driver_get_device(true);
	try {
		audioapi_driver_set_device(pdev, rdev);
	} catch(std::exception& e) {
		out() << "Error changing sound device: " << e.what() << std::endl;
		//Try to restore the device.
		try {
			audioapi_driver_set_device(old_play, old_rec);
		} catch(...) {
		}
	}
	//After failed change, we don't know what is selected.
	notify_sound_change(std::make_pair(audioapi_driver_get_device(true), audioapi_driver_get_device(false)));
}

bool platform::is_sound_enabled() throw()
{
	return sounds_enabled;
}


void platform::init()
{
	do_exit_dummy_event_loop = false;
	msgbuf.register_handler(msg_callback_obj);
	system_log.open("lsnes.log", std::ios_base::out | std::ios_base::app);
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes started at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	do_init_font();
	graphics_driver_init();
	audioapi_init();
	audioapi_driver_init();
	joystick_driver_init();
}

void platform::quit()
{
	joystick_driver_quit();
	audioapi_driver_quit();
	audioapi_quit();
	graphics_driver_quit();
	msgbuf.unregister_handler(msg_callback_obj);
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes shutting down at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log.close();
}

std::ostream& platform::out() throw(std::bad_alloc)
{
	static std::ostream* cached = NULL;
	int dummy;
	if(!cached)
		cached = new boost::iostreams::stream<window_output>(&dummy);
	return *cached;
}

messagebuffer platform::msgbuf(MAXMESSAGES, INIT_WIN_SIZE);


void platform::message(const std::string& msg) throw(std::bad_alloc)
{
	umutex_class h(msgbuf_lock());
	std::string msg2 = msg;
	while(msg2 != "") {
		std::string forlog;
		extract_token(msg2, forlog, "\n");
		msgbuf.add_message(forlog);
		if(system_log)
			system_log << forlog << std::endl;
	}
}

void platform::fatal_error() throw()
{
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes paniced at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log.close();
	graphics_driver_fatal_error();
	exit(1);
}

namespace
{
	mutex_class queue_lock;
	cv_class queue_condition;
	std::deque<keypress> keypresses;
	std::deque<std::string> commands;
	std::deque<std::pair<void(*)(void*), void*>> functions;
	volatile bool normal_pause;
	volatile bool modal_pause;
	volatile uint64_t continue_time;
	volatile uint64_t next_function;
	volatile uint64_t functions_executed;

	void init_threading()
	{
	}

	void internal_run_queues(bool unlocked) throw()
	{
		init_threading();
		if(!unlocked)
			queue_lock.lock();
		try {
			//Flush keypresses.
			while(!keypresses.empty()) {
				keypress k = keypresses.front();
				keypresses.pop_front();
				queue_lock.unlock();
				if(k.key1)
					k.key1->set_state(k.modifiers, k.value);
				if(k.key2)
					k.key2->set_state(k.modifiers, k.value);
				queue_lock.lock();
				queue_function_run = true;
			}
			//Flush commands.
			while(!commands.empty()) {
				std::string c = commands.front();
				commands.pop_front();
				queue_lock.unlock();
				lsnes_cmd.invoke(c);
				queue_lock.lock();
				queue_function_run = true;
			}
			//Flush functions.
			while(!functions.empty()) {
				std::pair<void(*)(void*), void*> f = functions.front();
				functions.pop_front();
				queue_lock.unlock();
				f.first(f.second);
				queue_lock.lock();
				++functions_executed;
				queue_function_run = true;
			}
			queue_condition.notify_all();
		} catch(std::bad_alloc& e) {
			OOM_panic();
		} catch(std::exception& e) {
			std::cerr << "Fault inside platform::run_queues(): " << e.what() << std::endl;
			exit(1);
		}
		if(!unlocked)
			queue_lock.unlock();
	}

	uint64_t on_idle_time;
	uint64_t on_timer_time;
	void reload_lua_timers()
	{
		on_idle_time = lua_timed_hook(LUA_TIMED_HOOK_IDLE);
		on_timer_time = lua_timed_hook(LUA_TIMED_HOOK_TIMER);
		queue_function_run = false;
	}
}

#define MAXWAIT 100000ULL

void platform::dummy_event_loop() throw()
{
	init_threading();
	while(!do_exit_dummy_event_loop) {
		umutex_class h(queue_lock);
		internal_run_queues(true);
		cv_timed_wait(queue_condition, h, microsec_class(MAXWAIT));
		random_mix_timing_entropy();
	}
}

void platform::exit_dummy_event_loop() throw()
{
	init_threading();
	do_exit_dummy_event_loop = true;
	umutex_class h(queue_lock);
	queue_condition.notify_all();
	usleep(200000);
}

void platform::flush_command_queue() throw()
{
	reload_lua_timers();
	queue_function_run = false;
	if(modal_pause || normal_pause)
		freeze_time(get_utime());
	init_threading();
	bool run_idle = false;
	while(true) {
		uint64_t now = get_utime();
		if(now >= on_timer_time) {
			lua_callback_do_timer();
			reload_lua_timers();
		}
		if(run_idle) {
			lua_callback_do_idle();
			reload_lua_timers();
			run_idle = false;
		}
		umutex_class h(queue_lock);
		internal_run_queues(true);
		if(!pausing_allowed)
			break;
		if(queue_function_run)
			reload_lua_timers();
		now = get_utime();
		uint64_t waitleft = 0;
		waitleft = (now < continue_time) ? (continue_time - now) : 0;
		waitleft = (modal_pause || normal_pause) ? MAXWAIT : waitleft;
		waitleft = min(waitleft, static_cast<uint64_t>(MAXWAIT));
		if(waitleft > 0) {
			if(now >= on_idle_time) {
				run_idle = true;
				waitleft = 0;
			}
			if(on_idle_time >= now)
				waitleft = min(waitleft, on_idle_time - now);
			if(on_timer_time >= now)
				waitleft = min(waitleft, on_timer_time - now);
			if(waitleft > 0) {
				cv_timed_wait(queue_condition, h, microsec_class(waitleft));
				random_mix_timing_entropy();
			}
		} else
			break;
		//If we had to wait, check queues at least once more.
	}
	if(!modal_pause && !normal_pause)
		unfreeze_time(get_utime());
}

void platform::set_paused(bool enable) throw()
{
	normal_pause = enable;
}

void platform::wait(uint64_t usec) throw()
{
	reload_lua_timers();
	continue_time = get_utime() + usec;
	init_threading();
	bool run_idle = false;
	while(true) {
		uint64_t now = get_utime();
		if(now >= on_timer_time) {
			lua_callback_do_timer();
			reload_lua_timers();
		}
		if(run_idle) {
			lua_callback_do_idle();
			run_idle = false;
			reload_lua_timers();
		}
		umutex_class h(queue_lock);
		internal_run_queues(true);
		if(queue_function_run)
			reload_lua_timers();
		now = get_utime();
		uint64_t waitleft = 0;
		waitleft = (now < continue_time) ? (continue_time - now) : 0;
		waitleft = min(static_cast<uint64_t>(MAXWAIT), waitleft);
		if(waitleft > 0) {
			if(now >= on_idle_time) {
				run_idle = true;
				waitleft = 0;
			}
			if(on_idle_time >= now)
				waitleft = min(waitleft, on_idle_time - now);
			if(on_timer_time >= now)
				waitleft = min(waitleft, on_timer_time - now);
			if(waitleft > 0) {
				cv_timed_wait(queue_condition, h, microsec_class(waitleft));
				random_mix_timing_entropy();
			}
		} else
			return;
	}
}

void platform::cancel_wait() throw()
{
	init_threading();
	continue_time = 0;
	umutex_class h(queue_lock);
	queue_condition.notify_all();
}

void platform::set_modal_pause(bool enable) throw()
{
	modal_pause = enable;
}

void platform::queue(const keypress& k) throw(std::bad_alloc)
{
	init_threading();
	umutex_class h(queue_lock);
	keypresses.push_back(k);
	queue_condition.notify_all();
}

void platform::queue(const std::string& c) throw(std::bad_alloc)
{
	init_threading();
	umutex_class h(queue_lock);
	commands.push_back(c);
	queue_condition.notify_all();
}

void platform::queue(void (*f)(void* arg), void* arg, bool sync) throw(std::bad_alloc)
{
	if(!_system_thread_available) {
		f(arg);
		return;
	}
	init_threading();
	umutex_class h(queue_lock);
	++next_function;
	functions.push_back(std::make_pair(f, arg));
	queue_condition.notify_all();
	if(sync)
		while(functions_executed < next_function && _system_thread_available) {
			cv_timed_wait(queue_condition, h, microsec_class(10000));
			random_mix_timing_entropy();
		}
}

void platform::run_queues() throw()
{
	internal_run_queues(false);
}

void platform::system_thread_available(bool av) throw()
{
	_system_thread_available = av;
}

namespace
{
	mutex_class _msgbuf_lock;
	framebuffer<false>* our_screen;

	struct painter_listener
	{
		painter_listener()
		{
			screenupdate.set(notify_screen_update, []() { graphics_driver_notify_screen(); });
			statusupdate.set(notify_status_update, []() { graphics_driver_notify_status(); });
			setscreen.set(notify_set_screen, [](framebuffer<false>& scr) { our_screen = &scr; });
		}
	private:
		struct dispatch_target<> screenupdate;
		struct dispatch_target<> statusupdate;
		struct dispatch_target<framebuffer<false>&> setscreen;
	} x;
}

mutex_class& platform::msgbuf_lock() throw()
{
	return _msgbuf_lock;
}

void platform::screen_set_palette(unsigned rshift, unsigned gshift, unsigned bshift) throw()
{
	if(!our_screen)
		return;
	if(our_screen->get_palette_r() == rshift &&
		our_screen->get_palette_g() == gshift &&
		our_screen->get_palette_b() == bshift)
		return;
	our_screen->set_palette(rshift, gshift, bshift);
	graphics_driver_notify_screen();
}

modal_pause_holder::modal_pause_holder()
{
	platform::set_modal_pause(true);
}

modal_pause_holder::~modal_pause_holder()
{
	platform::set_modal_pause(false);
}

bool platform::pausing_allowed = true;
double platform::global_volume = 1.0;
volatile bool queue_synchronous_fn_warning;
