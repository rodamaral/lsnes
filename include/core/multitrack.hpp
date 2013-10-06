#ifndef _multitrack__hpp__included__
#define _multitrack__hpp__included__

#include <map>
#include "library/threadtypes.hpp"
#include "library/controller-data.hpp"

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
	void enable(bool state);
	void set(unsigned port, unsigned controller, state s);
	void set_and_notify(unsigned port, unsigned controller, state s);
	void rotate(bool forward);
	state get(unsigned port, unsigned controller);
	bool is_enabled();
	void config_altered();
	void process_frame(controller_frame& input);
	bool any_records();
private:
	mutex_class mutex;
	bool enabled;
	std::map<std::pair<unsigned, unsigned>, state> controllerstate;
};

extern multitrack_edit multitrack_editor;

#endif
