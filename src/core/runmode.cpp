#include "core/instance.hpp"
#include "core/messages.hpp"
#include "core/movie.hpp"
#include "core/runmode.hpp"
#include <stdexcept>

namespace
{
	const uint64_t QUIT_MAGIC = 0x87dd4df349e5eff7ULL;
}

const uint64_t emulator_runmode::QUIT = 1;
const uint64_t emulator_runmode::NORMAL = 2;
const uint64_t emulator_runmode::LOAD = 4;
const uint64_t emulator_runmode::ADVANCE_FRAME = 8;
const uint64_t emulator_runmode::ADVANCE_SUBFRAME = 16;
const uint64_t emulator_runmode::SKIPLAG = 32;
const uint64_t emulator_runmode::SKIPLAG_PENDING = 64;
const uint64_t emulator_runmode::PAUSE = 128;
const uint64_t emulator_runmode::PAUSE_BREAK = 256;
const uint64_t emulator_runmode::CORRUPT = 512;

const unsigned emulator_runmode::P_START = 0;
const unsigned emulator_runmode::P_VIDEO = 1;
const unsigned emulator_runmode::P_SAVE = 2;
const unsigned emulator_runmode::P_NONE = 3;

emulator_runmode::emulator_runmode()
{
	mode = PAUSE;
	saved_mode = PAUSE;
	point = 0;
	magic = 0;
	advanced = false;
	cancel = false;
}

uint64_t emulator_runmode::get()
{
	revalidate();
	return mode;
}

void emulator_runmode::set(uint64_t m)
{
	if(!m || m & (m - 1) || m > CORRUPT)
		throw std::logic_error("Trying to set invalid runmode");
	if(m == QUIT) {
		magic = QUIT_MAGIC;
		mode = QUIT;
	} else
		mode = m;
	//If setting state, clear the variables to be at initial state.
	advanced = false;
	cancel = false;
}

bool emulator_runmode::is(uint64_t m)
{
	revalidate();
	return ((mode & m) != 0);
}

void emulator_runmode::start_load()
{
	if(mode != CORRUPT) {
		saved_mode = mode;
		mode = LOAD;
		saved_advanced = advanced;
		saved_cancel = cancel;
		advanced = false;
		cancel = false;
	}
}

void emulator_runmode::end_load()
{
	if(mode != CORRUPT) {
		mode = saved_mode;
		advanced = saved_advanced;
		cancel = saved_cancel;
		saved_mode = 128;
	}
}

void emulator_runmode::decay_skiplag()
{
	revalidate();
	if(mode == SKIPLAG_PENDING) {
		mode = SKIPLAG;
	}
}

void emulator_runmode::decay_break()
{
	revalidate();
	if(mode == PAUSE_BREAK) {
		mode = PAUSE;
	}
}

void emulator_runmode::revalidate()
{
	if(!mode || mode & (mode - 1) || (mode == QUIT && magic != QUIT_MAGIC) || mode > CORRUPT) {
		//Uh, oh.
		auto& core = CORE();
		if(core.mlogic)
			emerg_save_movie(core.mlogic->get_mfile(), core.mlogic->get_rrdata());
		messages << "WARNING: Emulator runmode undefined, invoked movie dump." << std::endl;
		mode = PAUSE;
	}
}

bool emulator_runmode::set_and_test_advanced()
{
	bool x = advanced;
	advanced = true;
	return x;
}

void emulator_runmode::set_cancel()
{
	cancel = true;
	if(mode == LOAD)
		saved_cancel = true;
}

bool emulator_runmode::clear_and_test_cancel()
{
	bool x = cancel;
	cancel = false;
	return x;
}

bool emulator_runmode::test_cancel()
{
	return cancel;
}

bool emulator_runmode::test_advanced()
{
	return advanced;
}

void emulator_runmode::set_point(unsigned _point)
{
	point = _point;
}

unsigned emulator_runmode::get_point()
{
	return point;
}
