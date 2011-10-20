#include "lua.hpp"
#include "cscd.hpp"
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
	struct avi_info
	{
		unsigned compression_level;
		uint32_t audio_sampling_rate;
		uint32_t keyframe_interval;
		uint32_t max_frames_per_segment;
	};

	boolean_setting dump_large("avi-large", false);
	numeric_setting dtb("avi-top-border", 0, 8191, 0);
	numeric_setting dbb("avi-bottom-border", 0, 8191, 0);
	numeric_setting dlb("avi-left-border", 0, 8191, 0);
	numeric_setting drb("avi-right-border", 0, 8191, 0);
	numeric_setting max_frames_per_segment("avi-maxframes", 0, 999999999, 0);

	class avi_avsnoop : public av_snooper
	{
	public:
		avi_avsnoop(const std::string& prefix, struct avi_info parameters) throw(std::bad_alloc)
		{
			_parameters = parameters;
			avi_cscd_dumper::global_parameters gp;
			avi_cscd_dumper::segment_parameters sp;
			gp.sampling_rate = parameters.audio_sampling_rate;
			gp.channel_count = 2;
			gp.audio_16bit = true;
			sp.fps_n = 60;
			sp.fps_d = 1;
			sp.dataformat = avi_cscd_dumper::PIXFMT_RGB15_NE;
			sp.width = 256;
			sp.height = 224;
			sp.default_stride = true;
			sp.stride = 512;
			sp.keyframe_distance = parameters.keyframe_interval;
			sp.deflate_level = parameters.compression_level;
			sp.max_segment_frames = parameters.max_frames_per_segment;
			vid_dumper = new avi_cscd_dumper(prefix, gp, sp);
			soundrate = av_snooper::get_sound_rate();
			audio_record_rate = parameters.audio_sampling_rate;
			soxdumper = new sox_dumper(prefix + ".sox", static_cast<double>(soundrate.first) /
				soundrate.second, 2);
			dcounter = 0;
			have_dumped_frame = false;
		}

		~avi_avsnoop() throw()
		{
			delete vid_dumper;
			delete soxdumper;
		}

		void frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
			throw(std::bad_alloc, std::runtime_error)
		{
			uint32_t hscl = 1;
			uint32_t vscl = 1;
			if(dump_large && _frame.width < 400)
				hscl = 2;
			if(dump_large && _frame.height < 400)
				vscl = 2;

			struct lua_render_context lrc;
			render_queue rq;
			lrc.left_gap = dlb;
			lrc.right_gap = drb;
			lrc.bottom_gap = dbb;
			lrc.top_gap = dtb;
			lrc.queue = &rq;
			lrc.width = _frame.width * hscl;
			lrc.height = _frame.height * vscl;
			lua_callback_do_video(&lrc);

			vid_dumper->wait_frame_processing();
			avi_cscd_dumper::segment_parameters sp;
			sp.fps_n = fps_n;
			sp.fps_d = fps_d;
			uint32_t x = 0x18100800;
			if(*reinterpret_cast<const uint8_t*>(&x) == 0x18)
				sp.dataformat = avi_cscd_dumper::PIXFMT_XRGB;
			else
				sp.dataformat = avi_cscd_dumper::PIXFMT_BGRX;
			sp.width = lrc.left_gap + hscl * _frame.width + lrc.right_gap;
			sp.height = lrc.top_gap + vscl * _frame.height + lrc.bottom_gap;
			sp.default_stride = true;
			sp.stride = 1024;
			sp.keyframe_distance = _parameters.keyframe_interval;
			sp.deflate_level = _parameters.compression_level;
			sp.max_segment_frames = _parameters.max_frames_per_segment;
			vid_dumper->set_segment_parameters(sp);
			dscr.reallocate(lrc.left_gap + hscl * _frame.width + lrc.right_gap, lrc.top_gap + vscl *
				_frame.height + lrc.bottom_gap, lrc.left_gap, lrc.top_gap, false);
			dscr.copy_from(_frame, hscl, vscl);
			rq.run(dscr);
			vid_dumper->video(dscr.memory);
			have_dumped_frame = true;
			vid_dumper->wait_frame_processing();
		}

		void sample(short l, short r) throw(std::bad_alloc, std::runtime_error)
		{
			dcounter += soundrate.first;
			while(dcounter < soundrate.second * audio_record_rate + soundrate.first) {
				if(have_dumped_frame)
					vid_dumper->audio(&l, &r, 1, avi_cscd_dumper::SNDFMT_SIGNED_16NE);
				dcounter += soundrate.first;
			}
			dcounter -= (soundrate.second * audio_record_rate + soundrate.first);
			if(have_dumped_frame)
				soxdumper->sample(l, r);
		}

		void end() throw(std::bad_alloc, std::runtime_error)
		{
			vid_dumper->end();
			soxdumper->close();
		}

		void gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
			authors, double gametime, const std::string& rerecords) throw(std::bad_alloc,
			std::runtime_error)
		{
			//We don't have place for this info and thus ignore it.
		}
	private:
		avi_cscd_dumper* vid_dumper;
		sox_dumper* soxdumper;
		screen dscr;
		unsigned dcounter;
		struct avi_info _parameters;
		bool have_dumped_frame;
		std::pair<uint32_t, uint32_t> soundrate;
		uint32_t audio_record_rate;
	};

	avi_avsnoop* vid_dumper;

	function_ptr_command<const std::string&> avi_dump("dump-avi", "Start AVI capture",
		"Syntax: dump-avi <level> <prefix>\nStart AVI capture to <prefix> using compression\n"
		"level <level> (0-18).\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
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
		});

	function_ptr_command<> end_avi("end-avi", "End AVI capture",
		"Syntax: end-avi\nEnd a AVI capture.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
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
		});
}
