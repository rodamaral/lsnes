#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/settings.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "library/minmax.hpp"

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>
#include <limits>

#define HISTORY_FRAMES 10

bool graphics_driver_is_dummy();

namespace
{
	uint64_t last_time_update = 0;
	uint64_t time_at_last_update = 0;
	bool time_frozen = true;
	uint64_t frame_number = 0;
	uint64_t frame_start_times[HISTORY_FRAMES];
	//Framerate.
	double nominal_framerate = 60;
	double multiplier_framerate = 1;
	mutex_class framerate_lock;
	bool turboed = false;

	uint64_t get_time(uint64_t curtime, bool update)
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

	double get_realized_fps()
	{
		if(frame_number < 2)
			return 0;
		unsigned loadidx = min(frame_number - 1, static_cast<uint64_t>(HISTORY_FRAMES) - 1);
		return (1000000.0 * loadidx) / (frame_start_times[0] - frame_start_times[loadidx] + 1);
	}

	void add_frame(uint64_t linear_time)
	{
		for(size_t i = HISTORY_FRAMES - 2; i < HISTORY_FRAMES; i--)
			frame_start_times[i + 1] = frame_start_times[i];
		frame_start_times[0] = linear_time;
		frame_number++;
	}

	std::pair<bool, double> read_fps()
	{
		double n, m;
		{
			umutex_class h(framerate_lock);
			n = nominal_framerate;
			m = multiplier_framerate;
		}
		if(m == std::numeric_limits<double>::infinity())
			return std::make_pair(true, 0);
		else
			return std::make_pair(false, n * m);
	}

	command::fnptr<> tturbo(lsnes_cmd, "toggle-turbo", "Toggle turbo",
		"Syntax: toggle-turbo\nToggle turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			turboed = !turboed;
		});

	command::fnptr<> pturbo(lsnes_cmd, "+turbo", "Activate turbo",
		"Syntax: +turbo\nActivate turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			turboed = true;
		});

	command::fnptr<> nturbo(lsnes_cmd, "-turbo", "Deactivate turbo",
		"Syntax: -turbo\nDeactivate turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			turboed = false;
		});

	keyboard::invbind turboh(lsnes_mapper, "+turbo", "Speed‣Turbo hold");
	keyboard::invbind turbot(lsnes_mapper, "toggle-turbo", "Speed‣Turbo toggle");
}


//Set the speed multiplier. Note that INFINITE is a valid multiplier.
void set_speed_multiplier(double multiplier) throw()
{
	umutex_class h(framerate_lock);
	multiplier_framerate = multiplier;
}

//Get the speed multiplier. Note that this may be INFINITE.
double get_speed_multiplier() throw()
{
	umutex_class h(framerate_lock);
	return multiplier_framerate;
}

void freeze_time(uint64_t curtime)
{
	get_time(curtime, true);
	time_frozen = true;
}

void unfreeze_time(uint64_t curtime)
{
	if(time_frozen)
		last_time_update = curtime;
	time_frozen = false;
}

void set_nominal_framerate(double fps) throw()
{
	umutex_class h(framerate_lock);
	nominal_framerate = fps;
}

double get_realized_multiplier() throw()
{
	umutex_class h(framerate_lock);
	return get_realized_fps() / nominal_framerate;
}

void ack_frame_tick(uint64_t usec) throw()
{
	unfreeze_time(usec);
	add_frame(get_time(usec, true));
}

uint64_t to_wait_frame(uint64_t usec) throw()
{
	auto target = read_fps();
	if(!frame_number || target.first || turboed || graphics_driver_is_dummy())
		return 0;
	uint64_t lintime = get_time(usec, true);
	uint64_t frame_lasted = lintime - frame_start_times[0];
	uint64_t frame_should_last = 1000000 / target.second;
	if(frame_lasted >= frame_should_last)
		return 0;	//We are late.
	uint64_t history_frames = min(frame_number, static_cast<uint64_t>(HISTORY_FRAMES));
	uint64_t history_lasted = lintime - frame_start_times[history_frames - 1];
	uint64_t history_should_last = history_frames * 1000000 / target.second;
	if(history_lasted >= history_should_last)
		return 0;
	return min(history_should_last - history_lasted, frame_should_last - frame_lasted);
}

uint64_t get_utime()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

#define MAXSLEEP 500000

void wait_usec(uint64_t usec)
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
