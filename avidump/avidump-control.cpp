#include "lua.hpp"
#include "avidump.hpp"
#include "sox.hpp"
#include "settings.hpp"
#include "misc.hpp"
#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <zlib.h>
#include "misc.hpp"
#include "avsnoop.hpp"
#include "command.hpp"

namespace
{
	boolean_setting dump_large("large-video", false);
	numeric_setting dtb("default-top-border", 0, 8191, 0);
	numeric_setting dbb("default-bottom-border", 0, 8191, 0);
	numeric_setting dlb("default-left-border", 0, 8191, 0);
	numeric_setting drb("default-right-border", 0, 8191, 0);
	numeric_setting max_frames_per_segment("max-frames-per-segment", 0, 999999999, 0);

	class avi_avsnoop : public av_snooper
	{
	public:
		avi_avsnoop(const std::string& prefix, struct avi_info parameters) throw(std::bad_alloc)
		{
			vid_dumper = new avidumper(prefix, parameters);
			soxdumper = new sox_dumper(prefix + ".sox", 32040.5, 2);
			dcounter = 0;
		}

		~avi_avsnoop() throw()
		{
			delete vid_dumper;
			delete soxdumper;
		}

		void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
			throw(std::bad_alloc, std::runtime_error)
		{
			vid_dumper->wait_idle();
			uint32_t hscl = 1;
			uint32_t vscl = 1;
			if(dump_large && _frame.width < 400)
				hscl = 2;
			if(dump_large && _frame.height < 400)
				vscl = 2;
			uint32_t _magic = 403703808;
			uint8_t* magic = reinterpret_cast<uint8_t*>(&_magic);
			dscr.set_palette(magic[2], magic[1], magic[0]);

			struct lua_render_context lrc;
			render_queue rq;
			lrc.left_gap = dlb;
			lrc.right_gap = drb;
			lrc.bottom_gap = dbb;
			lrc.top_gap = dtb;
			lrc.queue = &rq;
			lrc.width = _frame.width * hscl;
			lrc.height = _frame.height * vscl;
			lrc.rshift = magic[2];
			lrc.gshift = magic[1];
			lrc.bshift = magic[0];
			lua_callback_do_video(&lrc);

			dscr.reallocate(lrc.left_gap + hscl * _frame.width + lrc.right_gap, lrc.top_gap + vscl *
				_frame.height + lrc.bottom_gap, lrc.left_gap, lrc.top_gap, true);
			dscr.copy_from(_frame, hscl, vscl);
			rq.run(dscr);
			vid_dumper->on_frame(dscr.memory, dscr.width, dscr.height, fps_n, fps_d);
		}

		void sample(short l, short r) throw(std::bad_alloc, std::runtime_error)
		{
			dcounter += 81;
			if(dcounter < 64081)
				vid_dumper->on_sample(l, r);
			else
				dcounter -= 64081;
			soxdumper->sample(l, r);
		}

		void end() throw(std::bad_alloc, std::runtime_error)
		{
			vid_dumper->on_end();
			soxdumper->close();
		}

		void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
			authors, double gametime, const std::string& rerecords) throw(std::bad_alloc,
			std::runtime_error)
		{
			//We don't have place for this info and thus ignore it.
		}
	private:
		avidumper* vid_dumper;
		sox_dumper* soxdumper;
		screen dscr;
		unsigned dcounter;
	};

	avi_avsnoop* vid_dumper;

	class dump_video_command : public command
	{
	public:
		dump_video_command() throw(std::bad_alloc) : command("dump-avi") {}
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			tokensplitter t(args);
			std::string level = t;
			std::string prefix = t.tail();
			if(prefix == "")
				throw std::runtime_error("Expected prefix");
			if(vid_dumper)
				throw std::runtime_error("AVI dumping already in progress");
			unsigned long level2;
			try {
				level2 = parse_value<unsigned long>(level);
				if(level2 > 18)
					throw std::runtime_error("Level must be 0-18");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::runtime_error& e) {
				throw std::runtime_error("Bad AVI compression level '" + level + "': " + e.what());
			}
			struct avi_info parameters;
			parameters.compression_level = (level2 > 9) ? (level2 - 9) : level2;
			parameters.audio_sampling_rate = 32000;
			parameters.keyframe_interval = (level2 > 9) ? 300 : 1;
			parameters.max_frames_per_segment = max_frames_per_segment;
			try {
				vid_dumper = new avi_avsnoop(prefix, parameters);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << " at level " << level2 << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "Start AVI capture"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: dump-avi <level> <prefix>\n"
				"Start AVI capture to <prefix> using compression\n"
				"level <level> (0-18).\n";
		}
	} dump_video;

	class end_video_command : public command
	{
	public:
		end_video_command() throw(std::bad_alloc) : command("end-avi") {}
		void invoke(const std::string& args) throw(std::bad_alloc, std::runtime_error)
		{
			if(args != "")
				throw std::runtime_error("This command does not take parameters");
			if(!vid_dumper)
				throw std::runtime_error("No video dump in progress");
			try {
				vid_dumper->end();
				messages << "Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
		}
		std::string get_short_help() throw(std::bad_alloc) { return "End AVI capture"; }
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: end-avi\n"
				"End a AVI capture.\n";
		}
	} end_vieo;
}
