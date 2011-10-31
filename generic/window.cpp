#include "window.hpp"
#include "command.hpp"
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

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

	window_callback* wcb = NULL;
}

std::map<std::string, std::string>& window::get_emustatus() throw()
{
	return emustatus;
}

void window::init()
{
	graphics_init();
	sound_init();
	joystick_init();
}

void window::quit()
{
	joystick_quit();
	sound_quit();
	graphics_quit();
}

std::ostream& window::out() throw(std::bad_alloc)
{
	static std::ostream* cached = NULL;
	window* win = NULL;
	if(!cached)
		cached = new boost::iostreams::stream<window_output>(win);
	return *cached;
}

window_callback::~window_callback() throw()
{
}

void window_callback::on_close() throw()
{
}

void window_callback::on_click(int32_t x, int32_t y, uint32_t buttonmask) throw()
{
}

void window_callback::do_close() throw()
{
	if(wcb)
		wcb->on_close();
}

void window_callback::do_click(int32_t x, int32_t y, uint32_t buttonmask) throw()
{
	if(wcb)
		wcb->on_click(x, y, buttonmask);
}

void window_callback::set_callback_handler(window_callback& cb) throw()
{
	wcb = &cb;
}
