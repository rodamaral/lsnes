#include "cmdhelp/sound.hpp"
#include "core/audioapi.hpp"
#include "core/audioapi-driver.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/joystickapi.hpp"
#include "core/keymapper.hpp"
#include "core/messages.hpp"
#include "core/queue.hpp"
#include "core/random.hpp"
#include "core/window.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/threads.hpp"
#include "lua/lua.hpp"

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

volatile bool platform::do_exit_dummy_event_loop = false;

namespace
{
	command::fnptr<> identify_key(lsnes_cmds, CSOUND::showdrv,
		[]() throw(std::bad_alloc, std::runtime_error) {
			messages << "Graphics:\t" << graphics_driver_name() << std::endl;
			messages << "Sound:\t" << audioapi_driver_name() << std::endl;
			messages << "Joystick:\t" << joystick_driver_name() << std::endl;
		});

	command::fnptr<const std::string&> enable_sound(lsnes_cmds, CSOUND::enable,
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(args == "toggle") {
				if(!audioapi_driver_initialized())
					throw std::runtime_error("Sound failed to initialize and is disabled");
				platform::sound_enable(!platform::is_sound_enabled());
				return;
			}
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

	bool fuzzy_search_list(std::map<std::string, std::string> map, std::string& term)
	{
		if(term.length() > 0 && term[0] == '!') {
			std::map<std::string, std::string> rmap;
			for(auto i : map) rmap[i.second] = i.first;
			auto rterm = term.substr(1);
			for(auto i : rmap) {
				if(i.first.length() > rterm.length())
					continue;
				if(i.first.substr(0, rterm.length()) == rterm) {
					term = i.second;
					return true;
				}
			}
			return false;
		} else
			return true;
	}

	command::fnptr<const std::string&> change_playback_dev(lsnes_cmds, CSOUND::chpdev,
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			auto old_rec = audioapi_driver_get_device(true);
			auto args2 = args;
			if(!fuzzy_search_list(audioapi_driver_get_devices(false), args2)) {
				messages << "No sound device matching '" << args2 << "' found." << std::endl;
				return;
			}
			platform::set_sound_device(args2, old_rec);
		});

	command::fnptr<const std::string&> change_record_dev(lsnes_cmds, CSOUND::chrdev,
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			auto old_play = audioapi_driver_get_device(false);
			auto args2 = args;
			if(!fuzzy_search_list(audioapi_driver_get_devices(true), args2)) {
				messages << "No sound device matching '" << args2 << "' found." << std::endl;
				return;
			}
			platform::set_sound_device(old_play, args2);
		});

	command::fnptr<> show_devices(lsnes_cmds, CSOUND::showdev,
		[]() throw(std::bad_alloc, std::runtime_error) {
			messages << "Known playback devices:" << std::endl;
			auto pdevs = audioapi_driver_get_devices(false);
			auto cpdev = audioapi_driver_get_device(false);
			for(auto i : pdevs) {
				char ind = (i.first == cpdev) ? '*' : ' ';
				messages << ind << i.first << " (" << i.second << ")" << std::endl;
			}
			messages << "Known recording devices:" << std::endl;
			auto rdevs = audioapi_driver_get_devices(true);
			auto crdev = audioapi_driver_get_device(true);
			for(auto i : rdevs) {
				char ind = (i.first == crdev) ? '*' : ' ';
				messages << ind << i.first << " (" << i.second << ")" << std::endl;
			}
		});

	command::fnptr<> reset_audio(lsnes_cmds, CSOUND::reset,
		[]() throw(std::bad_alloc, std::runtime_error) {
			//Save the old devices. We save descriptions if possible, since handles change.
			auto cpdev = platform::get_sound_device_description(false);
			auto crdev = platform::get_sound_device_description(true);
			//Restart the system.
			audioapi_driver_quit();
			audioapi_driver_init();
			//Defaults.
			if(cpdev == "") cpdev = platform::get_sound_device_description(false);
			if(crdev == "") crdev = platform::get_sound_device_description(true);
			//Try to change device back.
			platform::set_sound_device_by_description(cpdev, crdev);
		});

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

void platform::sound_enable(bool enable) throw()
{
	audioapi_driver_enable(enable);
	sounds_enabled = enable;
	CORE().dispatch->sound_unmute(enable);
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
	CORE().dispatch->sound_change(std::make_pair(audioapi_driver_get_device(true),
		audioapi_driver_get_device(false)));
}

void platform::set_sound_device_by_description(const std::string& pdev, const std::string& rdev) throw()
{
	std::string old_play = audioapi_driver_get_device(false);
	std::string old_rec = audioapi_driver_get_device(true);
	auto pdevs = audioapi_driver_get_devices(false);
	auto rdevs = audioapi_driver_get_devices(true);
	for(auto i : pdevs)
		if(i.second == pdev) {
			old_play = i.first;
			break;
		}
	for(auto i : rdevs)
		if(i.second == rdev) {
			old_rec = i.first;
			break;
		}
	set_sound_device(old_play, old_rec);
}

std::string platform::get_sound_device_description(bool rec) throw(std::bad_alloc)
{
	auto dev = audioapi_driver_get_device(rec);
	auto devs = audioapi_driver_get_devices(rec);
	if(devs.count(dev))
		return devs[dev];
	else
		return "";
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
	lsnes_instance.audio->init();
	audioapi_driver_init();
	joystick_driver_init();
}

void platform::quit()
{
	joystick_driver_quit();
	audioapi_driver_quit();
	lsnes_instance.audio->quit();
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
	threads::alock h(msgbuf_lock());
	for(auto& forlog : token_iterator<char>::foreach(msg, {"\n"})) {
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
	audioapi_instance::disable_vu_updates();	//Don't call update VU, as that crashes.
	exit(1);
}

namespace
{
	volatile bool normal_pause;
	volatile bool modal_pause;
	volatile uint64_t continue_time;


	uint64_t on_idle_time;
	uint64_t on_timer_time;
	void reload_lua_timers()
	{
		auto& core = CORE();
		on_idle_time = core.lua2->timed_hook(LUA_TIMED_HOOK_IDLE);
		on_timer_time = core.lua2->timed_hook(LUA_TIMED_HOOK_TIMER);
		core.iqueue->queue_function_run = false;
	}
}

#define MAXWAIT 100000ULL

void platform::dummy_event_loop() throw()
{
	auto& core = CORE();
	while(!do_exit_dummy_event_loop) {
		threads::alock h(core.iqueue->queue_lock);
		core.iqueue->run_queue(true);
		threads::cv_timed_wait(core.iqueue->queue_condition, h, threads::ustime(MAXWAIT));
		random_mix_timing_entropy();
	}
}

void platform::exit_dummy_event_loop() throw()
{
	auto& core = CORE();
	do_exit_dummy_event_loop = true;
	threads::alock h(core.iqueue->queue_lock);
	core.iqueue->queue_condition.notify_all();
	usleep(200000);
}

void platform::flush_command_queue() throw()
{
	auto& core = CORE();
	reload_lua_timers();
	core.iqueue->queue_function_run = false;
	if(modal_pause || normal_pause)
		core.framerate->freeze_time(framerate_regulator::get_utime());
	bool run_idle = false;
	while(true) {
		uint64_t now = framerate_regulator::get_utime();
		if(now >= on_timer_time) {
			core.lua2->callback_do_timer();
			reload_lua_timers();
		}
		if(run_idle) {
			core.lua2->callback_do_idle();
			reload_lua_timers();
			run_idle = false;
		}
		threads::alock h(core.iqueue->queue_lock);
		core.iqueue->run_queue(true);
		if(!pausing_allowed)
			break;
		if(core.iqueue->queue_function_run)
			reload_lua_timers();
		now = framerate_regulator::get_utime();
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
				threads::cv_timed_wait(core.iqueue->queue_condition, h, threads::ustime(waitleft));
				random_mix_timing_entropy();
			}
		} else
			break;
		//If we had to wait, check queues at least once more.
	}
	if(!modal_pause && !normal_pause)
		core.framerate->unfreeze_time(framerate_regulator::get_utime());
}

void platform::set_paused(bool enable) throw()
{
	normal_pause = enable;
}

void platform::wait(uint64_t usec) throw()
{
	auto& core = CORE();
	reload_lua_timers();
	continue_time = framerate_regulator::get_utime() + usec;
	bool run_idle = false;
	while(true) {
		uint64_t now = framerate_regulator::get_utime();
		if(now >= on_timer_time) {
			core.lua2->callback_do_timer();
			reload_lua_timers();
		}
		if(run_idle) {
			core.lua2->callback_do_idle();
			run_idle = false;
			reload_lua_timers();
		}
		threads::alock h(core.iqueue->queue_lock);
		core.iqueue->run_queue(true);
		if(core.iqueue->queue_function_run)
			reload_lua_timers();
		//If usec is 0, never wait (waitleft can be nonzero if time counting screws up).
		if(!usec)
			return;
		now = framerate_regulator::get_utime();
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
				threads::cv_timed_wait(core.iqueue->queue_condition, h, threads::ustime(waitleft));
				random_mix_timing_entropy();
			}
		} else
			return;
	}
}

void platform::cancel_wait() throw()
{
	auto& core = CORE();
	continue_time = 0;
	threads::alock h(core.iqueue->queue_lock);
	core.iqueue->queue_condition.notify_all();
}

void platform::set_modal_pause(bool enable) throw()
{
	modal_pause = enable;
}

void platform::run_queues() throw()
{
	CORE().iqueue->run_queue(false);
}

namespace
{
	threads::lock _msgbuf_lock;
}

threads::lock& platform::msgbuf_lock() throw()
{
	return _msgbuf_lock;
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
