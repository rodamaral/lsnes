#include "video/cscd.hpp"
#include "video/sox.hpp"

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "lua/lua.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <cmath>
#include <sstream>
#include <zlib.h>

namespace
{
	uint32_t rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000,
		128000, 176400, 192000};

	uint32_t get_rate(uint32_t n, uint32_t d, unsigned mode)
	{
		if(mode == 0) {
			unsigned bestidx = 0;
			double besterror = 1e99;
			for(size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
				double error = fabs(log(static_cast<double>(d) * rates[i] / n));
				if(error < besterror) {
					besterror = error;
					bestidx = i;
				}
			}
			return rates[bestidx];
		} else if(mode == 1) {
			return static_cast<uint32_t>(n / d);
		} else if(mode == 2) {
			return static_cast<uint32_t>((n + d - 1) / d);
		}
	}

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
	numeric_setting clevel("avi-compression", 0, 18, 7);
	numeric_setting max_frames_per_segment("avi-maxframes", 0, 999999999, 0);
	numeric_setting soundrate_setting("avi-soundrate", 0, 2, 0);

	void waitfn();

	class avi_avsnoop : public information_dispatch
	{
	public:
		avi_avsnoop(const std::string& prefix, struct avi_info parameters) throw(std::bad_alloc)
			: information_dispatch("dump-avi-cscd")
		{
			enable_send_sound();
			_parameters = parameters;
			avi_cscd_dumper::global_parameters gp;
			avi_cscd_dumper::segment_parameters sp;
			soundrate = get_sound_rate();
			gp.sampling_rate = get_rate(soundrate.first, soundrate.second, soundrate_setting);
			sp.fps_n = 60;
			sp.fps_d = 1;
			sp.dataformat = avi_cscd_dumper::PIXFMT_XRGB;
			sp.width = 256;
			sp.height = 224;
			sp.default_stride = true;
			sp.stride = 512;
			sp.keyframe_distance = parameters.keyframe_interval;
			sp.deflate_level = parameters.compression_level;
			sp.max_segment_frames = parameters.max_frames_per_segment;
			vid_dumper = new avi_cscd_dumper(prefix, gp, sp);
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

		void on_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			uint32_t hscl = 1;
			uint32_t vscl = 1;
			if(dump_large && _frame.width < 400)
				hscl = 2;
			if(dump_large && _frame.height < 400)
				vscl = 2;
			render_video_hud(dscr, _frame, hscl, vscl, 16, 8, 0, dlb, dtb, drb, dbb, waitfn);

			avi_cscd_dumper::segment_parameters sp;
			sp.fps_n = fps_n;
			sp.fps_d = fps_d;
			uint32_t x = 0x18100800;
			if(*reinterpret_cast<const uint8_t*>(&x) == 0x18)
				sp.dataformat = avi_cscd_dumper::PIXFMT_XRGB;
			else
				sp.dataformat = avi_cscd_dumper::PIXFMT_BGRX;
			sp.width = dscr.width;
			sp.height = dscr.height;
			sp.default_stride = true;
			sp.stride = 1024;
			sp.keyframe_distance = _parameters.keyframe_interval;
			sp.deflate_level = _parameters.compression_level;
			sp.max_segment_frames = _parameters.max_frames_per_segment;
			vid_dumper->set_segment_parameters(sp);
			vid_dumper->video(dscr.memory);
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			dcounter += soundrate.first;
			while(dcounter < soundrate.second * audio_record_rate + soundrate.first) {
				if(have_dumped_frame)
					vid_dumper->audio(&l, &r, 1);
				dcounter += soundrate.first;
			}
			dcounter -= (soundrate.second * audio_record_rate + soundrate.first);
			if(have_dumped_frame)
				soxdumper->sample(l, r);
		}

		void on_dump_end()
		{
			vid_dumper->wait_frame_processing();
			vid_dumper->end();
			soxdumper->close();
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
		avi_cscd_dumper* vid_dumper;
	private:
		
		sox_dumper* soxdumper;
		screen dscr;
		unsigned dcounter;
		struct avi_info _parameters;
		bool have_dumped_frame;
		std::pair<uint32_t, uint32_t> soundrate;
		uint32_t audio_record_rate;
	};

	avi_avsnoop* vid_dumper;

	void waitfn()
	{
		vid_dumper->vid_dumper->wait_frame_processing();
	}

	class adv_avi_dumper : public adv_dumper
	{
	public:
		adv_avi_dumper() : adv_dumper("INTERNAL-AVI-CSCD") {information_dispatch::do_dumper_update(); }
		~adv_avi_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			return x;
		}

		bool wants_prefix(const std::string& mode) throw()
		{
			return true;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "AVI (internal CSCD)";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			return "";
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& prefix) throw(std::bad_alloc,
			std::runtime_error)
		{
			if(prefix == "")
				throw std::runtime_error("Expected prefix");
			if(vid_dumper)
				throw std::runtime_error("AVI(CSCD) dumping already in progress");
			unsigned long level2 = (unsigned long)clevel;
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
				x << "Error starting AVI(CSCD) dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping AVI(CSCD) to " << prefix << " at level " << level2 << std::endl;
			information_dispatch::do_dumper_update();
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No AVI(CSCD) video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "AVI(CSCD) Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending AVI(CSCD) dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;
	
	adv_avi_dumper::~adv_avi_dumper() throw()
	{
	}
}
