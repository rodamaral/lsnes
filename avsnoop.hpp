#ifndef _avsnoop__hpp__included__
#define _avsnoop__hpp__included__

#include "render.hpp"
#include <list>
#include <string>
#include <stdexcept>
#include "window.hpp"

/**
 * A/V snooper.
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
	virtual void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, window* win, bool dummy)
		throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * Dump a frame.
 * 
 * parameter _frame: The frame to dump.
 * parameter fps_n: Current fps numerator.
 * parameter fps_d: Current fps denomerator.
 * parameter win: Graphics system handle.
 */
	static void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, window* win) throw();

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
 * Dump a sample.
 * 
 * parameter l: Left channel sample.
 * parameter r: Right channel sample.
 * parameter win: Graphics system handle.
 */
	static void sample(short l, short r, window* win) throw();

/**
 * End dump.
 * 
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping sample.
 */
	virtual void end() throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * End dump.
 * 
 * parameter win: Graphics system handle.
 */
	static void end(window* win) throw();

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
 * Notify game information.
 * 
 * parameter gamename: Name of the game.
 * parameter authors: Authors of the run.
 * parameter gametime: Game time.
 * parameter rercords: Rerecord count.
 * parameter win: Graphics system handle.
 * throws std::bad_alloc Not enough memory.
 */
	static void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
		authors, double gametime, const std::string& rerecords, window* win) throw(std::bad_alloc);

/**
 * Send game info. This causes gameinfo method to be called on object this method is called on.
 */
	void send_gameinfo() throw();

/**
 * Is there dump in progress?
 * 
 * returns: True if dump is in progress, false if not.
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
 * New dump starting.
 */
		virtual void dump_starting() throw();
/**
 * Dump ending.
 */
		virtual void dump_ending() throw();
	};

/**
 * Add a notifier.
 * 
 * parameter notifier: New notifier to add.
 */
	static void add_dump_notifier(dump_notification& notifier) throw(std::bad_alloc);

/**
 * Remove a notifier.
 * 
 * parameter notifier: Existing notifier to remove.
 */
	static void remove_dump_notifier(dump_notification& notifier) throw();
};

#endif
