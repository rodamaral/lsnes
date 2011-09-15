#ifndef _videodumper2__hpp__included__
#define _videodumper2__hpp__included__

#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <stdexcept>
#include "window.hpp"
#include "videodumper.hpp"

/**
 * Forcibly ends dumping. Mainly useful for quitting.
 *
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Failed to end dump.
 */
void end_vid_dump() throw(std::bad_alloc, std::runtime_error);

/**
 * Dumps a frame. Does nothing if dumping is not in progress.
 * 
 * parameter ls: Screen to dump.
 * parameter rq: Render queue to run.
 * parameter left: Left border.
 * parameter right: Right border. 
 * parameter top: Top border.
 * parameter bottom: Bottom border.
 * parameter region: True if PAL, false if NTSC.
 * parameter win: Graphics system handle.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Failed to dump frame.
 */
void dump_frame(lcscreen& ls, render_queue* rq, uint32_t left, uint32_t right, uint32_t top, uint32_t bottom, 
	bool region, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * Dumps one sample of audio. Does nothing if dumping is not in progress.
 * 
 * parameter l_sample Left channel sample (-32768-32767)
 * parameter r_sample Right channel sample (-32768-32767)
 * parameter win Graphics System handle.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Failed to dump sample.
 */
void dump_audio_sample(int16_t l_sample, int16_t r_sample, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * Is the dump in progress?
 * 
 * returns: True if dump is in progress, false if not.
 */
bool dump_in_progress() throw();

/**
 * Fill rendering shifts.
 * 
 * parameter r: Shift for red component is written here.
 * parameter g: Shift for green component is written here.
 * parameter b: Shift for blue component is written here.
 */
void video_fill_shifts(uint32_t& r, uint32_t& g, uint32_t& b);

#endif
