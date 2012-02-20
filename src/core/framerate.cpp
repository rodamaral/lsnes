#include "core/framerate.hpp"
#include "core/settings.hpp"

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>

#define DEFAULT_NOMINAL_RATE 60
#define HISTORY_FRAMES 10

namespace
{
	uint64_t last_time_update = 0;
	uint64_t time_at_last_update = 0;
	bool time_frozen = true;
	uint64_t frame_number = 0;
	uint64_t frame_start_times[HISTORY_FRAMES];
	double nominal_rate = DEFAULT_NOMINAL_RATE;
	bool target_nominal = true;
	double target_fps = DEFAULT_NOMINAL_RATE;
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
		if(frame_number >= HISTORY_FRAMES)
 			return (1000000.0 * (HISTORY_FRAMES - 1)) / (frame_start_times[0] - frame_start_times[HISTORY_FRAMES - 1] + 1);
		return (1000000.0 * (frame_number - 1)) / (frame_start_times[0] - frame_start_times[frame_number - 1] + 1);
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

		void blank() throw(std::bad_alloc, std::runtime_error)
		{
			target_nominal = true;
			target_infinite = false;
			target_fps = nominal_rate;
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

	} targetfps;
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
	nominal_rate = fps;
	if(target_nominal) {
		target_fps = nominal_rate;
		target_infinite = false;
	}
}

double get_framerate() throw()
{
	return 100.0 * get_realized_fps() / nominal_rate;
}

void ack_frame_tick(uint64_t usec) throw()
{
	unfreeze_time(usec);
	add_frame(get_time(usec, true));
}

uint64_t to_wait_frame(uint64_t usec) throw()
{
	if(!frame_number || target_infinite)
		return 0;
	uint64_t lintime = get_time(usec, true);
	uint64_t frame_lasted = lintime - frame_start_times[0];
	uint64_t frame_should_last = 1000000 / target_fps;
	if(frame_lasted >= frame_should_last)
		return 0;	//We are late.
	uint64_t maxwait = frame_should_last - frame_lasted;
	uint64_t history_frames = (frame_number < HISTORY_FRAMES) ? frame_number : HISTORY_FRAMES;
	uint64_t history_lasted = lintime - frame_start_times[history_frames - 1];
	uint64_t history_should_last = history_frames * 1000000 / target_fps;
	if(history_lasted >= history_should_last)
		return 0;		
	uint64_t history_wait = history_should_last - history_lasted;
	if(history_wait > maxwait)
		history_wait = maxwait;
	return history_wait;
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
