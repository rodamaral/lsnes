#ifndef _framerate__hpp__included__
#define _framerate__hpp__included__

#include <cstdint>
#include "library/threads.hpp"
#include "library/command.hpp"

#define FRAMERATE_HISTORY_FRAMES 10

/**
 * Framerate regulator.
 */
class framerate_regulator
{
public:
	framerate_regulator(command::group& _cmd);
/**
 * Set the target speed multiplier.
 *
 * Parameter multiplier: The multiplier target. May be INFINITE.
 */
	void set_speed_multiplier(double multiplier) throw();

/**
 * Increase the speed to next step.
 */
	void increase_speed() throw();

/**
 * Decrease the speed to next step.
 */
	void decrease_speed() throw();

/**
 * Get the target speed multiplier.
 *
 * Returns: The multiplier. May be INFINITE.
 */
	double get_speed_multiplier() throw();

/**
 * Sets the nominal frame rate. Framerate limiting tries to maintain the nominal framerate when there is no other
 * explict framerate to maintain.
 */
	void set_nominal_framerate(double fps) throw();

/**
 * Returns the current realized framerate multiplier.
 *
 * returns: The framerate multiplier the system is currently archiving.
 */
	double get_realized_multiplier() throw();

/**
 * Freeze time.
 *
 * Parameter usec: Current time in microseconds.
 */
	void freeze_time(uint64_t usec);

/**
 * Unfreeze time.
 *
 * Parameter usec: Current time in microseconds.
 */
	void unfreeze_time(uint64_t usec);

/**
 * Acknowledge frame start for timing purposes. If time is frozen, it is automatically unfrozen.
 *
 * parameter usec: Current time (relative to some unknown epoch) in microseconds.
 */
	void ack_frame_tick(uint64_t usec) throw();

/**
 * Computes the number of microseconds to wait for next frame.
 *
 * parameter usec: Current time (relative to some unknown epoch) in microseconds.
 * returns: Number of more microseconds to wait.
 */
	uint64_t to_wait_frame(uint64_t usec) throw();

/**
 * Return microsecond-resolution time since unix epoch.
 */
	static uint64_t get_utime();

/**
 * Wait specified number of microseconds.
 */
	void wait_usec(uint64_t usec);
/**
 * Turbo flag.
 */
	bool turboed;
private:
	void set_speed_cmd(const std::string& args);
	uint64_t get_time(uint64_t curtime, bool update);
	double get_realized_fps();
	void add_frame(uint64_t linear_time);
	std::pair<bool, double> read_fps();
	//Step should be ODD.
	void set_speedstep(unsigned step);
	//Step can be EVEN if between steps.
	unsigned get_speedstep();
	uint64_t last_time_update;
	uint64_t time_at_last_update;
	bool time_frozen;
	uint64_t frame_number;
	uint64_t frame_start_times[FRAMERATE_HISTORY_FRAMES];
	//Framerate.
	double nominal_framerate;
	double multiplier_framerate;
	bool framerate_realtime_locked;
	threads::lock framerate_lock;
	command::group& cmd;
	command::_fnptr<> turbo_p;
	command::_fnptr<> turbo_r;
	command::_fnptr<> turbo_t;
	command::_fnptr<const std::string&> setspeed_t;
	command::_fnptr<> spd_inc;
	command::_fnptr<> spd_dec;
};

#endif
