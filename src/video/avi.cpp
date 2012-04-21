#include "video/sox.hpp"
#include "video/avi/writer.hpp"

#include "video/avi/codec.hpp"

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "lua/lua.hpp"
#include "library/minmax.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <cmath>
#include <sstream>
#include <zlib.h>
#ifdef WITH_SECRET_RABBIT_CODE
#include <samplerate.h>
#endif
#define RESAMPLE_BUFFER 1024



namespace
{
	uint32_t rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000,
		128000, 176400, 192000};

	uint32_t topowerof2(uint32_t base)
	{
		if(base & (base - 1) == 0)
			return base;	//Already power of two.
		base |= (base >> 16);
		base |= (base >> 8);
		base |= (base >> 4);
		base |= (base >> 2);
		base |= (base >> 1);
		return base + 1;
	}

	uint32_t get_rate(uint32_t n, uint32_t d, unsigned mode)
	{
		if(mode == 0) {
			auto best = std::make_pair(1e99, static_cast<size_t>(0));
			for(size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
				best = ::min(best, std::make_pair(fabs(log(static_cast<double>(d) * rates[i] / n)),
					i));
			return rates[best.second];
		} else if(mode == 1) {
			return static_cast<uint32_t>(n / d);
		} else if(mode == 2) {
			return static_cast<uint32_t>((n + d - 1) / d);
		} else if(mode == 3) {
			uint32_t x = n;
			uint32_t y = d;
			while(y) {
				uint32_t t = x % d;
				x = y;
				y = t;
			}
			return static_cast<uint32_t>(n / x);
		} else if(mode == 4) {
			uint32_t base = static_cast<uint32_t>((n + d - 1) / d);
			//Handle large values specially.
			if(base > 0xFA000000U)	return 0xFFFFFFFFU;
			if(base > 0xBB800000U)	return 0xFA000000U;
			if(base > 0xAC440000U)	return 0xBB800000U;
			uint32_t base_A = topowerof2((base + 7999) / 8000);
			uint32_t base_B = topowerof2((base + 11024) / 11025);
			uint32_t base_C = topowerof2((base + 11999) / 12000);
			return min(base_A * 8000, min(base_B * 11025, base_C * 12000));
		}
	}

	boolean_setting dump_large("avi-large", false);
	numeric_setting dtb("avi-top-border", 0, 8191, 0);
	numeric_setting dbb("avi-bottom-border", 0, 8191, 0);
	numeric_setting dlb("avi-left-border", 0, 8191, 0);
	numeric_setting drb("avi-right-border", 0, 8191, 0);
	numeric_setting max_frames_per_segment("avi-maxframes", 0, 999999999, 0);
#ifdef WITH_SECRET_RABBIT_CODE
	numeric_setting soundrate_setting("avi-soundrate", 0, 4, 0);
#else
	numeric_setting soundrate_setting("avi-soundrate", 0, 3, 0);
#endif

	std::pair<avi_video_codec_type*, avi_audio_codec_type*> find_codecs(const std::string& mode)
	{
		avi_video_codec_type* v = NULL;
		avi_audio_codec_type* a = NULL;
		std::string _mode = mode;
		size_t s = _mode.find_first_of("/");
		if(s < _mode.length()) {
			std::string vcodec = _mode.substr(0, s);
			std::string acodec = _mode.substr(s + 1);
			v = avi_video_codec_type::find(vcodec);
			a = avi_audio_codec_type::find(acodec);
		}
		return std::make_pair(v, a);
	}

	struct avi_info
	{
		std::string prefix;
		struct avi_video_codec* vcodec;
		struct avi_audio_codec* acodec;
		uint32_t sample_rate;
		uint16_t audio_chans;
		uint32_t max_frames;
	};

	struct avi_worker : public worker_thread
	{
		avi_worker(const struct avi_info& info);
		void entry();
		void queue_video(uint32_t* _frame, uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d);
		void queue_audio(int16_t* data, size_t samples);
	private:
		avi_writer aviout;
		uint32_t* frame;
		uint32_t frame_width;
		uint32_t frame_height;
		uint32_t frame_fps_n;
		uint32_t frame_fps_d;
		uint32_t segframes;
		uint32_t max_segframes;
		bool closed;
	};

#define WORKFLAG_QUEUE_FRAME 1
#define WORKFLAG_FLUSH 2
#define WORKFLAG_END 4


	avi_worker::avi_worker(const struct avi_info& info)
		: aviout(info.prefix, *info.vcodec, *info.acodec, info.sample_rate, info.audio_chans)
	{
		segframes = 0;
		max_segframes = info.max_frames;
		fire();
	}

	void avi_worker::queue_video(uint32_t* _frame, uint32_t width, uint32_t height, uint32_t fps_n, uint32_t fps_d)
	{
		rethrow();
		wait_busy();
		frame = _frame;
		frame_width = width;
		frame_height = height;
		frame_fps_n = fps_n;
		frame_fps_d = fps_d;
		set_busy();
		set_workflag(WORKFLAG_QUEUE_FRAME);
	}
	
	void avi_worker::queue_audio(int16_t* data, size_t samples)
	{
		rethrow();
		aviout.audio_queue().push(data, samples);
		set_workflag(WORKFLAG_FLUSH);
	}

	void avi_worker::entry()
	{
		while(1) {
			wait_workflag();
			uint32_t work = clear_workflag(~WORKFLAG_QUIT_REQUEST);
			//Flush the queue first in order to provode backpressure.
			if(work & WORKFLAG_FLUSH) {
				clear_workflag(WORKFLAG_FLUSH);
				aviout.flush();
			}
			//Then add frames if any.
			if(work & WORKFLAG_QUEUE_FRAME) {
				frame_object f;
				f.data = new uint32_t[frame_width * frame_height];
				f.width = frame_width;
				f.height = frame_height;
				f.fps_n = frame_fps_n;
				f.fps_d = frame_fps_d;
				f.force_break = (segframes == max_segframes && max_segframes > 0);
				if(f.force_break)
					segframes = 0;
				memcpy(&f.data[0], frame, 4 * frame_width * frame_height);
				frame = NULL;
				clear_workflag(WORKFLAG_QUEUE_FRAME);
				clear_busy();
				aviout.video_queue().push_back(f);
				segframes++;
				set_workflag(WORKFLAG_FLUSH);
			}
			//End the streaam if that is flagged.
			if(work & WORKFLAG_END) {
				if(!closed)
					aviout.close();
				closed = true;
				clear_workflag(WORKFLAG_END | WORKFLAG_FLUSH | WORKFLAG_QUEUE_FRAME);
			}
			//If signaled to quit and no more work, do so.
			if(work == WORKFLAG_QUIT_REQUEST) {
				if(!closed)
					aviout.close();
				closed = true;
				break;
			}
		}
	}

	void waitfn();

	class avi_avsnoop : public information_dispatch
	{
	public:
		avi_avsnoop(avi_info& info) throw(std::bad_alloc, std::runtime_error)
			: information_dispatch("dump-avi-int")
		{
			enable_send_sound();
			chans = info.audio_chans = 2;
			soundrate = get_sound_rate();
			audio_record_rate = info.sample_rate = get_rate(soundrate.first, soundrate.second,
				soundrate_setting);
			worker = new avi_worker(info);
			soxdumper = new sox_dumper(info.prefix + ".sox", static_cast<double>(soundrate.first) /
				soundrate.second, 2);
			dcounter = 0;
			have_dumped_frame = false;
			resampler = NULL;
			if(soundrate_setting == 4) {
				double ratio = 1.0 * audio_record_rate * soundrate.second / soundrate.first;
				sbuffer.resize(RESAMPLE_BUFFER * chans);
				sbuffer2.resize(RESAMPLE_BUFFER * chans);
				fbuffer.resize((RESAMPLE_BUFFER * ratio + 128) * chans + 128);
				fbuffer2.resize((RESAMPLE_BUFFER * ratio + 128) * chans + 128);
				sbuffer_fill = 0;
#ifdef WITH_SECRET_RABBIT_CODE
				int errc = 0;
				resampler = src_new(SRC_SINC_BEST_QUALITY, info.audio_chans, &errc);
				if(errc)
					throw std::runtime_error(std::string("Error initing libsamplerate: ") +
						src_strerror(errc));
#else
				throw std::runtime_error("HQ sample rate conversion not available");
#endif
			}
		}

		~avi_avsnoop() throw()
		{
#ifdef WITH_SECRET_RABBIT_CODE
			if(resampler)
				src_delete((SRC_STATE*)resampler);
#endif
			delete worker;
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
			render_video_hud(dscr, _frame, hscl, vscl, 0, 8, 16, dlb, dtb, drb, dbb, waitfn);
			worker->queue_video(dscr.memory, dscr.width, dscr.height, fps_n, fps_d);
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(resampler) {
				if(!have_dumped_frame)
					return;
				sbuffer[sbuffer_fill++] = l;
				sbuffer[sbuffer_fill++] = r;
				forward_samples(false);
				return;
			}
			short x[2];
			x[0] = l;
			x[1] = r;
			dcounter += soundrate.first;
			while(dcounter < soundrate.second * audio_record_rate + soundrate.first) {
				if(have_dumped_frame)
					worker->queue_audio(x, 2);
				dcounter += soundrate.first;
			}
			dcounter -= (soundrate.second * audio_record_rate + soundrate.first);
			if(have_dumped_frame)
				soxdumper->sample(l, r);
		}

		void on_dump_end()
		{
			if(worker) {
				forward_samples(true);
				worker->request_quit();
			}
			if(soxdumper)
				soxdumper->close();
			delete worker;
			delete soxdumper;
			worker = NULL;
			soxdumper = NULL;
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
		avi_worker* worker;
	private:
		void forward_samples(bool eos)
		{
			if(!eos && sbuffer_fill < sbuffer.size())
				return;
#ifdef WITH_SECRET_RABBIT_CODE
			double ratio = 1.0 * audio_record_rate * soundrate.second / soundrate.first;
			SRC_DATA block;
			src_short_to_float_array(&sbuffer[0], &sbuffer2[0], sbuffer_fill);
			block.data_in = &sbuffer2[0];
			block.data_out = &fbuffer2[0];
			block.input_frames = sbuffer_fill / chans;
			block.input_frames_used = 0;
			block.output_frames = fbuffer2.size() / chans;
			block.output_frames_gen = 0;
			block.end_of_input = eos ? 1 : 0;
			block.src_ratio = ratio;
			int errc = src_process((SRC_STATE*)resampler, &block);
			if(errc)
				throw std::runtime_error(std::string("Error using libsamplerate: ") +
					src_strerror(errc));
			src_float_to_short_array(&fbuffer2[0], &fbuffer[0], block.output_frames_gen * chans);
			worker->queue_audio(&fbuffer[0], block.output_frames_gen * chans);
			if(block.input_frames_used * chans < sbuffer_fill)
				memmove(&sbuffer[0], &sbuffer[block.output_frames_gen * chans], sbuffer_fill -
					block.input_frames_used * chans);
			sbuffer_fill -= block.input_frames_used * chans;
#endif
		}
		sox_dumper* soxdumper;
		screen<false> dscr;
		unsigned dcounter;
		bool have_dumped_frame;
		std::pair<uint32_t, uint32_t> soundrate;
		uint32_t audio_record_rate;
		void* resampler;
		std::vector<short> sbuffer;
		std::vector<float> sbuffer2;
		std::vector<float> fbuffer2;
		std::vector<short> fbuffer;
		size_t sbuffer_fill;
		uint32_t chans;
	};

	avi_avsnoop* vid_dumper;

	void waitfn()
	{
		vid_dumper->worker->wait_busy();
	}

	class adv_avi_dumper : public adv_dumper
	{
	public:
		adv_avi_dumper() : adv_dumper("INTERNAL-AVI") {information_dispatch::do_dumper_update(); }
		~adv_avi_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			for(auto v = avi_video_codec_type::find_next(NULL); v; v = avi_video_codec_type::find_next(v))
				for(auto a = avi_audio_codec_type::find_next(NULL); a;
					a = avi_audio_codec_type::find_next(a))
					x.insert(v->get_iname() + std::string("/") + a->get_iname());
			return x;
		}

		unsigned mode_details(const std::string& mode) throw()
		{
			return target_type_prefix;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "AVI (internal)";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			auto c = find_codecs(mode);
			return c.first->get_hname() + std::string(" / ") + c.second->get_hname();
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
				throw std::runtime_error("AVI dumping already in progress");
			struct avi_info info;
			info.audio_chans = 2;
			info.sample_rate = 32000;
			info.max_frames = max_frames_per_segment;
			info.prefix = prefix;
			auto c = find_codecs(mode);
			info.vcodec = c.first->get_instance();
			info.acodec = c.second->get_instance();
			try {
				vid_dumper = new avi_avsnoop(info);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting AVI dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping AVI (" << c.first->get_hname() << " / " << c.second->get_hname()
				<< ") to " << prefix << std::endl;
			information_dispatch::do_dumper_update();
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No AVI video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "AVI Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending AVI dump: " << e.what() << std::endl;
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
