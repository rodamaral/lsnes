#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/messages.hpp"
#include "video/tcp.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"

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

	void deleter_fn(void* f)
	{
		delete reinterpret_cast<std::ofstream*>(f);
	}

	class raw_dump_obj : public dumper_base
	{
	public:
		raw_dump_obj(master_dumper& _mdumper, dumper_factory_base& _fbase, const std::string& mode,
			const std::string& prefix)
			: dumper_base(_mdumper, _fbase), mdumper(_mdumper)
		{
			unsigned _mode = strhash(mode);
			bool _bits64 = IS_64(_mode);
			bool _swap = !IS_RGB(_mode);
			bool socket_mode = IS_TCP(_mode);

			if(prefix == "")
				throw std::runtime_error("Expected prefix");
			try {
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
					video = new std::ofstream(prefix + ".video", std::ios::out |
						std::ios::binary);
					audio = new std::ofstream(prefix + ".audio", std::ios::out |
						std::ios::binary);
					deleter = deleter_fn;
				}
				if(!*video || !*audio)
					throw std::runtime_error("Can't open output files");
				have_dumped_frame = false;
				swap = _swap;
				bits64 = _bits64;
				mdumper.add_dumper(*this);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting RAW dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping to " << prefix << std::endl;
		}
		~raw_dump_obj() throw()
		{
			mdumper.drop_dumper(*this);
			if(video)
				deleter(video);
			if(audio)
				deleter(audio);
			messages << "RAW Dump finished" << std::endl;
		}
		void on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
		{
			if(!video)
				return;
			uint32_t hscl, vscl;
			rpair(hscl, vscl) = our_rom.rtype->get_scale_factors(_frame.get_width(), _frame.get_height());
			if(bits64) {
				size_t w = dscr2.get_width();
				size_t h = dscr2.get_height();
				size_t s = dscr2.get_stride();
				std::vector<uint16_t> tmp;
				tmp.resize(8 * s + 8);
				uint32_t alignment = (16 - reinterpret_cast<size_t>(&tmp[0])) % 16 / 2;
				if(!render_video_hud(dscr2, _frame, fps_n, fps_d, hscl, vscl, 0, 0, 0, 0, NULL))
					return;
				for(size_t i = 0; i < h; i++) {
					if(!swap)
						framebuffer::copy_swap4(&tmp[alignment], dscr2.rowptr(i), s);
					else
						memcpy(&tmp[alignment], dscr2.rowptr(i), 8 * w);
					video->write(reinterpret_cast<char*>(&tmp[alignment]), 8 * w);
				}
			} else {
				size_t w = dscr.get_width();
				size_t h = dscr.get_height();
				size_t s = dscr2.get_stride();
				std::vector<uint8_t> tmp;
				tmp.resize(4 * s + 16);
				uint32_t alignment = (16 - reinterpret_cast<size_t>(&tmp[0])) % 16;
				if(!render_video_hud(dscr, _frame, fps_n, fps_d, hscl, vscl, 0, 0, 0, 0, NULL))
					return;
				for(size_t i = 0; i < h; i++) {
					if(!swap)
						framebuffer::copy_swap4(&tmp[alignment], dscr.rowptr(i), s);
					else
						memcpy(&tmp[alignment], dscr.rowptr(i), 4 * w);
					video->write(reinterpret_cast<char*>(&tmp[alignment]), 4 * w);
				}
			}
			if(!*video)
				messages << "Video write error" << std::endl;
			have_dumped_frame = true;
		}

		void on_sample(short l, short r)
		{
			if(have_dumped_frame && audio) {
				char buffer[4];
				serialization::s16b(buffer + 0, l);
				serialization::s16b(buffer + 2, r);
				audio->write(buffer, 4);
			}
		}
		void on_rate_change(uint32_t n, uint32_t d)
		{
			//Do nothing.
		}
		void on_gameinfo_change(const master_dumper::gameinfo& gi)
		{
			//Do nothing.
		}
		void on_end()
		{
			delete this;
		}
	private:
		std::ostream* audio;
		std::ostream* video;
		void (*deleter)(void* f);
		bool have_dumped_frame;
		struct framebuffer::fb<false> dscr;
		struct framebuffer::fb<true> dscr2;
		bool swap;
		bool bits64;
		master_dumper& mdumper;
	};

	class adv_raw_dumper : public dumper_factory_base
	{
	public:
		adv_raw_dumper() : dumper_factory_base("INTERNAL-RAW")
		{
			ctor_notify();
		}
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
		std::string mode_extension(const std::string& mode) throw()
		{
			return "";	//Nothing interesting.
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
		raw_dump_obj* start(master_dumper& _mdumper, const std::string& mode, const std::string& prefix)
			throw(std::bad_alloc, std::runtime_error)
		{
			return new raw_dump_obj(_mdumper, *this, mode, prefix);
		}
	} adv;

	adv_raw_dumper::~adv_raw_dumper() throw()
	{
	}
}
