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

namespace
{
	double nominal_rate = DEFAULT_NOMINAL_RATE;
	double fps_value = 0;
	const double exp_factor = 0.97;
	uint64_t last_frame_usec = 0;
	bool last_frame_usec_valid = false;
	bool target_nominal = true;
	double target_fps = DEFAULT_NOMINAL_RATE;
	bool target_infinite = false;
	uint64_t wait_duration = 0;

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
			else {
				std::ostringstream o;
				o << target_fps;
				return o.str();
			}
		}

	} targetfps;
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
	return fps_value;
}

void ack_frame_tick(uint64_t usec) throw()
{
	if(!last_frame_usec_valid) {
		last_frame_usec = usec;
		last_frame_usec_valid = true;
		return;
	}
	uint64_t frame_usec = usec - last_frame_usec;
	fps_value = exp_factor * fps_value + (1 - exp_factor) * (1000000.0 / frame_usec);
	last_frame_usec = usec;
}

uint64_t to_wait_frame(uint64_t usec) throw()
{
	//Very simple algorithm. TODO: Make better one.
	if(!last_frame_usec_valid || target_infinite)
		return 0;
	if(get_framerate() < target_fps && wait_duration > 0)
		wait_duration -= 1000;
	else if(get_framerate() > target_fps)
		wait_duration += 1000;
	return wait_duration;
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
