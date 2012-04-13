#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "core/joystick.hpp"
#include "core/window.hpp"
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

	std::string get_button_name(uint16_t code)
	{
		if(code <= KEY_MAX && buttonnames[code])
			return buttonnames[code];
		else
			return (stringfmt() << "Unknown button #" << code).str();
	}

	std::string get_axis_name(uint16_t code)
	{
		if(code <= ABS_MAX && axisnames[code])
			return axisnames[code];
		else
			return (stringfmt() << "Unknown axis #" << code).str();
	}

	bool read_one_input_event(int fd)
	{
		short x;
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
			joystick_report_button(fd, ev.code, ev.value != 0);
		if(ev.type == EV_ABS)
			joystick_report_axis(fd, ev.code, ev.value);
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
		joystick_create(fd, namebuffer);
		for(unsigned i = 0; i <= KEY_MAX; i++) {
			if(keys[i / div] & (1ULL << (i % div))) {
				joystick_new_button(fd, i, get_button_name(i));
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
					if(V[1] < 0)
					joystick_new_axis(fd, i, V[1], V[2], get_axis_name(i),
						(V[1] < 0) ? keygroup::KT_AXIS_PAIR : keygroup::KT_PRESSURE_MP);
					axis_count++;
				} else if(i % 2 == 0) {
					joystick_new_hat(fd, i, i + 1, 1, get_axis_name(i), get_axis_name(i + 1));
					hat_count++;
				}
			}
		}
		joystick_message(fd);
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
	joystick_quit();
}

#define POLL_WAIT 20000

void joystick_plugin::thread_fn() throw()
{
	while(!quit_signaled) {
		for(auto fd : joystick_set())
			while(read_one_input_event(fd));
		joystick_flush();
		usleep(POLL_WAIT);
	}
	quit_ack = true;
}

void joystick_plugin::signal() throw()
{
	quit_signaled = true;
	while(!quit_ack);
}

const char* joystick_plugin::name = "Evdev joystick plugin";
