#include "core/command.hpp"
#include "core/framerate.hpp"
#include "core/keymapper.hpp"
#include "core/settings.hpp"
#include "library/minmax.hpp"

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>

#define HISTORY_FRAMES 10

namespace
{
	uint64_t last_time_update = 0;
	uint64_t time_at_last_update = 0;
	bool time_frozen = true;
	uint64_t frame_number = 0;
	uint64_t frame_start_times[HISTORY_FRAMES];
	double nominal_rate = 100;
	bool target_nominal = true;
	double target_fps = 100.0;
	bool target_infinite = false;

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

	struct setting_targetfps : public setting
	{
		setting_targetfps() throw(std::bad_alloc)
			: setting("targetfps")
		{
		}

		bool blank(bool really) throw(std::bad_alloc, std::runtime_error)
		{
			if(!really)
				return true;
			target_nominal = true;
			target_infinite = false;
			target_fps = 100.0;
			return true;
		}

		bool is_set() throw()
		{
			return !target_nominal;
		}

		virtual void set(const std::string& value) throw(std::bad_alloc, std::runtime_error)
		{
			double tmp;
			const char* s;
			char* e;
			if(value == "infinite") {
				target_infinite = true;
				target_nominal = false;
				return;
			}
			s = value.c_str();
			tmp = strtod(s, &e);
			if(*e)
				throw std::runtime_error("Invalid frame rate");
			if(tmp < 0.001)
				throw std::runtime_error("Target frame rate must be at least 0.001fps");
			target_fps = tmp;
			target_infinite = false;
			target_nominal = false;
		}

		virtual std::string get() throw(std::bad_alloc)
		{
			if(target_nominal)
				return "";
			else if(target_infinite)
				return "infinite";
			else {
				std::ostringstream o;
				o << target_fps;
				return o.str();
			}
		}

		std::pair<bool, double> read() throw()
		{
			lock_holder lck(this);
			return std::make_pair(target_infinite, target_fps);
		}

		void set_nominal_framerate(double fps)
		{
			lock_holder(this);
			nominal_rate = fps;
			if(target_nominal) {
				target_fps = nominal_rate;
				target_infinite = false;
			}
		}

		double get_framerate()
		{
			lock_holder(this);
			return 100.0 * get_realized_fps() / nominal_rate;
		}
	} targetfps;

	bool turboed = false;

	function_ptr_command<> tturbo("toggle-turbo", "Toggle turbo",
		"Syntax: toggle-turbo\nToggle turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			turboed = !turboed;
		});

	function_ptr_command<> pturbo("+turbo", "Activate turbo",
		"Syntax: +turbo\nActivate turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			turboed = true;
		});

	function_ptr_command<> nturbo("-turbo", "Deactivate turbo",
		"Syntax: -turbo\nDeactivate turbo mode.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			turboed = false;
		});

	inverse_key turboh("+turbo", "Speed‣Turbo hold");
	inverse_key turbot("toggle-turbo", "Speed‣Turbo toggle");
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
	targetfps.set_nominal_framerate(fps);
}

double get_framerate() throw()
{
	return targetfps.get_framerate();
}

void ack_frame_tick(uint64_t usec) throw()
{
	unfreeze_time(usec);
	add_frame(get_time(usec, true));
}

uint64_t to_wait_frame(uint64_t usec) throw()
{
	auto target = targetfps.read();
	if(!frame_number || target.first || turboed)
		return 0;
	uint64_t lintime = get_time(usec, true);
	uint64_t frame_lasted = lintime - frame_start_times[0];
	uint64_t frame_should_last = 1000000 / (target.second * nominal_rate / 100);
	if(frame_lasted >= frame_should_last)
		return 0;	//We are late.
	uint64_t history_frames = min(frame_number, static_cast<uint64_t>(HISTORY_FRAMES));
	uint64_t history_lasted = lintime - frame_start_times[history_frames - 1];
	uint64_t history_should_last = history_frames * 1000000 / (target.second * nominal_rate / 100);
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
