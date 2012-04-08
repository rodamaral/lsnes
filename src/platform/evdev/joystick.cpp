#include "core/command.hpp"
#include "core/keymapper.hpp"
#include "core/window.hpp"

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

	enum event_type
	{
		ET_BUTTON,
		ET_AXIS,
		ET_HAT_X,
		ET_HAT_Y
	};

	struct event_mapping
	{
		uint16_t joystick;
		uint32_t tevcode;
		enum event_type type;
		uint16_t controlnum;
		int32_t axis_min;
		int32_t axis_max;
		int32_t current_status;
		uint32_t paired_mapping;	//Only hats.
		class keygroup* group;
	};

	struct event_mapping dummy_event_mapping = {
		9999,		//Joystick (don't care).
		99999,		//Tyep&Event code (don't care).
		ET_BUTTON,	//Not really.
		9999,		//Control num (don't care).
		0,		//Axis minimum (don't care).
		0,		//Axis maximum (don't care).
		0,		//Current status (don't care).
		0,		//This is not a hat, so don't care about paired mapping.
		NULL		//No associated key group.
	};

	std::set<keygroup*> keygroups;
	std::map<uint64_t, struct event_mapping> event_map;
	std::map<int, uint16_t> joystick_map;
	std::set<int> joysticks;
	std::map<uint16_t, std::string> joystick_names;
	uint16_t connected_joysticks = 0;

	uint16_t get_joystick_number(int fd)
	{
		if(joystick_map.count(fd))
			return joystick_map[fd];
		else
			return (joystick_map[fd] = connected_joysticks++);
	}

	uint64_t get_ev_id(uint16_t joystick, uint16_t type, uint16_t evcode)
	{
		return (static_cast<uint64_t>(joystick) << 32) |
			(static_cast<uint64_t>(type) << 16) |
			static_cast<uint64_t>(evcode);
	};

	uint64_t get_ev_id(uint16_t joystick, uint32_t tevcode)
	{
		return (static_cast<uint64_t>(joystick) << 32) |
			static_cast<uint64_t>(tevcode);
	};

	void create_button(int fd, uint16_t type, uint16_t evcode, uint16_t buttonnum)
	{
		uint16_t joynum = get_joystick_number(fd);
		std::ostringstream _name;
		_name << "joystick" << joynum << "button" << buttonnum;
		std::string name = _name.str();
		keygroup* grp = new keygroup(name, "joystick", keygroup::KT_KEY);
		keygroups.insert(grp);
		struct event_mapping evmap;
		evmap.joystick = joynum;
		evmap.tevcode = (static_cast<uint32_t>(type) << 16) | static_cast<uint32_t>(evcode);
		evmap.type = ET_BUTTON;
		evmap.controlnum = buttonnum;
		evmap.axis_min = evmap.axis_max = 0;
		evmap.current_status = 0;
		evmap.paired_mapping = 0;
		evmap.group = grp;
		event_map[get_ev_id(joynum, evmap.tevcode)] = evmap;
	}

	void create_axis(int fd, uint16_t type, uint16_t evcode, uint16_t axisnum, int32_t min, int32_t max)
	{
		uint16_t joynum = get_joystick_number(fd);
		std::ostringstream _name;
		_name << "joystick" << joynum << "axis" << axisnum;
		std::string name = _name.str();
		keygroup* grp;
		if(min < 0)
			grp = new keygroup(name, "joystick", keygroup::KT_AXIS_PAIR);
		else
			grp = new keygroup(name, "joystick", keygroup::KT_PRESSURE_MP);
		keygroups.insert(grp);
		struct event_mapping evmap;
		evmap.joystick = joynum;
		evmap.tevcode = (static_cast<uint32_t>(type) << 16) | static_cast<uint32_t>(evcode);
		evmap.type = ET_AXIS;
		evmap.controlnum = axisnum;
		evmap.axis_min = min;
		evmap.axis_max = max;
		evmap.current_status = 0;
		evmap.paired_mapping = 0;
		evmap.group = grp;
		event_map[get_ev_id(joynum, evmap.tevcode)] = evmap;
	}

	void create_hat(int fd, uint16_t type, uint16_t evcodeX, uint16_t evcodeY, uint16_t hatnum)
	{
		uint16_t joynum = get_joystick_number(fd);
		std::ostringstream _name;
		_name << "joystick" << joynum << "hat" << hatnum;
		std::string name = _name.str();
		keygroup* grp = new keygroup(name, "joystick", keygroup::KT_HAT);
		keygroups.insert(grp);
		struct event_mapping evmap1;
		evmap1.joystick = joynum;
		evmap1.tevcode = (static_cast<uint32_t>(type) << 16) | static_cast<uint32_t>(evcodeX);
		evmap1.type = ET_HAT_X;
		evmap1.controlnum = hatnum;
		evmap1.axis_min = -1;
		evmap1.axis_max = 1;
		evmap1.current_status = 0;
		evmap1.group = grp;
		struct event_mapping evmap2;
		evmap2.joystick = joynum;
		evmap2.tevcode = (static_cast<uint32_t>(type) << 16) | static_cast<uint32_t>(evcodeY);
		evmap2.type = ET_HAT_Y;
		evmap2.controlnum = hatnum;
		evmap2.axis_min = -1;
		evmap1.axis_max = 1;
		evmap2.current_status = 0;
		evmap2.group = grp;
		evmap1.paired_mapping = get_ev_id(joynum, evmap2.tevcode);
		evmap2.paired_mapping = get_ev_id(joynum, evmap1.tevcode);
		event_map[get_ev_id(joynum, evmap1.tevcode)] = evmap1;
		event_map[get_ev_id(joynum, evmap2.tevcode)] = evmap2;
	}

	struct event_mapping& get_mapping_for(uint16_t joystick, uint16_t type, uint16_t evcode)
	{
		uint64_t evid = get_ev_id(joystick, type, evcode);
		if(event_map.count(evid))
			return event_map[evid];
		else
			return dummy_event_mapping;
	}

	struct event_mapping& get_mapping_for(uint16_t joystick, uint32_t tevcode)
	{
		uint64_t evid = get_ev_id(joystick, tevcode);
		if(event_map.count(evid))
			return event_map[evid];
		else
			return dummy_event_mapping;
	}

	struct event_mapping& get_mapping_for_fd(int fd, uint16_t type, uint16_t evcode)
	{
		if(joystick_map.count(fd))
			return get_mapping_for(joystick_map[fd], type, evcode);
		else
			return dummy_event_mapping;
	}

	void update_mapping_for_fd(int fd, uint16_t type, uint16_t evcode, int32_t value)
	{
		struct event_mapping& e = get_mapping_for_fd(fd, type, evcode);
		e.current_status = value;
		if(!e.group)
			return;		//Dummy.
		int16_t v = 0;
		switch(e.type) {
		case ET_BUTTON:
			v = (e.current_status != 0);
			break;
		case ET_AXIS:
			v = -32768 + 65535 * (static_cast<double>(e.current_status) - e.axis_min) /
				(static_cast<double>(e.axis_max) - e.axis_min);
			break;
		case ET_HAT_X:
		case ET_HAT_Y: {
			uint32_t xaxis, yaxis;
			if(e.type == ET_HAT_X) {
				xaxis = get_ev_id(e.joystick, e.tevcode);
				yaxis = e.paired_mapping;
			} else {
				yaxis = get_ev_id(e.joystick, e.tevcode);
				xaxis = e.paired_mapping;
			}
			if(event_map[yaxis].current_status < 0)
				v |= 1;
			if(event_map[xaxis].current_status > 0)
				v |= 2;
			if(event_map[yaxis].current_status > 0)
				v |= 4;
			if(event_map[xaxis].current_status < 0)
				v |= 8;
			break;
		}
		}
		platform::queue(keypress(modifier_set(), *e.group, v));
		//e.group->set_position(v, modifier_set());
	}

	bool read_one_input_event(int fd)
	{
		struct input_event ev;
		int r = read(fd, &ev, sizeof(ev));
		if(r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
			return false;
		if(r < 0) {
			messages << "Error reading from joystick (fd=" << fd << "): " << strerror(errno)
				<< std::endl;
			return false;
		}
		update_mapping_for_fd(fd, ev.type, ev.code, ev.value);
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
		for(unsigned i = 0; i <= KEY_MAX; i++) {
			if(keys[i / div] & (1ULL << (i % div))) {
				create_button(fd, EV_KEY, i, button_count++);
			}
		}
		for(unsigned i = 0; i <= ABS_MAX; i++) {
			if(axes[i / div] & (1ULL << (i % div))) {
				if(i < ABS_HAT0X || i > ABS_HAT3Y) {
					int32_t min;
					int32_t max;
					int32_t V[5];
					if(ioctl(fd, EVIOCGABS(i), V) < 0) {
						int merrno = errno;
						messages << "Error getting parameters for axis " << i << " (fd="
							<< fd << "): " << strerror(merrno) << std::endl;
						continue;
					}
					min = V[1];
					max = V[2];
					create_axis(fd, EV_ABS, i, axis_count++, min, max);
				} else if(i % 2 == 0) {
					create_hat(fd, EV_ABS, i, i + 1, hat_count++);
				}
			}
		}
		uint16_t joynum = get_joystick_number(fd);
		joystick_names[joynum] = namebuffer;
		messages << "Found '" << namebuffer << "' (" << button_count << " buttons, " << axis_count
			<< " axes, " << hat_count << " hats)" << std::endl;
		joysticks.insert(fd);
		return true;
	}

	void open_and_probe(const std::string& filename)
	{
		int r = open(filename.c_str(), O_RDONLY | O_NONBLOCK);
		if(r < 0) {
			return;
		}
		if(!probe_joystick(r, filename)) {
			close(r);
		}
		return;
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
			open_and_probe(std::string("/dev/input/") + dentry->d_name);
		}
		closedir(d);
	}

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

	function_ptr_command<> show_joysticks("show-joysticks", "Show joysticks",
		"Syntax: show-joysticks\nShow joystick data.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i : joystick_names) {
				messages << "Joystick #" << i.first << ": " << i.second << std::endl;
				for(auto j : event_map) {
					if(j.second.joystick != i.first)
						continue;
					if(j.second.type == ET_BUTTON)
						messages << j.second.group->name() << ": "
							<< get_button_name(j.second.tevcode & 0xFFFF) << std::endl;
					if(j.second.type == ET_AXIS)
						messages << j.second.group->name() << ": "
							<< get_axis_name(j.second.tevcode & 0xFFFF)
							<< "[" << j.second.axis_min << "," << j.second.axis_max
							<< "]" << std::endl;
					if(j.second.type == ET_HAT_X || j.second.type == ET_HAT_Y)
						messages << j.second.group->name() << ": "
							<< get_axis_name(j.second.tevcode & 0xFFFF) << std::endl;
				}
			}
		});

	volatile bool quit_signaled = false;
	volatile bool quit_ack = false;
}

void joystick_plugin::init() throw()
{
	probe_all_joysticks();
	evdev_init_buttons(buttonnames);
	evdev_init_axes(axisnames);
	quit_ack = quit_signaled = false;
}

void joystick_plugin::quit() throw()
{
	quit_signaled = true;
	while(!quit_ack);
	for(int fd : joysticks)
		close(fd);
	for(auto i : keygroups)
		delete i;
	keygroups.clear();
	joystick_map.clear();
	joysticks.clear();
	joystick_names.clear();
	std::map<uint64_t, struct event_mapping> event_map;
	std::map<int, uint16_t> joystick_map;
	std::set<int> joysticks;
	std::map<uint16_t, std::string> joystick_names;
}

#define POLL_WAIT 20000

void joystick_plugin::thread_fn() throw()
{
	while(!quit_signaled) {
		for(int fd : joysticks)
			while(read_one_input_event(fd));
		usleep(POLL_WAIT);
	}
	quit_ack = true;
}

void joystick_plugin::signal() throw()
{
	quit_signaled = true;
}

const char* joystick_plugin::name = "Evdev joystick plugin";
