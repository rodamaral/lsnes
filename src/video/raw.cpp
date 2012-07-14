#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "video/tcp.hpp"
#include "library/serialization.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <fstream>
#include <zlib.h>

#define IS_RGB(m) (((m) + ((m) >> 3)) & 2)
#define IS_64(m) (m % 5 < 2)
#define IS_TCP(m) (((m % 5) * (m % 5)) % 5 == 1)

std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height);

namespace
{
	unsigned strhash(const std::string& str)
	{
		unsigned h = 0;
		for(size_t i = 0; i < str.length(); i++)
			h = (2 * h + static_cast<unsigned char>(str[i])) % 11;
		return h;
	}

	void deleter_fn(void* f)
	{
		delete reinterpret_cast<std::ofstream*>(f);
	}

	class raw_avsnoop : public information_dispatch
	{
	public:
		raw_avsnoop(const std::string& prefix, bool _swap, bool _bits64, bool socket_mode)
			: information_dispatch("dump-raw")
		{
			enable_send_sound();
			if(socket_mode) {
				socket_address videoaddr = socket_address(prefix);
				socket_address audioaddr = videoaddr.next();
				deleter = socket_address::deleter();
				video = audio = NULL;
				try {
					video = &videoaddr.connect();
					audio = &audioaddr.connect();
				} catch(...) {
					deleter(video);
					deleter(audio);
					throw;
				}
			} else {
				video = new std::ofstream(prefix + ".video", std::ios::out | std::ios::binary);
				audio = new std::ofstream(prefix + ".audio", std::ios::out | std::ios::binary);
				deleter = deleter_fn;
			}
			if(!*video || !*audio)
				throw std::runtime_error("Can't open output files");
			have_dumped_frame = false;
			swap = _swap;
			bits64 = _bits64;
		}

		~raw_avsnoop() throw()
		{
			if(video)
				deleter(video);
			if(audio)
				deleter(audio);
		}

		void on_frame(struct framebuffer_raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			if(!video)
				return;
			unsigned magic;
			if(bits64)
				magic = 0x30201000U;
			else
				magic = 0x18100800U;
			unsigned r = (reinterpret_cast<unsigned char*>(&magic))[swap ? 2 : 0];
			unsigned g = (reinterpret_cast<unsigned char*>(&magic))[1];
			unsigned b = (reinterpret_cast<unsigned char*>(&magic))[swap ? 0 : 2];
			auto scl = get_scale_factors(_frame.get_width(), _frame.get_height());
			uint32_t hscl = scl.first;
			uint32_t vscl = scl.second;
			if(bits64) {
				size_t w = dscr2.get_width();
				size_t h = dscr2.get_height();
				render_video_hud(dscr2, _frame, hscl, vscl, r, g, b, 0, 0, 0, 0, NULL);
				for(size_t i = 0; i < h; i++)
					video->write(reinterpret_cast<char*>(dscr2.rowptr(i)), 8 * w);
			} else {
				size_t w = dscr.get_width();
				size_t h = dscr.get_height();
				render_video_hud(dscr, _frame, hscl, vscl, r, g, b, 0, 0, 0, 0, NULL);
				for(size_t i = 0; i < h; i++)
					video->write(reinterpret_cast<char*>(dscr.rowptr(i)), 4 * w);
			}
			if(!*video)
				messages << "Video write error" << std::endl;
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(have_dumped_frame && audio) {
				char buffer[4];
				write16sbe(buffer + 0, l);
				write16sbe(buffer + 2, r);
				audio->write(buffer, 4);
			}
		}

		void on_dump_end()
		{
			deleter(video);
			deleter(audio);
			video = NULL;
			audio = NULL;
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		std::ostream* audio;
		std::ostream* video;
		void (*deleter)(void* f);
		bool have_dumped_frame;
		struct framebuffer<false> dscr;
		struct framebuffer<true> dscr2;
		bool swap;
		bool bits64;
	};

	raw_avsnoop* vid_dumper;

	class adv_raw_dumper : public adv_dumper
	{
	public:
		adv_raw_dumper() : adv_dumper("INTERNAL-RAW") {information_dispatch::do_dumper_update(); }
		~adv_raw_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			for(size_t i = 0; i < (socket_address::supported() ? 2 : 1); i++)
				for(size_t j = 0; j < 2; j++)
					for(size_t k = 0; k < 2; k++)
						x.insert(std::string("") + (i ? "tcp" : "") + (j ? "bgr" : "rgb")
							+ (k ? "64" : "32"));
			return x;
		}

		unsigned mode_details(const std::string& mode) throw()
		{
			return IS_TCP(strhash(mode)) ? target_type_special : target_type_prefix;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "RAW";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			unsigned _mode = strhash(mode);
			std::string x = std::string((IS_RGB(_mode) ? "RGB" : "BGR")) +
				(IS_64(_mode) ? " 64-bit" : " 32-bit") + (IS_TCP(_mode) ? " over TCP/IP" : "");
			return x;
		}

		bool busy()
		{
			return (vid_dumper != NULL);
		}

		void start(const std::string& mode, const std::string& prefix) throw(std::bad_alloc,
			std::runtime_error)
		{
			unsigned _mode = strhash(mode);
			bool bits64 = IS_64(_mode);
			bool swap = !IS_RGB(_mode);
			bool sock = IS_TCP(_mode);

			if(prefix == "")
				throw std::runtime_error("Expected prefix");
			if(vid_dumper)
				throw std::runtime_error("RAW dumping already in progress");
			try {
				vid_dumper = new raw_avsnoop(prefix, swap, bits64, sock);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting RAW dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << std::endl;
			information_dispatch::do_dumper_update();
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No RAW video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "RAW Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending RAW dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;
	
	adv_raw_dumper::~adv_raw_dumper() throw()
	{
	}
}
