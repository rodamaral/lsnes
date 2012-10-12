#include "lsnes.hpp"

#include "core/emucore.hpp"
#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/rom.hpp"

#include <stdexcept>
#include <cassert>
#include <cstring>
#include <fstream>


movie_logic::movie_logic() throw()
{
}

movie& movie_logic::get_movie() throw()
{
	return mov;
}

long movie_logic::new_frame_starting(bool dont_poll) throw(std::bad_alloc, std::runtime_error)
{
	mov.next_frame();
	controller_frame c = update_controls(false);
	if(!mov.readonly_mode()) {
		mov.set_controls(c);
		if(!dont_poll)
			mov.set_all_DRDY();
		if(c.axis3(0, 0, 1)) {
			long hi = c.axis3(0, 0, 2);
			long lo = c.axis3(0, 0, 3);
			mov.commit_reset(hi * 10000 + lo);
		}
	} else if(!dont_poll)
		mov.set_all_DRDY();
	return mov.get_reset_status();
}

short movie_logic::input_poll(unsigned port, unsigned dev, unsigned id) throw(std::bad_alloc, std::runtime_error)
{
	if(!mov.get_DRDY(port, dev, id)) {
		mov.set_controls(update_controls(true));
		mov.set_all_DRDY();
	}
	int16_t in = mov.next_input(port, dev, id);
	//std::cerr << "BSNES asking for (" << port << "," << dev << "," << id << ") (frame "
	//	<< mov.get_current_frame() << ") giving " << in << std::endl;
	return in;
}
