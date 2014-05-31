#include "video/sox.hpp"
#include "video/avi/writer.hpp"

#include "video/avi/codec.hpp"

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "lua/lua.hpp"
#include "library/minmax.hpp"
#include "library/workthread.hpp"
#include "core/messages.hpp"
#include "core/instance.hpp"
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

	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> dump_large(lsnes_setgrp, "avi-large",
		"AVI‣Large dump", false);
	settingvar::supervariable<settingvar::model_int<0, 32>> fixed_xfact(lsnes_setgrp, "avi-xfactor",
		"AVI‣Fixed X factor", 0);
	settingvar::supervariable<settingvar::model_int<0, 32>> fixed_yfact(lsnes_setgrp, "avi-yfactor",
		"AVI‣Fixed Y factor", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> dtb(lsnes_setgrp, "avi-top-border",
		"AVI‣Top padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> dbb(lsnes_setgrp, "avi-bottom-border",
		"AVI‣Bottom padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> dlb(lsnes_setgrp, "avi-left-border",
		"AVI‣Left padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 8191>> drb(lsnes_setgrp, "avi-right-border",
		"AVI‣Right padding", 0);
	settingvar::supervariable<settingvar::model_int<0, 999999999>> max_frames_per_segment(lsnes_setgrp,
		"avi-maxframes", "AVI‣Max frames per segment", 0);
#ifdef WITH_SECRET_RABBIT_CODE
	settingvar::enumeration soundrates {"nearest-common", "round-down", "round-up", "multiply",
		"High quality 44.1kHz", "High quality 48kHz"};
	settingvar::supervariable<settingvar::model_enumerated<&soundrates>> soundrate_setting(lsnes_setgrp,
		"avi-soundrate", "AVI‣Sound mode", 5);
#else
	settingvar::enumeration soundrates {"nearest-common", "round-down", "round-up", "multiply"};
	settingvar::supervariable<settingvar::model_enumerated<&soundrates>> soundrate_setting(lsnes_setgrp,
		"avi-soundrate", "AVI‣Sound mode", 2);
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

	struct avi_worker;

	struct resample_worker : public workthread::worker
	{
		resample_worker(avi_worker* _worker, double _ratio, uint32_t _nch);
		~resample_worker();
		void entry();
		void sendblock(short* block, size_t frames);
		void sendend();
		void set_ratio(double _ratio);
	private:
		std::vector<short> buffers;
		std::vector<float> buffers2;
		std::vector<float> buffers3;
		std::vector<short> buffers4;
		size_t bufused;
		double ratio;
		uint32_t nch;
		void* resampler;
		avi_worker* worker;
	};

	struct avi_worker : public workthread::worker
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
			uint32_t work = clear_workflag(~workthread::quit_request);
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
			if(work == workthread::quit_request) {
				if(!closed)
					aviout.close();
				closed = true;
				break;
			}
		}
	}

	resample_worker::resample_worker(avi_worker* _worker, double _ratio, uint32_t _nch)
		: worker(_worker)
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

	void resample_worker::set_ratio(double _ratio)
	{
		ratio = _ratio;
		buffers3.resize((RESAMPLE_BUFFER * nch * ratio) + 128 * nch);
		buffers4.resize((RESAMPLE_BUFFER * nch * ratio) + 128 * nch);
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

	class avi_dumper_obj : public dumper_base
	{
	public:
		avi_dumper_obj(master_dumper& _mdumper, dumper_factory_base& _fbase, const std::string& mode,
			const std::string& prefix)
			: dumper_base(_mdumper, _fbase), mdumper(_mdumper)
		{
			avi_video_codec_type* vcodec;
			avi_audio_codec_type* acodec;

			if(prefix == "")
				throw std::runtime_error("Expected prefix");
			struct avi_info info;
			info.audio_chans = 2;
			info.sample_rate = 32000;
			info.max_frames = max_frames_per_segment(*CORE().settings);
			info.prefix = prefix;
			rpair(vcodec, acodec) = find_codecs(mode);
			info.vcodec = vcodec->get_instance();
			info.acodec = acodec->get_instance();
			try {
				unsigned srate_setting = soundrate_setting(*CORE().settings);
				chans = info.audio_chans = 2;
				soundrate = mdumper.get_rate();
				audio_record_rate = info.sample_rate = get_rate(soundrate.first, soundrate.second,
					srate_setting);
				worker = new avi_worker(info);
				soxdumper = new sox_dumper(info.prefix + ".sox",
					static_cast<double>(soundrate.first) / soundrate.second, 2);
				dcounter = 0;
				have_dumped_frame = false;
				resampler_w = NULL;
				if(srate_setting == 4 || srate_setting == 5) {
					double ratio = 1.0 * audio_record_rate * soundrate.second / soundrate.first;
					sbuffer_fill = 0;
					sbuffer.resize(RESAMPLE_BUFFER * chans);
					resampler_w = new resample_worker(worker, ratio, chans);
				}
				mdumper.add_dumper(*this);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting AVI dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping AVI (" << vcodec->get_hname() << " / " << acodec->get_hname()
				<< ") to " << prefix << std::endl;
		}
		~avi_dumper_obj() throw()
		{
			if(worker) {
				if(resampler_w)
					resampler_w->sendend();
				worker->request_quit();
			}
			mdumper.drop_dumper(*this);
			if(resampler_w)
				delete resampler_w;
			delete worker;
			delete soxdumper;
			messages << "AVI Dump finished" << std::endl;
		}
		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			uint32_t hscl = 1, vscl = 1;
			unsigned fxfact = fixed_xfact(*CORE().settings);
			unsigned fyfact = fixed_yfact(*CORE().settings);
			if(fxfact != 0 && fyfact != 0) {
				hscl = fxfact;
				vscl = fyfact;
			} else if(dump_large(*CORE().settings)) {
				rpair(hscl, vscl) = our_rom.rtype->get_scale_factors(_frame.get_width(),
					_frame.get_height());
			}
			if(!render_video_hud(dscr, _frame, fps_n, fps_d, hscl, vscl, dlb(*CORE().settings),
				dtb(*CORE().settings), drb(*CORE().settings), dbb(*CORE().settings),
				[this]() -> void { this->worker->wait_busy(); }))
				return;
			worker->queue_video(dscr.rowptr(0), dscr.get_stride(), dscr.get_width(), dscr.get_height(),
				fps_n, fps_d);
			have_dumped_frame = true;
		}
		void on_sample(short l, short r)
		{
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
		void on_rate_change(uint32_t n, uint32_t d)
		{
			messages << "Warning: Changing AVI sound rate mid-dump is not supported!" << std::endl;
			//Try to do it anyway.
			soundrate = mdumper.get_rate();
			dcounter = 0;
			double ratio =  1.0 * audio_record_rate * soundrate.second / soundrate.first;
			if(resampler_w)
				resampler_w->set_ratio(ratio);
		}
		void on_gameinfo_change(const master_dumper::gameinfo& gi)
		{
			//Do nothing.
		}
		void on_end()
		{
			delete this;
		}
		avi_worker* worker;
		resample_worker* resampler_w;
	private:
		master_dumper& mdumper;
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

	class adv_avi_dumper : public dumper_factory_base
	{
	public:
		adv_avi_dumper() : dumper_factory_base("INTERNAL-AVI")
		{
			ctor_notify();
		}
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
			avi_video_codec_type* vcodec;
			avi_audio_codec_type* acodec;
			rpair(vcodec, acodec) = find_codecs(mode);
			return vcodec->get_hname() + std::string(" / ") + acodec->get_hname();
		}
		avi_dumper_obj* start(master_dumper& _mdumper, const std::string& mode, const std::string& prefix)
			throw(std::bad_alloc, std::runtime_error)
		{
			return new avi_dumper_obj(_mdumper, *this, mode, prefix);
		}
	} adv;

	adv_avi_dumper::~adv_avi_dumper() throw()
	{
	}

	void resample_worker::entry()
	{
		while(1) {
			wait_workflag();
			uint32_t work = clear_workflag(~workthread::quit_request);
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
				worker->queue_audio(&buffers4[0], block.output_frames_gen * nch);
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
			if(work == workthread::quit_request)
				break;
		}
	}
}
