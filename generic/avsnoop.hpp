#ifndef _avsnoop__hpp__included__
#define _avsnoop__hpp__included__

#include "render.hpp"
#include <list>
#include <string>
#include <stdexcept>

/**
 * A/V snooper. A/V snoopers allow code to snoop on audio samples and video frames, usually for purpose of dumping
 * them to video file.
 */
class av_snooper
{
public:
/**
 * Create new A/V snooper.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	av_snooper() throw(std::bad_alloc);

/**
 * Destroy A/V snooper. This will not call end method.
 */
	~av_snooper() throw();

/**
 * Dump a frame.
 *
 * parameter _frame: The frame to dump.
 * parameter fps_n: Current fps numerator.
 * parameter fps_d: Current fps denomerator.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping frame.
 */
	virtual void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d) throw(std::bad_alloc,
		std::runtime_error) = 0;

/**
 * Call frame() on all known A/V snoopers.
 *
 * parameter _frame: The frame to dump.
 * parameter fps_n: Current fps numerator.
 * parameter fps_d: Current fps denomerator.
 * throws std::bad_alloc: Not enough memory.
 */
	static void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, bool dummy)
		throw(std::bad_alloc);

/**
 * Dump a sample.
 *
 * parameter l: Left channel sample.
 * parameter r: Right channel sample.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping sample.
 */
	virtual void sample(short l, short r) throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * Call sample() on all known A/V snoopers.
 *
 * parameter l: Left channel sample.
 * parameter r: Right channel sample.
 * throws std::bad_alloc: Not enough memory.
 */
	static void sample(short l, short r, bool dummy) throw(std::bad_alloc);

/**
 * End dump.
 *
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping sample.
 */
	virtual void end() throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * Call end() on all known A/V snoopers.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void end(bool dummy) throw(std::bad_alloc);

/**
 * Notify game information.
 *
 * parameter gamename: Name of the game.
 * parameter authors: Authors of the run.
 * parameter gametime: Game time.
 * parameter rercords: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error recording this info.
 */
	virtual void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
		authors, double gametime, const std::string& rerecords) throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * Call gameinfo() on all known A/V snoopers. Also records the gameinfo.
 *
 * parameter gamename: Name of the game.
 * parameter authors: Authors of the run.
 * parameter gametime: Game time.
 * parameter rercords: Rerecord count.
 * throws std::bad_alloc Not enough memory.
 */
	static void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
		authors, double gametime, const std::string& rerecords, bool dummy) throw(std::bad_alloc);

/**
 * Send game info. If av_snooper::gameinfo() has been called, this causes gameinfo() method of this object to be
 * called with previously recorded information.
 */
	void send_gameinfo() throw();

/**
 * Is there at least one known A/V snooper?
 *
 * returns: True if there is at least one known A/V snooper, false if there are none.
 */
	static bool dump_in_progress() throw();

/**
 * Notifier for dumps starting/ending.
 */
	class dump_notification
	{
	public:
/**
 * Destructor.
 */
		virtual ~dump_notification() throw();
/**
 * Called on new dump starting.
 */
		virtual void dump_starting() throw();
/**
 * Called on dump ending.
 */
		virtual void dump_ending() throw();
	};

/**
 * Add a notifier for dumps starting/ending.
 *
 * parameter notifier: New notifier to add.
 */
	static void add_dump_notifier(dump_notification& notifier) throw(std::bad_alloc);

/**
 * Remove a notifier for dumps starting/ending.
 *
 * parameter notifier: Existing notifier to remove.
 */
	static void remove_dump_notifier(dump_notification& notifier) throw();
};

#endif
