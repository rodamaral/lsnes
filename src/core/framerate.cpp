#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "core/moviedata.hpp"
#include "library/minmax.hpp"

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>
#include <limits>

bool graphics_driver_is_dummy();

namespace
{
	command::fnptr<> CMD_tturbo(lsnes_cmds, "toggle-turbo", "Toggle turbo",
		"Syntax: toggle-turbo\nToggle turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().framerate->turboed = !CORE().framerate->turboed;
		});

	command::fnptr<> CMD_pturbo(lsnes_cmds, "+turbo", "Activate turbo",
		"Syntax: +turbo\nActivate turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().framerate->turboed = true;
		});

	command::fnptr<> CMD_nturbo(lsnes_cmds, "-turbo", "Deactivate turbo",
		"Syntax: -turbo\nDeactivate turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			CORE().framerate->turboed = false;
		});

	keyboard::invbind_info IBIND_turboh(lsnes_invbinds, "+turbo", "Speed‣Turbo hold");
	keyboard::invbind_info IBIND_turbot(lsnes_invbinds, "toggle-turbo", "Speed‣Turbo toggle");
}


framerate_regulator::framerate_regulator()
{
	last_time_update = 0;
	time_at_last_update = 0;
	time_frozen = true;
	frame_number = 0;
	for(unsigned i = 0; i < FRAMERATE_HISTORY_FRAMES; i++)
		frame_start_times[i] = 0;
	nominal_framerate = 60;
	multiplier_framerate = 1;
	turboed = false;
}

//Set the speed multiplier. Note that INFINITE is a valid multiplier.
void framerate_regulator::set_speed_multiplier(double multiplier) throw()
{
	threads::alock h(framerate_lock);
	multiplier_framerate = multiplier;
}

//Get the speed multiplier. Note that this may be INFINITE.
double framerate_regulator::get_speed_multiplier() throw()
{
	threads::alock h(framerate_lock);
	return multiplier_framerate;
}

void framerate_regulator::freeze_time(uint64_t curtime)
{
	get_time(curtime, true);
	time_frozen = true;
}

void framerate_regulator::unfreeze_time(uint64_t curtime)
{
	if(time_frozen)
		last_time_update = curtime;
	time_frozen = false;
}

void framerate_regulator::set_nominal_framerate(double fps) throw()
{
	threads::alock h(framerate_lock);
	nominal_framerate = fps;
}

double framerate_regulator::get_realized_multiplier() throw()
{
	threads::alock h(framerate_lock);
	return get_realized_fps() / nominal_framerate;
}

void framerate_regulator::ack_frame_tick(uint64_t usec) throw()
{
	unfreeze_time(usec);
	add_frame(get_time(usec, true));
}

uint64_t framerate_regulator::to_wait_frame(uint64_t usec) throw()
{
	auto target = read_fps();
	if(!frame_number || target.first || turboed || graphics_driver_is_dummy())
		return 0;
	uint64_t lintime = get_time(usec, true);
	uint64_t frame_lasted = lintime - frame_start_times[0];
	uint64_t frame_should_last = 1000000 / target.second;
	if(frame_lasted >= frame_should_last)
		return 0;	//We are late.
	uint64_t history_frames = min(frame_number, static_cast<uint64_t>(FRAMERATE_HISTORY_FRAMES));
	uint64_t history_lasted = lintime - frame_start_times[history_frames - 1];
	uint64_t history_should_last = history_frames * 1000000 / target.second;
	if(history_lasted >= history_should_last)
		return 0;
	return min(history_should_last - history_lasted, frame_should_last - frame_lasted);
}

uint64_t framerate_regulator::get_utime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

#define MAXSLEEP 500000

void framerate_regulator::wait_usec(uint64_t usec)
{
	uint64_t sleep_end = get_utime() + usec;
	while(1) {
		uint64_t time_now = get_utime();
		if(time_now >= sleep_end)
			break;
		if(sleep_end < time_now + MAXSLEEP)
			usleep(sleep_end - time_now);
		else
			usleep(MAXSLEEP);
	}
}

uint64_t framerate_regulator::get_time(uint64_t curtime, bool update)
{
	if(curtime < last_time_update || time_frozen)
		return time_at_last_update;
	if(update) {
		time_at_last_update += (curtime - last_time_update);
		last_time_update = curtime;
		return time_at_last_update;
	} else
		return time_at_last_update + (curtime - last_time_update);
}

double framerate_regulator::get_realized_fps()
{
	if(frame_number < 2)
		return 0;
	unsigned loadidx = min(frame_number - 1, static_cast<uint64_t>(FRAMERATE_HISTORY_FRAMES) - 1);
	return (1000000.0 * loadidx) / (frame_start_times[0] - frame_start_times[loadidx] + 1);
}

void framerate_regulator::add_frame(uint64_t linear_time)
{
	for(size_t i = FRAMERATE_HISTORY_FRAMES - 2; i < FRAMERATE_HISTORY_FRAMES; i--)
		frame_start_times[i + 1] = frame_start_times[i];
	frame_start_times[0] = linear_time;
	frame_number++;
}

std::pair<bool, double> framerate_regulator::read_fps()
{
	double n, m;
	{
		threads::alock h(framerate_lock);
		n = nominal_framerate;
		m = multiplier_framerate;
	}
	if(m == std::numeric_limits<double>::infinity())
		return std::make_pair(true, 0);
	else
		return std::make_pair(false, n * m);
}
