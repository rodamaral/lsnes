#ifndef _dispatch__hpp__included__
#define _dispatch__hpp__included__

#include "render.hpp"
#include "keymapper.hpp"

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
	~information_dispatch() throw();
/**
 * Window close event received (this is not emulator close!)
 *
 * The default handler does nothing.
 */
	virtual void on_close();
/**
 * Call all on_close() handlers.
 */
	static void do_close() throw();
/**
 * Click or release on window was receivied.
 *
 * The default handler does nothing.
 *
 * Parameter x: The x-coordinate of the click. May be negative or otherwise out of screen.
 * Parameter y: The y-coordinate of the click. May be negative or otherwise out of screen.
 * Parameter buttonmask: Bitfield giving current buttons held:
 *	Bit 0 => Left button is down/held.
 *	Bit 1 => Middle button is down/held.
 *	Bit 2 => Middle button is down/held.
 */
	virtual void on_click(int32_t x, int32_t y, uint32_t buttonmask);
/**
 * Call all on_click() handlers.
 */
	static void do_click(int32_t x, int32_t y, uint32_t buttonmask) throw();
/**
 * Sound mute/unmute status might have been changed.
 *
 * The default handler does nothing.
 *
 * Parameter unmuted: If true, the sound is now enabled. If false, the sound is now disabled.
 */
	virtual void on_sound_unmute(bool unmuted);
/**
 * Call all on_sound_unmute() handlers.
 */
	static void do_sound_unmute(bool unmuted) throw();
/**
 * Sound device might have been changed.
 *
 * The default handler does nothing.
 *
 * Parameter dev: The device name sound is now playing (if enabled) from.
 */
	virtual void on_sound_change(const std::string& dev);
/**
 * Call all on_sound_change() handlers.
 */
	static void do_sound_change(const std::string& dev) throw();
/**
 * Emulator mode might have been changed.
 *
 * The default handler does nothing.
 *
 * Parameter readonly: True if readonly mode is now active, false if now in readwrite mode.
 */
	virtual void on_mode_change(bool readonly);
/**
 * Call all on_mode_change() handlers.
 */
	static void do_mode_change(bool readonly) throw();
/**
 * Autohold on button might have been changed.
 *
 * The default handler does nothing.
 *
 * Parameter pid: The physical ID of controller (0-7).
 * Parameter ctrlnum: Physical control number (0-15).
 * Parameter newstate: True if autohold is now active, false if autohold is now inactive.
 */
	virtual void on_autohold_update(unsigned pid, unsigned ctrlnum, bool newstate);
/**
 * Call all on_autohold_update() handlers.
 */
	static void do_autohold_update(unsigned pid, unsigned ctrlnum, bool newstate) throw();
/**
 * Controller configuration may have been changed.
 *
 * The default handler does nothing.
 */
	virtual void on_autohold_reconfigure();
/**
 * Call all on_autohold_reconfigure() handlers.
 */
	static void do_autohold_reconfigure() throw();
/**
 * A setting may have changed.
 *
 * The default handler does nothing.
 *
 * Parameter setting: The setting that has possibly changed.
 * Parameter value: The new value for the setting.
 */
	virtual void on_setting_change(const std::string& setting, const std::string& value);
/**
 * Call all on_setting_change() handlers.
 */
	static void do_setting_change(const std::string& setting, const std::string& value) throw();
/**
 * A setting has been cleared (but it might have been cleared already).
 *
 * The default handler does nothing.
 *
 * Parameter setting: The setting that is now clear.
 */
	virtual void on_setting_clear(const std::string& setting);
/**
 * Call all on_setting_clear() handlers
 */
	static void do_setting_clear(const std::string& setting) throw();
/**
 * A raw frame has been received.
 *
 * The default handler does nothing.
 *
 * Parameter raw: The raw frame data. 512*512 element array.
 * Parameter hires: True if in hires mode (512 wide), false if not (256-wide).
 * Parameter interlaced: True if in interlaced mode (448/478 high), false if not (224/239 high).
 * Parameter overscan: True if overscan is active, false if not.
 * Parameter region: One of VIDEO_REGION_* contstants, giving the region this frame is from.
 */
	virtual void on_raw_frame(const uint32_t* raw, bool hires, bool interlaced, bool overscan, unsigned region);
/**
 * Call all on_raw_frame() handlers.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 */
	static void do_raw_frame(const uint32_t* raw, bool hires, bool interlaced, bool overscan, unsigned region)
		throw();
/**
 * A frame has been received.
 *
 * The default handler does nothing.
 *
 * Parameter _frame: The frame object.
 * Parameter fps_n: Numerator of current video fps.
 * Parameter fps_d: Denominator of current video fps.
 */
	virtual void on_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d);
/**
 * Call all on_frame() handlers.
 *
 * Calls on_new_dumper() on dumpers that had that not yet called.
 */
	static void do_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d) throw();
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
 * (Pseudo-)key has been pressed or released.
 *
 * Parameter modifiers: The modifier set currently active.
 * Parameter keygroup: The key group the (pseudo-)key is from.
 * Parameter subkey: Subkey index within the key group.
 * Parameter polarity: True if key is being pressed, false if being released.
 * Parameter name: Name of the key being pressed/released.
 */
	virtual void on_key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned subkey,
		bool polarity, const std::string& name);
/**
 * Call on_key_event() for all event handlers (or just one if keys are being grabbed).
 */
	static void do_key_event(const modifier_set& modifiers, keygroup& keygroup, unsigned subkey,
		bool polarity, const std::string& name) throw();
/**
 * Grab all key events.
 *
 * While key events are grabbed, do_key_event() only calls on_key_event() for the grabbing object.
 */
	void grab_keys() throw();
/**
 * Ungrab all key events.
 *
 * While key events are grabbed, do_key_event() only calls on_key_event() for the grabbing object.
 */
	void ungrab_keys() throw();
/**
 * Get name of target.
 */
	const std::string& get_name() throw();
/**
 * Set window main screen compensation parameters. This is used for mouse click reporting.
 *
 * parameter xoffset: X coordinate of origin.
 * parameter yoffset: Y coordinate of origin.
 * parameter hscl: Horizontal scaling factor.
 * parameter vscl: Vertical scaling factor.
 */
	static void do_click_compensation(uint32_t xoffset, uint32_t yoffset, uint32_t hscl, uint32_t vscl);
/**
 * Render buffer needs to be (possibly) resized, so that graphics plugin can update the mappings.
 *
 * Default implementation does nothing.
 *
 * parameter scr: The render buffer object.
 * parameter w: The width needed.
 * parameter h: The height needed.
 */
	virtual void on_screen_resize(screen& scr, uint32_t w, uint32_t h);
/**
 * Call on_screen_resize on all objects.
 */
	static void do_screen_resize(screen& scr, uint32_t w, uint32_t h) throw();
/**
 * Notify that render buffer updating starts.
 *
 * Default implementation does nothing.
 */
	virtual void on_render_update_start();
/**
 * Call on_render_update_start() in all objects.
 */
	static void do_render_update_start() throw();
/**
 * Notify that render buffer updating ends.
 *
 * Default implementation does nothing.
 */
	virtual void on_render_update_end();
/**
 * Call on_render_update_end() in all objects.
 */
	static void do_render_update_end() throw();
/**
 * Notify that status buffer has been updated.
 *
 * Default implementation does nothing.
 */
	virtual void on_status_update();
/**
 * Call on_status_update() in all objects.
 */
	static void do_status_update() throw();
private:
	static void update_dumpers(bool nocalls = false) throw();
	bool known_if_dumper;
	bool marked_as_dumper;
	std::string target_name;
	bool notified_as_dumper;
	bool grabbing_keys;
};

#endif
