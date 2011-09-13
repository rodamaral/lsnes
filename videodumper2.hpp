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
 * \brief End dumping.
 * 
 * Forcibly ends dumping. Mainly useful for quitting.
 *
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Failed to end dump.
 */
void end_vid_dump() throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Send control command.
 * 
 * Sends command for dumping.
 * 
 * \param cmd Command.
 * \param win Graphics system handle.
 * \return True if command was recognized, false if not.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Failed to start dump or invalid syntax.
 */
bool vid_dumper_command(std::string& cmd, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Dump a frame.
 * 
 * Dumps a frame. Does nothing if dumping is not in progress.
 * 
 * \param ls Screen to dump.
 * \param rq Render queue to run.
 * \param left Left border.
 * \param right Right border. 
 * \param top Top border.
 * \param bottom Bottom border.
 * \param region True if PAL, false if NTSC.
 * \param win Graphics system handle.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Failed to dump frame.
 */
void dump_frame(lcscreen& ls, render_queue* rq, uint32_t left, uint32_t right, uint32_t top, uint32_t bottom, 
	bool region, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Dump sample of audio.
 * 
 * Dumps one sample of audio. Does nothing if dumping is not in progress.
 * 
 * \param l_sample Left channel sample (-32768-32767)
 * \param r_sample Right channel sample (-32768-32767)
 * \param win Graphics System handle.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Failed to dump sample.
 */
void dump_audio_sample(int16_t l_sample, int16_t r_sample, window* win) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Is the dump in progress?
 */
bool dump_in_progress() throw();

/**
 * \brief Fill rendering shifts.
 */
void video_fill_shifts(uint32_t& r, uint32_t& g, uint32_t& b);

#endif
