#ifndef _advdumper__hpp__included__
#define _advdumper__hpp__included__

#include <string>
#include <set>
#include <stdexcept>
#include <iostream>

#include "library/framebuffer.hpp"
#include "library/dispatch.hpp"
#include "library/threads.hpp"
#include "library/text.hpp"

class master_dumper;
class dumper_factory_base;
class dumper_base;
class lua_state;

class dumper_factory_base
{
public:
/**
 * Notifier base.
 */
	class notifier
	{
	public:
		virtual ~notifier() throw();
		virtual void dumpers_updated() throw() = 0;
	};
/**
 * Detail flags.
 */
	static const unsigned target_type_mask;
	static const unsigned target_type_file;
	static const unsigned target_type_prefix;
	static const unsigned target_type_special;
/**
 * Register a dumper.
 *
 * Parameter id: The ID of dumper.
 * Throws std::bad_alloc: Not enough memory.
 */
	dumper_factory_base(const text& id) throw(std::bad_alloc);
/**
 * Unregister a dumper.
 */
	~dumper_factory_base();
/**
 * Get ID of dumper.
 *
 * Returns: The id.
 */
	const text& id() throw();
/**
 * Get set of all dumpers.
 *
 * Returns: The set.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::set<dumper_factory_base*> get_dumper_set() throw(std::bad_alloc);
/**
 * List all valid submodes.
 *
 * Returns: List of all valid submodes. Empty list means this dumper has no submodes.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::set<text> list_submodes() throw(std::bad_alloc) = 0;
/**
 * Get mode details
 *
 * parameter mode: The submode.
 * Returns: Mode details flags
 */
	virtual unsigned mode_details(const text& mode) throw() = 0;
/**
 * Get mode extensions. Only called if mode details specifies that output is a single file.
 *
 * parameter mode: The submode.
 * Returns: Mode extension
 */
	virtual text mode_extension(const text& mode) throw() = 0;
/**
 * Get human-readable name for this dumper.
 *
 * Returns: The name.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual text name() throw(std::bad_alloc) = 0;
/**
 * Get human-readable name for submode.
 *
 * Parameter mode: The submode.
 * Returns: The name.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual text modename(const text& mode) throw(std::bad_alloc) = 0;
/**
 * Start dump.
 *
 * parameter mode: The mode to dump using.
 * parameter targetname: The target filename or prefix.
 * returns: The dumper object.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Can't start dump.
 */
	virtual dumper_base* start(master_dumper& _mdumper, const text& mode, const text& targetname)
		throw(std::bad_alloc, std::runtime_error) = 0;
/**
 * Is hidden?
 */
	virtual bool hidden() const { return false; }
/**
 * Add dumper update notifier object.
 */
	static void add_notifier(notifier& n);
/**
 * Remove dumper update notifier object.
 */
	static void drop_notifier(notifier& n);
/**
 * Notify ctor finished.
 */
	void ctor_notify();
/**
 * Notify dumper change.
 */
	static void run_notify();
private:
	text d_id;
};

class master_dumper
{
public:
/**
 * Information about run.
 */
	struct gameinfo
	{
	public:
/**
 * Construct game info.
 */
		gameinfo() throw(std::bad_alloc);
/**
 * Game name.
 */
		text gamename;
/**
 * Run length in seconds.
 */
		double length;
/**
 * Rerecord count (base 10 ASCII)
 */
		text rerecords;
/**
 * Authors. The first components are real names, the second components are nicknames. Either (but not both) may be
 * blank.
 */
		std::vector<std::pair<text, text>> authors;
/**
 * Format human-redable representation of the length.
 *
 * Parameter digits: Number of sub-second digits to use.
 * Returns: The time formated.
 * Throws std::bad_alloc: Not enough memory.
 */
		text get_readable_time(unsigned digits) const throw(std::bad_alloc);
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
		text get_author_short(size_t idx) const throw(std::bad_alloc);
/**
 * Get long name of author (full name and nickname if present).
 *
 * Parameter idx: Index of author (0-based).
 * Returns: The long name.
 * Throws std::bad_alloc: Not enough memory.
 */
		text get_author_long(size_t idx) const throw(std::bad_alloc);
/**
 * Get rerecord count as a number. If rerecord count is too high, returns the maximum representatible count.
 *
 * Returns: The rerecord count.
 */
		uint64_t get_rerecords() const throw();
	};
/**
 * Notifier.
 */
	class notifier
	{
	public:
		virtual ~notifier() throw();
		virtual void dump_status_change() = 0;
	};
/**
 * Ctor.
 */
	master_dumper(lua_state& _lua2);
/**
 * Get instance for specified dumper.
 */
	dumper_base* get_instance(dumper_factory_base* dumper) throw();
/**
 * Is dumper busy in this instance?
 */
	bool busy(dumper_factory_base* dumper) throw()
	{
		return get_instance(dumper) != NULL;
	}
/**
 * Call start on dumper.
 */
	dumper_base* start(dumper_factory_base& factory, const text& mode, const text& targetname)
		throw(std::bad_alloc, std::runtime_error);
/**
 * Add dumper update notifier object.
 */
	void add_notifier(notifier& n);
/**
 * Remove dumper update notifier object.
 */
	void drop_notifier(notifier& n);
/**
 * Add dumper update notifier object.
 */
	void add_dumper(dumper_base& n);
/**
 * Remove dumper update notifier object.
 */
	void drop_dumper(dumper_base& n);
/**
 * Get number of active dumpers.
 */
	unsigned get_dumper_count() throw();
/**
 * Call all notifiers (on_frame).
 */
	void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d);
/**
 * Call all notifiers (on_sample).
 */
	void on_sample(short l, short r);
/**
 * Call all notifiers (on_rate_change)
 *
 * Also changes builtin rate variables.
 */
	void on_rate_change(uint32_t n, uint32_t d);
/**
 * Call all notifiers (on_gameinfo_change)
 *
 * Also changes builtin gameinfo structure.
 */
	void on_gameinfo_change(const gameinfo& gi);
/**
 * Get current sound rate in effect.
 */
	std::pair<uint32_t, uint32_t> get_rate();
/**
 * Get current gameinfo in effect.
 */
	const gameinfo& get_gameinfo();
/**
 * End all dumps.
 */
	void end_dumps();
/**
 * Set output stream.
 */
	void set_output(std::ostream* _output);
/**
 * Render Lua HUD on video.
 *
 * Parameter target: The target screen to render on.
 * Parameter source: The source screen to read.
 * Parameter hscl: The horizontal scale factor.
 * Parameter vscl: The vertical scale factor.
 * Parameter lgap: Left gap.
 * Parameter tgap: Top gap.
 * Parameter rgap: Right gap
 * Parameter bgap: Bottom gap.
 * Parameter fn: Function to call between running lua hooks and actually rendering.
 * Returns: True if frame should be dumped, false if not.
 */
	template<bool X> bool render_video_hud(struct framebuffer::fb<X>& target, struct framebuffer::raw& source,
		uint32_t hscl, uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap,
		std::function<void()> fn);
/**
 * Calculate number of sound samples to drop due to dropped frame.
 */
	uint64_t killed_audio_length(uint32_t fps_n, uint32_t fps_d, double& fraction);
private:
	void statuschange();
	friend class dumper_base;
	std::map<dumper_factory_base*, dumper_base*> dumpers;
	std::set<notifier*> notifications;
	std::set<dumper_base*> sdumpers;
	uint32_t current_rate_n;
	uint32_t current_rate_d;
	gameinfo current_gi;
	std::ostream* output;
	threads::rlock lock;
	lua_state& lua2;
};

class dumper_base
{
public:
	dumper_base();
	dumper_base(master_dumper& _mdumper, dumper_factory_base& _fbase);
	virtual ~dumper_base() throw();
/**
 * New frame available.
 */
	virtual void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d) = 0;
/**
 * New sample available.
 */
	virtual void on_sample(short l, short r) = 0;
/**
 * Sample rate is changing.
 */
	virtual void on_rate_change(uint32_t n, uint32_t d) = 0;
/**
 * Gameinfo is changing.
 */
	virtual void on_gameinfo_change(const master_dumper::gameinfo& gi) = 0;
/**
 * Dump is being forcibly ended.
 */
	virtual void on_end() = 0;
/**
 * Render Lua HUD on video. samples_killed is incremented if needed.
 *
 * Parameter target: The target screen to render on.
 * Parameter source: The source screen to read.
 * Parameter fps_n: Fps numerator.
 * Parameter fps_d: Fps denominator.
 * Parameter hscl: The horizontal scale factor.
 * Parameter vscl: The vertical scale factor.
 * Parameter lgap: Left gap.
 * Parameter tgap: Top gap.
 * Parameter rgap: Right gap
 * Parameter bgap: Bottom gap.
 * Parameter fn: Function to call between running lua hooks and actually rendering.
 * Returns: True if frame should be dumped, false if not.
 */
	template<bool X> bool render_video_hud(struct framebuffer::fb<X>& target, struct framebuffer::raw& source,
		uint32_t fps_n, uint32_t fps_d, uint32_t hscl, uint32_t vscl, uint32_t lgap, uint32_t tgap,
		uint32_t rgap, uint32_t bgap, std::function<void()> fn)
	{
		bool r = mdumper->render_video_hud(target, source, hscl, vscl, lgap, tgap, rgap, bgap, fn);
		if(!r)
			samples_killed += mdumper->killed_audio_length(fps_n, fps_d, akillfrac);
		return r;
	}
private:
	friend class master_dumper;
	uint64_t samples_killed;
	master_dumper* mdumper;
	dumper_factory_base* fbase;
	double akillfrac;
};

#endif
