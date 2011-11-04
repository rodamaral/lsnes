#ifndef _avsnoop__hpp__included__
#define _avsnoop__hpp__included__

#include "render.hpp"
#include <list>
#include <string>
#include <set>
#include <stdexcept>

/**
 * Video data region is NTSC.
 */
#define SNOOP_REGION_NTSC 0
/**
 * Video data region is PAL.
 */
#define SNOOP_REGION_PAL 1

/**
 * Information about run.
 */
struct gameinfo_struct
{
public:
/**
 * Construct game info.
 */
	gameinfo_struct() throw(std::bad_alloc);
/**
 * Game name.
 */
	std::string gamename;
/**
 * Run length in seconds.
 */
	double length;
/**
 * Rerecord count (base 10 ASCII)
 */
	std::string rerecords;
/**
 * Authors. The first components are real names, the second components are nicknames. Either (but not both) may be
 * blank.
 */
	std::vector<std::pair<std::string, std::string>> authors;
/**
 * Format human-redable representation of the length.
 *
 * Parameter digits: Number of sub-second digits to use.
 * Returns: The time formated.
 * Throws std::bad_alloc: Not enough memory.
 */
	std::string get_readable_time(unsigned digits) const throw(std::bad_alloc);
/**
 * Get number of authors.
 *
 * Returns: Number of authors.
 */
	size_t get_author_count() const throw();
/**
 * Get short name of author (nickname if present, otherwise full name).
 *
 * Parameter idx: Index of author (0-based).
 * Returns: The short name.
 * Throws std::bad_alloc: Not enough memory.
 */
	std::string get_author_short(size_t idx) const throw(std::bad_alloc);
/**
 * Get rerecord count as a number. If rerecord count is too high, returns the maximum representatible count.
 *
 * Returns: The rerecord count.
 */
	uint64_t get_rerecords() const throw();
};

/**
 * A/V snooper. A/V snoopers allow code to snoop on audio samples and video frames, usually for purpose of dumping
 * them to video file.
 */
class av_snooper
{
public:
/**
 * Create new A/V snooper, registering it for receiving callbacks.
 *
 * Parameter name: Name of the dumper.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	av_snooper(const std::string& name) throw(std::bad_alloc);

/**
 * Destroy A/V snooper, deregistering it. This will not call end() method.
 */
	~av_snooper() throw();

/**
 * Dump a frame.
 *
 * Parameter _frame: The frame to dump.
 * Parameter fps_n: Current fps numerator.
 * Parameter fps_d: Current fps denomerator.
 * Parameter raw: Raw frame data from bsnes.
 * Parameter hires: True if bsnes signals hires mode, false otherwise.
 * Parameter interlaced: True if bsnes signals interlaced mode, false otherwise.
 * Parameter overscan: True if bsnes signals overscan mode, false otherwise.
 * Parameter region: The region video data is for.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping frame.
 */
	virtual void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, const uint32_t* raw, bool hires,
		bool interlaced, bool overscan, unsigned region) throw(std::bad_alloc, std::runtime_error);

/**
 * Call frame() on all known A/V snoopers.
 *
 * Parameter _frame: The frame to dump.
 * Parameter fps_n: Current fps numerator.
 * Parameter fps_d: Current fps denomerator.
 * Parameter raw: Raw frame data from bsnes.
 * Parameter hires: True if bsnes signals hires mode, false otherwise.
 * Parameter interlaced: True if bsnes signals interlaced mode, false otherwise.
 * Parameter overscan: True if bsnes signals overscan mode, false otherwise.
 * Parameter region: The region video data is for.
 * throws std::bad_alloc: Not enough memory.
 */
	static void _frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, const uint32_t* raw, bool hires,
		bool interlaced, bool overscan, unsigned region) throw(std::bad_alloc);

/**
 * Dump a sample.
 *
 * parameter l: Left channel sample.
 * parameter r: Right channel sample.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping sample.
 */
	virtual void sample(short l, short r) throw(std::bad_alloc, std::runtime_error);

/**
 * Call sample() on all known A/V snoopers.
 *
 * parameter l: Left channel sample.
 * parameter r: Right channel sample.
 * throws std::bad_alloc: Not enough memory.
 */
	static void _sample(short l, short r) throw(std::bad_alloc);

/**
 * End dump.
 *
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error dumping sample.
 */
	virtual void end() throw(std::bad_alloc, std::runtime_error);

/**
 * Call end() on all known A/V snoopers.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void _end() throw(std::bad_alloc);

/**
 * Set sound sampling rate.
 *
 * parameter rate_n: Numerator of sampling rate.
 * parameter rate_d: Denomerator of sampling rate.
 */
	virtual void sound_rate(uint32_t rate_n, uint32_t rate_d) throw(std::bad_alloc, std::runtime_error);

/**
 * Call set_sound_rate() on all known A/V snoopers (and record the rate).
 *
 * parameter rate_n: Numerator of sampling rate.
 * parameter rate_d: Denomerator of sampling rate.
 */
	static void _sound_rate(uint32_t rate_n, uint32_t rate_d);

/**
 * Get the sound rate most recently set by _set_sound_rate().
 *
 * Returns: The first component is numerator of the sampling rate, the second component is the denomerator.
 */
	std::pair<uint32_t, uint32_t> get_sound_rate() throw();

/**
 * Notify game information.
 *
 * parameter gi: The information. Not guaranteed to remain stable after the call.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Error recording this info.
 */
	virtual void gameinfo(const struct gameinfo_struct& gi) throw(std::bad_alloc, std::runtime_error);

/**
 * Call gameinfo() on all known A/V snoopers. Also records the gameinfo.
 *
 * parameter gi: The information. Not guaranteed to remain stable after the call.
 * throws std::bad_alloc Not enough memory.
 */
	static void _gameinfo(const struct gameinfo_struct& gi) throw(std::bad_alloc);

/**
 * Get the last recorded game information.
 *
 * Returns: The last recorded gameinfo.
 */
	const struct gameinfo_struct& get_gameinfo() throw(std::bad_alloc);

/**
 * Is there at least one known A/V snooper?
 *
 * returns: True if there is at least one known A/V snooper, false if there are none.
 */
	static bool dump_in_progress() throw();
/**
 * Get the names of the active dumpers.
 */
	static const std::set<std::string>& active_dumpers();
/**
 * Notifier for dumps starting/ending.
 */
	class dump_notification
	{
	public:
/**
 * Register dump notifier.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
		dump_notification() throw(std::bad_alloc);
/**
 * Destructor. Deregister notifier.
 */
		virtual ~dump_notification() throw();
/**
 * Called on new dump starting.
 *
 * Parameter type: The type of new dumper.
 */
		virtual void dump_starting(const std::string& type) throw();
/**
 * Called on dump ending.
 *
 * Parameter type: The type of dumper going away.
 */
		virtual void dump_ending(const std::string& type) throw();
	};
private:
	std::string s_name;
};

#endif
