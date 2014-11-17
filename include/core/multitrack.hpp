#ifndef _multitrack__hpp__included__
#define _multitrack__hpp__included__

#include <map>
#include "library/threads.hpp"

class controller_state;
class movie_logic;
class emulator_dispatch;

class multitrack_edit
{
public:
	enum state
	{
		MT_PRESERVE,
		MT_OVERWRITE,
		MT_OR,
		MT_XOR
	};
	multitrack_edit(movie_logic& _mlogic, controller_state& _controls, emulator_dispatch& _dispatch,
		status_updater& _supdater, button_mapping& _buttons, command::group& _cmd);
	void enable(bool state);
	void set(unsigned port, unsigned controller, state s);
	void set_and_notify(unsigned port, unsigned controller, state s);
	void rotate(bool forward);
	state get(unsigned port, unsigned controller);
	bool is_enabled();
	void config_altered();
	void process_frame(portctrl::frame& input);
	bool any_records();
private:
	void do_mt_fwd();
	void do_mt_bw();
	void do_mt_set(const std::string& args);
	threads::lock mlock;
	bool enabled;
	std::map<std::pair<unsigned, unsigned>, state> controllerstate;
	movie_logic& mlogic;
	controller_state& controls;
	emulator_dispatch& edispatch;
	status_updater& supdater;
	button_mapping& buttons;
	command::group& cmd;
	command::_fnptr<> mt_f;
	command::_fnptr<> mt_b;
	command::_fnptr<const std::string&> mt_s;
};

#endif
