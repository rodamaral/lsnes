#include "lsnes.hpp"

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "interface/core.hpp"
#include "library/serialization.hpp"
#include "video/tcp.hpp"

#include <iomanip>
#include <cassert>
#include <cstring>
#include <sstream>
#include <zlib.h>
#include <sstream>
#include <fstream>
#include <stdexcept>

#define CUTOFF 2100000000
#define SDUMP_FLAG_HIRES 1
#define SDUMP_FLAG_INTERLACED 2
#define SDUMP_FLAG_OVERSCAN 4
#define SDUMP_FLAG_PAL 8

namespace
{
	void deleter_fn(void* f)
	{
		delete reinterpret_cast<std::ofstream*>(f);
	}

	class sdmp_avsnoop : public information_dispatch
	{
	public:
		sdmp_avsnoop(const std::string& prefix, const std::string& mode) throw(std::bad_alloc,
			std::runtime_error)
			: information_dispatch("dump-sdmp")
		{
			enable_send_sound();
			oprefix = prefix;
			sdump_ss = (mode != "ms");
			ssize = 0;
			next_seq = 0;
			dumped_pic = false;
			if(mode == "tcp") {
				out = &(socket_address(prefix).connect());
				deleter = socket_address::deleter();
			} else {
				out = NULL;
				deleter = deleter_fn;
			}
		}

		~sdmp_avsnoop() throw()
		{
			try {
				if(out)
					deleter(out);
			} catch(...) {
			}
		}

		void on_raw_frame(const uint32_t* raw, bool hires, bool interlaced, bool overscan, unsigned region)
		{
			unsigned flags = 0;
			flags |= (hires ? SDUMP_FLAG_HIRES : 0);
			flags |= (interlaced ? SDUMP_FLAG_INTERLACED : 0);
			flags |= (overscan ? SDUMP_FLAG_OVERSCAN : 0);
			flags |= (region == VIDEO_REGION_PAL ? SDUMP_FLAG_PAL : 0);
			unsigned char tbuffer[2049];
			if(!out || (ssize > CUTOFF && !sdump_ss)) {
				std::cerr << "Starting new segment" << std::endl;
				if(out)
					deleter(out);
				std::ostringstream str;
				if(sdump_ss)
					str << oprefix;
				else
					str << oprefix << "_" << std::setw(4) << std::setfill('0') << (next_seq++)
						<< ".sdmp";
				std::string str2 = str.str();
				out = new std::ofstream(str2.c_str(), std::ios::out | std::ios::binary);
				if(!*out)
					throw std::runtime_error("Failed to open '" + str2 + "'");
				write32ube(tbuffer, 0x53444D50U);
				write32ube(tbuffer + 4, emucore_get_video_rate().first);
				write32ube(tbuffer + 8, emucore_get_audio_rate().first);
				out->write(reinterpret_cast<char*>(tbuffer), 12);
				if(!out)
					throw std::runtime_error("Failed to write header to '" + str2 + "'");
				ssize = 12;
			}
			dumped_pic = true;
			tbuffer[0] = flags;
			for(unsigned i = 0; i < 512; i++) {
				for(unsigned j = 0; j < 512; j++)
					write32ube(tbuffer + (4 * j + 1), raw[512 * i + j]);
				out->write(reinterpret_cast<char*>(tbuffer + (i ? 1 : 0)), i ? 2048 : 2049);
			}
			if(!*out)
				throw std::runtime_error("Failed to write frame");
			ssize += 1048577;
		}

		void on_sample(short l, short r)
		{
			if(!out || !dumped_pic)
				return;
			unsigned char pkt[5];
			pkt[0] = 16;
			write16sbe(pkt + 1, l);
			write16sbe(pkt + 3, r);
			out->write(reinterpret_cast<char*>(pkt), 5);
			if(!*out)
				throw std::runtime_error("Failed to write sample");
			ssize += 5;
		}

		void on_dump_end()
		{
			deleter(out);
			out = NULL;
		}

		bool get_dumper_flag() throw()
		{
			return true;
		}
	private:
		std::string oprefix;
		bool sdump_ss;
		bool dumped_pic;
		uint64_t ssize;
		uint64_t next_seq;
		void (*deleter)(void* f);
		std::ostream* out;
	};

	sdmp_avsnoop* vid_dumper;

	class adv_sdmp_dumper : public adv_dumper
	{
	public:
		adv_sdmp_dumper() : adv_dumper("INTERNAL-SDMP") { information_dispatch::do_dumper_update(); }
		~adv_sdmp_dumper() throw();
		std::set<std::string> list_submodes() throw(std::bad_alloc)
		{
			std::set<std::string> x;
			x.insert("ss");
			x.insert("ms");
			x.insert("tcp");
			return x;
		}

		unsigned mode_details(const std::string& mode) throw()
		{
			if(mode == "ss")
				return target_type_file;
			if(mode == "ms")
				return target_type_prefix;
			if(mode == "tcp")
				return target_type_special;
			return target_type_mask;
		}

		std::string name() throw(std::bad_alloc)
		{
			return "SDMP";
		}
		
		std::string modename(const std::string& mode) throw(std::bad_alloc)
		{
			if(mode == "ss")
				return "Single-Segment";
			if(mode == "ms")
				return "Multi-Segment";
			if(mode == "tcp")
				return "over TCP/IP";
			return "What?";
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
				throw std::runtime_error("SDMP Dump already in progress");
			try {
				vid_dumper = new sdmp_avsnoop(prefix, mode);
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Error starting SDMP dump: " << e.what();
				throw std::runtime_error(x.str());
			}
			messages << "Dumping SDMP (" << mode << ") to " << prefix << std::endl;
			information_dispatch::do_dumper_update();
		}

		void end() throw()
		{
			if(!vid_dumper)
				throw std::runtime_error("No SDMP video dump in progress");
			try {
				vid_dumper->on_dump_end();
				messages << "SDMP Dump finished" << std::endl;
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::exception& e) {
				messages << "Error ending SDMP dump: " << e.what() << std::endl;
			}
			delete vid_dumper;
			vid_dumper = NULL;
			information_dispatch::do_dumper_update();
		}
	} adv;
	
	adv_sdmp_dumper::~adv_sdmp_dumper() throw()
	{
	}
}
