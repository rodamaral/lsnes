#include "video/sox.hpp"
#include "video/avi/writer.hpp"

#include "video/avi/codec.hpp"

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "lua/lua.hpp"
#include "library/minmax.hpp"
#include "library/workthread.hpp"
#include "core/misc.hpp"
#include "core/settings.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"

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
	class avi_avsnoop;
	avi_avsnoop* vid_dumper;
	uint64_t akill = 0;
	double akillfrac = 0;

	uint32_t rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000,
		128000, 176400, 192000};

	uint32_t topowerof2(uint32_t base)
	{
		if((base & (base - 1)) == 0)
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
		} else if(mode == 5)
			return 48000;
		return 48000;
	}

	settingvar::variable<settingvar::model_bool<settingvar::yes_no>> dump_large(lsnes_vset, "avi-large",
		"AVI‣Large dump", false);
	settingvar::variable<settingvar::model_int<0, 32>> fixed_xfact(lsnes_vset, "avi-xfactor",
		"AVI‣Fixed X factor", 0);
	settingvar::variable<settingvar::model_int<0, 32>> fixed_yfact(lsnes_vset, "avi-yfactor",
		"AVI‣Fixed Y factor", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> dtb(lsnes_vset, "avi-top-border", "AVI‣Top padding", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> dbb(lsnes_vset, "avi-bottom-border",
		"AVI‣Bottom padding", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> dlb(lsnes_vset, "avi-left-border",
		"AVI‣Left padding", 0);
	settingvar::variable<settingvar::model_int<0, 8191>> drb(lsnes_vset, "avi-right-border", "AVI‣Right padding",
		0);
	settingvar::variable<settingvar::model_int<0, 999999999>> max_frames_per_segment(lsnes_vset, "avi-maxframes",
		"AVI‣Max frames per segment", 0);
#ifdef WITH_SECRET_RABBIT_CODE
	settingvar::enumeration soundrates {"nearest-common", "round-down", "round-up", "multiply",
		"High quality 44.1kHz", "High quality 48kHz"};
	settingvar::variable<settingvar::model_enumerated<&soundrates>> soundrate_setting(lsnes_vset, "avi-soundrate",
		"AVI‣Sound mode", 5);
#else
	settingvar::enumeration soundrates {"nearest-common", "round-down", "round-up", "multiply"};
	settingvar::variable<settingvar::model_enumerated<&soundrates>> soundrate_setting(lsnes_vset, "avi-soundrate",
		"AVI‣Sound mode", 2);
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

	struct resample_worker : public worker_thread
	{
		resample_worker(double _ratio, uint32_t _nch);
		~resample_worker();
		void entry();
		void sendblock(short* block, size_t frames);
		void sendend();
	private:
		std::vector<short> buffers;
		std::vector<float> buffers2;
		std::vector<float> buffers3;
		std::vector<short> buffers4;
		size_t bufused;
		double ratio;
		uint32_t nch;
		void* resampler;
	};

	struct avi_worker : public worker_thread
	{
		avi_worker(const struct avi_info& info);
		~avi_worker();
		void entry();
		void queue_video(uint32_t* _frame, uint32_t stride, uint32_t width, uint32_t height, uint32_t fps_n,
			uint32_t fps_d);
		void queue_audio(int16_t* data, size_t samples);
	private:
		avi_writer aviout;
		uint32_t* frame;
		uint32_t frame_width;
		uint32_t frame_stride;
		uint32_t frame_height;
		uint32_t frame_fps_n;
		uint32_t frame_fps_d;
		uint32_t segframes;
		uint32_t max_segframes;
		bool closed;
		avi_video_codec* ivcodec;
	};

#define WORKFLAG_QUEUE_FRAME 1
#define WORKFLAG_FLUSH 2
#define WORKFLAG_END 4

	avi_worker::avi_worker(const struct avi_info& info)
		: aviout(info.prefix, *info.vcodec, *info.acodec, info.sample_rate, info.audio_chans)
	{
		ivcodec = info.vcodec;
		segframes = 0;
		max_segframes = info.max_frames;
		fire();
	}

	avi_worker::~avi_worker()
	{
	}

	void avi_worker::queue_video(uint32_t* _frame, uint32_t stride, uint32_t width, uint32_t height,
		uint32_t fps_n, uint32_t fps_d)
	{
		rethrow();
		wait_busy();
		frame = _frame;
		frame_width = width;
		frame_stride = stride;
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
				f.stride = frame_stride;
				f.odata = new uint32_t[f.stride * frame_height + 16];
				f.data = f.odata;
				while(reinterpret_cast<size_t>(f.data) % 16)
					f.data++;
				f.width = frame_width;
				f.height = frame_height;
				f.fps_n = frame_fps_n;
				f.fps_d = frame_fps_d;
				f.force_break = (segframes == max_segframes && max_segframes > 0);
				if(f.force_break)
					segframes = 0;
				auto wc = get_wait_count();
				ivcodec->send_performance_counters(wc.first, wc.second);
				framebuffer::copy_swap4(reinterpret_cast<uint8_t*>(f.data), frame,
					f.stride * frame_height);
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

	resample_worker::resample_worker(double _ratio, uint32_t _nch)
	{
		ratio = _ratio;
		nch = _nch;
		buffers.resize(RESAMPLE_BUFFER * nch);
		buffers2.resize(RESAMPLE_BUFFER * nch);
		buffers3.resize((RESAMPLE_BUFFER * nch * ratio) + 128 * nch);
		buffers4.resize((RESAMPLE_BUFFER * nch * ratio) + 128 * nch);
		bufused = 0;
#ifdef WITH_SECRET_RABBIT_CODE
		int errc = 0;
		resampler = src_new(SRC_SINC_BEST_QUALITY, nch, &errc);
		if(errc)
			throw std::runtime_error(std::string("Error initing libsamplerate: ") +
				src_strerror(errc));
#else
		throw std::runtime_error("HQ sample rate conversion not available");
#endif
		fire();
	}

	resample_worker::~resample_worker()
	{
#ifdef WITH_SECRET_RABBIT_CODE
		src_delete((SRC_STATE*)resampler);
#endif
	}

	void resample_worker::sendend()
	{
		rethrow();
		set_workflag(WORKFLAG_END);
		request_quit();
	}

	void resample_worker::sendblock(short* block, size_t frames)
	{
again:
		rethrow();
		wait_busy();
		if(bufused + frames < RESAMPLE_BUFFER) {
			memcpy(&buffers[bufused * nch], block, 2 * nch * frames);
			bufused += frames;
			block += (frames * nch);
			frames = 0;
		} else if(bufused < RESAMPLE_BUFFER) {
			size_t processable = RESAMPLE_BUFFER - bufused;
			memcpy(&buffers[bufused * nch], block, 2 * nch * processable);
			block += (processable * nch);
			frames -= processable;
			bufused = RESAMPLE_BUFFER;
		}
		set_busy();
		set_workflag(WORKFLAG_QUEUE_FRAME);
		if(frames > 0)
			goto again;
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
			resampler_w = NULL;
			if(soundrate_setting == 4 || soundrate_setting == 5) {
				double ratio = 1.0 * audio_record_rate * soundrate.second / soundrate.first;
				sbuffer_fill = 0;
				sbuffer.resize(RESAMPLE_BUFFER * chans);
				resampler_w = new resample_worker(ratio, chans);
			}
		}

		~avi_avsnoop() throw()
		{
			if(resampler_w)
				delete resampler_w;
			delete worker;
			delete soxdumper;
		}

		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			uint32_t hscl = 1;
			uint32_t vscl = 1;
			auto scl = our_rom.rtype->get_scale_factors(_frame.get_width(), _frame.get_height());
			if(fixed_xfact != 0 && fixed_yfact != 0) {
				hscl = fixed_xfact;
				vscl = fixed_yfact;
			} else if(dump_large) {
				hscl = scl.first;
				vscl = scl.second;
			}
			if(!render_video_hud(dscr, _frame, hscl, vscl, dlb, dtb, drb, dbb, waitfn)) {
				akill += killed_audio_length(fps_n, fps_d, akillfrac);
				return;
			}
			worker->queue_video(dscr.rowptr(0), dscr.get_stride(), dscr.get_width(), dscr.get_height(),
				fps_n, fps_d);
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(akill) {
				akill--;
				return;
			}
			if(resampler_w) {
				if(!have_dumped_frame)
					return;
				sbuffer[sbuffer_fill++] = l;
				sbuffer[sbuffer_fill++] = r;
				if(sbuffer_fill == sbuffer.size()) {
					resampler_w->sendblock(&sbuffer[0], sbuffer_fill / chans);
					sbuffer_fill = 0;
				}
				soxdumper->sample(l, r);
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
				if(resampler_w)
					resampler_w->sendend();
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
		resample_worker* resampler_w;
	private:
		sox_dumper* soxdumper;
		framebuffer::fb<false> dscr;
		unsigned dcounter;
		bool have_dumped_frame;
		std::pair<uint32_t, uint32_t> soundrate;
		uint32_t audio_record_rate;
		std::vector<short> sbuffer;
		size_t sbuffer_fill;
		uint32_t chans;
	};

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

		std::string mode_extension(const std::string& mode) throw()
		{
			return "";	//Not interesting
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
			akill = 0;
			akillfrac = 0;
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

	void resample_worker::entry()
	{
		while(1) {
			wait_workflag();
			uint32_t work = clear_workflag(~WORKFLAG_QUIT_REQUEST);
			if(work & (WORKFLAG_QUEUE_FRAME | WORKFLAG_END)) {
#ifdef WITH_SECRET_RABBIT_CODE
again:
				SRC_DATA block;
				src_short_to_float_array(&buffers[0], &buffers2[0], bufused * nch);
				block.data_in = &buffers2[0];
				block.data_out = &buffers3[0];
				block.input_frames = bufused;
				block.input_frames_used = 0;
				block.output_frames = buffers3.size() / nch;
				block.output_frames_gen = 0;
				block.end_of_input = (work & WORKFLAG_END) ? 1 : 0;
				block.src_ratio = ratio;
				int errc = src_process((SRC_STATE*)resampler, &block);
				if(errc)
					throw std::runtime_error(std::string("Error using libsamplerate: ") +
					src_strerror(errc));
				src_float_to_short_array(&buffers3[0], &buffers4[0], block.output_frames_gen * nch);
				vid_dumper->worker->queue_audio(&buffers4[0], block.output_frames_gen * nch);
				if((size_t)block.input_frames_used < bufused)
					memmove(&buffers[0], &buffers[block.output_frames_gen * nch], (bufused -
						block.input_frames_used) * nch);
				bufused -= block.input_frames_used;
				if(block.output_frames_gen > 0 && work & WORKFLAG_END)
					goto again;	//Try again to get all the samples.
#endif
				clear_workflag(WORKFLAG_END | WORKFLAG_FLUSH | WORKFLAG_QUEUE_FRAME);
				clear_busy();
				if(work & WORKFLAG_END)
					return;
			}
			if(work == WORKFLAG_QUIT_REQUEST)
				break;
		}
	}
}
