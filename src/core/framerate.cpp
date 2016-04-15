#include "core/command.hpp"
#include "core/framerate.hpp"
#include "cmdhelp/turbo.hpp"
#include "core/instance.hpp"
#include "core/keymapper.hpp"
#include "core/moviedata.hpp"
#include "core/messages.hpp"
#include "library/minmax.hpp"

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <unistd.h>
#include <limits>
#include <cmath>

bool graphics_driver_is_dummy();

framerate_regulator::framerate_regulator(command::group& _cmd)
	: cmd(_cmd),
	turbo_p(cmd, CTURBO::p, [this]() { this->turboed = true; }),
	turbo_r(cmd, CTURBO::r, [this]() { this->turboed = false; }),
	turbo_t(cmd, CTURBO::t, [this]() { this->turboed = !this->turboed; }),
	setspeed_t(cmd, CTURBO::ss, [this](const std::string& args) { this->set_speed_cmd(args); }),
	spd_inc(cmd, CTURBO::inc, [this]() { this->increase_speed(); }),
	spd_dec(cmd, CTURBO::dec, [this]() { this->decrease_speed(); })
{
	framerate_realtime_locked = false;
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
	framerate_realtime_locked = false;
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
	double old_nominal_framerate = nominal_framerate;
	nominal_framerate = fps;
	//If framerate is realtime-locked, adjust the framerate multiplier as nominal framerate changes.
	//E.g. if multiplier is 1/30 and nominal framerate changes from 60 to 30, the multiplier needs to be
	//adjusted to 1/15.
	if(framerate_realtime_locked) {
		multiplier_framerate *= old_nominal_framerate / nominal_framerate;
	}
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

void framerate_regulator::set_speed_cmd(const std::string& args)
{
	if(args == "turbo") {
		set_speed_multiplier(std::numeric_limits<double>::infinity());
		return;
	}
	try {
		double mul = parse_value<double>(args);
		if(mul <= 0)
			throw 42;	//Zero and negative is not allowed.
		set_speed_multiplier(mul);
	} catch(...) {
		messages << "Expected positive speed multiplier or \"turbo\"" << std::endl;
	}
}

namespace
{
	double seconds_per_frame[] = {4, 3, 2, 1, 0.5, 0.2};
	double relative_speed[] = {0.01, 0.04, 0.1, 0.2, 0.25, 0.333, 0.5, 1, 2, 3, 5, 10};

	std::vector<std::pair<double, bool>> construct_speedscale(double basefps)
	{
		std::vector<std::pair<double, bool>> ret;
		unsigned idx1 = 0;
		unsigned idx2 = 0;
		unsigned size1 = sizeof(seconds_per_frame)/sizeof(seconds_per_frame[0]);
		unsigned size2 = sizeof(relative_speed)/sizeof(relative_speed[0]);
		while(idx1 < size1 || idx2 < size2) {
			double x1 = std::numeric_limits<double>::infinity();
			double x2 = std::numeric_limits<double>::infinity();
			if(idx1 < size1) x1 = 1 / (seconds_per_frame[idx1] * basefps);
			if(idx2 < size2) x2 = relative_speed[idx2];
			if(x1 < x2) {
				ret.push_back(std::make_pair(x1, true));
				idx1++;
			} else if(x1 > x2) {
				ret.push_back(std::make_pair(x2, false));
				idx2++;
			} else {
				ret.push_back(std::make_pair(x2, true));
				idx1++;
				idx2++;
			}
		}
		return ret;
	}
}

//Step should be ODD.
void framerate_regulator::set_speedstep(unsigned step)
{
	auto scale = construct_speedscale(nominal_framerate);
	step = (step - 1) / 2;
	if(step >= scale.size()) {
		//Infinity.
		multiplier_framerate = std::numeric_limits<double>::infinity();
		framerate_realtime_locked = false;
		messages << "Speed set to turbo." << std::endl;
		return;
	}
	auto _step = scale[step];
	multiplier_framerate = _step.first;
	framerate_realtime_locked = _step.second;
	if(framerate_realtime_locked)
		messages << "Speed set to " << multiplier_framerate * nominal_framerate << "fps." << std::endl;
	else
		messages << "Speed set to " << (double)(unsigned)(multiplier_framerate * 1000) / 10 << "%."
			<< std::endl;
}

#define SPD_TOLERANCE 1e-10

//{1/100, 1/fps, 2/fps, 1/10, 1/5, 1/3, 1/2, 1, 2, 3, 5, 10}
//Step can be EVEN if between steps.
unsigned framerate_regulator::get_speedstep()
{
	auto scale = construct_speedscale(nominal_framerate);
	if(multiplier_framerate == std::numeric_limits<double>::infinity())
		return 2 * scale.size() + 1;	//Infinity.
	unsigned idx = 0;
	for(auto i: scale) {
		if(multiplier_framerate < i.first)
			return idx;	//Between steps.
		if(fabs(multiplier_framerate) - i.first < SPD_TOLERANCE)
			return idx + 1;	//On step.
		idx += 2;
	}
	return 2 * scale.size();	//Above maximum step but below infinity.
}

void framerate_regulator::increase_speed() throw()
{
	threads::alock h(framerate_lock);
	unsigned step = get_speedstep();
	if(step < 2) return;	//At or below minimum speed in scale.
	if(step & 1)
		step-=2;	//If step is odd, decrement by 2 (full step).
	else
		step--;		//If step is even, decrement by 1 to reach previous step.
	set_speedstep(step);
}

void framerate_regulator::decrease_speed() throw()
{
	threads::alock h(framerate_lock);
	unsigned step = get_speedstep();
	if(multiplier_framerate == std::numeric_limits<double>::infinity()) return;	//Already turbo.
	if(step & 1)
		step+=2;	//If step is odd, increment by 2 (full step).
	else
		step++;		//If step is even, increment by 1 to reach next step.
	set_speedstep(step);
}
