#ifndef _runmode__hpp__included__
#define _runmode__hpp__included__

#include <cstdint>

class emulator_runmode
{
public:
	const static uint64_t QUIT;
	const static uint64_t NORMAL;
	const static uint64_t LOAD;
	const static uint64_t ADVANCE_FRAME;
	const static uint64_t ADVANCE_SUBFRAME;
	const static uint64_t SKIPLAG;
	const static uint64_t SKIPLAG_PENDING;
	const static uint64_t PAUSE;
	const static uint64_t PAUSE_BREAK;
	const static uint64_t CORRUPT;

	const static unsigned P_START;
	const static unsigned P_VIDEO;
	const static unsigned P_SAVE;
	const static unsigned P_NONE;
/**
 * Ctor.
 */
	emulator_runmode();
/**
 * Save current mode and set mode to LOAD.
 */
	void start_load();
/**
 * Restore saved mode.
 */
	void end_load();
/**
 * Decay SKIPLAG_PENDING to SKIPLAG.
 */
	void decay_skiplag();
/**
 * Decay PAUSE_BREAK into PAUSE.
 */
	void decay_break();
/**
 * Is paused?
 */
	bool is_paused() { return is(PAUSE | PAUSE_BREAK); }
/**
 * Is paused normally?
 */
	bool is_paused_normal() { return is(PAUSE); }
/**
 * Is paused debug break?
 */
	bool is_paused_break() { return is(PAUSE_BREAK); }
/**
 * Is advancing frames?
 */
	bool is_advance_frame() { return is(ADVANCE_FRAME); }
/**
 * Is advancing subframes?
 */
	bool is_advance_subframe() { return is(ADVANCE_SUBFRAME); }
/**
 * Is advancing (frames or subframes)?
 */
	bool is_advance() { return is(ADVANCE_FRAME|ADVANCE_SUBFRAME); }
/**
 * Is skipping lag?
 */
	bool is_skiplag() { return is(SKIPLAG); }
/**
 * Is running free?
 */
	bool is_freerunning() { return is(NORMAL); }
/**
 * Is special?
 */
	bool is_special() { return is(QUIT|LOAD|CORRUPT); }
/**
 * Is load?
 */
	bool is_load() { return is(LOAD); }
/**
 * Is quit?
 */
	bool is_quit() { return is(QUIT); }
/**
 * Set pause.
 */
	void set_pause() { set(PAUSE); }
/**
 * Set break.
 */
	void set_break() { set(PAUSE_BREAK); }
/**
 * Set quit.
 */
	void set_quit() { set(QUIT); }
/**
 * Set freerunning.
 */
	void set_freerunning() { set(NORMAL); }
/**
 * Set advance frame.
 *
 * The advanced and cancel flags are cleared.
 */
	void set_frameadvance() { set(ADVANCE_FRAME); }
/**
 * Set advance subframe.
 *
 * The advanced and cancel flags are cleared.
 */
	void set_subframeadvance() { set(ADVANCE_SUBFRAME); }
/**
 * Set pending skiplag.
 */
	void set_skiplag_pending() { set(SKIPLAG_PENDING); }
/**
 * Set pause or freerunning.
 */
	void set_pause_cond(bool paused) { set(paused ? PAUSE : NORMAL); }
/**
 * Set advanced flag and return previous value.
 */
	bool set_and_test_advanced();
/**
 * Set cancel flag.
 */
	void set_cancel();
/**
 * Test and clear cancel flag.
 */
	bool clear_and_test_cancel();
/**
 * Is cancel flag set?
 */
	bool test_cancel();
/**
 * Is advanced flag set?
 */
	bool test_advanced();
/**
 * Test corrupt flag.
 */
	bool is_corrupt() { return is(CORRUPT); }
/**
 * Set corrupt flag.
 */
	void set_corrupt() { set(CORRUPT); }
/**
 * Clear corrupt flag.
 */
	void clear_corrupt() { set(LOAD); }
/**
 * Set current point
 */
	void set_point(unsigned _point);
/**
 * Get current point
 */
	unsigned get_point();
/**
 * Get the current runmode.
 */
	uint64_t get();
private:
	void revalidate();
	void set(uint64_t m);
	bool is(uint64_t m);
	uint64_t mode;
	uint64_t saved_mode;
	uint64_t magic;		//If mode is QUIT, this has to be QUIT_MAGIC.
	//Flags relating to repeating advance.
	bool advanced;		//This is second or subsequent advance.
	bool cancel;		//Cancel advance at next oppurtunity.
	bool saved_advanced;
	bool saved_cancel;
	//Current point.
	unsigned point;
};

#endif
