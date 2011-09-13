#include "videodumper.hpp"
#include "videodumper2.hpp"
#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <zlib.h>
#include "misc.hpp"
#include "fieldsplit.hpp"

avidumper* vid_dumper = NULL;

void update_movie_state();

bool vid_dumper_command(std::string& cmd, window* win) throw(std::bad_alloc, std::runtime_error)
{
	std::map<std::string, std::string>* y;
	std::map<std::string, std::string> _y;
	if(win)
		y = &(win->get_emustatus());
	else
		y = &_y;
	if(is_cmd_prefix(cmd, "dump-video")) {
		std::string syntax = "Syntax: dump-video <0-18> <prefix>";
		tokensplitter t(cmd);
		std::string dummy = t;
		std::string level = t;
		std::string prefix = t.tail();
		if(prefix == "")
			throw std::runtime_error(syntax);
		if(vid_dumper)
			throw std::runtime_error("Video dumping already in progress");
		unsigned long level2;
		try {
			level2 = parse_value<unsigned long>(level);
			if(level2 > 18)
				throw std::runtime_error("Level must be 0-18");
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::runtime_error& e) {
			throw std::runtime_error("Bad video compression level '" + level + "': " + e.what());
		}
		struct avi_info parameters;
		parameters.compression_level = (level2 > 9) ? (level2 - 9) : level2;
		parameters.audio_drop_counter_inc = 81;
		parameters.audio_drop_counter_max = 64081;
		parameters.audio_sampling_rate = 32000;
		parameters.audio_native_sampling_rate = 32040.5;
		parameters.keyframe_interval = (level2 > 9) ? 300 : 1;
		try {
			vid_dumper = new avidumper(prefix, parameters);
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::exception& e) {
			std::ostringstream x;
			x << "Error starting dump: " << e.what();
			throw std::runtime_error(x.str());
		}
		out(win) << "Dumping to " << prefix << " at level " << level2 << std::endl;
		update_movie_state();
		return true;
	} else if(is_cmd_prefix(cmd, "end-video")) {
		if(!vid_dumper)
			throw std::runtime_error("No video dump in progress");
		try {
			vid_dumper->on_end();
			out(win) << "Dump finished" << std::endl;
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::exception& e) {
			out(win) << "Error ending dump: " << e.what() << std::endl;
		}
		delete vid_dumper;
		vid_dumper = NULL;
		update_movie_state();
		return true;
	} else
		return false;
}

void end_vid_dump() throw(std::bad_alloc, std::runtime_error)
{
	if(vid_dumper)
		try {
			vid_dumper->on_end();
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			std::cerr << "Error ending dump: " << e.what() << std::endl;
		}
}

screen dscr;

void dump_frame(lcscreen& ls, render_queue* rq, uint32_t left, uint32_t right, uint32_t top, uint32_t bottom, 
	bool region, window* win) throw(std::bad_alloc, std::runtime_error)
{
	if(vid_dumper)
		try {
			uint32_t _magic = 403703808;
			uint8_t* magic = reinterpret_cast<uint8_t*>(&_magic);
			dscr.reallocate(left + ls.width + right, top + ls.height + bottom, left, top, true);
			dscr.set_palette(magic[2], magic[1], magic[0]);
			dscr.copy_from(ls, 1, 1);
			if(rq)
				rq->run(dscr);
			assert(dscr.memory);
			assert(dscr.width);
			assert(dscr.height);
			uint32_t fps_n = 10738636;
			uint32_t fps_d = 178683;
			if(region) {
				fps_n = 322445;
				fps_d = 6448;
			}
			vid_dumper->on_frame(dscr.memory, dscr.width, dscr.height, fps_n, fps_d);
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::exception& e) {
			out(win) << "Error sending video frame: " << e.what() << std::endl;
		}
	if(rq)
		rq->clear();
}

void dump_audio_sample(int16_t l_sample, int16_t r_sample, window* win) throw(std::bad_alloc, std::runtime_error)
{
	if(vid_dumper)
		try {
			vid_dumper->on_sample(l_sample, r_sample);
		} catch(std::bad_alloc& e) {
			OOM_panic(win);
		} catch(std::exception& e) {
			out(win) << "Error sending audio sample: " << e.what() << std::endl;
		}
}

bool dump_in_progress() throw()
{
	return (vid_dumper != NULL);
}

void video_fill_shifts(uint32_t& r, uint32_t& g, uint32_t& b)
{
	r = dscr.active_rshift;
	g = dscr.active_gshift;
	b = dscr.active_bshift;
}
