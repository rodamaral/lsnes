#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "library/serialization.hpp"
#include "video/tcp.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <fstream>
#include <deque>
#include <zlib.h>
#define INBUF_PIXELS 4096
#define OUTBUF_ADVANCE 4096


namespace
{
	setting_var<setting_var_model_int<0,9>> clevel(lsnes_vset, "jmd-compression", "JMD‣Compression", 7);
	uint64_t akill = 0;
	double akillfrac = 0;

	void deleter_fn(void* f)
	{
		delete reinterpret_cast<std::ofstream*>(f);
	}

	class jmd_avsnoop : public information_dispatch
	{
	public:
		jmd_avsnoop(const std::string& filename, bool tcp_flag) throw(std::bad_alloc)
			: information_dispatch("dump-jmd")
		{
			enable_send_sound();
			complevel = clevel;
			if(tcp_flag) {
				jmd = &(socket_address(filename).connect());
				deleter = socket_address::deleter();
			} else {
				jmd = new std::ofstream(filename.c_str(), std::ios::out | std::ios::binary);
				deleter = deleter_fn;
			}
			if(!*jmd)
				throw std::runtime_error("Can't open output JMD file.");
			last_written_ts = 0;
			//Write the segment tables.
			//Stream #0 is video.
			//Stream #1 is PCM audio.
			//Stream #2 is Gameinfo.
			//Stream #3 is Dummy.
			char header[] = {
				/* Magic */
				-1, -1, 0x4A, 0x50, 0x43, 0x52, 0x52, 0x4D, 0x55, 0x4C, 0x54, 0x49, 0x44, 0x55, 0x4D,
				0x50,
				/* Channel count. */
				0x00, 0x04,
				/* Video channel header. */
				0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 'v', 'i',
				/* Audio channel header. */
				0x00, 0x01, 0x00, 0x01, 0x00, 0x02, 'a', 'u',
				/* Gameinfo channel header. */
				0x00, 0x02, 0x00, 0x05, 0x00, 0x02, 'g', 'i',
				/* Dummy channel header. */
				0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 'd', 'u'
			};
			jmd->write(header, sizeof(header));
			if(!*jmd)
				throw std::runtime_error("Can't write JMD header and segment table");
			have_dumped_frame = false;
			audio_w = 0;
			audio_n = 0;
			video_w = 0;
			video_n = 0;
			maxtc = 0;
			soundrate = get_sound_rate();
			try {
				on_gameinfo(get_gameinfo());
			} catch(std::exception& e) {
				messages << "Can't write gameinfo: " << e.what() << std::endl;
			}
		}

		~jmd_avsnoop() throw()
		{
			try {
				on_dump_end();
			} catch(...) {
			}
		}

		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			if(!render_video_hud(dscr, _frame, 1, 1, 0, 8, 16, 0, 0, 0, 0, NULL)) {
				akill += killed_audio_length(fps_n, fps_d, akillfrac);
				return;
			}
			frame_buffer f;
			f.ts = get_next_video_ts(fps_n, fps_d);
			//We'll compress the frame here.
			f.data = compress_frame(dscr.rowptr(0), dscr.get_width(), dscr.get_height());
			frames.push_back(f);
			flush_buffers(false);
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(akill) {
				akill--;
				return;
			}
			uint64_t ts = get_next_audio_ts();
			if(have_dumped_frame) {
				sample_buffer s;
				s.ts = ts;
				s.l = l;
				s.r = r;
				samples.push_back(s);
				flush_buffers(false);
			}
		}

		void on_dump_end()
		{
			if(!jmd)
				return;
			flush_buffers(true);
			if(last_written_ts > maxtc) {
				deleter(jmd);
				jmd = NULL;
				return;
			}
			char dummypacket[8] = {0x00, 0x03};
			write32ube(dummypacket + 2, maxtc - last_written_ts);
			last_written_ts = maxtc;
			jmd->write(dummypacket, sizeof(dummypacket));
			if(!*jmd)
				throw std::runtime_error("Can't write JMD ending dummy packet");
			deleter(jmd);
			jmd = NULL;
		}

		void on_gameinfo(const struct gameinfo_struct& gi)
		{
			std::string authstr;
			for(size_t i = 0; i < gi.get_author_count(); i++) {
				if(i != 0)
					authstr = authstr + ", ";
				authstr = authstr + gi.get_author_short(i);
			}
			//TODO: Implement
		}

		bool get_dumper_flag() throw()
		{
			return true;
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

		framebuffer::fb<false> dscr;
		unsigned dcounter;
		bool have_dumped_frame;
		uint64_t audio_w;
		uint64_t audio_n;
		uint64_t video_w;
		uint64_t video_n;
		uint64_t maxtc;
		std::pair<uint32_t, uint32_t> soundrate;
		struct frame_buffer
		{
			uint64_t ts;
			std::vector<char> data;
		};
		struct sample_buffer
		{
			uint64_t ts;
			short l;
			short r;
		};

		std::deque<frame_buffer> frames;
		std::deque<sample_buffer> samples;

		std::vector<char> compress_frame(uint32_t* memory, uint32_t width, uint32_t height)
		{
			std::vector<char> ret;
			z_stream stream;
			memset(&stream, 0, sizeof(stream));
			if(deflateInit(&stream, complevel) != Z_OK)
				throw std::runtime_error("Can't initialize zlib stream");

			size_t usize = 4;
			ret.resize(4);
			write16ube(&ret[0], width);
			write16ube(&ret[2], height);
			uint8_t input_buffer[4 * INBUF_PIXELS];
			size_t ptr = 0;
			size_t pixels = static_cast<size_t>(width) * height;
			bool input_clear = true;
			bool flushed = false;
			size_t bsize = 0;
			while(1) {
				if(input_clear) {
					size_t pixel = ptr;
					for(unsigned i = 0; i < INBUF_PIXELS && pixel < pixels; i++, pixel++)
						write32ule(input_buffer + (4 * i), memory[pixel]);
					bsize = pixel - ptr;
					ptr = pixel;
					input_clear = false;
					//Now the input data to compress is in input_buffer, bsize elements.
					stream.next_in = reinterpret_cast<uint8_t*>(input_buffer);
					stream.avail_in = 4 * bsize;
				}
				if(!stream.avail_out) {
					if(flushed)
						usize += (OUTBUF_ADVANCE - stream.avail_out);
					flushed = true;
					ret.resize(usize + OUTBUF_ADVANCE);
					stream.next_out = reinterpret_cast<uint8_t*>(&ret[usize]);
					stream.avail_out = OUTBUF_ADVANCE;
				}
				int r = deflate(&stream, (ptr == pixels) ? Z_FINISH : 0);
				if(r == Z_STREAM_END)
					break;
				if(r != Z_OK)
					throw std::runtime_error("Can't deflate data");
				if(!stream.avail_in)
					input_clear = true;
			}
			usize += (OUTBUF_ADVANCE - stream.avail_out);
			deflateEnd(&stream);

			ret.resize(usize);
			return ret;
		}

		void flush_buffers(bool force)
		{
			while(!frames.empty() || !samples.empty()) {
				if(frames.empty() || samples.empty()) {
					if(!force)
						return;
					else if(!frames.empty()) {
						frame_buffer& f = frames.front();
						flush_frame(f);
						frames.pop_front();
					} else if(!samples.empty()) {
						sample_buffer& s = samples.front();
						flush_sample(s);
						samples.pop_front();
					}
					continue;
				}
				frame_buffer& f = frames.front();
				sample_buffer& s = samples.front();
				if(f.ts <= s.ts) {
					flush_frame(f);
					frames.pop_front();
				} else {
					flush_sample(s);
					samples.pop_front();
				}
			}
		}

		void flush_frame(frame_buffer& f)
		{
			//Channel 0, minor 1.
			char videopacketh[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
			write32ube(videopacketh + 2, f.ts - last_written_ts);
			last_written_ts = f.ts;
			unsigned lneed = 0;
			uint64_t datasize = f.data.size();	//Possibly upcast to avoid warnings.
			for(unsigned shift = 63; shift > 0; shift -= 7)
				if(datasize >= (1ULL << shift))
					videopacketh[7 + lneed++] = 0x80 | ((datasize >> shift) & 0x7F);
			videopacketh[7 + lneed++] = (datasize & 0x7F);

			jmd->write(videopacketh, 7 + lneed);
			if(!*jmd)
				throw std::runtime_error("Can't write JMD video packet header");
			if(datasize > 0)
				jmd->write(&f.data[0], datasize);
			if(!*jmd)
				throw std::runtime_error("Can't write JMD video packet body");
		}

		void flush_sample(sample_buffer& s)
		{
			//Channel 1, minor 1, payload 4.
			char soundpacket[12] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04};
			write32ube(soundpacket + 2, s.ts - last_written_ts);
			last_written_ts = s.ts;
			write16sbe(soundpacket + 8, s.l);
			write16sbe(soundpacket + 10, s.r);
			jmd->write(soundpacket, sizeof(soundpacket));
			if(!*jmd)
				throw std::runtime_error("Can't write JMD sound packet");
		}

		std::ostream* jmd;
		void (*deleter)(void* f);
		uint64_t last_written_ts;
		unsigned complevel;
	};

	jmd_avsnoop* vid_dumper;

	class adv_jmd_dumper : public adv_dumper
	{
	public:
		adv_jmd_dumper() : adv_dumper("INTERNAL-JMD") {information_dispatch::do_dumper_update(); }
		~adv_jmd_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			x.insert("file");
			x.insert("tcp");
			return x;
		}

		unsigned mode_details(const std::string& mode) throw()
		{
			return (mode == "tcp") ? target_type_special : target_type_file;
		}

		std::string mode_extension(const std::string& mode) throw()
		{
			return "jmd";	//Ignored if tcp mode.
		}

		std::string name() throw(std::bad_alloc)
		{
			return "JMD";
		}

		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			return (mode == "tcp") ? "over TCP/IP" : "to file";
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& prefix) throw(std::bad_alloc,
			std::runtime_error)
		{
			if(prefix == "")
				throw std::runtime_error("Expected target");
			if(vid_dumper)
				throw std::runtime_error("JMD dumping already in progress");
			try {
				vid_dumper = new jmd_avsnoop(prefix, mode == "tcp");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting JMD dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << " at level " << clevel << std::endl;
			information_dispatch::do_dumper_update();
			akill = 0;
			akillfrac = 0;
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No JMD video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "JMD Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending JMD dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;

	adv_jmd_dumper::~adv_jmd_dumper() throw()
	{
	}
}
