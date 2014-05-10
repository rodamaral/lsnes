#include "lsnes.hpp"

#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/rom.hpp"

#include <stdexcept>
#include <cassert>
#include <cstring>
#include <fstream>

movie_logic::movie_logic() throw()
{
	mf = NULL;
	mov = NULL;
	rrd = NULL;
}

void movie_logic::set_movie(movie& _mov, bool free_old) throw()
{
	auto tmp = mov;
	mov = &_mov;
	if(free_old) delete tmp;
}

movie& movie_logic::get_movie() throw(std::runtime_error)
{
	if(!mov)
		throw std::runtime_error("No movie");
	return *mov;
}

void movie_logic::set_mfile(moviefile& _mf, bool free_old) throw()
{
	auto tmp = mf;
	mf = &_mf;
	if(free_old) delete tmp;
}

moviefile& movie_logic::get_mfile() throw(std::runtime_error)
{
	if(!mf)
		throw std::runtime_error("No movie");
	return *mf;
}

void movie_logic::set_rrdata(rrdata_set& _rrd, bool free_old) throw()
{
	auto tmp = rrd;
	rrd = &_rrd;
	if(free_old) delete tmp;
}

rrdata_set& movie_logic::get_rrdata() throw(std::runtime_error)
{
	if(!rrd)
		throw std::runtime_error("No movie");
	return *rrd;
}

void movie_logic::new_frame_starting(bool dont_poll) throw(std::bad_alloc, std::runtime_error)
{
	mov->next_frame();
	controller_frame c = update_controls(false);
	if(!mov->readonly_mode()) {
		mov->set_controls(c);
		if(!dont_poll)
			mov->set_all_DRDY();
	} else if(!dont_poll)
		mov->set_all_DRDY();
}

short movie_logic::input_poll(unsigned port, unsigned dev, unsigned id) throw(std::bad_alloc, std::runtime_error)
{
	if(!mov)
		return 0;
	if(!mov->get_DRDY(port, dev, id)) {
		mov->set_controls(update_controls(true));
		mov->set_all_DRDY();
	}
	return mov->next_input(port, dev, id);
}

void movie_logic::release_memory()
{
	delete rrd;
	rrd = NULL;
	delete mov;
	mov = NULL;
	delete mf;
	mf = NULL;
}
