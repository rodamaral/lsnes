#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"
#include "core/render.hpp"
#include "core/window.hpp"

#include <fstream>
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
	function_ptr_command<> identify_key("show-plugins", "Show plugins in use",
		"Syntax: show-plugins\nShows plugins in use.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::message(std::string("Graphics:\t") + graphics_plugin_name);
			window::message(std::string("Sound:\t") + sound_plugin_name);
			window::message(std::string("Joystick:\t") + joystick_plugin_name);
		});

	function_ptr_command<const std::string&> enable_sound("enable-sound", "Enable/Disable sound",
		"Syntax: enable-sound <on/off>\nEnable or disable sound.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			std::string s = args;
			if(s == "on" || s == "true" || s == "1" || s == "enable" || s == "enabled") {
				if(!window::sound_initialized())
					throw std::runtime_error("Sound failed to initialize and is disabled");
				window::sound_enable(true);
			} else if(s == "off" || s == "false" || s == "0" || s == "disable" || s == "disabled") {
				if(window::sound_initialized())
					window::sound_enable(false);
			} else
				throw std::runtime_error("Bad sound setting");
		});

	function_ptr_command<const std::string&> set_sound_device("set-sound-device", "Set sound device",
		"Syntax: set-sound-device <id>\nSet sound device to <id>.\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			if(!window::sound_initialized())
				throw std::runtime_error("Sound failed to initialize and is disabled");
			window::set_sound_device(args);
		});

	function_ptr_command<> get_sound_devices("show-sound-devices", "Show sound devices",
		"Syntax: show-sound-devices\nShow listing of available sound devices\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(!window::sound_initialized())
				throw std::runtime_error("Sound failed to initialize and is disabled");
			auto r = window::get_sound_devices();
				auto s = window::get_current_sound_device();
				std::string dname = "unknown";
				if(r.count(s))
					dname = r[s];
			window::out() << "Detected " << r.size() << " sound output devices."
				<< std::endl;
			for(auto i : r)
				window::out() << "Audio device " << i.first << ": " << i.second << std::endl;
			window::out() << "Currently using device " << window::get_current_sound_device() << " ("
				<< dname << ")" << std::endl;
		});

	function_ptr_command<> get_sound_status("show-sound-status", "Show sound status",
		"Syntax: show-sound-status\nShow current sound status\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			window::out() << "Sound plugin: " << sound_plugin_name << std::endl;
			if(!window::sound_initialized())
				window::out() << "Sound initialization failed, sound disabled" << std::endl;
			else {
				auto r = window::get_sound_devices();
				auto s = window::get_current_sound_device();
				std::string dname = "unknown";
				if(r.count(s))
					dname = r[s];
				window::out() << "Current sound device " << s << " (" << dname << ")" << std::endl;
			}
		});

	std::map<std::string, std::string> emustatus;

	class window_output
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;
		window_output(window* win)
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
				window::message(foo);
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
			window::notify_message();
		}
	} msg_callback_obj;

	std::ofstream system_log;
	bool sounds_enabled = true;
}

std::map<std::string, std::string>& window::get_emustatus() throw()
{
	return emustatus;
}

void window::sound_enable(bool enable) throw()
{
	_sound_enable(enable);
	sounds_enabled = enable;
	information_dispatch::do_sound_unmute(enable);
}

void window::set_sound_device(const std::string& dev) throw()
{
	try {
		_set_sound_device(dev);
	} catch(std::exception& e) {
		out() << "Error changing sound device: " << e.what() << std::endl;
	}
	//After failed change, we don't know what is selected.
	information_dispatch::do_sound_change(get_current_sound_device());
}

bool window::is_sound_enabled() throw()
{
	return sounds_enabled;
}


void window::init()
{
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
	graphics_init();
	sound_init();
	joystick_init();
}

void window::quit()
{
	joystick_quit();
	sound_quit();
	graphics_quit();
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

std::ostream& window::out() throw(std::bad_alloc)
{
	static std::ostream* cached = NULL;
	window* win = NULL;
	if(!cached)
		cached = new boost::iostreams::stream<window_output>(win);
	return *cached;
}

messagebuffer window::msgbuf(MAXMESSAGES, INIT_WIN_SIZE);


void window::message(const std::string& msg) throw(std::bad_alloc)
{
	std::string msg2 = msg;
	while(msg2 != "") {
		size_t s = msg2.find_first_of("\n");
		std::string forlog;
		if(s >= msg2.length()) {
			msgbuf.add_message(forlog = msg2);
			if(system_log)
				system_log << forlog << std::endl;
			break;
		} else {
			msgbuf.add_message(forlog = msg2.substr(0, s));
			if(system_log)
				system_log << forlog << std::endl;
			msg2 = msg2.substr(s + 1);
		}
	}
}

void window::fatal_error() throw()
{
	time_t curtime = time(NULL);
	struct tm* tm = localtime(&curtime);
	char buffer[1024];
	strftime(buffer, 1023, "%Y-%m-%d %H:%M:%S %Z", tm);
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log << "lsnes paniced at " << buffer << std::endl;
	system_log << "-----------------------------------------------------------------------" << std::endl;
	system_log.close();
	fatal_error2();
	exit(1);
}
