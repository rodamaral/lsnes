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



namespace
{
	unsigned strhash(const std::string& str)
	{
		unsigned h = 0;
		for(size_t i = 0; i < str.length(); i++)
			h = (2 * h + static_cast<unsigned char>(str[i])) % 11;
		return h;
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
				video = audio = NULL;
				try {
					video = &videoaddr.connect();
					audio = &audioaddr.connect();
				} catch(...) {
					delete video;
					delete audio;
					throw;
				}
			} else {
				video = new std::ofstream(prefix + ".video", std::ios::out | std::ios::binary);
				audio = new std::ofstream(prefix + ".audio", std::ios::out | std::ios::binary);
			}
			if(!*video || !*audio)
				throw std::runtime_error("Can't open output files");
			have_dumped_frame = false;
			swap = _swap;
			bits64 = _bits64;
		}

		~raw_avsnoop() throw()
		{
			delete video;
			delete audio;
		}

		void on_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d)
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
			uint32_t hscl = (_frame.width < 400) ? 2 : 1;
			uint32_t vscl = (_frame.height < 400) ? 2 : 1;
			if(bits64) {
				render_video_hud(dscr2, _frame, hscl, vscl, r, g, b, 0, 0, 0, 0, NULL);
				for(size_t i = 0; i < dscr2.height; i++)
					video->write(reinterpret_cast<char*>(dscr2.rowptr(i)), 8 * dscr2.width);
			} else {
				render_video_hud(dscr, _frame, hscl, vscl, r, g, b, 0, 0, 0, 0, NULL);
				for(size_t i = 0; i < dscr.height; i++)
					video->write(reinterpret_cast<char*>(dscr.rowptr(i)), 4 * dscr.width);
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
			delete video;
			delete audio;
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
		bool have_dumped_frame;
		struct screen<false> dscr;
		struct screen<true> dscr2;
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
