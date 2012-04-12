#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "core/window.hpp"
#include "library/joyfun.hpp"
#include "library/string.hpp"

#include <unistd.h>
#include <map>
#include <dirent.h>
#include <set>
#include <string>
#include <cstring>
#include <cctype>
#include <sstream>
#include <cerrno>
#include <fcntl.h>
#include <cstdint>
extern "C"
{
#include <linux/input.h>
}

extern void evdev_init_buttons(const char** x);
extern void evdev_init_axes(const char** x);

namespace
{
	const char* axisnames[ABS_MAX + 1] = {0};
	const char* buttonnames[KEY_MAX + 1] = {0};
	std::map<int, joystick_model> joysticks;
	std::map<int, unsigned> joystick_nums;
	std::map<std::pair<int, unsigned>, keygroup*> axes;
	std::map<std::pair<int, unsigned>, keygroup*> buttons;
	std::map<std::pair<int, unsigned>, keygroup*> hats;
	unsigned joystick_count = 0;

	std::string get_button_name(uint16_t code)
	{
		if(code <= KEY_MAX && buttonnames[code])
			return buttonnames[code];
		else {
			std::ostringstream str;
			str << "Unknown button #" << code << std::endl;
			return str.str();
		}
	}

	std::string get_axis_name(uint16_t code)
	{
		if(code <= ABS_MAX && axisnames[code])
			return axisnames[code];
		else {
			std::ostringstream str;
			str << "Unknown axis #" << code << std::endl;
			return str.str();
		}
	}

	void create_button(int fd, unsigned jnum, uint16_t code)
	{
		unsigned n = joysticks[fd].new_button(code, get_button_name(code));
		std::string name = (stringfmt() << "joystick" << jnum << "button" << n).str();
		buttons[std::make_pair(fd, n)] = new keygroup(name, "joystick", keygroup::KT_KEY);
	}

	void create_axis(int fd, unsigned jnum, uint16_t code, int32_t min, int32_t max)
	{
		unsigned n = joysticks[fd].new_axis(code, min, max, get_axis_name(code));
		std::string name = (stringfmt() << "joystick" << jnum << "axis" << n).str();
		if(min < 0)
			axes[std::make_pair(fd, n)] = new keygroup(name, "joystick", keygroup::KT_AXIS_PAIR);
		else
			axes[std::make_pair(fd, n)] = new keygroup(name, "joystick", keygroup::KT_PRESSURE_MP);
	}

	void create_hat(int fd, unsigned jnum, uint16_t codeX, uint16_t codeY)
	{
		unsigned n = joysticks[fd].new_hat(codeX, codeY, 1, get_axis_name(codeX), get_axis_name(codeY));
		std::string name = (stringfmt() << "joystick" << jnum << "hat" << n).str();
		hats[std::make_pair(fd, n)] = new keygroup(name, "joystick", keygroup::KT_HAT);
	}

	bool read_one_input_event(int fd)
	{
		short x;
		joystick_model& m = joysticks[fd];
		struct input_event ev;
		int r = read(fd, &ev, sizeof(ev));
		if(r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
			return false;
		if(r < 0) {
			messages << "Error reading from joystick (fd=" << fd << "): " << strerror(errno)
				<< std::endl;
			return false;
		}
		if(ev.type == EV_KEY)
			m.report_button(ev.code, ev.value != 0);
		if(ev.type == EV_ABS)
			m.report_axis(ev.code, ev.value);
		return true;
	}

	bool probe_joystick(int fd, const std::string& filename)
	{
		const size_t div = 8 * sizeof(unsigned long);
		unsigned long keys[(KEY_MAX + div) / div] = {0};
		unsigned long axes[(ABS_MAX + div) / div] = {0};
		unsigned long evtypes[(EV_MAX + div) / div] = {0};
		char namebuffer[256];
		unsigned button_count = 0;
		unsigned axis_count = 0;
		unsigned hat_count = 0;
		if(ioctl(fd, EVIOCGBIT(0, sizeof(evtypes)), evtypes) < 0) {
			int merrno = errno;
			messages << "Error probing joystick (evmap; " << filename << "): " << strerror(merrno)
				<< std::endl;
			return false;
		}
		if(!(evtypes[EV_KEY / div] & (1 << EV_KEY % div)) || !(evtypes[EV_ABS / div] & (1 << EV_ABS % div))) {
			messages << "Input (" << filename << ") doesn't look like joystick" << std::endl;
			return false;
		}
		if(ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) {
			int merrno = errno;
			messages << "Error probing joystick (keymap; " <<filename << "): " << strerror(merrno)
				<< std::endl;
			return false;
		}
		if(ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) < 0) {
			int merrno = errno;
			messages << "Error probing joystick (axismap; " << filename << "): " << strerror(merrno)
				<< std::endl;
			return false;
		}
		if(ioctl(fd, EVIOCGNAME(sizeof(namebuffer)), namebuffer) <= 0) {
			int merrno = errno;
			messages << "Error probing joystick (name; " << filename << "): " << strerror(merrno)
				<< std::endl;
			return false;
		}
		joysticks[fd].name(namebuffer);
		unsigned jnum = joystick_count++;
		joystick_nums[fd] = jnum;
		
		for(unsigned i = 0; i <= KEY_MAX; i++) {
			if(keys[i / div] & (1ULL << (i % div))) {
				create_button(fd, jnum, i);
				button_count++;
			}
		}
		for(unsigned i = 0; i <= ABS_MAX; i++) {
			if(axes[i / div] & (1ULL << (i % div))) {
				if(i < ABS_HAT0X || i > ABS_HAT3Y) {
					int32_t V[5];
					if(ioctl(fd, EVIOCGABS(i), V) < 0) {
						int merrno = errno;
						messages << "Error getting parameters for axis " << i << " (fd="
							<< fd << "): " << strerror(merrno) << std::endl;
						continue;
					}
					create_axis(fd, jnum, i, V[1], V[2]);
					axis_count++;
				} else if(i % 2 == 0) {
					create_hat(fd, jnum, i, i + 1);
					hat_count++;
				}
			}
		}
		messages << "Found '" << namebuffer << "' (" << button_count << " buttons, " << axis_count
			<< " axes, " << hat_count << " hats)" << std::endl;
		return true;
	}

	void probe_all_joysticks()
	{
		DIR* d = opendir("/dev/input");
		struct dirent* dentry;
		if(!d) {
			int merrno = errno;
			messages << "Can't list /dev/input: " << strerror(merrno) << std::endl;
			return;
		}
		while((dentry = readdir(d)) != NULL) {
			if(strlen(dentry->d_name) < 6)
				continue;
			if(strncmp(dentry->d_name, "event", 5))
				continue;
			for(size_t i = 5; dentry->d_name[i]; i++)
				if(!isdigit(static_cast<uint8_t>(dentry->d_name[i])))
					continue;
			std::string filename = std::string("/dev/input/") + dentry->d_name;
			int r = open(filename.c_str(), O_RDONLY | O_NONBLOCK);
			if(r < 0)
				continue;
			if(!probe_joystick(r, filename))
				close(r);
		}
		closedir(d);
	}

	function_ptr_command<> show_joysticks("show-joysticks", "Show joysticks",
		"Syntax: show-joysticks\nShow joystick data.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : joystick_nums)
				messages << joysticks[i.first].compose_report(i.second) << std::endl;
		});

	volatile bool quit_signaled = false;
	volatile bool quit_ack = false;
}

void joystick_plugin::init() throw()
{
	evdev_init_buttons(buttonnames);
	evdev_init_axes(axisnames);
	probe_all_joysticks();
	quit_ack = quit_signaled = false;
}

void joystick_plugin::quit() throw()
{
	quit_signaled = true;
	while(!quit_ack);
	for(auto i : joysticks)	close(i.first);
	for(auto i : axes)	delete i.second;
	for(auto i : buttons)	delete i.second;
	for(auto i : hats)	delete i.second;
	joysticks.clear();
	joystick_nums.clear();
	axes.clear();
	buttons.clear();
	hats.clear();
}

#define POLL_WAIT 20000

void joystick_plugin::thread_fn() throw()
{
	while(!quit_signaled) {
		for(auto fd : joysticks)
			while(read_one_input_event(fd.first));
		short x;
		for(auto i : buttons)
			if(joysticks[i.first.first].button(i.first.second, x))
				platform::queue(keypress(modifier_set(), *i.second, x));
		for(auto i : axes)
			if(joysticks[i.first.first].axis(i.first.second, x))
				platform::queue(keypress(modifier_set(), *i.second, x));
		for(auto i : hats)
			if(joysticks[i.first.first].hat(i.first.second, x))
				platform::queue(keypress(modifier_set(), *i.second, x));
		usleep(POLL_WAIT);
	}
	quit_ack = true;
}

void joystick_plugin::signal() throw()
{
	quit_signaled = true;
}

const char* joystick_plugin::name = "Evdev joystick plugin";
