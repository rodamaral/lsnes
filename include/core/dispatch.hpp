#ifndef _dispatch__hpp__included__
#define _dispatch__hpp__included__

#include "library/framebuffer.hpp"
#include "library/dispatch.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <set>
#include <map>

/**
 * Video data region is NTSC.
 */
#define VIDEO_REGION_NTSC 0
/**
 * Video data region is PAL.
 */
#define VIDEO_REGION_PAL 1

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
 * Information dispatch.
 *
 * This class handles dispatching information between the components of the emulator.
 *
 * Each kind of information has virtual method on_foo() which can be overridden to handle events of that type, and
 * static method do_foo(), which calls on_foo on all known objects.
 *
 * The information is delivered to each instance of this class.
 */
class information_dispatch
{
public:
/**
 * Create new object to receive information dispatch events.
 *
 * Parameter name: The name for the object.
 * Throws std::bad_alloc: Not enough memory.
 */
	information_dispatch(const std::string& name) throw(std::bad_alloc);
/**
 * Destroy object.
 */
	virtual ~information_dispatch() throw();
/**
 * A frame has been received.
 *
 * The default handler does nothing.
 *
 * Parameter _frame: The frame object.
 * Parameter fps_n: Numerator of current video fps.
 * Parameter fps_d: Denominator of current video fps.
 */
	virtual void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d);
/**
 * Call all on_frame() handlers.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 */
	static void do_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d) throw();
/**
 * A sample has been received.
 *
 * The default handler does nothing.
 *
 * Parameter l: The left channel sample (16 bit signed).
 * Parameter r: The right channel sample (16 bit signed).
 */
	virtual void on_sample(short l, short r);
/**
 * Call all on_sample() handlers.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 */
	static void do_sample(short l, short r) throw();
/**
 * Dump is ending.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 *
 * The default handler does nothing.
 */
	virtual void on_dump_end();
/**
 * Call all on_dump_end() handlers.
 */
	static void do_dump_end() throw();
/**
 * Sound sampling rate is changing
 *
 * The default handler prints warning if dumper flag is set.
 *
 * Parameter rate_n: Numerator of the new sampling rate in Hz.
 * Parameter rate_d: Denominator of the new sampling rate in Hz.
 */
	virtual void on_sound_rate(uint32_t rate_n, uint32_t rate_d);
/**
 * Call all on_sound_rate() methods and save the parameters.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 */
	static void do_sound_rate(uint32_t rate_n, uint32_t rate_d) throw();
/**
 * Get the sound rate most recently set by on_sound_rate().
 *
 * Returns: The first component is the numerator, the second is the denominator.
 */
	static std::pair<uint32_t, uint32_t> get_sound_rate() throw();
/**
 * Game information is changing.
 *
 * The default handler does nothing.
 *
 * Parameter gi: The new game info.
 */
	virtual void on_gameinfo(const struct gameinfo_struct& gi);
/**
 * Call all on_gameinfo() handlers and save the gameinfo.
 */
	static void do_gameinfo(const struct gameinfo_struct& gi) throw();
/**
 * Get the gameinfo most recently set by do_gameinfo().
 *
 * Returns: The gameinfo.
 */
	static const struct gameinfo_struct& get_gameinfo() throw();
/**
 * Return the dumper flag for this dispatch target.
 *
 * The default implementation returns false.
 *
 * If dumper flag is set:
 *	- on_sound_rate() default handler prints a warning.
 *	- All dumping related do_* functions triggers calls to to on_new_dumper() on all handlers the next time they
 *	  are called.
 *	- Destroying the handler triggers calls to on_destroy_dumper() on all handlers (if on_new_dumper() has been
 *	  called).
 */
	virtual bool get_dumper_flag() throw();
/**
 * Notify that a new dumper is joining.
 *
 * Parameter dumper: The name of the dumper object.
 */
	virtual void on_new_dumper(const std::string& dumper);
/**
 * Notify that a dumper is leaving.
 *
 * Parameter dumper: The name of the dumper object.
 */
	virtual void on_destroy_dumper(const std::string& dumper);
/**
 * Get number of active dumpers.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 *
 * Returns: The dumper count.
 */
	static unsigned get_dumper_count() throw();
/**
 * Get set of active dumpers.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 *
 * Returns: The set of dumper names.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::set<std::string> get_dumpers() throw(std::bad_alloc);
/**
 * Get name of target.
 */
	const std::string& get_name() throw();
/**
 * Notify that some dumper has attached, deattached, popped into existence or disappeared.
 *
 * Default implementation does nothing.
 */
	virtual void on_dumper_update();
/**
 * Call on_dumper_update on on all objects.
 */
	static void do_dumper_update() throw();
protected:
/**
 * Call to indicate this target is interested in sound sample data.
 */
	void enable_send_sound() throw(std::bad_alloc);
private:
	static void update_dumpers(bool nocalls = false) throw();
	bool known_if_dumper;
	bool marked_as_dumper;
	std::string target_name;
	bool notified_as_dumper;
};

void dispatch_set_error_streams(std::ostream* stream);

extern struct dispatch::source<> notify_autohold_reconfigure;
extern struct dispatch::source<unsigned, unsigned, unsigned, bool> notify_autohold_update;
extern struct dispatch::source<unsigned, unsigned, unsigned, unsigned, unsigned> notify_autofire_update;
extern struct dispatch::source<> notify_close;
extern struct dispatch::source<framebuffer::fb<false>&> notify_set_screen;
extern struct dispatch::source<std::pair<std::string, std::string>> notify_sound_change;
extern struct dispatch::source<> notify_screen_update;
extern struct dispatch::source<> notify_status_update;
extern struct dispatch::source<bool> notify_sound_unmute;
extern struct dispatch::source<bool> notify_mode_change;
extern struct dispatch::source<> notify_core_change;
extern struct dispatch::source<> notify_title_change;
extern struct dispatch::source<bool> notify_core_changed;
extern struct dispatch::source<> notify_new_core;
extern struct dispatch::source<> notify_voice_stream_change;
extern struct dispatch::source<> notify_vu_change;
extern struct dispatch::source<> notify_subtitle_change;
extern struct dispatch::source<unsigned, unsigned, int> notify_multitrack_change;

#endif
