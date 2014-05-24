#ifndef _multitrack__hpp__included__
#define _multitrack__hpp__included__

#include <map>
#include "library/threads.hpp"
#include "library/controller-data.hpp"
#include "core/movie.hpp"

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
	multitrack_edit(movie_logic& _mlogic, controller_state& _controls);
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
	threads::lock mlock;
	bool enabled;
	std::map<std::pair<unsigned, unsigned>, state> controllerstate;
	movie_logic& mlogic;
	controller_state& controls;
};

#endif
