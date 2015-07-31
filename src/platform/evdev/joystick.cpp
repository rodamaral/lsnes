#include "core/command.hpp"
#include "core/joystickapi.hpp"
#include "core/keymapper.hpp"
#include "core/messages.hpp"
#include "library/minmax.hpp"
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

namespace
{
	const char* axisnames[64] = {
		"X", "Y", "Z", "RX", "RY", "RZ", "THROTTLE", "RUDDER", "WHEEL", "GAS", "BRAKE", "Unknown axis #11",
		"Unknown axis #12", "Unknown axis #13", "Unknown axis #14", "Unknown axis #15", "HAT0X", "HAT0Y",
		"HAT1X", "HAT1Y", "HAT2X", "HAT2Y", "HAT3X", "HAT3Y", "PRESSURE", "DISTANCE", "TILT_X", "TILT_Y",
		"TOOL_WIDTH", "Unknown axis #29", "Unknown axis #30", "Unknown axis #31", "VOLUME", "Unknown axis #33",
		"Unknown axis #34", "Unknown axis #35", "Unknown axis #36", "Unknown axis #37", "Unknown axis #38",
		"Unknown axis #39", "MISC", "Unknown axis #41", "Unknown axis #42", "Unknown axis #43",
		"Unknown axis #44", "Unknown axis #45", "Unknown axis #46", "MT_SLOT", "MT_TOUCH_MAJOR",
		"MT_TOUCH_MINOR", "MT_WIDTH_MAJOR", "MT_WIDTH_MINOR", "MT_ORIENTATION", "MT_POSITION_X",
		"MT_POSITION_Y", "MT_TOOL_TYPE", "MT_BLOB_ID", "MT_TRACKING_ID", "MT_PRESSURE", "MT_DISTANCE",
		"Unknown axis #60", "Unknown axis #61", "Unknown axis #62", "Unknown axis #63"
	};
	const char* buttonnames[768] = {
		"RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "MINUS", "EQUAL", "BACKSPACE",
		"TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "LEFTBRACE", "RIGHTBRACE", "ENTER",
		"LEFTCTRL", "A", "S", "D", "F", "G", "H", "J", "K", "L", "SEMICOLON", "APOSTROPHE", "GRAVE",
		"LEFTSHIFT", "BACKSLASH", "Z", "X", "C", "V", "B", "N", "M", "COMMA", "DOT", "SLASH", "RIGHTSHIFT",
		"KPASTERISK", "LEFTALT", "SPACE", "CAPSLOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9",
		"F10", "NUMLOCK", "SCROLLLOCK", "KP7", "KP8", "KP9", "KPMINUS", "KP4", "KP5", "KP6", "KPPLUS", "KP1",
		"KP2", "KP3", "KP0", "KPDOT", "Unknown button #84", "ZENKAKUHANKAKU", "102ND", "F11", "F12", "RO",
		"KATAKANA", "HIRAGANA", "HENKAN", "KATAKANAHIRAGANA", "MUHENKAN", "KPJPCOMMA", "KPENTER", "RIGHTCTRL",
		"KPSLASH", "SYSRQ", "RIGHTALT", "LINEFEED", "HOME", "UP", "PAGEUP", "LEFT", "RIGHT", "END", "DOWN",
		"PAGEDOWN", "INSERT", "DELETE", "MACRO", "MUTE", "VOLUMEDOWN", "VOLUMEUP", "POWER", "KPEQUAL",
		"KPPLUSMINUS", "PAUSE", "SCALE", "KPCOMMA", "HANGUEL", "HANJA", "YEN", "LEFTMETA", "RIGHTMETA",
		"COMPOSE", "STOP", "AGAIN", "PROPS", "UNDO", "FRONT", "COPY", "OPEN", "PASTE", "FIND", "CUT", "HELP",
		"MENU", "CALC", "SETUP", "SLEEP", "WAKEUP", "FILE", "SENDFILE", "DELETEFILE", "XFER", "PROG1", "PROG2",
		"WWW", "MSDOS", "SCREENLOCK", "DIRECTION", "CYCLEWINDOWS", "MAIL", "BOOKMARKS", "COMPUTER", "BACK",
		"FORWARD", "CLOSECD", "EJECTCD", "EJECTCLOSECD", "NEXTSONG", "PLAYPAUSE", "PREVIOUSSONG", "STOPCD",
		"RECORD", "REWIND", "PHONE", "ISO", "CONFIG", "HOMEPAGE", "REFRESH", "EXIT", "MOVE", "EDIT",
		"SCROLLUP", "SCROLLDOWN", "KPLEFTPAREN", "KPRIGHTPAREN", "NEW", "REDO", "F13", "F14", "F15", "F16",
		"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "Unknown button #195", "Unknown button #196",
		"Unknown button #197", "Unknown button #198", "Unknown button #199", "PLAYCD", "PAUSECD", "PROG3",
		"PROG4", "DASHBOARD", "SUSPEND", "CLOSE", "PLAY", "FASTFORWARD", "BASSBOOST", "PRINT", "HP", "CAMERA",
		"SOUND", "QUESTION", "EMAIL", "CHAT", "SEARCH", "CONNECT", "FINANCE", "SPORT", "SHOP", "ALTERASE",
		"CANCEL", "BRIGHTNESSDOWN", "BRIGHTNESSUP", "MEDIA", "SWITCHVIDEOMODE", "KBDILLUMTOGGLE",
		"KBDILLUMDOWN", "KBDILLUMUP", "SEND", "REPLY", "FORWARDMAIL", "SAVE", "DOCUMENTS", "BATTERY",
		"BLUETOOTH", "WLAN", "UWB", "UNKNOWN", "VIDEO_NEXT", "VIDEO_PREV", "BRIGHTNESS_CYCLE",
		"BRIGHTNESS_ZERO", "DISPLAY_OFF", "WIMAX", "RFKILL", "MICMUTE", "Unknown button #249",
		"Unknown button #250", "Unknown button #251", "Unknown button #252", "Unknown button #253",
		"Unknown button #254", "Unknown button #255", "Button 0", "Button 1", "Button 2", "Button 3",
		"Button 4", "Button 5", "Button 6", "Button 7", "Button 8", "Button 9", "Unknown button #266",
		"Unknown button #267", "Unknown button #268", "Unknown button #269", "Unknown button #270",
		"Unknown button #271", "Button LEFT", "Button RIGHT", "Button MIDDLE", "Button SIDE", "Button EXTRA",
		"Button FORWARD", "Button BACK", "Button TASK", "Unknown button #280", "Unknown button #281",
		"Unknown button #282", "Unknown button #283", "Unknown button #284", "Unknown button #285",
		"Unknown button #286", "Unknown button #287", "Button TRIGGER", "Button THUMB", "Button THUMB2",
		"Button TOP", "Button TOP2", "Button PINKIE", "Button BASE", "Button BASE2", "Button BASE3",
		"Button BASE4", "Button BASE5", "Button BASE6", "Unknown button #300", "Unknown button #301",
		"Unknown button #302", "Button DEAD", "Button A", "Button B", "Button C", "Button X", "Button Y",
		"Button Z", "Button TL", "Button TR", "Button TL2", "Button TR2", "Button SELECT", "Button START",
		"Button MODE", "Button THUMBL", "Button THUMBR", "Unknown button #319", "Button TOOL_PEN",
		"Button TOOL_RUBBER", "Button TOOL_BRUSH", "Button TOOL_PENCIL", "Button TOOL_AIRBRUSH",
		"Button TOOL_FINGER", "Button TOOL_MOUSE", "Button TOOL_LENS", "Button TOOL_QUINTTAP",
		"Unknown button #329", "Button TOUCH", "Button STYLUS", "Button STYLUS2", "Button TOOL_DOUBLETAP",
		"Button TOOL_TRIPLETAP", "Button TOOL_QUADTAP", "Button GEAR_DOWN", "Button GEAR_UP",
		"Unknown button #338", "Unknown button #339", "Unknown button #340", "Unknown button #341",
		"Unknown button #342", "Unknown button #343", "Unknown button #344", "Unknown button #345",
		"Unknown button #346", "Unknown button #347", "Unknown button #348", "Unknown button #349",
		"Unknown button #350", "Unknown button #351", "OK", "SELECT", "GOTO", "CLEAR", "POWER2", "OPTION",
		"INFO", "TIME", "VENDOR", "ARCHIVE", "PROGRAM", "CHANNEL", "FAVORITES", "EPG", "PVR", "MHP",
		"LANGUAGE", "TITLE", "SUBTITLE", "ANGLE", "ZOOM", "MODE", "KEYBOARD", "SCREEN", "PC", "TV", "TV2",
		"VCR", "VCR2", "SAT", "SAT2", "CD", "TAPE", "RADIO", "TUNER", "PLAYER", "TEXT", "DVD", "AUX", "MP3",
		"AUDIO", "VIDEO", "DIRECTORY", "LIST", "MEMO", "CALENDAR", "RED", "GREEN", "YELLOW", "BLUE",
		"CHANNELUP", "CHANNELDOWN", "FIRST", "LAST", "AB", "NEXT", "RESTART", "SLOW", "SHUFFLE", "BREAK",
		"PREVIOUS", "DIGITS", "TEEN", "TWEN", "VIDEOPHONE", "GAMES", "ZOOMIN", "ZOOMOUT", "ZOOMRESET",
		"WORDPROCESSOR", "EDITOR", "SPREADSHEET", "GRAPHICSEDITOR", "PRESENTATION", "DATABASE", "NEWS",
		"VOICEMAIL", "ADDRESSBOOK", "MESSENGER", "DISPLAYTOGGLE", "SPELLCHECK", "LOGOFF", "DOLLAR", "EURO",
		"FRAMEBACK", "FRAMEFORWARD", "CONTEXT_MENU", "MEDIA_REPEAT", "10CHANNELSUP", "10CHANNELSDOWN",
		"IMAGES", "Unknown button #443", "Unknown button #444", "Unknown button #445", "Unknown button #446",
		"Unknown button #447", "DEL_EOL", "DEL_EOS", "INS_LINE", "DEL_LINE", "Unknown button #452",
		"Unknown button #453", "Unknown button #454", "Unknown button #455", "Unknown button #456",
		"Unknown button #457", "Unknown button #458", "Unknown button #459", "Unknown button #460",
		"Unknown button #461", "Unknown button #462", "Unknown button #463", "FN", "FN_ESC", "FN_F1", "FN_F2",
		"FN_F3", "FN_F4", "FN_F5", "FN_F6", "FN_F7", "FN_F8", "FN_F9", "FN_F10", "FN_F11", "FN_F12", "FN_1",
		"FN_2", "FN_D", "FN_E", "FN_F", "FN_S", "FN_B", "Unknown button #485", "Unknown button #486",
		"Unknown button #487", "Unknown button #488", "Unknown button #489", "Unknown button #490",
		"Unknown button #491", "Unknown button #492", "Unknown button #493", "Unknown button #494",
		"Unknown button #495", "Unknown button #496", "BRL_DOT1", "BRL_DOT2", "BRL_DOT3", "BRL_DOT4",
		"BRL_DOT5", "BRL_DOT6", "BRL_DOT7", "BRL_DOT8", "BRL_DOT9", "BRL_DOT10", "Unknown button #507",
		"Unknown button #508", "Unknown button #509", "Unknown button #510", "Unknown button #511",
		"NUMERIC_0", "NUMERIC_1", "NUMERIC_2", "NUMERIC_3", "NUMERIC_4", "NUMERIC_5", "NUMERIC_6", "NUMERIC_7",
		"NUMERIC_8", "NUMERIC_9", "NUMERIC_STAR", "NUMERIC_POUND", "Unknown button #524",
		"Unknown button #525", "Unknown button #526", "Unknown button #527", "CAMERA_FOCUS", "WPS_BUTTON",
		"TOUCHPAD_TOGGLE", "TOUCHPAD_ON", "TOUCHPAD_OFF", "CAMERA_ZOOMIN", "CAMERA_ZOOMOUT", "CAMERA_UP",
		"CAMERA_DOWN", "CAMERA_LEFT", "CAMERA_RIGHT", "Unknown button #539", "Unknown button #540",
		"Unknown button #541", "Unknown button #542", "Unknown button #543", "Unknown button #544",
		"Unknown button #545", "Unknown button #546", "Unknown button #547", "Unknown button #548",
		"Unknown button #549", "Unknown button #550", "Unknown button #551", "Unknown button #552",
		"Unknown button #553", "Unknown button #554", "Unknown button #555", "Unknown button #556",
		"Unknown button #557", "Unknown button #558", "Unknown button #559", "Unknown button #560",
		"Unknown button #561", "Unknown button #562", "Unknown button #563", "Unknown button #564",
		"Unknown button #565", "Unknown button #566", "Unknown button #567", "Unknown button #568",
		"Unknown button #569", "Unknown button #570", "Unknown button #571", "Unknown button #572",
		"Unknown button #573", "Unknown button #574", "Unknown button #575", "Unknown button #576",
		"Unknown button #577", "Unknown button #578", "Unknown button #579", "Unknown button #580",
		"Unknown button #581", "Unknown button #582", "Unknown button #583", "Unknown button #584",
		"Unknown button #585", "Unknown button #586", "Unknown button #587", "Unknown button #588",
		"Unknown button #589", "Unknown button #590", "Unknown button #591", "Unknown button #592",
		"Unknown button #593", "Unknown button #594", "Unknown button #595", "Unknown button #596",
		"Unknown button #597", "Unknown button #598", "Unknown button #599", "Unknown button #600",
		"Unknown button #601", "Unknown button #602", "Unknown button #603", "Unknown button #604",
		"Unknown button #605", "Unknown button #606", "Unknown button #607", "Unknown button #608",
		"Unknown button #609", "Unknown button #610", "Unknown button #611", "Unknown button #612",
		"Unknown button #613", "Unknown button #614", "Unknown button #615", "Unknown button #616",
		"Unknown button #617", "Unknown button #618", "Unknown button #619", "Unknown button #620",
		"Unknown button #621", "Unknown button #622", "Unknown button #623", "Unknown button #624",
		"Unknown button #625", "Unknown button #626", "Unknown button #627", "Unknown button #628",
		"Unknown button #629", "Unknown button #630", "Unknown button #631", "Unknown button #632",
		"Unknown button #633", "Unknown button #634", "Unknown button #635", "Unknown button #636",
		"Unknown button #637", "Unknown button #638", "Unknown button #639", "Unknown button #640",
		"Unknown button #641", "Unknown button #642", "Unknown button #643", "Unknown button #644",
		"Unknown button #645", "Unknown button #646", "Unknown button #647", "Unknown button #648",
		"Unknown button #649", "Unknown button #650", "Unknown button #651", "Unknown button #652",
		"Unknown button #653", "Unknown button #654", "Unknown button #655", "Unknown button #656",
		"Unknown button #657", "Unknown button #658", "Unknown button #659", "Unknown button #660",
		"Unknown button #661", "Unknown button #662", "Unknown button #663", "Unknown button #664",
		"Unknown button #665", "Unknown button #666", "Unknown button #667", "Unknown button #668",
		"Unknown button #669", "Unknown button #670", "Unknown button #671", "Unknown button #672",
		"Unknown button #673", "Unknown button #674", "Unknown button #675", "Unknown button #676",
		"Unknown button #677", "Unknown button #678", "Unknown button #679", "Unknown button #680",
		"Unknown button #681", "Unknown button #682", "Unknown button #683", "Unknown button #684",
		"Unknown button #685", "Unknown button #686", "Unknown button #687", "Unknown button #688",
		"Unknown button #689", "Unknown button #690", "Unknown button #691", "Unknown button #692",
		"Unknown button #693", "Unknown button #694", "Unknown button #695", "Unknown button #696",
		"Unknown button #697", "Unknown button #698", "Unknown button #699", "Unknown button #700",
		"Unknown button #701", "Unknown button #702", "Unknown button #703", "Button TRIGGER_HAPPY1",
		"Button TRIGGER_HAPPY2", "Button TRIGGER_HAPPY3", "Button TRIGGER_HAPPY4", "Button TRIGGER_HAPPY5",
		"Button TRIGGER_HAPPY6", "Button TRIGGER_HAPPY7", "Button TRIGGER_HAPPY8", "Button TRIGGER_HAPPY9",
		"Button TRIGGER_HAPPY10", "Button TRIGGER_HAPPY11", "Button TRIGGER_HAPPY12", "Button TRIGGER_HAPPY13",
		"Button TRIGGER_HAPPY14", "Button TRIGGER_HAPPY15", "Button TRIGGER_HAPPY16", "Button TRIGGER_HAPPY17",
		"Button TRIGGER_HAPPY18", "Button TRIGGER_HAPPY19", "Button TRIGGER_HAPPY20", "Button TRIGGER_HAPPY21",
		"Button TRIGGER_HAPPY22", "Button TRIGGER_HAPPY23", "Button TRIGGER_HAPPY24", "Button TRIGGER_HAPPY25",
		"Button TRIGGER_HAPPY26", "Button TRIGGER_HAPPY27", "Button TRIGGER_HAPPY28", "Button TRIGGER_HAPPY29",
		"Button TRIGGER_HAPPY30", "Button TRIGGER_HAPPY31", "Button TRIGGER_HAPPY32", "Button TRIGGER_HAPPY33",
		"Button TRIGGER_HAPPY34", "Button TRIGGER_HAPPY35", "Button TRIGGER_HAPPY36", "Button TRIGGER_HAPPY37",
		"Button TRIGGER_HAPPY38", "Button TRIGGER_HAPPY39", "Button TRIGGER_HAPPY40", "Unknown button #744",
		"Unknown button #745", "Unknown button #746", "Unknown button #747", "Unknown button #748",
		"Unknown button #749", "Unknown button #750", "Unknown button #751", "Unknown button #752",
		"Unknown button #753", "Unknown button #754", "Unknown button #755", "Unknown button #756",
		"Unknown button #757", "Unknown button #758", "Unknown button #759", "Unknown button #760",
		"Unknown button #761", "Unknown button #762", "Unknown button #763", "Unknown button #764",
		"Unknown button #765", "Unknown button #766", "Unknown button #767"
	};

	std::map<int, unsigned> gamepad_map;

	int read_one_input_event(int fd)
	{
		struct input_event ev;
		int r = read(fd, &ev, sizeof(ev));
		if(r < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
			return 0;
		if(r < 0) {
			if(errno == ENODEV) {
				messages << "Joystick #" << gamepad_map[fd] << " disconnected." << std::endl;
				return -1;  //Disconnected.
			}
			messages << "Error reading from joystick (fd=" << fd << "): " << strerror(errno)
				<< std::endl;
			return 0;
		}
		if(ev.type == EV_KEY)
			lsnes_gamepads[gamepad_map[fd]].report_button(ev.code, ev.value != 0);
		if(ev.type == EV_ABS)
			lsnes_gamepads[gamepad_map[fd]].report_axis(ev.code, ev.value);
		return 1;
	}

	bool probe_joystick(int fd, const std::string& filename)
	{
		const size_t div = 8 * sizeof(unsigned long);
		unsigned long keys[(KEY_MAX + div) / div] = {0};
		unsigned long axes[(ABS_MAX + div) / div] = {0};
		unsigned long evtypes[(EV_MAX + div) / div] = {0};
		char namebuffer[256];
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
		unsigned jid = gamepad_map[fd] = lsnes_gamepads.add(namebuffer);
		gamepad::pad& ngp = lsnes_gamepads[jid];
		for(unsigned i = 0; i <= KEY_MAX; i++)
			if(keys[i / div] & (1ULL << (i % div)))
				ngp.add_button(i, buttonnames[i]);
		for(unsigned i = 0; i <= ABS_MAX; i++)
			if(axes[i / div] & (1ULL << (i % div))) {
				if(i < ABS_HAT0X || i > ABS_HAT3Y) {
					int32_t V[5];
					if(ioctl(fd, EVIOCGABS(i), V) < 0) {
						int merrno = errno;
						messages << "Error getting parameters for axis " << i << " (fd="
							<< fd << "): " << strerror(merrno) << std::endl;
						continue;
					}
					ngp.add_axis(i, V[1], V[2], V[1] == 0, axisnames[i]);
				} else if(i % 2 == 0)
					ngp.add_hat(i, i + 1, 1, axisnames[i], axisnames[i + 1]);
			}
		messages << "Joystick #" << jid << " online: " << namebuffer << std::endl;
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

#define POLL_WAIT 50000

	void do_fd_zero(fd_set& s)
	{
		FD_ZERO(&s);
	}

	struct _joystick_driver drv = {
		.init = []() -> void {
			quit_signaled = false;
			quit_ack = false;
			probe_all_joysticks();
			quit_ack = quit_signaled = false;
		},
		.quit = []() -> void {
			quit_signaled = true;
			while(!quit_ack);
		},
		.thread_fn = []() -> void {
			while(!quit_signaled) {
				fd_set rfds;
				do_fd_zero(rfds);
				int limit = 0;
				for(auto fd : gamepad_map) {
					limit = max(limit, fd.first + 1);
					FD_SET(fd.first, &rfds);
				}
				if(!limit) {
					usleep(POLL_WAIT);
					continue;
				}
				struct timeval tv;
				tv.tv_sec = 0;
				tv.tv_usec = POLL_WAIT;
				int r = select(limit, &rfds, NULL, NULL, &tv);
				if(r <= 0)
					continue;
				std::set<int> cleanup;
				for(auto fd : gamepad_map)
					if(FD_ISSET(fd.first, &rfds)) {
						while(true) {
							int r = read_one_input_event(fd.first);
							if(!r)
								break;
							if(r < 0) {
								cleanup.insert(fd.first);
								break;
							}
						}
					}
				for(auto i : cleanup) {
					unsigned jid = gamepad_map[i];
					close(i);
					gamepad_map.erase(i);
					lsnes_gamepads[jid].set_online(false);
					messages << "Gamepad #" << jid << "[" << lsnes_gamepads[jid].name()
						<< "] disconnected." << std::endl;
				}
			}
			//Get rid of joystick handles.
			for(auto fd : gamepad_map) {
				close(fd.first);
				lsnes_gamepads[fd.second].set_online(false);
			}
			gamepad_map.clear();

			quit_ack = true;
		},
		.signal = []() -> void {
			quit_signaled = true;
			while(!quit_ack);
		},
		.name = []() -> const char* { return "Evdev joystick plugin"; }
	};
	struct joystick_driver _drv(drv);
}
