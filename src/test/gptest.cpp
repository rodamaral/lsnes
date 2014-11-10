#include "interface/controller.hpp"
#include "controller-parse.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <sys/time.h>

#include "../src/emulation/bsnes-legacy/ports.inc"

const char* ports_json = "{"
"\"buttons\":{"
"\"B\":{\"type\":\"button\", \"name\":\"B\"},"
"\"Y\":{\"type\":\"button\", \"name\":\"Y\"},"
"\"select\":{\"type\":\"button\", \"name\":\"select\", \"symbol\":\"s\"},"
"\"start\":{\"type\":\"button\", \"name\":\"start\", \"symbol\":\"S\"},"
"\"up\":{\"type\":\"button\", \"name\":\"up\", \"symbol\":\"↑\", \"macro\":\"^\", \"movie\":\"u\"},"
"\"down\":{\"type\":\"button\", \"name\":\"down\", \"symbol\":\"↓\", \"macro\":\"v\", \"movie\":\"d\"},"
"\"left\":{\"type\":\"button\", \"name\":\"left\", \"symbol\":\"←\", \"macro\":\"<\", \"movie\":\"l\"},"
"\"right\":{\"type\":\"button\", \"name\":\"right\", \"symbol\":\"→\", \"macro\":\">\", \"movie\":\"r\"},"
"\"A\":{\"type\":\"button\", \"name\":\"A\"},"
"\"X\":{\"type\":\"button\", \"name\":\"X\"},"
"\"L\":{\"type\":\"button\", \"name\":\"L\"},"
"\"R\":{\"type\":\"button\", \"name\":\"R\"},"
"\"ext0\":{\"type\":\"button\", \"name\":\"ext0\", \"symbol\":\"0\", \"macro\":\"E0\"},"
"\"ext1\":{\"type\":\"button\", \"name\":\"ext1\", \"symbol\":\"1\", \"macro\":\"E1\"},"
"\"ext2\":{\"type\":\"button\", \"name\":\"ext2\", \"symbol\":\"2\", \"macro\":\"E2\"},"
"\"ext3\":{\"type\":\"button\", \"name\":\"ext3\", \"symbol\":\"3\", \"macro\":\"E3\"},"
"\"trigger\":{\"type\":\"button\", \"name\":\"trigger\", \"symbol\":\"T\"},"
"\"cursor\":{\"type\":\"button\", \"name\":\"cursor\", \"symbol\":\"C\"},"
"\"turbo\":{\"type\":\"button\", \"name\":\"turbo\", \"symbol\":\"U\"},"
"\"pause\":{\"type\":\"button\", \"name\":\"pause\", \"symbol\":\"P\"},"
"\"xaxis\":{\"type\":\"lightgun\", \"name\":\"xaxis\", \"min\":-16, \"max\":271},"
"\"yaxis\":{\"type\":\"lightgun\", \"name\":\"yaxis\", \"min\":-16, \"max\":255},"
"\"xmotion\":{\"type\":\"raxis\", \"name\":\"xaxis\", \"min\":-255, \"max\":255, \"centers\":true},"
"\"ymotion\":{\"type\":\"raxis\", \"name\":\"yaxis\", \"min\":-255, \"max\":255, \"centers\":true},"
"\"framesync\":{\"type\":\"button\", \"name\":\"framesync\", \"symbol\":\"F\", \"shadow\":true},"
"\"reset\":{\"type\":\"button\", \"name\":\"reset\", \"symbol\":\"R\", \"shadow\":true},"
"\"hard\":{\"type\":\"button\", \"name\":\"hard\", \"symbol\":\"H\", \"shadow\":true},"
"\"rhigh\":{\"type\":\"axis\", \"name\":\"rhigh\", \"shadow\":true},"
"\"rlow\":{\"type\":\"axis\", \"name\":\"rlow\", \"shadow\":true},"
"\"shadownull\":{\"type\":\"null\", \"shadow\":true}"
"},\"controllers\":{"
"\"gamepad\":{\"type\":\"gamepad\", \"class\":\"gamepad\", \"buttons\":["
"\"buttons/B\", \"buttons/Y\", \"buttons/select\", \"buttons/start\", \"buttons/up\", \"buttons/down\","
"\"buttons/left\", \"buttons/right\", \"buttons/A\", \"buttons/X\", \"buttons/L\", \"buttons/R\""
"]},"
"\"gamepad16\":{\"type\":\"gamepad16\", \"class\":\"gamepad\", \"buttons\":["
"\"buttons/B\", \"buttons/Y\", \"buttons/select\", \"buttons/start\", \"buttons/up\", \"buttons/down\","
"\"buttons/left\", \"buttons/right\", \"buttons/A\", \"buttons/X\", \"buttons/L\", \"buttons/R\","
"\"buttons/ext0\", \"buttons/ext1\", \"buttons/ext2\", \"buttons/ext3\""
"]},"
"\"justifier\":{\"type\":\"justifier\", \"class\":\"justifier\", \"buttons\":["
"\"buttons/xaxis\", \"buttons/yaxis\", \"buttons/trigger\", \"buttons/start\""
"]},"
"\"mouse\":{\"type\":\"mouse\", \"class\":\"mouse\", \"buttons\":["
"\"buttons/xmotion\", \"buttons/ymotion\", \"buttons/L\", \"buttons/R\""
"]},"
"\"superscope\":{\"type\":\"superscope\", \"class\":\"superscope\", \"buttons\":["
"\"buttons/xaxis\", \"buttons/yaxis\", \"buttons/trigger\", \"buttons/cursor\", \"buttons/turbo\","
"\"buttons/pause\""
"]},"
"\"system\":{\"type\":\"(system)\", \"class\":\"(system)\", \"buttons\":["
"\"buttons/framesync\", \"buttons/reset\", \"buttons/rhigh\", \"buttons/rlow\""
"]},"
"\"system_hreset\":{\"type\":\"(system)\", \"class\":\"(system)\", \"buttons\":["
"\"buttons/framesync\", \"buttons/reset\", \"buttons/rhigh\", \"buttons/rlow\", \"buttons/hard\""
"]},"
"\"system_compact\":{\"type\":\"(system)\", \"class\":\"(system)\", \"buttons\":["
"\"buttons/framesync\", \"buttons/reset\", \"buttons/shadownull\", \"buttons/shadownull\","
"\"buttons/hard\""
"]}"
"},\"ports\":["
"{\"symbol\":\"gamepad\", \"name\":\"gamepad\", \"hname\":\"gamepad\", \"controllers\":["
"\"controllers/gamepad\""
"],\"legal\":[1,2]},"
"{\"symbol\":\"gamepad16\", \"name\":\"gamepad16\", \"hname\":\"gamepad (16 buttons)\", \"controllers\":["
"\"controllers/gamepad16\""
"],\"legal\":[1,2]},"
"{\"symbol\":\"ygamepad16\", \"name\":\"ygamepad16\", \"hname\":\"Y-cabled gamepad (16 buttons)\", \"controllers\":["
"\"controllers/gamepad16\", \"controllers/gamepad16\""
"],\"legal\":[1,2]},"
"{\"symbol\":\"justifier\", \"name\":\"justifier\", \"hname\":\"Justifier\", \"controllers\":["
"\"controllers/justifier\""
"],\"legal\":[2]},"
"{\"symbol\":\"justifiers\", \"name\":\"justifiers\", \"hname\":\"2 Justifiers\", \"controllers\":["
"\"controllers/justifier\", \"controllers/justifier\""
"],\"legal\":[2]},"
"{\"symbol\":\"mouse\", \"name\":\"mouse\", \"hname\":\"Mouse\", \"controllers\":["
"\"controllers/mouse\""
"],\"legal\":[1, 2]},"
"{\"symbol\":\"multitap\", \"name\":\"multitap\", \"hname\":\"Multitap\", \"controllers\":["
"\"controllers/gamepad\", \"controllers/gamepad\", \"controllers/gamepad\", \"controllers/gamepad\""
"],\"legal\":[1, 2]},"
"{\"symbol\":\"multitap16\", \"name\":\"multitap16\", \"hname\":\"Multitap (16 buttons)\", \"controllers\":["
"\"controllers/gamepad16\", \"controllers/gamepad16\", \"controllers/gamepad16\","
"\"controllers/gamepad16\""
"],\"legal\":[1, 2]},"
"{\"symbol\":\"none\", \"name\":\"none\", \"hname\":\"None\", \"controllers\":[],\"legal\":[1, 2]},"
"{\"symbol\":\"superscope\", \"name\":\"superscope\", \"hname\":\"Super Scope\", \"controllers\":["
"\"controllers/superscope\""
"],\"legal\":[2]},"
"{\"symbol\":\"psystem\", \"name\":\"system\", \"hname\":\"system\", \"controllers\":["
"\"controllers/system\""
"],\"legal\":[0]},"
"{\"symbol\":\"psystem_hreset\", \"name\":\"system\", \"hname\":\"system\", \"controllers\":["
"\"controllers/system_hreset\""
"],\"legal\":[0]},"
"{\"symbol\":\"psystem_compact\", \"name\":\"system\", \"hname\":\"system\", \"controllers\":["
"\"controllers/system_compact\""
"],\"legal\":[0]}"
"]"
"}";

void show_data(unsigned char* buffer, size_t bufsize)
{
	std::ostringstream s;
	for(unsigned i = 0; i < bufsize; i++)
		s << std::hex << std::setfill('0') << std::setw(2) << (int)buffer[i] << " ";
	std::cout << s.str() << std::endl;
}

void test_port(portctrl::type* p, unsigned bits)
{
	char out[512];
	unsigned char buffer[512];
	unsigned char buffer2[512];
	unsigned char buffer3[512];
	memset(buffer, 255, 512);
	memset(buffer3, 0, 512);
	for(int i = 0; i < p->storage_size; i++)
		buffer[i] = rand();
	if(bits % 8) {
		buffer[bits / 8] &= ((1 << (bits % 8)) - 1);
	}
	int x = p->serialize(p, buffer, out);
	out[x] = '|';
	out[x+1] = 'X';
	out[x+2] = '\0';
	//std::cout << p->name << ":\t" << out << std::endl;
	int k = 0;
	if(out[0] == '|') k++;
	int y = p->deserialize(p, buffer2, out + k);
	if(memcmp(buffer, buffer2, p->storage_size) || y + k != x)
		std::cerr << "Error! (" << out << ") [" << p->hname << "]" << std::endl;

	if(p->controller_info->controllers.size()) {
		int pc = rand() % p->controller_info->controllers.size();
		int pi = rand() % p->controller_info->controllers[pc].buttons.size();
		short v = 1;
		if(p->controller_info->controllers[pc].buttons[pi].is_analog())
			v = rand();
		if(p->controller_info->controllers[pc].buttons[pi].type == portctrl::button::TYPE_NULL)
			v = 0;
		p->write(p, buffer3, pc, pi, v);
		for(int i = 0; i < p->controller_info->controllers.size(); i++) {
			for(int j = 0; j < p->controller_info->controllers[pc].buttons.size(); j++) {
				int k2 = p->read(p, buffer3, i, j);
				if(k2 != v && (i == pc && j == pi)) {
					std::cerr << "Error (" << i << "," << j << "," << k2 << ")!=" << v
						<< std::endl;
					show_data(buffer3, p->storage_size);
				}
				if(k2 != 0 && (i != pc || j != pi))
					std::cerr << "Error (" << i << "," << j << "," << k2 << ")!=0" << std::endl;
			}
		}
	}

}

uint64_t get_utime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

int main()
{

	JSON::node portsdata(ports_json);
	portctrl::type_generic Sgamepad(portsdata, "ports/0");
	portctrl::type_generic Sgamepad16(portsdata, "ports/1");
	portctrl::type_generic Sygamepad16(portsdata, "ports/2");
	portctrl::type_generic Sjustifier(portsdata, "ports/3");
	portctrl::type_generic Sjustifiers(portsdata, "ports/4");
	portctrl::type_generic Smouse(portsdata, "ports/5");
	portctrl::type_generic Smultitap(portsdata, "ports/6");
	portctrl::type_generic Smultitap16(portsdata, "ports/7");
	portctrl::type_generic Snone(portsdata, "ports/8");
	portctrl::type_generic Ssuperscope(portsdata, "ports/9");
	portctrl::type_generic Spsystem(portsdata, "ports/10");
	portctrl::type_generic Spsystem_hreset(portsdata, "ports/11");
	portctrl::type_generic Spsystem_compact(portsdata, "ports/12");

	unsigned char buffer[512];

	controller_set s1;
	s1.ports.push_back(&psystem);
	s1.ports.push_back(&multitap16);
	s1.ports.push_back(&multitap16);
	for(unsigned i = 0; i < 8; i++)
		s1.logical_map.push_back(std::make_pair(i / 4 + 1, i % 4));
	portctrl::type_set& _fixed = portctrl::type_set::make(s1.ports, s1.portindex());
	portctrl::frame fixed(buffer, _fixed);

	controller_set s2;
	s2.ports.push_back(&Spsystem);
	s2.ports.push_back(&Smultitap16);
	s2.ports.push_back(&Smultitap16);
	for(unsigned i = 0; i < 8; i++)
		s2.logical_map.push_back(std::make_pair(i / 4 + 1, i % 4));
	portctrl::type_set& _variable = portctrl::type_set::make(s2.ports, s2.portindex());
	portctrl::frame variable(buffer, _variable);

	char out[512];
	srand(time(NULL));

	for(unsigned i = 0; i < 32; i++)
		buffer[i] = rand();
/*

	uint64_t t1 = get_utime();
	for(unsigned i = 0; i < 10000000; i++) {
		fixed.serialize(out);
	}
	uint64_t d = get_utime() - t1;
	std::cout << "Fixed Serialize: " << (double)d / 1000000 << std::endl;

	t1 = get_utime();
	for(unsigned i = 0; i < 10000000; i++) {
		variable.serialize(out);
	}
	d = get_utime() - t1;
	std::cout << "Variable Serialize: " << (double)d / 1000000 << std::endl;

	t1 = get_utime();
	for(unsigned i = 0; i < 10000000; i++) {
		fixed.deserialize(out);
	}
	d = get_utime() - t1;
	std::cout << "Fixed Unserialize: " << (double)d / 1000000 << std::endl;

	t1 = get_utime();
	for(unsigned i = 0; i < 10000000; i++) {
		variable.deserialize(out);
	}
	d = get_utime() - t1;
	std::cout << "Variable Unserialize: " << (double)d / 1000000 << std::endl;
	return 0;
*/

	srand(time(NULL));
	std::cerr << "Running..." << std::endl;
	while(true) {
		test_port(&Sgamepad, 12);
		test_port(&Sgamepad16, 16);
		test_port(&Sygamepad16, 32);
		test_port(&Sjustifier, 2);
		test_port(&Sjustifiers, 4);
		test_port(&Smouse, 2);
		test_port(&Smultitap, 48);
		test_port(&Smultitap16, 64);
		test_port(&Snone, 0);
		test_port(&Ssuperscope, 4);
		test_port(&Spsystem, 2);
		test_port(&Spsystem_hreset, 3);
		test_port(&Spsystem_compact, 3);
	}
	return 0;
}
