#include "lua.hpp"
#include "jmd.hpp"
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
	class jmd_avsnoop : public av_snooper
	{
	public:
		jmd_avsnoop(const std::string& filename, unsigned level) throw(std::bad_alloc)
			: av_snooper("JMD")
		{
			vid_dumper = new jmd_dumper(filename, level);
			have_dumped_frame = false;
			audio_w = 0;
			audio_n = 0;
			video_w = 0;
			video_n = 0;
			maxtc = 0;
			soundrate = av_snooper::get_sound_rate();
			try {
				gameinfo(av_snooper::get_gameinfo());
			} catch(std::exception& e) {
				messages << "Can't write gameinfo: " << e.what() << std::endl;
			}
		}

		~jmd_avsnoop() throw()
		{
			delete vid_dumper;
		}

		void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, const uint32_t* raw, bool hires,
			bool interlaced, bool overscan, unsigned region) throw(std::bad_alloc, std::runtime_error)
		{
			struct lua_render_context lrc;
			render_queue rq;
			lrc.left_gap = 0;
			lrc.right_gap = 0;
			lrc.bottom_gap = 0;
			lrc.top_gap = 0;
			lrc.queue = &rq;
			lrc.width = _frame.width;
			lrc.height = _frame.height;
			lua_callback_do_video(&lrc);
			dscr.set_palette(0, 8, 16);
			dscr.reallocate(lrc.left_gap + _frame.width + lrc.right_gap, lrc.top_gap + _frame.height +
				lrc.bottom_gap, lrc.left_gap, lrc.top_gap, false);
			dscr.copy_from(_frame, 1, 1);
			rq.run(dscr);

			vid_dumper->video(get_next_video_ts(fps_n, fps_d), dscr.memory, dscr.width, dscr.height);
			have_dumped_frame = true;
		}

		void sample(short l, short r) throw(std::bad_alloc, std::runtime_error)
		{
			uint64_t ts = get_next_audio_ts();
			if(have_dumped_frame)
				vid_dumper->audio(ts, l, r);
		}

		void end() throw(std::bad_alloc, std::runtime_error)
		{
			vid_dumper->end(maxtc);
		}

		void gameinfo(const struct gameinfo_struct& gi) throw(std::bad_alloc, std::runtime_error)
		{
			std::string authstr;
			for(size_t i = 0; i < gi.get_author_count(); i++) {
				if(i != 0)
					authstr = authstr + ", ";
				authstr = authstr + gi.get_author_short(i);
			}
			vid_dumper->gameinfo(gi.gamename, authstr, 1000000000ULL * gi.length, gi.get_rerecords());
		}
	private:
		uint64_t get_next_video_ts(uint32_t fps_n, uint32_t fps_d)
		{
			uint64_t ret = video_w;
			video_w += (1000000000ULL * fps_d) / fps_n;
			video_n += (1000000000ULL * fps_d) % fps_n;
			if(video_n >= fps_n) {
				video_n -= fps_n;
				video_w++;
			}
			maxtc = (ret > maxtc) ? ret : maxtc;
			return ret;
		}

		uint64_t get_next_audio_ts()
		{
			uint64_t ret = audio_w;
			audio_w += (1000000000ULL * soundrate.second) / soundrate.first;
			audio_n += (1000000000ULL * soundrate.second) % soundrate.first;
			if(audio_n >= soundrate.first) {
				audio_n -= soundrate.first;
				audio_w++;
			}
			maxtc = (ret > maxtc) ? ret : maxtc;
			return ret;
		}

		jmd_dumper* vid_dumper;
		screen dscr;
		unsigned dcounter;
		bool have_dumped_frame;
		uint64_t audio_w;
		uint64_t audio_n;
		uint64_t video_w;
		uint64_t video_n;
		uint64_t maxtc;
		std::pair<uint32_t, uint32_t> soundrate;
	};

	jmd_avsnoop* vid_dumper;

	function_ptr_command<const std::string&> jmd_dump("dump-jmd", "Start JMD capture",
		"Syntax: dump-jmd <level> <file>\nStart JMD capture to <file> using compression\n"
		"level <level> (0-9).\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			tokensplitter t(args);
			std::string level = t;
			std::string prefix = t.tail();
			if(prefix == "")
				throw std::runtime_error("Expected filename");
			if(vid_dumper)
				throw std::runtime_error("JMD dumping already in progress");
			unsigned long level2;
			try {
				level2 = parse_value<unsigned long>(level);
				if(level2 > 9)
					throw std::runtime_error("JMD Level must be 0-9");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::runtime_error& e) {
				throw std::runtime_error("Bad JMD compression level '" + level + "': " + e.what());
			}
			try {
				vid_dumper = new jmd_avsnoop(prefix, level2);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting JMD dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << " at level " << level2 << std::endl;
		});

	function_ptr_command<> end_avi("end-jmd", "End JMD capture",
		"Syntax: end-jmd\nEnd a JMD capture.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			if(!vid_dumper)
				throw std::runtime_error("No JMD video dump in progress");
			try {
				vid_dumper->end();
				messages << "JMD Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending JMD dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
		});
}
